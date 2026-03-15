#include<ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include<string.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include "DHTesp.h"

/*-----------------khai báo pin ------------------------*/
#define DHTpin D7
#define LED_output D6

const uint16_t recv_pin = 14;  // Chân nhận tín hiệu IR (D5)
//const uint16_t LED_output = 13; //Chân output LED (D6)
//const uint16_t DHT_output = 12; //Chân cảm biến nhiệt độ (D7)

IRrecv irrecv(recv_pin);  // Khởi tạo đối tượng nhận IR
decode_results results;  // Khai báo kết quả nhận tín hiệu IR
/*----------------------------------------------------------------*/

int AC_state = 0;

DHTesp dht;
//----Khai báo WIFI---------------
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
  incommingMessage.trim();
  String check_on = "turn on the light";
  String check_off = "turn off the light";
  if (incommingMessage == check_on) {
    digitalWrite(LED_output, HIGH);
    Serial.println("LED_output is high");
  } else if (incommingMessage == check_off) {
    digitalWrite(LED_output, LOW);
  }
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
  irrecv.enableIRIn();
  pinMode(LED_output, OUTPUT);

  dht.setup(DHTpin,DHTesp::DHT11);

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
  DynamicJsonDocument doc_2(1024);
  char mqtt_message[128];
  delay(dht.getMinimumSamplingPeriod());
  float h = dht.getHumidity();
  float t = dht.getTemperature();

  doc_2["humidity"]=h;
  doc_2["temperature"]=t;
  doc_2["AC_state"] = AC_state;

  //----Kiểm tra xem có nhận được tín hiệu hồng ngoại không----//
  //*****Cái này mình dùng để gửi tín hiệu hồng ngoại cho mắt thu hồng ngoại dùng trên arduino uno****//
  if (irrecv.decode(&results)) {
    Serial.println("Received IR signal:");
    long int hexCode = results.value;
    Serial.println(hexCode, HEX);  // In mã tín hiệu dưới dạng số thập phân

    if(hexCode == 0xF7C03F){ //Có thể thay đổi cho tương đồng mã bên nhận và bên thu//
      AC_state = 1;
    }
    else if(hexCode == 0xF7C02F){ //Có thể thay đổi cho tương đồng mã bên nhận và bên thu//
      AC_state = 0;
    }
    irrecv.resume();
  }
  //--------------------------------------------------------//

  //---------Gửi tin nhắn lên gateway-----------------//
  serializeJson(doc_2,mqtt_message);
  if(millis()-timeUpdata>5000){ //Mỗi 5s gửi một lần//

    publishMessage("esp-1", mqtt_message, true);

    timeUpdata=millis();
  }
}

