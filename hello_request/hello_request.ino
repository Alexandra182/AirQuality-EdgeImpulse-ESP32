#include <ArduinoWebsockets.h>
#include <WiFi.h>

const char* ssid = "<YOUR_SSID>"; //Enter SSID
const char* password = "<YOUR_PASSWORD>"; //Enter Password
const char* websockets_server_host = "ws://remote-mgmt.edgeimpulse.com/"; //Enter server adress
const uint16_t websockets_server_port = 8080; // Enter server port

using namespace websockets;

WebsocketsClient client;

char input[] = "{\"hello\":"
               "{\"version\":3,"
               "\"apiKey\":\"<YOUR_API_KEY>\","
               "\"deviceId\": \"<YOUR_DEVICE_ID>\","
               "\"deviceType\":\"<YOUR_DEVICE_TYPE>\","
               "\"connection\": \"ip\","
               "\"sensors\": [{"
                  "\"name\": \"Air Quality\","
                  "\"frequencies\": [0],"
                  "\"maxSampleLengthS\": 60000}],"
               "\"supportsSnapshotStreaming\": false"
               "}}";

void setup() {
  Serial.begin(115200);
  // Connect to wifi
  WiFi.begin(ssid, password);

  // Wait some time to connect to wifi
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
    Serial.print(".");
    delay(1000);
  }

  // Check if connected to wifi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No Wifi!");
    return;
  }

  Serial.println("Connected to Wifi, Connecting to server.");
  // try to connect to Websockets server
  bool connected = client.connect(websockets_server_host);
  if (connected) {
    Serial.println("Connected!");
    client.send(input);
  } else {
    Serial.println("Not Connected!");
  }

  // run callback when messages are received
  client.onMessage([&](WebsocketsMessage message) {
    Serial.print("Got Message: ");
    Serial.println(message.data());
  });
}

void loop() {
  // let the websockets client check for incoming messages
  if (client.available()) {
    client.poll();
  }
  delay(500);
}
