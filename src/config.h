//
//  Copyright (C) 2017 Ronald Guest <http://about.me/ronguest>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include "Adafruit_NeoPixel.h"
#include <EEPROM.h>
#include "AdafruitIO.h"
#include "AdafruitIO_WiFi.h"
#include "credentials.h"

// EEPROM setup
const int eepromSize = 128;
unsigned int addrColor = 0;
unsigned int addrSwitchOn = addrColor + sizeof(addrColor);
long one, two, three, four;   // used to read a long from EEPROM

long color;
long previousColor;               // Stores the previous color, used when we toggle from off to on
enum modes { manual = 0, dance = 1, theaterRainbow = 2, rainbowMode = 3 };
modes myMode = manual;
//boolean ledOn = true;

//US Central Time Zone (Chicago, Houston)
//static const char ntpServerName[] = "us.pool.ntp.org";
static const char ntpServerName[] = "time.nist.gov";
const int timeZone = 0;     // Using the Timezone library now which does it's own TZ and DST correction
TimeChangeRule usCDT = {"CDT", Second, dowSunday, Mar, 2, -300};
TimeChangeRule usCST = {"CST", First, dowSunday, Nov, 2, -360};
Timezone usCT(usCDT, usCST);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get the TZ abbrev
time_t local;

////// Code and variables for NTP syncing
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
int ntpTime;
// Create an ESP8266 WiFiClient class to connect to the AIO server.
WiFiClient client;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
boolean timeIsSet = false;

int hours = 0;                      // Track hours
int minutes = 0;                    // Track minutes
int seconds = 0;                    // Track seconds
int dayOfWeek = 0;                  // Sunday == 1

int previousHour = 0;
int offTime = 23;                   // Turn off LED strip at 11pm
int onTime = 5;                     // Turn on LED strip at 5am
int sunsetTime = 18;                // Not true sunset, for now, just 6pm
boolean switchOn = true;            // Keep track of whether we are turned on or not

void pushColor(long);
uint32_t Wheel(byte);
void turnOff();
void theaterChaseRainbow(uint8_t);
void rainbowCycle(uint8_t);
void rainbow(uint8_t);
void danceMode();
void handleOnOffMessage(AdafruitIO_Data*);
void handleModeMessage(AdafruitIO_Data*);
void handleColorMessage(AdafruitIO_Data*);
void sendColor(long);
time_t getNtpTime();
void sendNTPPacket(IPAddress&);
