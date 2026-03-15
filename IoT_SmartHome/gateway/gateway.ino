//---------Thay đổi theo blynk server của bạn------------//
#define BLYNK_TEMPLATE_ID "....."
#define BLYNK_TEMPLATE_NAME "......."
#define BLYNK_AUTH_TOKEN "......"

#include<ESP8266WiFi.h>
#include "DHTesp.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <string.h>
#include <BlynkSimpleEsp8266.h>
#include <time.h>


//----Thay đổi thành thông tin Wifi của bạn---------------
const char* ssid = "......";      //Wifi connect
const char* password = "......";   //Password

//----Thay đổi thành thông tin server HiveMQ của bạn---------------
const char* mqtt_server = ".....";
const int mqtt_port = 8883;
const char* mqtt_username = "....."; //User
const char* mqtt_password = "....."; //Password

int state_led;
int detector, AC_state;
double humidity, temperature;
time_t start, end;


//--------------------------------------------------
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
//------------Connect to MQTT Broker-----------------------------
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientID =  "ESPClient-";
    clientID += String(random(0xffff),HEX);
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");  
      client.subscribe("esp-1");
      client.subscribe("esp32-cam");
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
  if(String(topic) == "esp32-cam"){
      String incommingMessage = "";
    for(int i=0; i<length;i++){ incommingMessage += (char)payload[i];}
    Serial.println("Message received from ["+String(topic)+"]"+incommingMessage);

    DynamicJsonDocument doc_1(128);
    DeserializationError error2 = deserializeJson(doc_1, incommingMessage);

    if (error2) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error2.c_str());
      return;
    }
    //-----Kiểm tra xem có phát hiện được người từ esp32-cam không-----//
    detector = doc_1["human"];
    //Serial.printf("humans = %i \r\n", detector);
    //-----Nếu "có" thì tính xem khoảng thời gian không có người trong bao lâu rồi-----//
    if(detector == 0){
      end = time(NULL);
    }

    //-----Nếu "không" thì bắt đầu tính thời gian không có người------//
    else if(detector == 1){
      start = time(NULL);
      //Serial.printf("start = %d \r\n", start);
    }
  }

//----Kiếm tra xem có nhận được tin nhắn từ esp1 không---------
  if(String(topic) == "esp-1"){
    String incommingMessage = "";
    for(int i=0; i<length;i++){ incommingMessage += (char)payload[i];}
    Serial.println("Message received from ["+String(topic)+"]"+incommingMessage);

    DynamicJsonDocument doc_2(128);

    DeserializationError error = deserializeJson(doc_2, incommingMessage);
    
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    humidity = doc_2["humidity"];
    temperature = doc_2["temperature"];
    AC_state = doc_2["AC_state"];
  }

  // Hiển thị giá trị nhận được
  Serial.print("Humidity: ");
  Serial.println(humidity);
  Serial.print("Temperature: ");
  Serial.println(temperature);
  Serial.print("AC: ");
  Serial.println(AC_state);

  // Gửi dữ liệu lên Blynk
  Blynk.virtualWrite(V1, temperature);  // Gửi nhiệt độ lên Virtual Pin V1
  Blynk.virtualWrite(V2, humidity);
  Blynk.virtualWrite(V3, AC_state);
  Blynk.virtualWrite(V4, detector);
}
//-----Method for Publishing MQTT Messages---------
void publishMessage(const char* topic, String payload, boolean retained){
  if(client.publish(topic,payload.c_str(),true))
    Serial.println("Message sent ["+String(topic)+"]: "+payload);
}


void setup() {
  Serial.begin(9600);
  while(!Serial) delay(1);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);

  setup_wifi();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

}
unsigned long timeUpdata=millis();

BLYNK_WRITE(V0)
{
  int button = param.asInt();
  if(button == 1){
    publishMessage("gateway", "turn on the light", true);
    state_led = 1;
    start = time(NULL);
    //Serial.printf("start1 = %d \r\n", start);
    
  }
  else if(button == 0){
    publishMessage("gateway", "turn off the light", true);
    state_led = 0;
  }
}

void warning()
{
  //Xác định khoảng thời gian giữa 2 lần không có người
  Serial.printf("Time is: %d s \r\n", end - start);

  //-----Nếu đèn hoặc điều hòa bật và thời gian không có người trong nhà là  10s thì gửi cảnh báo-------//
  if(state_led == 1 && (end -start) >= 10){
    //Serial.printf("state led = %i and time = %d \r\n", state_led, end - start);
    Blynk.logEvent("warning", String("Nobody's at home and the light is on "));
  }
  if(AC_state == 1 && (end-start) >= 10){
    Blynk.logEvent("warning", String("Nobody's at home and Air Conditioner is on "));
  }
}

void loop() {
  Blynk.run();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  //read DHT11
  if(millis()-timeUpdata>5000){
    warning();

    /*DynamicJsonDocument doc(1024);
    doc["humidity"]=h;
    doc["temperature"]=t;
    char mqtt_message[128];
    serializeJson(doc,mqtt_message);*/

    String message = Serial.readString();
    if(message.length() > 0){
      publishMessage("gateway", message, true);
    }
    timeUpdata=millis();
  }
}