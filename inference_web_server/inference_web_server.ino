/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "SparkFunCCS811.h"
#include <air-quality-esp32_inferencing.h>
#include <Arduino_JSON.h>

/*********************** Web Server **************************/

const char* ssid = "<SSID>";
const char* password = "<PASS>";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

/*********************** Edge Impulse **************************/

#define FREQUENCY_HZ        EI_CLASSIFIER_FREQUENCY
#define INTERVAL_MS         (1000 / (FREQUENCY_HZ + 1))
#define CCS811_ADDR         0x5B

CCS811 airQualitySensor(CCS811_ADDR);

static unsigned long last_interval_ms = 0;
// to classify 1 frame of data you need EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE values
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
// keep track of where we are in the feature array
size_t feature_ix = 0;

/*********************** Functions **************************/

float anomalyScore = 0;

// Get Sensor Readings and return JSON object
String getSensorReadings() {
  airQualitySensor.readAlgorithmResults();
  readings["co2"] = String(airQualitySensor.getCO2());
  readings["tvoc"] = String(airQualitySensor.getTVOC());
  readings["result"] = String(anomalyScore);

  String jsonString = JSON.stringify(readings);
  Serial.println(jsonString);
  return jsonString;
}

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else {
    Serial.println("SPIFFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  initSPIFFS();

  // Initialize air quality sensor
  Wire.begin();
  if (airQualitySensor.begin() == false) {
    Serial.print("CCS811 error. Please check wiring. Freezing...");
    while (1)
      ;
  }

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");

  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String();
  });

  events.onConnect([](AsyncEventSourceClient * client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  // Start server
  server.begin();
}

void loop() {
  if (millis() > last_interval_ms + INTERVAL_MS) {
    last_interval_ms = millis();

    if (airQualitySensor.dataAvailable()) {

      // read sensor data in exactly the same way as in the Data Forwarder example
      airQualitySensor.readAlgorithmResults();

      // fill the features buffer
      features[feature_ix++] = airQualitySensor.getCO2();
      Serial.println(feature_ix);
    
      events.send("ping", NULL, millis());
      events.send(getSensorReadings().c_str(), "new_readings" , millis());
    }

    // features buffer full? then classify!
    if (feature_ix == EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
      ei_impulse_result_t result;

      // create signal from features frame
      signal_t signal;
      numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
      Serial.println("passed");
      // run classifier
      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
      Serial.println("classifier");

      ei_printf("run_classifier returned: %d\n", res);
      if (res != 0) return;

      // print predictions
      ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);

      // print the predictions
      for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        ei_printf("%s:\t%.5f\n", result.classification[ix].label, result.classification[ix].value);
      }

      anomalyScore = result.classification[0].value;

      // reset features frame
      feature_ix = 0;
    }
  }
}
