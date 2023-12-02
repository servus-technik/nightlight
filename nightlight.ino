/*
 * Night light
 *
 * This code operates a night light powered by WS2812B LED strip.
 * It provides several modes, which can be triggered locally by a button or remote via MQTT:
 *
 * - local button on: simulates sun rise, remains in white, waiting for next trigger (mode "on")
 * - local button off: simulates sun set, all LEDs off, waiting for next trigger (mode "off")
 * - MQTT can trigger button on/off
 * - MQTT can trigger special good night animation (LEDs show blue/green animation, mode "goodnight"). Pressing the local button changes to all LEDs white (mode: "on") 
 *
 * Copyright (c) 2023 Martin Arend
 * This software is published under GNU General Public License version 3 or later
 * https://www.gnu.org/licenses/gpl-3.0.txt 
 * 
 */


// Import libraries 
#include <ESP8266WiFi.h>                // Wifi Client library
#include <PubSubClient.h>               // MQTT library
#include <Adafruit_NeoPixel.h>          // NeoPixel library

// Clients
WiFiClient espClient;
PubSubClient client(espClient);

// Credentials für WiFi und MQTT Broker
const char* SSID = "YOUR SSID";
const char* PSK = "YOUR WLAN PASSWORD";
const char* MQTT_BROKER = "XXX.XXX.XXX.XXX";         // Broker IP Adresse
const char* MQTT_USER = "MQTT USERNAME";             // Broker User
const char* MQTT_PASSWORD = "MQTT USER PASSWORD";    // Broker Passwort

// define setup for LED strip
#define LED_PIN         2               // D1 Mini data pin used for WS2812B LED strip (GPIO2 = D4)
#define LED_COUNT       78              // number of LEDs in your WS2812B LED strip
#define LED_BRIGHTNESS  200             // 0 to 255 max
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// define button
#define BUTTON_PIN      4               // data pin to be used for button (GPIO4 = D2)

// definition of variables/constants
char PSTATE;                            // state machine, can be (b)oot, (o)ff, (a)ctive, (g)oodnight
int buttonstate = 0;                    // button: the current reading from the input pin
int vanim = 25;                         // delay between animation steps in ms

// define LED colors
uint32_t strip_white = strip.Color(255, 255, 200);
uint32_t tcolor = strip.Color(0, 0, 0); // needed as temporary color variable for animations


// ***************************************************************************
// Setup routine
// ***************************************************************************
void setup() {
  // initialize digital pin LED_BUILTIN as an output to indicate successfull booting.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Set software serial baud to 115200;
  Serial.begin(115200);
  Serial.println("Nightlight: booting");
  
  // connecting to WiFi network
  Serial.println("Nightlight: connecting to WiFi");
  setup_wifi();
  Serial.println("Nightlight: connected to WiFi");

  //connecting to MQTT broker
  Serial.println("Nightlight: connecting to MQTT");
  client.setServer(MQTT_BROKER, 1883);
  client.setCallback(callback);
  Serial.println("Nightlight: connected to MQTT");
  client.publish("nightlight/mode", "ready");
  client.publish("nightlight/modeset", "ok");
  client.publish("nightlight/ON_ACTUAL", "false");
    
  // setup Statemachine
  PSTATE = 'b';

  // setup button pin
  pinMode(BUTTON_PIN, INPUT);

  // initialize strip to all LEDs off and set brightness
  Serial.println("Nightlight: Initialize LED strip");
  strip.begin();
  strip.clear();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.show();
  Serial.println("Nightlight: LED strip initialized");

  // LED off after successful setup.
  // digitalWrite(LED_BUILTIN, HIGH);
}

// initialize WiFi Connection
void setup_wifi() {
    WiFi.begin(SSID, PSK);

    // blink LED during WiFi search.
    // digitalWrite(LED_BUILTIN, HIGH);
 
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        // blink LED during WiFi search.
        // digitalWrite(LED_BUILTIN, LOW);
    }
}

// ***************************************************************************
void gnanimation() {
  // set strip into "goodnight mode"
  client.publish("nightlight/ON_ACTUAL", "true");
  for (int i = 0; i < 50; i++) { 
    strip.rainbow	(	0, 1, 255, i*5, true); 	
    strip.show();
    delay(vanim);
  }
  strip.rainbow	(	0, 1, 255, 255, true); 	
  strip.show();
  client.publish("nightlight/mode", "goodnight");
}

// ***************************************************************************
void onanimation() {
  // set strip into "on mode"
  client.publish("nightlight/ON_ACTUAL", "true");
  for (int i = 0; i < LED_COUNT/2; i++) { 
    strip.setPixelColor((LED_COUNT/2)-i, 50, 50, 50);
    strip.setPixelColor((LED_COUNT/2)+i, 50, 50, 50);
    
    if (i>0) {
      strip.setPixelColor((LED_COUNT/2)-(i-1), 100, 100, 100);
      strip.setPixelColor((LED_COUNT/2)+(i-1), 100, 100, 100);
    }

    if (i>1) {
      strip.setPixelColor((LED_COUNT/2)-(i-2), 150, 150, 150);
      strip.setPixelColor((LED_COUNT/2)+(i-2), 150, 150, 150);
    }

    if (i>2) {
      strip.setPixelColor((LED_COUNT/2)-(i-3), 200, 200, 200);
      strip.setPixelColor((LED_COUNT/2)+(i-3), 200, 200, 200);
    }

    if (i>3) {
      strip.setPixelColor((LED_COUNT/2)-(i-4), 255, 255, 255);
      strip.setPixelColor((LED_COUNT/2)+(i-4), 255, 255, 255);
    }
    
    strip.show();
    delay(3*vanim);
  }

  strip.fill(strip_white, 0, LED_COUNT);
  strip.show();
  client.publish("nightlight/mode", "on");
}

// ***************************************************************************
void offanimation() {
  // set strip into "off mode"
  client.publish("nightlight/ON_ACTUAL", "false");
  for (int i = 0; i < 250; i++) { 
    uint32_t tcolor = strip.Color(255-i, 255-i, 255-i);
    strip.fill(tcolor, 0, LED_COUNT);
    strip.show();
    delay(vanim);
  }
  delay(vanim);
  strip.clear();
  strip.show(); 
  client.publish("nightlight/mode", "off");
}

// ***************************************************************************
// Main routine
// ***************************************************************************
void loop() {
  if (!client.connected()) {
    // while (!client.connected()) {
      Serial.println("Nightlight: Reconnect MQTT");
      client.connect("nightlight", MQTT_USER, MQTT_PASSWORD);
      client.subscribe("nightlight/mode");
      client.subscribe("nightlight/modeset");
      delay(500);
    // }
  }
  client.loop();

  // switch off builtin LED
  digitalWrite(LED_BUILTIN, HIGH);

  // read button
  buttonstate=digitalRead(BUTTON_PIN); 
  if (buttonstate == HIGH) { 
    Serial.println("button pressed and debounced");
    switch (PSTATE) {
      case 'g':
      Serial.println("Set mode to on");
      onanimation();
      PSTATE='a';               // change from mode (g)oodnight to (a)ctive
      break;
    case 'a':
      Serial.println("Set mode to off");
      offanimation();
      PSTATE='o';               // change from mode (a)ctive to (o)ff 
      break;
    default: // default = off
      Serial.println("Set mode to on");
      onanimation();
      PSTATE='a';               // change from mode (o)ff to (a)ctive
      break;
    }
  }

}

// ***************************************************************************
void callback(char* topic, byte* payload, unsigned int length) {
  if(strcmp(topic, "nightlight/modeset") == 0) {
    String msg = "";
    for (byte i = 0; i < length; i++) {
        char tmp = char(payload[i]);
        msg += tmp;
    }
    Serial.println("Mode: " + msg);

    // decode message
    if (msg == "off") {
      Serial.println("Nightlight Mode: off");
      if (PSTATE != 'o') {
        offanimation();
        PSTATE = 'o';
        client.publish("nightlight/modeset", "ok");
      }
    }
    if (msg == "on") {
      Serial.println("Nightlight Mode: on");
      if (PSTATE != 'a') {
        onanimation();
        PSTATE = 'a';
        client.publish("nightlight/modeset", "ok");
      }
    }
    if (msg == "goodnight") {
      Serial.println("Nightlight Mode: goodnight");
      if (PSTATE != 'g') {
        gnanimation();
        PSTATE = 'g';
        client.publish("nightlight/modeset", "ok");
      }
    }
  }  
}