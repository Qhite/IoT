#include "DHT.h"

#define PIN 2
#define DHTTYPE DHT22

DHT dht(PIN,DHTTYPE);

#include <Arduino.h>
#include <Phpoc.h>
#include <SPI.h>
#include <Timer.h>

#include <ArduinoJson.h>

static PhpocClient phpocClient;

#define INTERVAL 4000

#define HTTP_QUERY_SIZE 1400
uint8_t httpQuery[HTTP_QUERY_SIZE];

char *server = "api.openweathermap.org";
char *apiKey = "f19cb175165325beb3ffea3644f75e47";
char *cityNames[] = {"seoul"};
char *currentWeather = "GET /data/2.5/weather?";

uint16_t queryLength = 0;
bool isRunning = false;
String httpResponse = "";
int numberOfCity = 0;

float h,t,hic;
static char celTemp[7];
static char humi[7];
String tempasd, id, desc;
bool alreadyConnected = false;
String s;

void setup() {
  Serial.begin(115200);
  while (!Serial);// wait for serial port to connect.
  Serial.println();

  Phpoc.begin(PF_LOG_SPI | PF_LOG_NET);
  Serial.println(Phpoc.localIP());

  numberOfCity = sizeof(cityNames) / sizeof(char*);

  dht.begin();
  
}

void loop() {  
  apiLoop();
  httpLoop();
  DHTread();
  html();
  
}

void apiLoop()
{
  static int index = 0;
  static unsigned long prevMills = 0;
  bool callApi = false;

  unsigned long currentMills = millis();
  if(prevMills == 0 || (currentMills - prevMills >= INTERVAL))
  {
    prevMills = currentMills;
    callApi = true;
  }

  if(callApi)
  {
    queryLength = getQueryCurrentWeather(httpQuery, cityNames[index++], apiKey);
    if(index >= numberOfCity)
      index = 0;
      
    httpResponse = "";
    if(phpocClient.connect(server, 80))
    {
      isRunning = true;
      phpocClient.write(httpQuery, queryLength);
    }
  }
}

char buf[1024];
String f = "\r\n\r\n";
void httpLoop()
{
  if(isRunning)  
  {
    uint16_t cnt = phpocClient.available();
    if(cnt > 0)
    {
      uint8_t tryRead = cnt > 128 ? 128 : cnt;
      memset(buf, 0, 1024);
      uint16_t recv = phpocClient.read(buf, tryRead);
      
      httpResponse = httpResponse + buf;      
      
      int idx = httpResponse.lastIndexOf(f);
      if(idx >= 0)
        httpResponse = httpResponse.substring(idx + 4);
    }
    
    if(!phpocClient.connected())
    {
      isRunning = false;
      
      //----------------------------------------
      // JSON
      Serial.println(httpResponse.c_str());
      
      DynamicJsonBuffer jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(httpResponse);
      if(!root.success())
      {
        Serial.println("parseObject failed.");
        phpocClient.stop();
        isRunning = false;
        return;
      }
      
      uint16_t resultCode = root["cod"];
      //Serial.println(resultCode);
      if(resultCode == 200)
      {
        //
        JsonObject& weather = root["weather"][0];
        String weatherId = weather["id"]; 
        String cityName = root["name"]; 
        double temperature = root["main"]["temp"]; 
        double tempMax = root["main"]["temp_max"];
        String temp = String(lround(temperature));
        String humidity = root["main"]["humidity"]; 
        String weatherDesc = weather["description"]; 
        
        tempasd = temperature;
        id = humidity;
        desc = weatherDesc;
      }
      else
      {
        // fail.
      }
      //----------------------------------------
      
      Serial.println("disconnected");
      phpocClient.stop();
      
    }
  }
}

uint16_t writeString(const char* string, uint8_t* buf, uint16_t pos) 
{
    const char* idp = string;
    uint16_t i = 0;
 
    while (*idp) 
    {
        buf[pos++] = *idp++;
        i++;
    }
    
    return pos;
}

uint16_t getQueryCurrentWeather(uint8_t* query, char* cityName, char* apiKey)
{
  uint16_t queryLength = 0;

  queryLength = writeString(currentWeather, query, queryLength);
  queryLength = writeString("q=", query, queryLength);
  queryLength = writeString(cityName, query, queryLength);
  queryLength = writeString("&units=metric&appid=", httpQuery, queryLength);
  queryLength = writeString(apiKey, query, queryLength);  

  queryLength = writeString(" HTTP/1.1\r\nHost: api.openweathermap.org\r\n", query, queryLength);
  queryLength = writeString("Connection: Closed\r\n\r\n", query, queryLength);
  
  return queryLength;
}

void DHTread() {
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("Sensor Error");
    strcpy(celTemp,"Failed");
    strcpy(humi,"Falied");
  } else {
    hic = dht.computeHeatIndex(t,h,false);
    dtostrf(hic, 6, 2 , celTemp);
    dtostrf(h, 6, 2, humi);
  }
}

void html() {
  PhpocServer server(80);
  PhpocClient client = server.available();
  server.begin();
  if (client) {
    //bool blank = true;
    
    //while (client.connected()) {
      //if (client.available()) {
        //char c = client.read();
        
        //if (c == '\n' && blank) {
        if (!alreadyConnected) {
          //DHTread();
         
          client.flush();
          Serial.println("We have a new client");
          //s += "HTTP/1.1 200 OK\n";
          //s += "Context-Type: text/html\n";
          //s += "Connection: close\n";
          //s += "Refresh: 5\n";
          s += "<!DOCTYPE HTML>\n";
          s += "<html>\n";
          s += "<head></head><body><h1>PHPoC Web Server Test</h1><h2>Web Data</h2><h3>Current Temperature: ";
          s += tempasd;
          s += "*C</h3>\n";
          s += "<h3>Humidity: ";
          s += id;
          s += "%</h3>\n";
          s += "<h3>Weather: ";
          s += desc;
          s += "</h3>\n\n";
          s += "<h2>Sensor Data</h2><h3>Temperature: ";
          s += celTemp;
          s += "*C</h3>\n<h3>Humidity: ";
          s += humi;
          s += "%</h3>";
          s += "</body></html>";
          alreadyConnected = true;
          client.println(s);
          client.stop();
        }
        /*
        if(c == '\n') {
          blank = true;
        }

        else if (c != '\r') {
          blank = false;
        }*/
      //}
    //}
  
  }
}

