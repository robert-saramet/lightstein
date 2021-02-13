#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <RTClib.h>
#include <TM1637Display.h>


#define CLK 2
#define DIO 15

#define DHTPIN 27
#define DHTTYPE DHT11

#define BUZZ 4
#define BUZZ_POW 5

#define RED_LED 12
#define GREEN_LED 13
#define BLUE_LED 14


RTC_DS3231 rtc;
TM1637Display display = TM1637Display(CLK, DIO);
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

const char* ssid = "UPC Connect Box (AP)";
const char* password = "";

const char* mqtt_server = "192.168.0.143";
long lastMsg = 0;
char msg[50];
int value = 0;

int alarm1 = 1100;
bool power = 1;
unsigned char lampMode = 1;

const int freq = 5000;
const int resolution = 8;
const int redChannel = 0;
const int greenChannel = 1;
const int blueChannel = 2;


unsigned char redValue;
unsigned char greenValue;
unsigned char blueValue;

unsigned char RGB[3];
short lampDelay;


void setup(){
  Serial.begin(115200);
  Serial.println("Started serial communication");

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);

   if(! rtc.begin()){
    Serial.println("RTC not found");
    while (1);
  }
  //if(rtc.lostPower()){
    //Serial.println("RTC lost power, setting time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //}

  //rtc.adjust(DateTime(2021, 2, 12, 12, 15, 0));

  display.setBrightness(5);
  display.clear();

  dht.begin();

  pinMode(BUZZ, OUTPUT);
  pinMode(BUZZ_POW, OUTPUT);
  //pinMode(RED_LED, OUTPUT);
  //pinMode(GREEN_LED, OUTPUT);
  //pinMode(BLUE_LED, OUTPUT);

  ledcSetup(redChannel, freq, resolution);
  ledcSetup(greenChannel, freq, resolution);
  ledcSetup(blueChannel, freq, resolution);

  ledcAttachPin(RED_LED, redChannel);
  ledcAttachPin(GREEN_LED, greenChannel);
  ledcAttachPin(BLUE_LED, blueChannel);
}


void loop(){
  Clock();
  MQTT();
  lamp();
}


void setup_wifi(){
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP adress: ");
  Serial.println(WiFi.localIP());
}


void callback(char* topic, byte* message, unsigned int length){
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++){
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "esp32/input/power"){
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      power = 1;
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      power = 0;    
    }
  }
  else if (String(topic) == "esp32/input/mode"){
    lampMode = messageTemp(unsigned char).c_str;
  }
  else if (String(topic) == "esp32/input/color/red"){
    redValue = unsigned char(messageTemp).c_str();
    Serial.print("Red LED set to");
    Serial.println(messageTemp);
  }
  else if (String(topic) == "esp32/input/color/green"){
    greenValue = unsigned char(messageTemp).c_str();
    Serial.print("Green LED set to");
    Serial.println(messageTemp);
  }
  else if (String(topic) == "esp32/input/color/blue"){
    blueValue = unsigned char(messageTemp).c_str();
    Serial.print("Blue LED set to");
    Serial.println(messageTemp);
  }
}


void reconnect(){
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")){
      Serial.println("connected");
      client.subscribe("esp32/input/power");
      client.subscribe("esp32/input/mode");
      client.subscribe("esp32/input/color/red");
      client.subscribe("esp32/input/color/green");
      client.subscribe("esp32/input/color/blue");
    } else{
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}


void MQTT(){
   if (!client.connected()) reconnect();
   client.loop();
  
   long now = millis();
   if (now - lastMsg > 5000) {
     lastMsg = now;
      
     float temperature = dht.readTemperature();   
     Serial.print("Temperature: ");
     Serial.println(temperature);
     client.publish("esp32/output/temperature", String(temperature).c_str());
  
     float humidity = dht.readHumidity();
     Serial.print("Humidity: ");
     Serial.println(humidity);
     client.publish("esp32/output/humidity", String(humidity).c_str());
   }
}



void Clock(){
   DateTime now = rtc.now();
   int displaytime = (now.hour() * 100) + now.minute();
   Serial.println(displaytime);
  
   display.showNumberDecEx(displaytime, 0b01000000, true);
   delay (1000);
   display.showNumberDec(displaytime, true);
  
   while(displaytime == alarm1) buzzer();
}


void buzzer(){
  unsigned char i, j;
  digitalWrite(BUZZ_POW, HIGH);
  
    for (i = 0; i <125; i++) // create tone
    {
      digitalWrite (BUZZ, HIGH) ; //send tone
      delay (1) ;
      digitalWrite (BUZZ, LOW) ; //no tone
      delay (1) ;
    }
    
    for (i = 0; i <125; i++) //different tone
    {
      digitalWrite (BUZZ, HIGH) ; 
      delay (2) ;
      digitalWrite (BUZZ, LOW) ;
      delay (2) ;
    }
    
  digitalWrite(BUZZ_POW, LOW);
  delay(300);
}


/*
void colon(){
  uint8_t segto;
  int value = 1244;
  segto = 0x80 | display.encodeDigit((value/ 100)%10);
  display.setSegments(&segto, 1, 1);
}
*/



void lamp(){
  if (power){
    switch (lampMode){
      case 0: //manual mode
        lampManual();
        break;
       case 1: //auto mode
        lampAuto();
        break;
       case 2: //temperature mode
        lampTemp();
        break;
       case 3: //fireplace mode
        lampFire();
        break;
       default:
        lampAuto();
    }
  }
  else return; 
}



void lampManual() {
  ledcWrite(redChannel, redValue);
  ledcWrite(greenChannel, greenValue);
  ledcWrite(blueChannel, blueValue);
}



void lampAuto() {
  for (int y=0; y<=3; y++) {
    if (y=1){
      for (int x=0;x<=255;x++){
        RGB[0]=255-x;
        RGB[1]=0;
        RGB[2]=0+x;
        ledcWrite(redChannel,RGB[0]);
        ledcWrite(greenChannel,RGB[1]);
        ledcWrite(blueChannel,RGB[2]);
        delay(lampDelay);
      }
    }
    if (y=2){
      for (int x=0;x<=255;x++){
        RGB[0]=0;
        RGB[1]=0+x;
        RGB[2]=255-x;
        ledcWrite(redChannel,RGB[0]);
        ledcWrite(greenChannel,RGB[1]);
        ledcWrite(blueChannel,RGB[2]);
        delay(lampDelay);
      }
    }
    if (y=3){
      for (int x=0;x<=255;x++){
        RGB[0]=0+x;
        RGB[1]=255-x;
        RGB[2]=0;
        ledcWrite(redChannel,RGB[0]);
        ledcWrite(greenChannel,RGB[1]);
        ledcWrite(blueChannel,RGB[2]);
        delay(lampDelay);
      }
    }
  }
}



void lampTemp() {
  
}



void lampFire(){
  
}