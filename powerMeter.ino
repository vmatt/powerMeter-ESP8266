#include "EmonLib.h"
#include "ESP8266WiFi.h"
#include "WIFIClient.h"
#include "vector"
#include "WiFiUDP.h"
#include <ESP8266HTTPClient.h>
#define UDP_PORT 6969
using namespace std;

//measurement
EnergyMonitor emon1;
double average;
double Irms;
std::vector< double > meas;
static int check = 0;
//wifi
WiFiClient client;
const char* ssid     = "";          // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = ""; 
HTTPClient http;

WiFiUDP UDP;
IPAddress UDPIP(192,168,0,122);     //The static IP address of the deivce
char packetBuffer[255]; 
char  replyBuffer[] = "ack";



void sendAmper(double amp) {
    http.begin("http://.../logger.php?amper="+String(amp));   //the URL for the php logger
    int httpCode = http.GET();   
    http.end();
    return; 
}

void setup()
{
  Serial.begin(9600);
  delay(100);
  emon1.current(A0, 30); 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);       // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(5000);
    Serial.print(++i); Serial.print(' ');
  }
  
  Serial.println('\n');
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); 
  sendAmper(99999);
}

double measure () {

  Irms = (emon1.calcIrms(1480));
  Irms = Irms-(0.1064309542*pow(1.0986371705,Irms)); //Linear regression correction of amp values.
  if (Irms <= 0){Irms = 0;}
  meas.push_back(Irms);
  int now = millis();
    if (now >= check) {
     check = now + 57834;                            //logging every minute, minus cca 2 sec, because network delays
     average = accumulate(meas.begin(), meas.end(), 0.0)/meas.size();
     meas.clear();  
     sendAmper(average);   
     
    }
  return Irms; 
  }
void loop()
{ 
  char f[255] = {0};
  double Ir = measure();
  delay(5000);
  snprintf(f, 255, "I:%lf", Ir);
  Serial.println(f);
  UDP.beginPacket("192.168.0.255", UDP_PORT);   //send values to 196.168.0 broadcast address for testing purposes
  UDP.write(f);
  UDP.endPacket();
 
}  
