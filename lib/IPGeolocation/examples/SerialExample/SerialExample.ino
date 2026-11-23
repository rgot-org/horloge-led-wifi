
#include <WiFi.h>



#define DEBUG 1
#include "IPGeolocation.h"


// Replace with your wifi credentials
const char * ssid = "MonReseauWiFi";
const char * password = "MonMotDePasseWiFi";

// Get an API Key by registering on
// https://ipgeolocation.io

//String Key = "VOTRE_CLE_IPREGISTRY"; // ABSTRACT
//String Key = "VOTRE_CLE_IPREGISTRY"; //ipgeolocalisation
String Key = "VOTRE_CLE_IPREGISTRY"; //IPREGISTRY

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("WiFi Failed");
      while(1) {
          delay(1000);
      }
  }
  else {
      Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  //IPGeolocation location(Key,"ABSTRACT");
  IPGeolocation location(Key, "IPREGISTRY");
  IPGeo IPG;
  location.updateStatus(&IPG);
  Serial.println(IPG.city);
  Serial.println(IPG.zip_code);
  Serial.println(IPG.country);
  Serial.println(IPG.country_code);
  Serial.println(IPG.latitude);
  Serial.println(IPG.longitude);
  //Serial.println(IPG.tz);
  Serial.println(IPG.offset);
  //Serial.println(location.getResponse());
}

void loop() {


 // delay(600000);       // Wait for 600 seconds
  // put your main code here, to run repeatedly:
}