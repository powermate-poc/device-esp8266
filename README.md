# PowerMate Device: ESP8266

Includes the source code for the ESP8266 PowerMate device.

## Setup

### Requirements

You must have an ESP8266 with an magnetometer.
The following pins from the sensor must be connected to the ESP8266:

| Sensor | ESP8266 |
|--------|---------|
| VCC    | 5V      |
| GND    | G       |
| SCL    | D2      |
| SDA    | D1      |

Pins not mentioned must not be connected.

It is recommended to use VSCode with the PlatformIO extension to build and upload the code to the ESP8266.
On some platforms it might be required to also install the [Arduino IDE](https://www.arduino.cc/en/software), due to missing drivers.

### Possible errors

When uploading fails, try to alter the upload and monitor speed in the `platformio.ini` file.
Set it to for example `115200`, and replace occurrences in the code.

## Usage

In VSCode select the `huzzah` environment.
In the platformio toolbar, click on `Upload and Monitor` to build and upload the code to the ESP8266.
A terminal displaying the ESP8266 output should open.

In order to restart the ESP8266, press the reset button on the ESP8266.

In order to perform a full reset of the ESP8266, select the `Erase Flash` option in the platformio toolbar, and restart the ESP8266.
