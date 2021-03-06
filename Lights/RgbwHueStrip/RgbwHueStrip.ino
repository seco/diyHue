#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NeoPixelBus.h>

#include <ESP8266WebServer.h>


const char* ssid = "MikroTik";
const char* password = "nustiuceparola";

#define lightsCount 3
#define pixelCount 60
#define startup_brightness 0
#define startup_color 0
// 0 = warm_white, 1 =  neutral, 2 = cold_white, 3 = red, 4 = green, 5 = blue

// if you want to setup static ip uncomment these 3 lines and line 72
//IPAddress strip_ip ( 192,  168,   10,  95);
//IPAddress gateway_ip ( 192,  168,   10,   1);
//IPAddress subnet_mask(255, 255, 255,   0);

uint8_t rgb[lightsCount][3];
bool light_state[lightsCount], level[lightsCount][3];
int fade[lightsCount];
float step_level[lightsCount][3], current_rgb[lightsCount][3];
byte mac[6];

ESP8266WebServer server(80);

RgbwColor red = RgbwColor(255, 0, 0, 0);
RgbwColor green = RgbwColor(0, 255, 0, 0);
RgbwColor white = RgbwColor(255);
RgbwColor black = RgbwColor(0);

NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip(pixelCount);

int getArgValue(String name)
{
  for (uint8_t i = 0; i < server.args(); i++)
    if (server.argName(i) == name)
      return server.arg(i).toInt();
  return -1;
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


void setup() {
  Serial.begin(115200);
  // this resets all the neopixels to an off state
  strip.Begin();
  strip.Show();

  // Show that the NeoPixels are alive
  delay(120); // Apparently needed to make the first few pixels animate correctly

  //WiFi.config(strip_ip, gateway_ip, subnet_mask);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (startup_brightness == 0) {
    infoLight(white);

    while (WiFi.status() != WL_CONNECTED) {
      infoLight(red);
      delay(500);
      Serial.print(".");
    }
    // Show that we are connected
    infoLight(green);
    
    //setup default warm_white on power on
    for (int i = 0; i < lightsCount; i++) {
      rgb[i][0] = 254; rgb[i][1] = 145; rgb[i][2] = 40;
    }

  } else {
    //setup start color/brightness and fade
    for (int i = 0; i < lightsCount; i++) {
      if ( startup_color == 0) {
        rgb[i][0] = (int) 254 * (startup_brightness / 255.0f); rgb[i][1] = (int) 145 * (startup_brightness / 255.0f); rgb[i][2] = (int) 40 * (startup_brightness / 255.0f);
      } else if ( startup_color == 1){
        rgb[i][0] = 254 * (startup_brightness / 255.0f); rgb[i][1] = 177 * (startup_brightness / 255.0f); rgb[i][2] = 111 * (startup_brightness / 255.0f);
      } else if ( startup_color == 2){
        rgb[i][0] = 254 * (startup_brightness / 255.0f); rgb[i][1] = 233 * (startup_brightness / 255.0f); rgb[i][2] = 216 * (startup_brightness / 255.0f);
      }  else if ( startup_color == 3){
        rgb[i][0] = 254 * (startup_brightness / 255.0f); rgb[i][1] = 0; rgb[i][2] = 0;
      }  else if ( startup_color == 4){
        rgb[i][0] = 0; rgb[i][1] = 254 * (startup_brightness / 255.0f); rgb[i][2] = 0;
      }  else if ( startup_color == 5){
        rgb[i][0] = 0; rgb[i][1] = 0; rgb[i][2] = 254 * (startup_brightness / 255.0f);
      }
      step_level[i][0] = rgb[i][0] / 100.0f; step_level[i][1] = rgb[i][1] / 100.0f; step_level[i][2] = rgb[i][2] / 100.0f;
      level[i][0] = true; level[i][1] = true; level[i][2] = true;
      light_state[i] = true;
    }
  }

  WiFi.macAddress(mac);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH


  server.on("/set", []() {
    int light = getArgValue("light") - 1;
    rgb[light][0] = getArgValue("r");
    rgb[light][1] = getArgValue("g");
    rgb[light][2] = getArgValue("b");
    fade[light] = getArgValue("fade");
    if (fade[light] == -1) fade[light] = 400;
    server.send(200, "text/plain", "OK, light = " + (String)(light + 1) + ", R:" + (String)rgb[light][0] + " ,B:" + (String)rgb[light][1] + " ,G:" + (String)rgb[light][2]);
    step_level[light][0] = (rgb[light][0] - current_rgb[light][0]) / (fade[light] / 2);
    step_level[light][1] = (rgb[light][1] - current_rgb[light][1]) / (fade[light] / 2);
    step_level[light][2] = (rgb[light][2] - current_rgb[light][2]) / (fade[light] / 2);
    rgb[light][0] > current_rgb[light][0] ? level[light][0] = true : level[light][0] = false;
    rgb[light][1] > current_rgb[light][1] ? level[light][1] = true : level[light][1] = false;
    rgb[light][2] > current_rgb[light][2] ? level[light][2] = true : level[light][2] = false;
    light_state[light] = true;
  });

  server.on("/off", []() {
    int light = getArgValue("light") - 1;
    fade[light] = getArgValue("fade");
    if (fade[light] == -1) fade[light] = 150;
    server.send(200, "text/plain", "OK, light = " + (String)(light - 1));
    step_level[light][0] = current_rgb[light][0] / (fade[light] / 1.5);
    step_level[light][1] = current_rgb[light][1] / (fade[light] / 1.5);
    step_level[light][2] = current_rgb[light][2] / (fade[light] / 1.5);
    light_state[light] = false;

  });

  server.on("/on", []() {
    int light = getArgValue("light") - 1;
    fade[light] = getArgValue("fade");
    if (fade[light] == -1) fade[light] = 150;
    server.send(200, "text/plain", "OK, light = " + (String)light);
    step_level[light][0] = rgb[light][0] / (fade[light] / 1.5);
    step_level[light][1] = rgb[light][1] / (fade[light] / 1.5);
    step_level[light][2] = rgb[light][2] / (fade[light] / 1.5);
    level[light][0] = true;
    level[light][1] = true;
    level[light][2] = true;
    light_state[light] = true;
  });

  server.on("/detect", []() {
    server.send(200, "text/plain", "{\"hue\": \"strip\",\"lights\": " + (String)lightsCount + ",\"type\": \"rgb\",\"mac\": \"" + String(mac[5], HEX) + ":"  + String(mac[4], HEX) + ":" + String(mac[3], HEX) + ":" + String(mac[2], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[0], HEX) + "\"}");
  });


  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  lightEngine();
}

void infoLight(RgbwColor color) {
  // Flash the strip in the selected color. White = booted, green = WLAN connected, red = WLAN could not connect
  for (int i = 0; i < pixelCount; i++)
  {
    strip.SetPixelColor(i, color);
    strip.Show();
    delay(10);
    strip.SetPixelColor(i, black);
    strip.Show();
  }
}

void lightEngine() {
  for (int i = 0; i < lightsCount; i++) {
    if (light_state[i]) {
      if (rgb[i][0] != (int)current_rgb[i][0] || rgb[i][1] != (int)current_rgb[i][1] || rgb[i][2] != (int)current_rgb[i][2]) {
        if (rgb[i][0] != (int)current_rgb[i][0]) current_rgb[i][0] += step_level[i][0];
        if (rgb[i][1] != (int)current_rgb[i][1]) current_rgb[i][1] += step_level[i][1];
        if (rgb[i][2] != (int)current_rgb[i][2]) current_rgb[i][2] += step_level[i][2];
        if (level[i][0] && current_rgb[i][0] > rgb[i][0]) current_rgb[i][0] = rgb[i][0];
        if (level[i][1] && current_rgb[i][1] > rgb[i][1]) current_rgb[i][1] = rgb[i][1];
        if (level[i][2] && current_rgb[i][2] > rgb[i][2]) current_rgb[i][2] = rgb[i][2];
        if (!level[i][0] && current_rgb[i][0] < rgb[i][0]) current_rgb[i][0] = rgb[i][0];
        if (!level[i][1] && current_rgb[i][1] < rgb[i][1]) current_rgb[i][1] = rgb[i][1];
        if (!level[i][2] && current_rgb[i][2] < rgb[i][2]) current_rgb[i][2] = rgb[i][2];
        for (int j = 0; j < pixelCount / lightsCount ; j++)
        {
          int white_level = 255;
          for (int k = 0; k < 3 ; k++) {
            if (current_rgb[i][k] < white_level)
              white_level = current_rgb[i][k];
          }
          strip.SetPixelColor(j + i * pixelCount / lightsCount, RgbwColor(current_rgb[i][0], current_rgb[i][1], current_rgb[i][2], white_level));
        }
      }
    } else {
      if ((int)current_rgb[i][0] != 0 || (int)current_rgb[i][1] != 0 || (int)current_rgb[i][2] != 0) {
        if ((int)current_rgb[i][0] != 0) current_rgb[i][0] -= step_level[i][0];
        if ((int)current_rgb[i][1] != 0) current_rgb[i][1] -= step_level[i][1];
        if ((int)current_rgb[i][2] != 0) current_rgb[i][2] -= step_level[i][2];
        if ((int)current_rgb[i][0] < 0) current_rgb[i][0] = 0;
        if ((int)current_rgb[i][1] < 0) current_rgb[i][1] = 0;
        if ((int)current_rgb[i][2] < 0) current_rgb[i][2] = 0;
        for (int j = 0; j < pixelCount / lightsCount ; j++)
        {
          int white_level = 255;
          for (int k = 0; k < 3 ; k++) {
            if (current_rgb[i][k] < white_level)
              white_level = current_rgb[i][k];
          }
          strip.SetPixelColor(j + i * pixelCount / lightsCount, RgbwColor(current_rgb[i][0], current_rgb[i][1], current_rgb[i][2], white_level));
        }
      }
    }
    strip.Show();
    delay(fade[0] / 400);
  }
}
