# ESP8266 Weather Station

This project is my take on the ever popular Weather Station. Mine is based on an ESP8266, a .96 inch OLED display and a BME280 environmental sensor array.  Weather Stations seem to be a very popular project. Mine differentiates itself from the others by using a BME280 sensor array instead of the popular DHT22 temperature and humidity sensor. The BME280 has a temperature, humidity and air pressure sensor. It also uses the I2C interface. The .96 inch OLED display used is also I2C. It can be purchased as either I2C or SPI or both. I went with the I2C version to simplify the wiring. With both the OLED display and the BME280 using I2C and 3.3V is was very easy to make a Y cable to connect both devices to the ESP8266.

While developing this project I came across multiple weather station projects on the Internet that use the ESP8266, the same OLED display and the BME280. So this is not an original idea, but it is an original implementation.

The BME280 provides inside environment data. Outside weather information is obtained from OpenWeatherMap.org. You will need to signup with OpenWeatherMap.org to get a key to access the weather data. They offer a free service, which is what I used. See How to get an OpenWeatherMap ID.pdf for instructions on how to obtain a key.

An NTP time server is used to get the time of day and day-of-the-week.

The weather, time and environment data are displayed on the OLED display. Each piece of information has its own formatted screen. The screens are displayed for five seconds before switching to another. OpenWeatherMap.org is accessed every fifteen minutes to refresh the weather information. The BME280 is read about every fifty-five seconds.

The ESP8266 is also setup to be a web server. All of the weather information can be accessed using a browser from your phone, tablet of computer. One of the screens that is displayed shows the IP address of the web server.
 
## Getting Started

The file WeatherStation.pdf has detailed instructions and photos to aid in building your own. It also has links to where to find the libraries used.
The folder WeatherStation has the Arduino source code for the ESP8266.

### Prerequisites

You will need the Arduino IDE Version 1.8.0 or later with the ESP8266 boards addon.

The file Bom.pdf has a list of all the parts used and where I got them from as well as some information on crimper tools to use with Dupont crimps.

