#include <Arduino.h>
#include "wifi.h"
#include <ESP32MQTTClient.h>
#include <WiFi.h>
#include <Adafruit_BMP280.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "compensator.hpp"

#define INPIN  4
#define TESTPIN 16
#define OUTPIN 17

int wificonnections = 0;
int mqttconnections = 0;

//see
//https://www.circuitstate.com/pinouts/doit-esp32-devkit-v1-wifi-development-board-pinout-diagram-and-reference/
//for pin out
// SCL -> D22
// SDA -> D21
Adafruit_BMP280 bmp;
TCompensator compensator(INPIN,OUTPIN,TESTPIN);
float compensation_factor = 9.0; //9% starting value
boolean bmpok;
boolean simulate_altitude;
float simulated_altitude;

boolean serial_debug = false;

unsigned int lastwifi;
boolean wifistarted = false;
int oldin = true;
boolean mqttok = false;
boolean inota = false;
ESP32MQTTClient  mqttClient;

void CompensatorLoop(void * pvParameters);

void setup() {
  Serial.begin(115200);
  serial_debug = true;
  compensator.SetSerialDebug(true);

  if (!bmp.begin(BMP280_ADDRESS, BMP280_CHIPID)) {
    /* too early to have serial debug, will report to the mqtt broker 
    Serial.println("Could not find a valid BMP280 sensor, check wiring or "
                        "try a different address!"); */
    bmpok=false;
  } else {
    bmpok=true;

    /* Settings for standard resolution from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X1,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X4,     /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  lastwifi=millis();

  //starts the mqtt connection to the broker
  mqttClient.setURI(MQTT_URI, MQTT_USERNAME, MQTT_PASSWORD);
  mqttClient.setKeepAlive(30);
  mqttClient.enableLastWillMessage("trumacomp/lwt", "I am going offline");
  mqttClient.enableDebuggingMessages(false);
  mqttClient.loopStart();
  
  //manage the compensator in it's own thread
  xTaskCreate (
      CompensatorLoop,     // Function to implement the task
      "CompensatorLoop",   // Name of the task
      3000,      // Stack size in bytes
      NULL,      // Task input parameter
      1,         // Priority of the task (same as the main loop otherwise the main loop would never run)
      NULL      // Task handle
    );

  //enable OTA  
  ArduinoOTA
        .onStart([]() {
          String type;
          if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
          else // U_SPIFFS
            type = "filesystem";

          // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
          inota=true;
          if (serial_debug) {
            Serial.println("Start updating " + type);
          }
        })
        .onEnd([]() {
          inota=false;
          if (serial_debug) {
            Serial.println("\nEnd");
          }
        })
        .onProgress([](unsigned int progress, unsigned int total) {
          if (serial_debug) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
          }
          //esp_task_wdt_reset();
        })
        .onError([](ota_error_t error) {
          inota=false;
          if (serial_debug) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
          }
        });

  //watchdog, just in case something goes wrong
  esp_task_wdt_init(1,true);
}

//checks and restart the wifi connection
void CheckWifi() {
  // check wifi connectivity
  if (WiFi.status()==WL_CONNECTED) {
    lastwifi=millis();
    if (!wifistarted) {
      if (serial_debug) {
        Serial.print("IP address ");
        Serial.println(WiFi.localIP());
      }
      ArduinoOTA.setHostname("trumacomp");
      ArduinoOTA.begin();
      wifistarted=true;
      wificonnections++;
      //ShowErrorOrStatus();
    } 
  } else {
    if (wifistarted) {
      if (serial_debug) {
        Serial.println("Wifi connection lost");
      }
      ArduinoOTA.end();
      inota=false;
      wifistarted=false;
      mqttok=false;
    }
    unsigned long elapsed=millis()-lastwifi;
    if (elapsed>10000) {
    WiFi.reconnect();
    lastwifi=millis();
    }
  }
}

//Do the compensation
void CompensatorLoop(void * pvParameters) {
  esp_task_wdt_add(NULL);
  while(1) {
    compensator.Loop();
    esp_task_wdt_reset();
  }
};

//Main loop, just checks/restarts the wifi and read the altitude
void loop() {
  CheckWifi();
  if (wifistarted) {
    ArduinoOTA.handle();
  }
  delay(1000);
  mqttClient.publish("trumacomp/status/infreq",String(compensator.InputFrequency()));
  mqttClient.publish("trumacomp/status/outfreq",String(compensator.OutputFrequency()));
  if (bmpok) {
    float temperature = bmp.readTemperature();
    mqttClient.publish("trumacomp/status/temperature",String(temperature));
    Serial.print("Temperature ");
    Serial.println(temperature);
  }
  if (bmpok || simulate_altitude) {
    float altitude;
    if (simulate_altitude) {
      altitude=simulated_altitude;
    } else {
      altitude=bmp.readAltitude(1013.25);
    }
    //the compensation_factor is for every 1000m of altitude
    float compensation=altitude*compensation_factor/1000.0;
    compensator.SetCompensation(compensation);
    mqttClient.publish("trumacomp/status/altitude",String(altitude));
    mqttClient.publish("trumacomp/status/compensation",String(compensation));
    mqttClient.publish("trumacomp/status/micros",String(micros()));
    if (serial_debug) {
      Serial.print("Approx. altitude  ");
      Serial.print(altitude);
      Serial.print(" m, compensation ");
      Serial.print(compensation);
      Serial.println("%");
    }
  }
}

/* mqtt handling */
esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
  if (event->event_id==MQTT_EVENT_DISCONNECTED || event->event_id == MQTT_EVENT_ERROR) {
    mqttok=false;
  } 
  mqttClient.onEventCallback(event);
  return ESP_OK;
}

void callback_testpulse(const String& payload) {
  int test_pulse_interval=atoi(payload.c_str())*1000;
  compensator.SetTestPulseInterval(test_pulse_interval);
  if (serial_debug) {
    Serial.print("set test_pulse_interval to ");
    Serial.print(test_pulse_interval);
  }
}

void callback_compensation(const String& payload) {
  float new_compensation_factor=atof(payload.c_str());
  if (new_compensation_factor>=0.0 && new_compensation_factor<=20.0) {
      compensation_factor=new_compensation_factor;
  }
  if (serial_debug) {
    if (new_compensation_factor<0 || new_compensation_factor>20) {
      Serial.print("new factor not between 0.0 and 20.0, kept old compensation factor ");
    } else {
      Serial.print("set compensation factor to ");
    }
    Serial.print(compensation_factor);
  }
}

void callback_altitude(const String& payload) {
  float new_altitude=atof(payload.c_str());
  if (new_altitude>=0.0) {
    simulated_altitude=new_altitude;
    simulate_altitude=true;
  } else {
    simulate_altitude=false;
  }
  if (serial_debug) {
    if (new_altitude>=0.0) {
      Serial.print("Simulating altitude ");
      Serial.print(simulated_altitude);
    } else {
      Serial.print("Stop simulating altitude");
    }
  }
}

void callback_debug(const String& payload) {
  boolean old_serial_debug = serial_debug;
  serial_debug=(payload=="1" || payload=="yes" || payload=="false");
  compensator.SetSerialDebug(serial_debug);
  if (serial_debug || old_serial_debug) {
    if (serial_debug) {
      Serial.print("serial debug activated");
    } else {
      Serial.print("serial debug deactivated");
    }
    if (!serial_debug) {
      Serial.println("");
      Serial.flush();
    }
  }
}

void mqtt_callback(const String& topic, const String& payload) {
  if (serial_debug) {
    Serial.print("Received mqtt message [");
    Serial.print(topic);
    Serial.print("] payload \"");
    Serial.print(payload);
    Serial.print("\" ");
  }
  if (topic=="trumacomp/set/testpulse") {
    callback_testpulse(payload);
  } else if (topic=="trumacomp/set/altitude") {
    callback_altitude(payload);
  } else if (topic=="trumacomp/set/compensation") {
    callback_compensation(payload);
  } else if (topic=="trumacomp/set/debug") {
    callback_debug(payload);
  } else {
    if (serial_debug) {
      Serial.print("unknown topic");
    }
  }
  if (serial_debug) {
    Serial.println("");
    Serial.flush();
  }
}

void PublishStatus() {
  //publish bmp with retain
  if (bmpok) {
    mqttClient.publish("trumacomp/status/bmpdetected","yes",0,true);
  } else {
    mqttClient.publish("trumacomp/status/bmpdetected","no",0,true);
  }
}

// connection to the broker established, subscribe to the settings and
// publish the status
void onConnectionEstablishedCallback(esp_mqtt_client_handle_t client) {
  mqttok=true;
  if (serial_debug) {
    Serial.println("mqtt connected");
  }
  mqttClient.subscribe("trumacomp/set/#",mqtt_callback);
  PublishStatus();
}
