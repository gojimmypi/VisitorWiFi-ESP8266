//
// Visitor WiFi Connect
// 
// ESP8266 code to programmatically conenct to a visitor WiFi hotspot that is open, however requires the
// user to press a button on a web page that says "I accept the terms and conditions".
// 

#include "htmlHelper.h"
#include <ESP8266WiFi.h>
//  include <user_interface.h>
extern "C" {
#include "user_interface.h"
}
#include <stdlib.h>



// My config is stored in myPrivateSettings.h file 
// if you choose not to use such a file, set this to false, and specify values below
#define USE_myPrivateSettings true

#if USE_myPrivateSettings == true
#	include "/workspace/myPrivateSettings.h"
#else
	const char* WIFI_SSID = "my-wifi-SSID"; //  "Organization Visitor WiFi";
	const char* WIFI_PWD = "my-WiFi-PASSWORD"; // blank for visitor access, or specify a password
#endif


const char* targetHost = "gojimmypi-dev-imageconvert2bmp.azurewebsites.net"; // note this MUST be added to wifiAccessRedirect to avoid getting the captcha response!

// for reference only:
//const char* wifiAccessRedirect = "/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html%26ap_mac=00:11:22:33:44:55%26client_mac=cc:11:22:33:44:55%26wlan=Visitor%20WiFi%26redirect=www.google.com/";
//const char* wifiAccessReferrer = "/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html&ap_mac=00:11:22:33:44:55&client_mac=cc:11:22:33:44:55&wlan=Visitor%20WiFi&redirect=www.google.com/";

String url;
const String CrLf = "\n\r";

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println();
  


  // sometimes we seem to wait forever to connect, so after a bunch of attempts WiFi.begin again
  const int MAX_WIFI_ATTEMPTS = 100;
  int i = MAX_WIFI_ATTEMPTS;
  while (WiFi.status() != WL_CONNECTED) {
	  Serial.print("Connecting to ");
	  Serial.println(WIFI_SSID);
	  WiFi.begin(WIFI_SSID, WIFI_PWD);
	  while ((WiFi.status() != WL_CONNECTED) && (i > 0)) {
		  delay(500);
		  Serial.print(".");
		  i--;
	  }
	  Serial.println(CrLf + "Trying again....");
  }


  // We start by connecting to a WiFi network
  //Serial.print("Connecting to ");
  //Serial.println(WIFI_SSID);
  //WiFi.begin(WIFI_SSID, WIFI_PWD);
  //while (WiFi.status() != WL_CONNECTED) {
	 // delay(500);
	 // Serial.print(".");
  //}

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("DNS address: ");
  Serial.println(WiFi.dnsIP());
  Serial.print("Gateway address: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Hostname: ");
  Serial.println(WiFi.hostname());
  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Diagnostics: ");
  WiFi.printDiag(Serial);
  Serial.println();
  Serial.print("ESP.getFreeHeap() =");
  Serial.println(ESP.getFreeHeap());
}




void loop() {
   
  // We now create a URI for the request
  url = "/";


  //**************************************************************************************************************
  // when we make the original request, if we've not pressed the "I accept the terms" button yet, 
  // we'll get a page reponse like this when visiting a web page:
  //
  // HTTP/1.1 200 OK
  // Location: http://1.1.1.1/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html&ap_mac=00:11:22:33:44:55&client_mac=11:22:33:44:55:66&wlan=My%20Visitor%20WiFi&redirect=www.google.com/
  // Content-Type: text/html
  // Content-Length: 440
  //
  // <HTML><HEAD><TITLE> Web Authentication Redirect</TITLE>
  // <META http-equiv="Cache-control" content="no-cache">
  // <META http-equiv="Pragma" content="no-cache">
  // <META http-equiv="Expires" content="-1">
  // <META http-equiv="refresh" content="1; URL=http://1.1.1.1/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.htmlap_mac=00:11:22:33:44:55&client_mac=11:22:33:44:55:66&wlan=My%20Visitor%20WiFi&redirect=www.google.com/">
  // </HEAD></HTML>
  // 
  // assumes the access point is called My Visitor Wifi, and we are trying to connect to google.com
  //
  // in particular, note the META refresh redirector.  TODO: programmatically extract the META redirect path
  //**************************************************************************************************************

  //**************************************************************************************************************

  // This will send the request to the server
  String htmlString;
  const char* internetHostCheck = "gojimmypi-dev-imageconvert2bmp.azurewebsites.net"; // some well known, reliable internet url (ideally small html payload)

  htmlString = String("GET http://") + String(internetHostCheck) + url + " HTTP/1.1\r\n" +
	  "Host: " + String(internetHostCheck) + "\r\n" +
	  "Content-Encoding: identity" + "\r\n" +
	  "Connection: Keep-Alive\r\n\r\n";

  //htmlHelper myHtmlHelper = new htmlHelper(targetHost, 80, htmlString);
  //int connectionStatus = myHtmlHelper.Send();



  int connectionStatus = htmlSend(internetHostCheck, 80, htmlString);
  if (connectionStatus == 0) {
	  Serial.println("Connected to internet!");
  }
  else if (connectionStatus == 1) {
	  Serial.println("Accepting Terms and Conditions...");

//	  Serial.print("ResponseLocation=");
//	  Serial.println(ResponseLocation);

	  Serial.println();

//	  Serial.print("ap_mac queryStringValue=");
//	  Serial.print(queryStringValue(ResponseLocation, "ap_mac"));

	  doAcceptTermsAndConditions();
  }
  else {
	  Serial.println("Error connecting.");
	  delay(500000);
  }



  Serial.println("\r\n");
  Serial.println("Requesting initial host: \r\n");

  //htmlString = "";
  //htmlString += "GET http://" + String(targetHost) + String("/") + " HTTP/1.1\r\n";
  //htmlString += "Host: " + String(targetHost) + "\r\n";
  //htmlString += "Connection: keep-alive\r\n";
  ////htmlString += "Upgrade-Insecure-Requests: 0\r\n";
  ////htmlString += String(wifiUserAgent) + "\r\n"; // "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; ASTE; rv:11.0) like Gecko\r\n";
  ////htmlString += "Accept-Language: en-US\r\n";
  ////htmlString += "Content-Encoding: identity\r\n"; // Indicates the identity function (i.e. no compression, nor modification). This token, except if explicitly specified, is always deemed acceptable.
  //Request += "\r\n"; // blank line before content

  //htmlSend(targetHost, 80, htmlString);


  htmlString = String("GET http://") + String(targetHost) + url + " HTTP/1.1\r\n" +
	  "Host: " + String(targetHost) + "\r\n" +
	  "Content-Encoding: identity" + "\r\n" +
	  "Connection: Keep-Alive\r\n\r\n";

  htmlSend(targetHost, 80, htmlString);



  ////**************************************************************************************************************
  //Serial.println("\r\n");
  //Serial.println("Requesting URL redirect: \r\n");
  ////**************************************************************************************************************
  //htmlString = String("GET ") + "/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html%26ap_mac=00:11:22:33:44:55%26client_mac=cc:11:22:33:44:55%26wlan=Visitor%20WiFi%26redirect=www.google.com/" + " HTTP/1.1\r\n" +
	 // "Host: " + String(accessHost) + "\r\n" +
	 // "Connection: keep-alive\r\n\r\n";


  htmlString = String("GET http://www.w3.org/ HTTP/1.1\r\n") +
	  "Host: www.w3.org\r\n" +
	  "Content-Encoding: identity" + "\r\n" +
	  "Connection: Keep-Alive\r\n\r\n";

  htmlSend("www.w3.org", 80, htmlString);



  Serial.println("Done; Halt\r\n");
//  client.stop();
//  client.stopAll();
  while (1) {
	  delay(100000);
  }


  ////**************************************************************************************************************
  ////**************************************************************************************************************
  ////**************************************************************************************************************
  ////**************************************************************************************************************
  ////**************************************************************************************************************
  ////**************************************************************************************************************
  ////**************************************************************************************************************
  ////**************************************************************************************************************




}
