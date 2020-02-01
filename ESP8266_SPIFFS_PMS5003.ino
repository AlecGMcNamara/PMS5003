/*
Alec McNamara Jan 2020 alecgmcnamara@gmail.com

Board type 'NodeMCU 10.(ESP-12E Module) Board manager esp8266
by esp8266 Community version 2.6.3

Connections
 PMS5003   NodeMCU
   VCC        Vin       +5V
   GND        GND        0V
   TX         D4        GPIO2    

1) Upload files in data folder
2) Compile and upload program
3) Connect to 'PMS-5003' wireless network (no password)
4) goto address http://192.168.1.1
*/

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SoftwareSerial.h>

const char* ssid = "PMS-5003";

SoftwareSerial pmsSerial(2,3);
AsyncWebServer server(80);
IPAddress     apIP(192, 168, 1, 1);

struct pms5003data {
  uint16_t framelen;
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um, particles_25um, particles_50um, particles_100um;
  uint16_t unused;
  uint16_t checksum;
};
struct pms5003data data;

float moving_average10 = 0;
float moving_average25 = 0;
float moving_average100 = 0;
int count = 0;
  
// Replaces placeholder with PMS data value
String processor(const String& var){
  if(var == "P1"){ return String(data.pm10_standard); }
  else if (var == "P2"){ return String(data.pm25_standard); }
  else if (var == "P3"){ return String(data.pm100_standard); }
  else if (var == "P4"){ return String(data.pm10_env); }
  else if (var == "P5"){ return String(data.pm25_env); }
  else if (var == "P6"){ return String(data.pm100_env); }
  else if (var == "P7"){ return String(data.particles_03um); }
  else if (var == "P8"){ return String(data.particles_05um); }
  else if (var == "P9"){ return String(data.particles_10um); }
  else if (var == "P10"){ return String(data.particles_25um); }
  else if (var == "P11"){ return String(data.particles_50um); }
  else if (var == "P12"){ return String(data.particles_100um); }
  else if (var == "PA1"){ return String(moving_average10); }
  else if (var == "PA2"){ return String(moving_average25); }
  else if (var == "PA3"){ return String(moving_average100); }
  else if (var == "C1"){ return String("(count = " + String(count) +")"); }
  else if (var == "TM1"){ return String(calcRunTime()); }    
}

// calculate runtime since start hh:mm:ss
  String calcRunTime()  {
  unsigned long runMillis= millis();
  unsigned long allSeconds=millis()/1000;
  int runHours= allSeconds/3600;
  int secsRemaining=allSeconds%3600;
  int runMinutes=secsRemaining/60;
  int runSeconds=secsRemaining%60;
  char buf[50];//was 28 - test started 6:25pm
  sprintf(buf,"Runtime (hh:mm:ss) %02d:%02d:%02d",runHours,runMinutes,runSeconds);
  return(buf); }

String handleUpdate()
{ 
   String updatedata= String (data.pm10_standard) + "," + String(data.pm25_standard)  + "," + String(data.pm100_standard) + ",";
        updatedata += String(data.pm10_env) + "," + String(data.pm25_env) + "," + String(data.pm100_env) + ",";
        updatedata += String(data.particles_03um) + "," + String(data.particles_05um) + "," + String(data.particles_10um) + ",";
        updatedata += String(data.particles_25um) + "," + String(data.particles_50um) + "," + String(data.particles_100um) + ",";
        updatedata += String(moving_average10) + "," + String(moving_average25) + "," + String(moving_average100) + ",";
        updatedata += "Moving Average (count = " + String(count) +") ," + calcRunTime();   
        return(updatedata);
}
 
void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
 
// Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred mounting SPIFFS");
    return;
  }

  Serial.println("Setting AP (Access Point)…");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid);
  
  Serial.print("SSID = ");
  Serial.print( &ssid[0]);
  Serial.print(" IP = ");
  IPAddress IP = WiFi.softAPIP();
  Serial.println(IP);  

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", handleUpdate().c_str());
  });
  // Route to load style.css file
  server.on("/PMS.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/PMS.css", "text/css");
  });

  // Route to load bg.gif file
  server.on("/bg.gif", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/bg.gif", "text/css");
  });

// Start server
   server.begin();

// PMS5003 sensor baud rate 9600
   pmsSerial.begin(9600);
}
 
void loop(){
  readPMSdata(&pmsSerial); 
}

boolean readPMSdata(Stream *s) {
  if (! s->available()) {
    return false;
  }
  // Read a byte at a time until we get to the special '0x42' start-byte
  if (s->peek() != 0x42) {
    s->read();
    return false;
  }
  // Now read all 32 bytes
  if (s->available() < 32) {
    return false;
  }    
  uint8_t buffer[32];    
  uint16_t sum = 0;
  s->readBytes(buffer, 32);
  // get checksum ready
  for (uint8_t i=0; i<30; i++) {
    sum += buffer[i];
  }
  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[15];
  for (uint8_t i=0; i<15; i++) {
    buffer_u16[i] = buffer[2 + i*2 + 1];
    buffer_u16[i] += (buffer[2 + i*2] << 8);
  }
  memcpy((void *)&data, (void *)buffer_u16, 30);
  if (sum != data.checksum) {
    Serial.println("Checksum failure");
    return false;
  }
  //calculate moving averages
 if(data.pm25_standard > 0)  {   // wait until warmed up 
      if( count < 21600 ) count++;  // maximum 6 hour moving average 
      float difference = (data.pm10_standard - moving_average10) / count;
      moving_average10 += difference;
      difference = (data.pm25_standard - moving_average25) / count;
      moving_average25 += difference;
      difference = (data.pm100_standard - moving_average100) / count;
      moving_average100 += difference; }
  
 return true;
}
