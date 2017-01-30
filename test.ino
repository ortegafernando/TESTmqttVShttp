//ESP.getResetInfoPtr
extern "C" {
#include <user_interface.h>
}

//Tipo 0
#include <ESP8266WiFi.h> // ESP8266WiFi.h library
// make TCP connections
WiFiClient client;

const char* ssid     = "Internetete";
const char* password = "gabino27";
const char* host = "api.thingspeak.com";
const char* writeAPIKey = "WSU827KMOVTCAZRO";

//Tipo 1
#include <PubSubClient.h>
// Initialize the PuBSubClient library
PubSubClient mqttClient(client);
// Define the ThingSpeak MQTT broker
const char* mqttserver = "mqtt.thingspeak.com";
const char* channelID = "220480";

byte tipo = 1;
//If define DEEPSLEEP both methods will deepsleep instead of delay. Also, will send to Thingspeak miliseconds for sending de message
#define DEEPSLEEP
#define SLEEPTIME 120  //segundos
/*
 * Tipo:
 * 0: sin sleep, con HTTP, en la oficina (habitacion de estudiar), envio cada 120 segundos
 * 1: idem al 0, pero mediante MQTT
 */

#ifdef DEEPSLEEP
    //Con deepSleep y dado que Thingspeak solo me deja mandar un dato cada 15 segundos, lo que hago es guardar el tiempo en el RTCmem y luego envio el tiempo "con el siguiente".
    #define RTCMEMORYSTART 64
    #define COLLECT 27
    typedef struct {
      int magicNumber;
      unsigned long tiempo; 
    } rtcManagementStruc;
    rtcManagementStruc rtcManagement;
    byte buckets = 0;
#endif


float vcc = 0;
unsigned long tiempo = 0;

void setup() {

  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.print("Booted ");  

    rst_info *rsti;
    rsti = ESP.getResetInfoPtr();

    Serial.print("rsti->reason: ");
    Serial.println(rsti->reason);

  #ifdef DEEPSLEEP

//enum rst_reason {
//        REASON_DEFAULT_RST              = 0,    /* normal startup by power on */
//        REASON_WDT_RST                  = 1,    /* hardware watch dog reset */
//        REASON_EXCEPTION_RST    = 2,    /* exception reset, GPIO status won’t change */
//        REASON_SOFT_WDT_RST     = 3,    /* software watch dog reset, GPIO status won’t change */
//        REASON_SOFT_RESTART     = 4,    /* software restart ,system_restart , GPIO status won’t change */
//        REASON_DEEP_SLEEP_AWAKE = 5,    /* wake up from deep-sleep */
//        REASON_EXT_SYS_RST      = 6             /* external system reset */
//};

  //if (rsti->reason==0){Serial.println("CH_PD button from deep sleep = SW1");};
  //// PowerOn from normal run, not from deep sleep is 0, the same as CH_PD from deep sleep !!!
  //if (rsti->reason==5){Serial.println("Power ON from deep sleep!");};
  //if (rsti->reason==6){Serial.println("RST button from deep sleep = SW2");}
  
    //Paquetes de 4 bytes, que es como se organiza la memoria del RTC
    buckets = (sizeof(rtcManagement) / 4);
    if (buckets == 0) buckets = 1;
    
    switch (rsti->reason) {
      case 5:
        //Serial.println(" from RTC-RESET (ResetInfo.reason = 5)");
        //Leer en paquetes de 4
        system_rtc_mem_read(RTCMEMORYSTART, &rtcManagement, buckets * 4);   
        if (rtcManagement.magicNumber == COLLECT) {
          //Parece que esta bien el numero de verificación => la RTCmem no se ha borrado => cojo el tiempo
          tiempo = rtcManagement.tiempo;
        } else {
          //Reinicio el sistema para la siguiente
          rtcManagement.magicNumber = COLLECT;
          rtcManagement.tiempo = 0;
          system_rtc_mem_write(RTCMEMORYSTART, &rtcManagement, buckets * 4);          
        }
        break;
      case 6:
        //Serial.println(" from POWER-UP (ResetInfo.reason = 6)");
        //Reinicio el sistema para la siguiente
        rtcManagement.magicNumber = COLLECT;
        rtcManagement.tiempo = 0;
        system_rtc_mem_write(RTCMEMORYSTART, &rtcManagement, buckets * 4);      
        break;
    }
  #endif
  
  //Connect to WiFi network
  WiFi.persistent(false);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  if (tipo == 1) {
    // Set the MQTT broker details
    mqttClient.setServer(mqttserver, 1883);
  }
}

void loop() {

  switch (tipo) {
    case 0:
      if (!client.connect(host, 80)) {
        return;
      }
      {
        String url = "/update?api_key=";      
        url+=writeAPIKey;
        url+="&field1=";
        url+=String( analogRead(0) * 4.2 / 1023 );
        #ifdef DEEPSLEEP
          if (tiempo != 0) {
            url+="&field2=";
            url+=String(tiempo);
          }
        #endif
        url+="\r\n";
        // Request to the server
        client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" + 
                   "Connection: close\r\n\r\n");
      }
      client.stop();
      break;
    case 1:
      // Check if MQTT client has connected else reconnect
      if (!mqttClient.connected()) 
      {
        reconnect();
      }
      // Call the loop continuously to establish connection to the server
      mqttClient.loop();
      mqttpublish();
      break;
  }
  #ifdef DEEPSLEEP
      //Grabo el tiempo hasta aqui
      tiempo = millis();  
      rtcManagement.tiempo = tiempo; 
      //Y lo guardo en la memoria RTC para el siguente reinicio tras el reset
      system_rtc_mem_write(RTCMEMORYSTART, &rtcManagement, buckets * 4); 
      ESP.deepSleep(120*1000000);  //microseconds
  #else
      delay(120*1000);
  #endif  
}

//TIPO 1
void reconnect() 
{
  // Loop until we're reconnected
  while (!mqttClient.connected()) 
  {
    if (!mqttClient.connect("ESP8266")) 
    {
      // Wait 5 seconds before retrying to connect again
      delay(5000);
    }
  }
}
void mqttpublish() {
  // Create data string to send to ThingSpeak
  String data="field3=";
  if (tipo == 0) {
    data="field1=";
  }
  data+=String( analogRead(0) * 4.2 / 1023 );
  #ifdef DEEPSLEEP
    if (tiempo != 0) {
      if (tipo == 1) {
        data+="&field4=";
      } else {
        data+="&field2=";
      }
      data+=String(tiempo);
    }
  #endif
  // Get the data string length
  int length = data.length();
  char msgBuffer[length];
  // Convert data string to character buffer
  data.toCharArray(msgBuffer,length+1);
  //Serial.println(msgBuffer);
  String url="channels/";
  url+=channelID;
  url+="/publish/";
  url+=writeAPIKey;
  length = url.length();
  char urlBuffer[length];
  url.toCharArray(urlBuffer,length+1);  
  // Publish data to ThingSpeak. Replace <YOUR-CHANNEL-ID> with your channel ID and <YOUR-CHANNEL-WRITEAPIKEY> with your write API key
  mqttClient.publish(urlBuffer,msgBuffer);
  // note the last connection time
  //lastConnectionTime = millis();
}
