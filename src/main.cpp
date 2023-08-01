#include "LittleFS.h"
#include "sntp.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <QMC5883LCompass.h>
#include <WiFiManager.h>

BearSSL::X509List *client_crt;
BearSSL::PrivateKey *client_key;
BearSSL::X509List *rootCert;

QMC5883LCompass magnet_sensor;

WiFiClientSecure wiFiClient;
PubSubClient pubSubClient(wiFiClient);

char *topic;

char *aws_endpoint;
char *client_id;

void setCurrentTime() {
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    // Serial.print("Waiting for NTP time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        // Serial.print(".");
        now = time(nullptr);
    }
    // Serial.println("");
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    // Serial.print("Current time: ");
    // Serial.print(asctime(&timeinfo));
}

void load_file(const char *filename, char **buffer) {
    if (LittleFS.exists(filename)) {
        auto file = LittleFS.open(filename, "r");
        if (!file) {
            Serial.print(filename);
            Serial.println(" file failed to open");
            return;
        }

        *buffer = (char *)malloc(file.size());
        size_t bytesRead = file.readBytes(*buffer, file.size());
        (*buffer)[bytesRead] = '\0';
        file.close();        
        
        Serial.println(*buffer);
    } else {
        Serial.print(filename);
        Serial.println(" does not exist");
    }
}

void load_cert(const char *filename, BearSSL::X509List **cert) {
    if (LittleFS.exists(filename)) {
        auto file = LittleFS.open(filename, "r");
        if (!file) {
            Serial.print(filename);
            Serial.println(" file failed to open");
            return;
        }

        *cert = new BearSSL::X509List(file);
        file.close();
    } else {
        Serial.print(filename);
        Serial.println(" does not exist");
    }
}

void load_key(const char *filename, BearSSL::PrivateKey **key) {
    if (LittleFS.exists(filename)) {
        auto file = LittleFS.open(filename, "r");
        if (!file) {
            Serial.print(filename);
            Serial.println(" file failed to open");
            return;
        }

        *key = new BearSSL::PrivateKey(file);
        file.close();
    } else {
        Serial.print(filename);
        Serial.println(" does not exist");
    }
}

void load_config() {
    if (LittleFS.begin()) {
        load_file("aws", &aws_endpoint);
        load_file("id", &client_id);

        load_cert("client_cert", &client_crt);
        load_cert("root_cert", &rootCert);

        load_key("key", &client_key);

        LittleFS.end();
    } else {
        Serial.println("LittleFS failed to begin");
    }
}

void save_file(const char *filename, const char *content) {
    if (LittleFS.exists(filename)) {
        if (!LittleFS.remove(filename)) {
            Serial.println("Failed to delete existing file.");
            return;
        }
    }

    File file = LittleFS.open(filename, "w");
    if (!file) {
        Serial.println("Failed to create file.");
        return;
    }

    size_t bytesWritten = file.write(content);
    if (bytesWritten == strlen(content)) {
        Serial.println("File written successfully.");
    } else {
        Serial.println("Failed to write file.");
    }

    file.close();
}

void fresh_setup() {
    WiFiManagerParameter awsEndpointParam("awsEndpoint", "", "", 128);
    WiFiManagerParameter clientIdParam("deviceId", "", "", 128);
    WiFiManagerParameter clientCertParam("clientCert", "", "", 1500);
    WiFiManagerParameter privateKeyParam("privateKey", "", "", 2000);
    WiFiManagerParameter rootCaParam("rootCa", "", "", 1500);

    Serial.println("Connecting to WiFi");
    WiFiManager wifiManager;
    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.addParameter(&awsEndpointParam);
    wifiManager.addParameter(&clientIdParam);
    wifiManager.addParameter(&clientCertParam);
    wifiManager.addParameter(&privateKeyParam);
    wifiManager.addParameter(&rootCaParam);
    wifiManager.autoConnect();
    Serial.println("WiFi connected, IP address: ");
    Serial.println(WiFi.localIP());

    if (LittleFS.begin()) {
        save_file("aws", awsEndpointParam.getValue());
        save_file("id", clientIdParam.getValue());
        save_file("client_cert", clientCertParam.getValue());
        save_file("root_cert", rootCaParam.getValue());
        save_file("key", privateKeyParam.getValue());
    } else {
        Serial.println("Failed to begin LittleFS");
    }
}

void regular_setup() {
    WiFiManager wifiManager;
    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.autoConnect();
    Serial.println("WiFi connected, IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() {
    Serial.begin(921600);
    Serial.println();

    load_config();

    if (client_key == nullptr) {
        Serial.println("no key loaded, performing fresh setup");
        fresh_setup();
        ESP.reset();
    } else {
        Serial.println("key loaded, assuming setup configuration");
        regular_setup();
    }

    Serial.println("Config loaded");

    Serial.println("Defining topic");
    Serial.println(client_id);
    topic = (char *)malloc((strlen(client_id) + 5) * sizeof(char));
    strcpy(topic, client_id);
    strcat(topic, "/data\0");
    Serial.println(topic);

    Serial.println("Setting certificate");
    wiFiClient.setClientRSACert(client_crt, client_key);
    wiFiClient.setTrustAnchors(rootCert);

    Serial.println("Setting server");
    pubSubClient.setServer(aws_endpoint, 8883);

    Serial.println("Getting current time");
    sntp_stop(); // speeds up the connection with UDP servers
    sntp_init(); // speeds up the connection with UDP servers
    setCurrentTime();

    Serial.println("Starting sensor");
    magnet_sensor.init();
    Serial.println("Setup done!");
}

void pubSubCheckConnect() {
    if (!pubSubClient.connected()) {
        Serial.print("PubSubClient connecting to: ");
        Serial.print(aws_endpoint);
        while (!pubSubClient.connected()) {
            Serial.print(".");
            pubSubClient.connect(client_id);
        }
        Serial.println(" connected");
    }
    pubSubClient.loop();
}

void addMeasurement(JsonArray &array, const char *name, float value) {
    // Create a JsonObject for the measurement
    JsonObject measurement = array.createNestedObject();

    // Add the name and value to the measurement
    measurement["name"] = name;
    measurement["value"] = value;
}

char *to_json(float magnet_x, float magnet_y, float magnet_z, float abs_value) {
    DynamicJsonDocument doc(256);

    // Create an array for measurements
    JsonArray measurements = doc.createNestedArray("measurements");

    // Add measurements to the array
    addMeasurement(measurements, "x", magnet_x);
    addMeasurement(measurements, "y", magnet_y);
    addMeasurement(measurements, "z", magnet_z);
    addMeasurement(measurements, "abs", abs_value);

    // Serialize the JSON document to a char array
    const size_t bufferSize = 256;
    char *buffer = new char[bufferSize];
    serializeJson(doc, buffer, bufferSize);

    return buffer;
}

void loop() {
    Serial.println("loop");
    pubSubCheckConnect();

    int x, y, z;

    // Read compass values
    magnet_sensor.read();

    // Return XYZ readings
    x = magnet_sensor.getX();
    y = magnet_sensor.getY();
    z = magnet_sensor.getZ();
    double abs = sqrt((x * x + y * y + z * z));

    Serial.printf("%d, %d, %d, %f\n", x, y, z, abs);
    auto message = to_json(x, y, z, abs);
    Serial.println(*message);
    auto published = pubSubClient.publish(topic, message);
    if (!published) {
        Serial.println("Publish failed");
    }
    free(message);

    delay(1000);
}