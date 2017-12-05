// Author: James R. Behrens
// WeatherStation for ESP8266 using I2C OLED SSD1306 display and I2C BME280. Will not work for Arduino
// micros. Uses the brzo_I2C driver library for ESP8266. Outside weather information is obtained from
// OpenWeatherMap.org. Time is obtained from an NTP server.
// Data is displayed on the OLED display and the web server shows the same information.

// The .96" OLED display that is used is 128x64 pixels. If you change any of the messages and your text
// does not show on the display it is most likey due to your message being too long for the font you
// are using. Select a smaller font or reduce the number of characters in your message. The font used
// is a proportional font, therefore kerning will determine the number of characters that will fit on
// one line.
// These are approximations, your mileage may vary.
// ArialMT_Plain_10 - about 22 characters, 8 lines
// ArialMT_Plain_16 - about 14 charaters, 4 lines
// ArialMT_Plain_24 - about 10 characters, 2 lines

// The BME280 board that I used has an I2C address of 0x76. The Adafruit board used address 0x77.
// If you use the Adafruit board then change BMEADRS in the BME280.h file to 0x77 or whatever is
// correct for your board.

// The OpenWeatherMap web page should not be accessed more than once every ten minutes. Not all of
// the data returned is used. If you wnat to you can parse out sunrise and sunset times and ceate
// showXxxxx() function for them and add them to the array of functions to show data on the OLED
// display.

// Day Light Savings time starts on the second Sunday in March and ends on the second Sunday in
// November.

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

#include "SSD1306Brzo.h"
#include "BME280.h"       // register definitions for BME280

// ESP8266-12F (LoLin) board
// Select NodeMCU 1.0 (ESP-12E Module) for LoLin Board

// Use D0-D10 for GPIO names.
// Use D0-D10 for GPIO names.
// D4 -> built-in blue LED

// Initialize the OLED display using brzo_i2c library. Library is bit-banged so any pins can be used.
// using D1 & D2 leaves the UART and SPI pins free.
#define SDA   D2    // GPIO4
#define SCL   D1    // GPIO5

// SPI pins
// SCLK -> D5
// MISO -> D6
// MOSI -> D7
// CS   -> D8

// OLED display dimensions in pixels
#define OLED_X    128
#define OLED_Y    64

#define SEALEVELPRESSURE_HPA (1013.25)

// put the SSID and password for your WiFi network here
const char* ssid = "Birdland";
const char* password = "kcntr837b7";

// put your OpenWeatherMap.com Key and the zip code here
const char* owmkey   = "c275c977517f6e3d343f10611f8ab103";
const char* owmzip = "14502,us";


// Create the OLed display object using the brzo_I2C library
SSD1306Brzo display(0x3C, SDA, SCL);

// create the WiFiServer object, use port 80
WiFiServer server(80);  // server on port 80

// Raw time returned is in seconds since 1970. To adjust for timezones subtract
// the number of seconds difference for your timezone. Negative value will
// subtract time, positive value will add time
#define TZ_EASTERN  -18000    // number of seconds in five hours
#define TZ_CENTRAL  -14400    // number of seconds in four hours
#define TZ_MOUTAIN  -10800    // number of seconds in three hours
#define TZ_PACIFIC  -7200     // number of seconds in two hours

// Adjust time for your timezone by changing TZ_EASTERN to one of the other values.
#define TIMEZONE    TZ_EASTERN    // change this to your timezone
#define DST         1             // set to 0 to disable daylight ssavings time

// create NTP client and set your timezone, default NTP server is; time.nist.gov
WiFiUDP    ntpUDP;
NTPClient  ntpClient(ntpUDP, TIMEZONE);

// create OpenWeatherMap client
WiFiClient  owmClient;

//===== inside environmental conditions from BME280 =====
float temperature;
float humidity;
float pressure;
float lastPressure;

//===== outside conditions from OpenWeatherMap.com =====
String  Description;
double  OutTemperature;
double  WindSpeed;
int     WindDirection;
double  OutPressure;
double  OutLastPressure;
double  OutHumidity;

// BME280 transfer buffer
uint8_t buffer[20];

//=====BME280 Calibration Data =====
int32_t   dig_T1;
int32_t   dig_T2;
int32_t   dig_T3;

uint16_t  dig_P1;
int16_t   dig_P2;
int16_t   dig_P3;
int16_t   dig_P4;
int16_t   dig_P5;
int16_t   dig_P6;
int16_t   dig_P7;
int16_t   dig_P8;
int16_t   dig_P9;

uint8_t   dig_H1;
int16_t   dig_H2;
uint8_t   dig_H3;
int16_t   dig_H4;
int16_t   dig_H5;
int8_t    dig_H6;

int32_t   t_fine;

// buffer for creating text messages & web pages
char      msg[80];

const char  *dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//**************************************************************************************
// Center the text in the display at the specified line.
//**************************************************************************************
void centerText(char *text, uint16_t line)
{
  uint16_t p;

  p = OLED_X - display.getStringWidth(text, strlen(text));
  p >>= 1;
  display.drawString(p, line, text);
} //  centerText

//**************************************************************************************
// Show IP address.
//**************************************************************************************
void showIP(void)
{
  display.setFont(ArialMT_Plain_24);
  centerText("Web Page", 0);
  
  display.setFont(ArialMT_Plain_16);
  centerText((char *)WiFi.localIP().toString().c_str(), 32);
} //  showIP

//**************************************************************************************
// Show day and time on the OLED display. Converts to 12Hr clock.
//**************************************************************************************
void showTime(void)
{
  int h, m, s, dow;
  
  time_t seconds = ntpClient.getEpochTime();
  tm     *ptime = localtime(&seconds);

  // daylight savings time starts on the second Sunday in November and ends on the
  // second Sunday in March
  // adjust for daylight savings time if DST is enabled and it is a Sunday
  if (DST && 0 == ptime->tm_wday)
  {
    // March and tm_isdst indicates time to start daylight savings time
    if (2 == ptime->tm_mon && ptime->tm_isdst)
    {
      // adjust for daylight savings time started
      ntpClient.setTimeOffset(TIMEZONE + 3600);
    }
    else if (10 == ptime->tm_mon && !ptime->tm_isdst)
    {
      // November and !tm_isdst indicates end of daylight savings time
      // adjust for daylight savings time ended
      ntpClient.setTimeOffset(TIMEZONE);
    }
  }
    
  h = ntpClient.getHours();
  m = ntpClient.getMinutes();
  s = ntpClient.getSeconds();
  dow = ntpClient.getDay();   // 0 -> Sunday

  // 12 hr clock
  if (h > 12)
    h -= 12;

  // noon and midnight will show as 12:xx:xx
  if (0 == h)
    h = 12;
    
  display.setFont(ArialMT_Plain_24);
  centerText((char *)dayNames[dow], 0);
  
  sprintf(msg, "%d:%02d:%02d", h, m, s);
  centerText(msg, 32);
} //  showTime

//**************************************************************************************
// Show weather on the OLED display.
//**************************************************************************************
void showWeather(void)
{
  display.setFont(ArialMT_Plain_24);
  centerText("Weather", 0);

  if (Description.length() > 9)
    display.setFont(ArialMT_Plain_16);
  
  centerText((char *)Description.c_str(), 32);
} //  showWeather

//**************************************************************************************
// Convert wind direction in degrees to a text heading.
//**************************************************************************************
char *windDirToStr(void)
{ 
  char *ps = "North";

  // convert direction in degress to direction as text
  if (WindDirection < 22.5)
    ps = "North";
  else if (WindDirection < 67.5)
    ps = "NorthEast";
  else if (WindDirection < 112.5)
    ps = "East";
  else if (WindDirection < 157.5)
    ps = "SouthEast";
  else if (WindDirection < 202.5)
    ps = "South";
  else if (WindDirection < 247.5)
    ps = "SouthWest";
  else if (WindDirection < 292.5)
    ps = "West";
  else if (WindDirection < 237.5)
    ps = "Northwest";
  
  return ps;
} //  windDirToStr

//**************************************************************************************
// Show wind speed and direction on the OLED display.
//**************************************************************************************
void showWind(void)
{
  int s = WindSpeed * 10.0;
  char  *wdir;
  
  display.setFont(ArialMT_Plain_16);
  centerText("Wind", 0);

  wdir = windDirToStr();
  /*
  // convert direction in degress to direction as text
  if (WindDirection < 22.5)
    wdir = "North";
  else if (WindDirection < 67.5)
    wdir = "NorthEast";
  else if (WindDirection < 112.5)
    wdir = "East";
  else if (WindDirection < 157.5)
    wdir = "SouthEast";
  else if (WindDirection < 202.5)
    wdir = "South";
  else if (WindDirection < 247.5)
    wdir = "SouthWest";
  else if (WindDirection < 292.5)
    wdir = "West";
  else if (WindDirection < 237.5)
    wdir = "Northwest";
*/
//  sprintf(msg, "From %s", wdir); 
  sprintf(msg, "%s at", wdir); 
  centerText(msg, 16);

  display.setFont(ArialMT_Plain_24);
  sprintf(msg, "%d.%d MPH", s / 10, s% 10); 
  centerText(msg, 32);
} //  showWind

//**************************************************************************************
// Show temperature on the OLED display in Fahrenheit.
//**************************************************************************************
void showInsideTemperature(void)
{
  showTemperature("Inside", temperature);
} //  showInsideTemperature

void showOutsideTemperature(void)
{
  showTemperature("Outside", OutTemperature);
} //  showOutsideTemperature

void showTemperature(char *s, double temp)
{
  int t;
  
  t = (int)(temp * 10.0);
  
  display.setFont(ArialMT_Plain_16);
  centerText(s, 0);
  centerText("Temperature", 16);

  display.setFont(ArialMT_Plain_24);
  sprintf(msg, "%3d.%2d F", t / 10, t % 10);
  centerText(msg, 32);
} //  showTemperature

//**************************************************************************************
// Show humidity on the OLED disply.
//**************************************************************************************
void showInsideHumidity(void)
{
  showHumidity("Inside", humidity);
} //  showInsideHumidity

void showOutsideHumidity(void)
{
  showHumidity("Outside", OutHumidity);
} //  showOutsideHumidity

void showHumidity(char *s, double humid)
{
  int32_t h = humid * 10.0;
  
  display.setFont(ArialMT_Plain_16);
  centerText(s, 0);
  centerText("Humidity", 16);

  display.setFont(ArialMT_Plain_24);
  sprintf(msg, "%3d.%d \%", h / 10, h % 10);
  centerText(msg, 32);
} //  showHumidity

//**************************************************************************************
// Show the barometric pressure on the OLED display.
// Pressure is in millibars.
//**************************************************************************************
void showInsideBarometer(void)
{
  showBarometer("Inside", pressure);
} //  showInsideBarometer

void showOutsideBarometer(void)
{
  showBarometer("Outside", OutPressure);
} //  showOutsideBarometer

void showBarometer(char *s, double press)
{
  int32_t p = press * 10.0;
  
  display.setFont(ArialMT_Plain_16);
  centerText(s, 0);
  centerText("Barometer", 16);

  display.setFont(ArialMT_Plain_24);
  sprintf(msg, "%3d.%d mb", p / 10, p % 10);
  centerText(msg, 32);
} //  showBarometer

//**************************************************************************************
// Connect OpenWeatherMap.com and request weather information.
//**************************************************************************************
void getWeatherData(void)
{
  char    c;
  String  data;
  int     timeout = 0;
  
  // create OpenWeatherMap client
  WiFiClient  owmClient;

  Serial.println("Connectiong to Open Weather Map");
  
  if (owmClient.connect("api.openweathermap.org", 80))
  {
    owmClient.print("GET /data/2.5/weather?zip=");
    owmClient.print(owmzip);
    owmClient.print("&APPID=");
    owmClient.println(owmkey);
    
    // wait for the client to connect and data be available
    while(owmClient.connected() && !owmClient.available())
    {
      delay(1);
      timeout++;
      if (timeout > 5000)
      {
        Serial.println("OpenWeatherMap connection timed out");
        Serial.println("Check your key");
        Serial.println("Try again later");

        // close the connection
        owmClient.stop();
        return;
      }
    } //  while
      
    Serial.print("Connection took: ");
    Serial.print(timeout);
    Serial.println("ms");
    
    // collect data from open weather map into a String, elininating braces,
    // double quotes and trailing CR/LF.
    while(owmClient.connected() || owmClient.available())
    {
      // we are connected
      c = owmClient.read();   // read a byte at a time
      switch (c)
      {
        case '{':   // remove curly braces
        case '}':
        case '[':   // remove braces
        case ']':
        case '"':   // remove double quotes
        case '\r':  // remove carriage return
        case '\n':  // remove linefeed (new line)
          break;
        default:    // keep the rest
          data = data + c;
          break;
      }
    } //  while

    // we have the data, parse it
    Serial.println(data);
    parseWeatherData(data);
        
    // close the connection
    owmClient.stop();
  }
  else
  {
    Serial.println("Open Weather Map Connection failed");
    return;
  }
} //  getWeatherData

//**************************************************************************************
// Parse the data from OpenWeatherMap.com into local variables. The following fields
// are available; "description", "main:temp", "temp_min", "temp_max", "visibility",
// "wind:speed", "deg", "gust", "pressure", "humidity", "sunrise", "sunset".
// Not all available information is parsed.
//**************************************************************************************
void parseWeatherData(String data)
{
  int     pos, p1;

  // parse the data fields we are interested in
  pos = data.indexOf(',');    
  while (-1 != pos)
  {
    String s = data.substring(0, pos);
    p1 = s.indexOf(':') + 1;
    if (s.startsWith("description"))
    {
      Description = s.substring(p1);
    }
    else if (s.startsWith("main:temp"))
    {
      p1 = s.lastIndexOf(':') + 1;
      OutTemperature = atof(s.substring(p1).c_str());
      // temperature is in Kelvin, convert to Celsius
      OutTemperature -= 273.15;
      // convert Celsius to Fahrenheit
      OutTemperature = (OutTemperature * 9.0 / 5.0) + 32.0;
    }
    else if (s.startsWith("wind:speed"))
    {
      p1 = s.lastIndexOf(':') + 1;
      WindSpeed = atof(s.substring(p1).c_str());
    }
    else if (s.startsWith("deg"))
    {
      WindDirection = atoi(s.substring(p1).c_str());
    }
    else if (s.startsWith("pressure"))
    {
      OutPressure = atof(s.substring(p1).c_str());
    }
    else if (s.startsWith("humidity"))
    {
      OutHumidity = atof(s.substring(p1).c_str());
    }

    // eat the data just parsed
    data.remove(0, pos + 1);
    pos = data.indexOf(',');    
  } //  while
} //  parseWeatherData  

//**************************************************************************************
// Helper function to take two byte from a buffer and combine
// them into a 16bit value.
// convert two bytes to an unsigned short (16 bit)
//**************************************************************************************
uint16_t convert(uint8_t *pbuf)
{
  uint8_t lsb = *pbuf++;
  uint16_t  s = (uint16_t)*pbuf;
  return (s << 8) | lsb;
} //  convert

//**************************************************************************************
// Read calibration/compensation data from the BME280. The values
// are mainly converted to 16bits and stored in 32bit variables.
//**************************************************************************************
void Bme280GetCal(void)
{
  brzo_i2c_start_transaction(BMEADRS, 100);
  // read BME280 calibration/compensation data in to local memory
  // temperature compensation
  // set starting register to read from
  buffer[0] = BME280_DIG_T1_LSB_REG;
  brzo_i2c_write(buffer, 1, false);
  // read six bytes from the BME280
  brzo_i2c_read(buffer, 6, false);

  // data is in the buffer, convert to signed and unsigned integers (32 bits)
  // data is in lsb/msb format
  dig_T1 = (int32_t)convert(buffer);
  dig_T2 = (int32_t)convert(buffer + 2);
  dig_T3 = (int32_t)convert(buffer + 4);

  // pressure compensation
  // set starting register to read from
  buffer[0] = BME280_DIG_P1_LSB_REG;
  brzo_i2c_write(buffer, 1, false);
  // read six bytes from the BME280
  brzo_i2c_read(buffer, 18, false);

  // data is in the buffer, convert to signed and unsigned shorts (16 bits)
  // data is in lsb/msb format
  dig_P1 = convert(buffer);
  dig_P2 = (int16_t)convert(buffer + 2);
  dig_P3 = (int16_t)convert(buffer + 4);
  dig_P4 = (int16_t)convert(buffer + 6);
  dig_P5 = (int16_t)convert(buffer + 8);
  dig_P6 = (int16_t)convert(buffer + 10);
  dig_P7 = (int16_t)convert(buffer + 12);
  dig_P8 = (int16_t)convert(buffer + 14);
  dig_P9 = (int16_t)convert(buffer + 16);

  // humidity compensation
  // set starting register to read from
  buffer[0] = BME280_DIG_H1_REG;
  brzo_i2c_write(buffer, 1, false);
  // read six bytes from the BME280
  brzo_i2c_read(&dig_H1, 1, false);
  
  sprintf(msg, "%d", dig_H1);
  Serial.println(msg);
  
  // set starting register to read from
  buffer[0] = BME280_DIG_H2_LSB_REG;
  brzo_i2c_write(buffer, 1, false);
  // read six bytes from the BME280
  brzo_i2c_read(buffer, 7, false);

  // data is in the buffer, convert to signed and unsigned shorts (16 bits)
  // data is in lsb/msb format
  dig_H2 = (int16_t)convert(buffer);
  dig_H3 = buffer[2];

  // now it gets funky, dig_H4 lsn is the lsn of BME280_DIG_H5_REG
  dig_H4 = (int16_t)buffer[3];
  dig_H4 = (dig_H4 << 4) | (buffer[4] & 0x0F);

  // dig_H5 lsn is the msn of BME280_DIG_H5_REG
  dig_H5 = (int16_t)buffer[6];
  dig_H5 = (dig_H5 << 4) | (buffer[5] >> 4);
  
  dig_H6 = (int8_t)buffer[6];
} //  Bme280GetCal

//**************************************************************************************
// Read the temperature registers of the BME280, apply calibration data
// and convert to Celcius degress. Store result in temperature.
//**************************************************************************************
void bme280GetTemperature(void)
{
  int32_t raw, var1, var2;

  brzo_i2c_start_transaction(BMEADRS, 100);
  // set starting register to read from
  buffer[0] = BME280_TEMPERATURE_MSB_REG;
  brzo_i2c_write(buffer, 1, false);
  // read three bytes from the BME280
  brzo_i2c_read(buffer, 3, false);

  // convert to a 20bit signed value stored in a 32bit signed value
  raw = (int32)buffer[0];
  raw <<= 8;
  raw |= buffer[1];
  raw <<= 4;
  raw |= (buffer[2] >> 4) & 0x0F;

  var1 = (((raw >> 3) - (dig_T1 << 1)) * dig_T2) >> 11;
  var2 = (((((raw >> 4) - dig_T1) * ((raw >> 4) - dig_T1)) >> 12) * dig_T3) >> 14;

  t_fine = var1 + var2; 
  temperature = (t_fine * 5 + 128) >> 8;
  temperature /= 100.0;

  // convert to Fahrenheit degrees from Celcius
  temperature = (temperature * 9 / 5) + 32;
} //  bme280GetTemperature

//**************************************************************************************
// Read the BME280 humidity registers, apply calibration data and convert
// to humidity percentage. Store result in humidity.
//**************************************************************************************
void bme280GetHumidity(void)
{
  // compensate and convert humidity to a percentage
  int32_t raw;

  // read the humidity registers
  brzo_i2c_start_transaction(BMEADRS, 100);
  // set starting register to read from
  buffer[0] = BME280_HUMIDITY_MSB_REG;
  brzo_i2c_write(buffer, 1, false);
  // read three bytes from the BME280
  brzo_i2c_read(buffer, 2, false);

  raw = buffer[0];
  raw = (raw << 8) | buffer[1];
  
  humidity = 0.0;
  float fh = t_fine - 76800.0;
  if (fh > 0.0)
  {
    fh = (raw - (dig_H4 * 64.0 + dig_H5 / 16384.0 * fh)) * (dig_H2 / 65536.0 * (1.0 + dig_H6 / 67108864.0 * fh * (1.0 + dig_H3 / 67108864.0 * fh)));

    fh *= (1.0 - dig_H1 * fh / 524288.0);

    if (fh > 100.0)
    {
      fh = 100.0;
    }
    else if (fh < 0.0)
      fh = 0.0;

    humidity = fh;
  }
} //  bme280GetHumidity

//**************************************************************************************
// Read the pressure registers of the BME280, apply calibration data and convert
// to pressure in millibars. Store the result in pressure. Save previous pressure
// in lastPressure.
//**************************************************************************************
void bme280GetPressure(void)
{
  // compensate and convert pressure to inches of Hg
  int32_t raw;
  float fp, v1, v2;

  pinMode(D4, OUTPUT);
  digitalWrite(D4, LOW);

  brzo_i2c_start_transaction(BMEADRS, 100);
  // set starting register to read from
  buffer[0] = BME280_PRESSURE_MSB_REG;
  brzo_i2c_write(buffer, 1, false);
  // read three bytes from the BME280
  brzo_i2c_read(buffer, 3, false);

  // convert to a 20bit signed value stored in a 32bit signed value
  raw = (int32)buffer[0];
  raw <<= 8;
  raw |= buffer[1];
  raw <<= 4;
  raw |= (buffer[2] >> 4) & 0x0F;

  // 1KPa = 0.29531inHg
  
  v1 = (t_fine / 2.0) - 64000.0;
  v2 = (((v1 / 4.0) * (v1 / 4.0)) / 2048) * dig_P6;
  v2 += ((v1 * dig_P5) * 2.0);
  v2 = (v2 / 4.0) + (dig_P4 * 65536.0);
  v1 = (((dig_P3 * (((v1 / 4.0) * (v1 / 4.0)) / 8192)) / 8) + ((dig_P2 * v1) / 2.0)) / 262144;
  v1 = ((32768 + v1) * dig_P1) / 32768;
  
  // chcek for possible division by zero
  if (v1 != 0)
  {
    fp = ((1048576 - raw) - (v2 / 4096)) * 3125;
    if (fp < 0x80000000)
      fp = (fp * 2.0) / v1;
    else
      fp = (fp / v1) * 2;

    v1 = (dig_P9 * (((fp / 8.0) * (fp / 8.0)) / 8192.0)) / 4096;
    v2 = ((fp / 4.0) * dig_P8) / 8192.0;
    fp += ((v1 + v2 + dig_P7) / 16.0);
  }  
  else
  {
    // something went wrong with the pressure calculation or we just got
    // spaced !!!
    fp = 0.0;
  }
  
  // 1 KPa = 0.29531 inHg (inches of mercury)
  // fair weather -> 1022mb or greater
  // foul weathre -> 988mb or less

  // pressure is in hPa or millibars
  fp /= 100.0;
  lastPressure = pressure;
  pressure = fp;
} //  bme280GetPressure

//**************************************************************************************
// Initialize the brzo_I2C dirver, OLED display and BME280 sensor array.
// The BME280 calibration/compensation data is read into memory.
//**************************************************************************************
void setup(void)
{
  int x = 0;
  time_t  seconds;
  tm      *ptime;
    
  Serial.begin(115200);

  // Initialise the display.
  display.init();

//  display.flipScreenVertically();   // pins on top, default is pins on bottom

  // clear the display
  display.clear();

  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // validate the BME280 is present
  brzo_i2c_start_transaction(BMEADRS, 100);
  // select the  register
  buffer[0] = BME280_CHIP_ID_REG;
  brzo_i2c_write(buffer, 1, false);
  // read the chip ID
  brzo_i2c_read(buffer, 1, false);
  if (BME280_CHIP_ID == buffer[0])
  {
    // the BME280 is present and working
    Serial.println("BME280 found at 0x76");

    // initialize BME280
    buffer[0] = BME280_CTRL_MEAS_REG;
    buffer[1] = 0x00;
    brzo_i2c_write(buffer, 2, false);

    buffer[0] = BME280_CONFIG_REG;
    buffer[1] = 0xA0;
    brzo_i2c_write(buffer, 2, false);
    
    buffer[0] = BME280_CTRL_HUMIDITY_REG;
    buffer[1] = 0x01;
    brzo_i2c_write(buffer, 2, false);
    
    buffer[0] = BME280_CTRL_MEAS_REG;
    buffer[1] = 0x27;
    brzo_i2c_write(buffer, 2, false);

    // read the calibration/compensation data into memory
    Bme280GetCal();

    //===== Connect to WiFi network =====
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    // clear the display
    display.clear();
    display.setFont(ArialMT_Plain_10);

    sprintf(msg, "Connecting to: %s", ssid);
    display.drawString(0, 0, msg);
    display.display();
  
    WiFi.begin(ssid, password);
  
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      display.drawString(x, 8, ".");
      x += 8;
      display.display();
    }
    
    Serial.println("");
    Serial.println("WiFi connected");

    display.drawString(0, 24, "WiFi connected");
    display.display();
  
    // Start the server
    server.begin();
    Serial.println("Server started");

    display.drawString(0, 32, "Web Server started");
    display.display();
  
    // Print the IP address
    Serial.print("Use this URL to connect: ");
    Serial.print("http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
    
    // start NTP client
    ntpClient.begin();

    // update the stored time
    ntpClient.update();
    seconds = ntpClient.getEpochTime();
    ptime = localtime(&seconds);

    Serial.println(ptime->tm_isdst);
    Serial.println(asctime(ptime));

    if (DST && ptime->tm_isdst)
    {
      // advance the time by one hour
      ntpClient.setTimeOffset(TIMEZONE + 3600);
      ntpClient.update();
    }
    
    // get information from OpenWeatherMap.org
    getWeatherData();
    
    delay(5000);
  }
  else
  {
    // the BME280 was not found
    Serial.println("BME280 not found");
    Serial.println("Check your wiring");
    Serial.println("SCL should be wired to D1");
    Serial.println("SDA should be wired to D2");
    Serial.println("Make sure the BME280 is wired to 3.3V and ground");
    Serial.println("All other pins of the BME280 should be left unconnected");

    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "BME280 not found");
    display.drawString(0, 0, "Check your wiring");
    display.drawString(0, 0, "SCL should be wired to D1");
    display.drawString(0, 0, "SDA should be wired to D2");
    display.display();

    // stall here until the watchdog triggers a restart.
    while(true);
  }
} //  setup

//**************************************************************************************
// Client requests cause an HTTP formated web page to be returned. The web page is
// created from the available data.
//**************************************************************************************
void handleClient(void)
{
  int sensor;
  
  // Check if a client has connected
  WiFiClient client = server.available();
  if (client)
  {
    digitalWrite(D4, HIGH);
    // Return the response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println(""); //  do not forget this one
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
    client.print("<HEAD><TITLE>ESP8266 Weather Station</TITLE>");
    // css to set font of whole page
    client.print("<style>body{font-size:30px;}</style>");

    // auto-refresh page every 15 seconds
//    client.print("<meta http-equiv=\"refresh\" content=\"15\">");

    // jsvsscript reload button
    client.print("<script>function reloadPage(){location.reload(forceGet=true);}</script>");

    // end of the HEAD
    client.println("</HEAD>");

    // centered title for the page
    client.println("<H2 align=\"center\">ESP8266 Weather Station</H2>");

    // inside data - uses a 3 line, 3 column table to show the data
    client.print("<BODY text=\"white\" bgcolor=\"blue\">");
    // centered, no border, 20 pixels between cells, 5 pixels padding within a cell
    client.print("<TABLE align=\"center\" border=\"0\" cellspacing=\"20\" celpadding=\"5\">");
    
    // first row of the inside table
    client.print("<TR>");
    client.print("<TH colspan=\"3\" align=\"center\">Inside</TH>");
    client.print("</TR>");

    // second row
    client.print("<TR>");
    client.print("<TH align=\"center\">Temperature</TH>");
    client.print("<TH align=\"center\">Humidity</TH>");
    client.print("<TH align=\"center\">Barometer</TH>");
    client.print("</TR>");

    // third row
    client.print("<TR>");
    sensor = (int)(temperature * 10.0);
    sprintf(msg, "<TD align=\"center\">%d.%d F</TD>", sensor / 10, sensor % 10);
    client.print(msg);
    
    sensor = (int)(humidity * 10.0);
    sprintf(msg, "<TD align=\"center\">%d.%d \%</TD>", sensor / 10, sensor % 10);
    client.print(msg);

    sensor = (int)(pressure * 10.0);
    sprintf(msg, "<TD align=\"center\">%d.%d millibars</TD>", sensor / 10, sensor % 10);
    client.print(msg);

    client.println("</TR></TABLE></BODY></html>");

    // outside data - uses a 4 line, 3 column table to show the data
    // centered, no border, 20 pixels between cells, 5 pixels padding within a cell
    client.print("<BODY><TABLE align=\"center\" border=\"0\" cellspacing=\"20\" celpadding=\"5\">");
    
    // first row of the outside table
    client.print("<TR>");
    client.print("<TD colspan=\"3\" align=\"center\">");
    sprintf(msg, "Weather - %s</TD></TR>", Description.c_str());
    client.print(msg);

    // second row of the outside table
    client.print("<TR>");
    client.print("<TD colspan=\"3\" align=\"center\">Wind from the ");
    client.print(windDirToStr());
    sensor = (int)(WindSpeed * 10.0);
    sprintf(msg, " at %d.%d MPH</TD>", sensor / 10, sensor % 10);
    client.print(msg);
 
    // third row
    client.print("<TR>");
    client.print("<TH align=\"center\">Temperature</TH>");
    client.print("<TH align=\"center\">Humidity</TH>");
    client.print("<TH align=\"center\">Barometer</TH>");
    client.print("</TR>");

    // fourth row
    client.print("<TR>");
    sensor = (int)(OutTemperature * 10.0);
    sprintf(msg, "<TD align=\"center\">%d.%d F</TD>", sensor / 10, sensor % 10);
    client.print(msg);
    
    sensor = (int)(OutHumidity * 10.0);
    sprintf(msg, "<TD align=\"center\">%d.%d \%</TD>", sensor / 10, sensor % 10);
    client.print(msg);

    sensor = (int)(OutPressure * 10.0);
    sprintf(msg, "<TD align=\"center\">%d.%d millibars</TD>", sensor / 10, sensor % 10);
    client.print(msg);

    client.print("</TR></TABLE></BODY>");
    
//    client.println("<H3 align=\"center\">Press Refresh to update the data</H3>");

    // css to center the button
    client.print("<div style=\"text-align:center;\">");
    // add a button to refresh the page. onclick = name of javascript function defined
    // in <script></script>.
    client.print("<input align=\"center\" type=\"button\" value=\"Refresh Page\" onclick=\"reloadPage()\" style=\"font-size : 50px;\">");
    client.print("</div>");
    client.println("</html>");
  }
  else
    digitalWrite(D4, LOW);
} //  handleClient

#define PAGE_DURATION     5000      // 5 seconds in milliseconds
#define WEATHER_DURATION  54000000  // 15 minutes in milliseconds
#define TIME_DURATION     1000      // one second in milliseconds

// this defines a data type for a pointer to a function that returns a void and takes
// no arguments.
typedef void (*Page)(void);

// this is an array of function pointers. each member of the array a function that will
// format and display information on the OLED display. you can add more members to the
// array or like showTime, repeat it. the number of entries is automatically calculated
// at compile time. Loop() will show each entry for PAGE_DURATION milliseconds. the name
// of a function is a pointer to the function.
 
Page Pages[] = {showTime, showInsideTemperature, showInsideHumidity, showInsideBarometer, 
                showIP, showTime, showWeather, showWind,
                showOutsideTemperature, showOutsideHumidity, showOutsideBarometer};

int  NumPages = (sizeof(Pages) / sizeof(Page));
long TimeSinceLastPageSwitch = 0;
long TimeSinceLastWeatherUpdate = 0;
long TimeSinceLastTimeUpdate = 0;

int  Sensor = 0;
bool flag = true;

//**************************************************************************************
// Main loop. Periodically sample the sensors and display the
// data.
//**************************************************************************************
void loop(void)
{
  handleClient();

  if (flag)
  {
    flag = false;

    if (!Sensor)
    {
      // read the sensors
      bme280GetTemperature();
      bme280GetHumidity();
      bme280GetPressure();
    }
    
    // clear the display
    display.clear();
    
    // run the selected function to draw a new screen 
    Pages[Sensor]();
  
    // write the buffer to the display
    display.display();
  }

  // change the OLED display every 5 seconds
  if (millis() - TimeSinceLastPageSwitch > PAGE_DURATION)
  {
    Sensor = (Sensor + 1) % NumPages;
    TimeSinceLastPageSwitch = millis();
    flag = true;
  }

  // refresh the outside weather data every 15 minutes
  if (millis() - TimeSinceLastWeatherUpdate > WEATHER_DURATION)
  {
    TimeSinceLastWeatherUpdate = millis();
    getWeatherData();
  }

  if (showTime == Pages[Sensor])
  {
    if (millis() - TimeSinceLastTimeUpdate > TIME_DURATION)
    {
      TimeSinceLastTimeUpdate = millis();
      // clear the display
      display.clear();
      
      Pages[Sensor]();
    
      // write the buffer to the display
      display.display();
    }
  }
} //  loop

