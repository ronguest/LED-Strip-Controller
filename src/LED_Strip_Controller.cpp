//
//  Copyright (C) 2017 Ronald Guest <http://about.me/ronguest>

#include <Arduino.h>
#include "config.h"

// Configure based on your Neopixel strip characteristics
#define PIXEL_PIN     5
#define PIXEL_COUNT   30
#define PIXEL_TYPE    NEO_GRB + NEO_KHZ800
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

// set up the Adafruit IO feeds
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Feed *colorFeed = io.feed("color");
AdafruitIO_Feed *modeFeed = io.feed("np_mode");
AdafruitIO_Feed *statusFeed = io.feed("np_status");
AdafruitIO_Feed *onOffFeed = io.feed("np_on_off");
AdafruitIO_Data data;

void setup() {
  Serial.begin(115200);
  //while(! Serial);            // wait for serial monitor to open
  EEPROM.begin(eepromSize);
  randomSeed(analogRead(0));

  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // set up a message handlers
  colorFeed->onMessage(handleColorMessage);
  modeFeed->onMessage(handleModeMessage);
  onOffFeed->onMessage(handleOnOffMessage);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  // Get current time using NTP
  Udp.begin(localPort);
  ntpTime = getNtpTime();
  if (ntpTime != 0) {
    setTime(ntpTime);
    timeIsSet = true;
  } else {
    Serial.println("Failed to set the initial time");
    statusFeed->save("Failed to set the initial time");
  }

  pixels.begin();

  four = EEPROM.read(addrColor);
  three = EEPROM.read(addrColor + 1);
  two = EEPROM.read(addrColor + 2);
  one = EEPROM.read(addrColor + 3);
  color = (four & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
  Serial.print("Read color from EEPROM: "); Serial.println(color);
  previousColor = 0;

  if (EEPROM.read(addrSwitchOn) == 0) {
    switchOn = false;
  } else {
    switchOn = true;
  }
  Serial.print("Set switchOn from EEPROM: "); Serial.println(switchOn);

  String s;
  s = EEPROM.read(addrMode);
  Serial.println("EEPROM says mode string is: " + s);
  myMode = atoi(s.c_str());
  Serial.print("Mode is: ");Serial.println(myMode);

  pushColor(color);
  pixels.show();
  statusFeed->save("Started");      // Log that we are ready to rock and roll
}

void loop() {
  // io.run() should always be at the top of your loop
  io.run();

  time_t local = usCT.toLocal(now(), &tcr);
  hours = hour(local);

  // Perform actions at certain times of the day (hours)
  // Only trigger checks when the hours changes from previousHour
  // Logic: when the hour changes check to see if it is time to turn off. Update previousHour so we won't check again until hour changes again
  // We also sync with NTP time once an hour
  if (hours != previousHour) {
    previousHour = hours;
    // Sync time at 5am every day, or if time failed set on boot
    if((hours == 5) || !timeIsSet) {
      // Try an NTP time sync so we don't stray too far
      ntpTime = getNtpTime();
      if (ntpTime != 0) {
        setTime(ntpTime);
        timeIsSet = true;
      } else {
        Serial.println("NTP sync failed");
      }
    }

    if (hours == offTime) {
      turnOff();
    }

    // Same if it is time to turn the LEDs on
    if (hours == onTime) {
      Serial.print("onTime hours = "); Serial.println(hours);
      //color = previousColor;
      if (myMode == manualMode) {
        color = 0xffef32e;
      }
      statusFeed->save("Time to turn LEDs on");
      pushColor(color);
      pixels.show();
    }

    // If it is (fake) sunset, change to a more orangne color
    // TODO: fetch actual sunset time for wunderground
    if ((hours == sunsetTime) && (myMode == manualMode)) {
      color = 0x9c5c00;
      statusFeed->save("Switch to sunset color");
      pushColor(color);
      pixels.show();
    }
  }

  // Check if we should update LED strip
  if ((millis() - lastUpdate) > updateCycle) {
    rainbow();
    lastUpdate = millis();
  }
}

// Each call to rainbow runs through an update of each pixel
uint16_t cycleColor = 0;
void rainbow() {
  for(uint16_t i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, Wheel((i+cycleColor) & 255));
  }
  pixels.show();
  cycleColor++;
  if (cycleColor > 255) cycleColor = 0;
}

void dance() {
  uint16_t i, j;

  Serial.println("Slow dance mode");
  //for(j=0; j<256; j++) {
  for(j=0; j<256; j+=17) {
    for(i=0; i<pixels.numPixels()-3;i+=3) {

      for (int k=i; k<i+3; k++) {
        pixels.setPixelColor(k, random(1,0xffffff));
      }
    }
    pixels.show();
    //Serial.print("Dance: pixel 16 color is: "); Serial.println(pixels.getPixelColor(16));
    //Serial.println("Dance: wait");
    delay(1000);
    if (switchOn == false) return;
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3,0);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3,0);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0,0);
}

// Turn off the LEDs
void turnOff() {
      previousColor = color;
      color = 0;
      statusFeed->save("Turn LEDs off");
      pushColor(color);
      pixels.show();
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < pixels.numPixels(); i=i+3) {
        pixels.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      pixels.show();
      delay(wait);

      for (uint16_t i=0; i < pixels.numPixels(); i=i+3) {
        pixels.setPixelColor(i+q, 0);        //turn every third pixel off
      }
      if (switchOn == false) return;
    }
  }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256 * 1; j++) { // cycles of all colors on wheel
    for(i=0; i< pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel(((i * 256 / pixels.numPixels()) + j) & 255));
    }
    pixels.show();
    delay(wait);
    if (switchOn == false) return;
  }
}

// np_on_off - Turn LED strip on and off, off
void handleOnOffMessage(AdafruitIO_Data *data) {
  Serial.print("On/Off recevied: ");
  Serial.println(data->value());

  if (strncmp(data->value(), "OFF", 3) == 0) {
    Serial.println("Turn LEDs off");
    switchOn = false;
    turnOff();
    EEPROM.write(addrSwitchOn, 0);
  } else {
    switchOn = true;
    // We don't want previousColor to ever be Off (0) so we don't change it in this case
    color = previousColor;
    statusFeed->save("Turn LEDs on");
    pushColor(color);
    pixels.show();
    EEPROM.write(addrSwitchOn, 1);
  }
  EEPROM.commit();
}

// this function is called whenever a 'np_mode' message
// is received from Adafruit IO
void handleModeMessage(AdafruitIO_Data *data) {
  String message = "Mode set to ";

  // print mode value
  Serial.print("Received Mode: ");
  Serial.println(data->value());

  myMode = (uint8_t) atoi(data->value());
  message += myMode;

  Serial.print("Set mode to: ");Serial.println(myMode);
  statusFeed->save(message);
  EEPROM.write(addrMode, myMode);
  EEPROM.commit();
}

// this function is called whenever a 'color' message
// is received from Adafruit IO. it was attached to
// the color feed in the setup() function above.
void handleColorMessage(AdafruitIO_Data *data) {
  String message = "Color set to ";
  // print RGB values and hex value
  //Serial.print("Received HEX: ");
  //Serial.println(data->value());
  message += data->value();
  Serial.println(message);
  statusFeed->save(message);

  color = data->toNeoPixel();

  pushColor(color);
  pixels.show();
}

// IS THIS REALLY NEEDED SINCE I SWITCHED TO EEPROM?
// For persistent color changes initiated by the Feather, we need to set the color at
// Adafruit IO's color feed so it is retained even if we restart or there is a hiccup
// For example, when we set a sunset color from this code, we need to tell the feed
// We will get a update message in response which will trigger the actual color change
void sendColor(long color) {
  String colorString;

  colorString = "#" + String(color, HEX);
  Serial.print("Sending color: "); Serial.println(colorString);
  colorFeed->save(colorString);
}

void pushColor(long color) {
  // Save color to EEPROM
  EEPROM.write(addrColor, (color & 0xFF));
  EEPROM.write(addrColor + 1, ((color >> 8) & 0xFF));
  EEPROM.write(addrColor + 2, ((color >> 16) & 0xFF));
  EEPROM.write(addrColor + 3, ((color >> 24) & 0xFF));
  EEPROM.commit();
  Serial.print("Wrote color to EEPROM: "); Serial.println(color);

  for(int i=0; i<PIXEL_COUNT; ++i) {
    pixels.setPixelColor(i, color);
  }
}

time_t getNtpTime() {
  IPAddress ntpServerIP; // NTP server's ip address

  if(WiFi.status() == WL_CONNECTED) {
    while (Udp.parsePacket() > 0) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // discard any previously received packets, I made this change not sure if needed though
    }
    Serial.print("Transmit NTP Request ");
    // get a random server from the pool
    WiFi.hostByName(ntpServerName, ntpServerIP);
    Serial.print(ntpServerName);
    Serial.print(": ");
    Serial.println(ntpServerIP);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) { // Extending wait from 1500 to 2-3k seemed to avoid the sync problem, but now it doesn't help
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        Serial.println("Receive NTP Response");
        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      }
    }
    Serial.println("No NTP Response");
    return 0; // return 0 if unable to get the time
  }
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  int result;
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  result = Udp.beginPacket(address, 123); //NTP requests are to port 123
  result = Udp.write(packetBuffer, NTP_PACKET_SIZE);
  result = Udp.endPacket();
}
