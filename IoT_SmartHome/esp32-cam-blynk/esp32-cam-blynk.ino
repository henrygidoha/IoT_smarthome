
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

//----Thay đổi thành thông tin của bạn---------------
const char* ssid = "";      //Wifi connect
const char* password = "";   //Password

const char* mqtt_server = "";
const int mqtt_port = 8883;
const char* mqtt_username = ""; //User
const char* mqtt_password = ""; //Password
//--------------------------------------------------
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

void setup_wifi() {
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}
//------------Connect to MQTT Broker-----------------------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientID =  "ESPClient-";
    clientID += String(random(0xffff),HEX);
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");  
      client.subscribe("gateway");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//-----Call back Method for Receiving MQTT message---------
void callback(char* topic, byte* payload, unsigned int length) {
  String incommingMessage = "";
  for(int i=0; i<length;i++){ incommingMessage += (char)payload[i];}
  Serial.println("Message received from ["+String(topic)+"]"+incommingMessage);
}
//-----Method for Publishing MQTT Messages---------
void publishMessage(const char* topic, String payload, boolean retained){
  if(client.publish(topic,payload.c_str(),true))
    Serial.println("Message sent ["+String(topic)+"]: "+payload);
}


void setup() {
  Serial.begin(9600);
  while(!Serial) delay(1);

  setup_wifi();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}
unsigned long timeUpdata=millis();


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  //read DHT11
  if(millis()-timeUpdata>5000){

    String message = Serial.readString();
    if(message.length() > 0){
      publishMessage("esp32-cam", message, true);
    }
    timeUpdata=millis();
  }
}