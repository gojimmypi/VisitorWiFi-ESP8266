//
// Visitor WiFi Connect
// 
// ESP8266 code to programmatically conenct to a visitor WiFi hotspot that is open, however requires the
// user to press a button on a web page that says "I accept the terms and conditions".
// 

#include <ESP8266WiFi.h>
//  include <user_interface.h>
extern "C" {
#include "user_interface.h"
}
#include <stdlib.h>


const int ESP_MIN_HEAP = 4096; // we'll monitor operations to ensure we never run out of space (e.g. a massive web page)
int MAX_WEB_RESPONSE = (ESP.getFreeHeap() > 10000 ? ESP.getFreeHeap() - 10000 : 10000);
const int myDebugLevel = 3; // 3 is maximum verbose level

// My config is stored in myPrivateSettings.h file 
// if you choose not to use such a file, set this to false, and specify values below
#define USE_myPrivateSettings true

#if USE_myPrivateSettings == true
#    include "/workspace/myPrivateSettings.h"; 
#else
const char* WIFI_SSID = "my-wifi-SSID"; //  "Organization Visitor WiFi";
const char* WIFI_PWD = "my-WiFi-PASSWORD"; // blank for visitor access, or specify a password
#endif


const char* internetHostCheck = "gojimmypi-dev-imageconvert2bmp.azurewebsites.net"; // some well known, reliable internet url (ideally small html payload)

const char* targetHost = "gojimmypi-dev-imageconvert2bmp.azurewebsites.net"; // note this MUST be added to wifiAccessRedirect to avoid getting the captcha response!
const char* accessHost = "1.1.1.1"; // 1.1.1.1 is typically the address of a Cisco WiFi guest Access Point. TODO extract from http response
const char* wifiUserAgent = "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; ASTE; rv:11.0) like Gecko";
const int MAX_CONNECTION_TIMEOUT_MILLISECONDS = 8000;

// for reference only:
//const char* wifiAccessRedirect = "/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html%26ap_mac=00:11:22:33:44:55%26client_mac=cc:11:22:33:44:55%26wlan=Visitor%20WiFi%26redirect=www.google.com/";
//const char* wifiAccessReferrer = "/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html&ap_mac=00:11:22:33:44:55&client_mac=cc:11:22:33:44:55&wlan=Visitor%20WiFi&redirect=www.google.com/";

const String CrLf = "\n\r";
String url;
String htmlString;
String PostData;          // = "buttonClicked=4&redirect_url=www.google.com&action=http://1.1.1.1/login.html";
String Request;           // = "POST /login.html HTTP/1.1\r\n";
String ResponseLocation; 
String ResponseContentLocation;
String accessRedirect;    // a value like "/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html%26ap_mac=00:11:22:33:44:55%26client_mac=cc:11:22:33:44:55%26wlan=Visitor%20WiFi%26redirect=www.google.com/"
bool isOutOfMemory;

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

// Use WiFiClient class to create TCP connections
WiFiClient client;
unsigned long timeout = millis();

//**************************************************************************************************************
// myMacAddressString return this device's mac address in lowercase hex:  ab:12:cd:34:ef:56
//**************************************************************************************************************
String myMacAddressString() {
	String res;
	char myMac[18] = { 0 };
	for (int i = 0; i < 6; i++) {
		sprintf(&myMac[i * 3], "%02X:", WiFi.macAddress()[i]); // format each number as 2 digit hex, followed by colon.  "12:"
	}
	res = (String)myMac;
	res.toLowerCase();
	return res.substring(0,res.length() - 1); // minus 1 in order to not return the trailing ":"
}

//**************************************************************************************************************
// htmlTagValue - return innerHTML text between <start>and</start>  (in this case, the word: and )
//**************************************************************************************************************
String htmlTagValue(String  thisHTML, String thisTag) {
	String tagStart = "<" + thisTag  + ">";
	String tagEnd = "</" + thisTag  + ">";
	int startPosition = thisHTML.indexOf(tagStart) + tagStart.length();      // add the length of the tag, as we are not interested in having it in result
	//Serial.print("START = ");
	//Serial.println(startPosition);
	int endPosition = thisHTML.indexOf(tagEnd);
	String res = thisHTML.substring(startPosition, endPosition);
	res.trim();
	return res;
}

//**************************************************************************************************************
// return the html query string value in urlString that was specified in keyString
//    exmple:  queryStringValue("?a=1&b=42!","b") should return a value of "42!"
//**************************************************************************************************************
String queryStringValue(String urlString, String keyString) {
	String thisUrl = urlString;
	thisUrl.replace("?", "&");
	thisUrl.replace("/n", "&");

	String tagStart = "&" + keyString + "=";
	int startPosition = urlString.indexOf(tagStart) + tagStart.length();
	int endPosition = urlString.indexOf("&", startPosition + 1);
	if ((startPosition > 0) && (endPosition > 0) && (endPosition > startPosition)) {
		return urlString.substring(startPosition, endPosition);
	}
	else {
		return "";
	}

}

//**************************************************************************************************************
// getHeaderValue - return header value of keyword
//
// html header values are formatted:
//   keyword: value
//
// e.g. Content-Length: 38826
//**************************************************************************************************************
String getHeaderValue(String keyWord, String str) {
	if ((str.length() < 1) || (keyWord.length() < 1)) {
		return "";
	}
	String keyText = keyWord;
	String thisString = str;
	String resultStr = "";
	if (thisString.startsWith("\n")) { thisString = thisString.substring(1); } // we [read until \r] so the \n may come before or after the \r
	if (thisString.startsWith("\r")) { thisString = thisString.substring(1); } 
	thisString.trim();

	keyText.trim();
	if (keyText.endsWith(":")) { keyText += " "; } // we need to end with a colon AND a space, so add the space if we have only colon
	if (!keyText.endsWith(": ")) { keyText += ": "; } // add both colopn and space if not provided

	//Serial.print("Looking at: [" + thisString + "] for keyword: [" + keyText + "]");
	int keyLength = keyText.length();
	if (thisString.startsWith(keyText)) {
		//Serial.print(" Found!");
		resultStr = thisString.substring(keyLength);
	}
	return resultStr;
}

// uint OutValue
void getHeaderValue(String keyWord, String str,  uint& OutValue) {
	String resultStr = "";
	int result;
	resultStr = getHeaderValue(keyWord, str);
	if (resultStr != "") {
		//Serial.print(" Found!");
		result = resultStr.toInt();
		OutValue = result;
	}
}

// String OutValue
void getHeaderValue(String keyWord, String str, String& OutValue) {
	String resultStr = "";
	resultStr = getHeaderValue(keyWord, str);
	if (resultStr != "") {
		OutValue = resultStr;
	}
}


uint ResponseContentLength = -1;
//**************************************************************************************************************
//  htmlSend - send thisHeader to targetHost:targetPort
//
//  return codes:   0 success
//                  1 success, but with redirect
//                  2 failed to connect
//                  3 client timeout (see MAX_CONNECTION_TIMEOUT_MILLISECONDS)
//                  4 out of memory (rare, but perhaps already running low on memory or unusually large header)
//                  5 content to large to load (would be out of memory if content attempted to load)
//**************************************************************************************************************
int htmlSend(const char* thisHost, int thisPort, String sendHeader) {
	int countReadResponseAttempts = 5; // this a is a somewhat arbitrary number, mainly to handle large HTML payloads
	String thisResponse; thisResponse = "";
	String thisResponseHeader; thisResponseHeader = "";

	Serial.println("*****************************************************************"); 
	Serial.print("Connecting to ");
	Serial.println(thisHost); // e.g. www.google.com, no http, no path, just dns name

	if (!client.connect(thisHost, thisPort)) {
		Serial.println("connection failed");
		return 2;
	}

	Serial.println("Sending HTML: ");
	Serial.println(sendHeader);


	isOutOfMemory = (ESP.getFreeHeap() < ESP_MIN_HEAP); // we never want to read a header so big that it exceeds available memory
	if (isOutOfMemory) {
		Serial.println("Starting out of memory!");
	}
	else {
		Serial.print("Memory free heap: ");
		Serial.println(ESP.getFreeHeap());
	}

	// BEGIN TIME SENSITIVE SECTION (edit with care, don't waste CPU, yield to OS!)
	client.flush(); // discard any incoming data
	yield();
	client.print(sendHeader); //blocks until either data is sent and ACKed, or timeout occurs (currently hard-coded to 5 seconds). 
	timeout = millis();
	while (client.available() == 0) {
		yield(); // give the OS a little breathing room 
		if ((millis() - timeout) > MAX_CONNECTION_TIMEOUT_MILLISECONDS) {
			Serial.println(">>> Client Timeout !");
			client.flush();
			client.stop();
			return 3;
		}
		delay(10); // TODO does delay offer any benefit over yield() ?
	}

	bool endOfHeader = false; // we'll keep track of where the html header ends, and where the payload begins
	int thisLineCount = 0;
	String line;
	while (countReadResponseAttempts > 0) {
		// Read all the lines of the reply from server and print them to Serial
		while (client.available()) {
			delay(1);
			// we'll always incrementally read header lines, but we may not read content if it is too large.
			if ((!endOfHeader) || (ResponseContentLength < (ESP.getFreeHeap() - ESP_MIN_HEAP))) {
				line = client.readStringUntil('\r'); // note that the char AFTER this is often a \n - but the documentation says to read until \r
			}
			else {
				Serial.println("Content too large! Flushing client to discard Rx data...");
				client.flush();
				client.stop();
				return 5; // abort, we cannot load this content
			}

			if (line.startsWith("Location: ") ) {
				ResponseLocation = line.substring(10);
			}
			if (line.startsWith("\nLocation: ") ) { // we [read until \r] so the \n may come before or after the \r
				ResponseLocation = line.substring(11);
			}

			yield(); // give the OS a little breathing room when loading large documents
			if (ESP.getFreeHeap() > ESP_MIN_HEAP) {
				Serial.print(".");
				if (!endOfHeader) { 
					// we only look for these values in the header, never the content!
					getHeaderValue("Content-Location", line, ResponseContentLocation);
					getHeaderValue("Content-Length", line, ResponseContentLength);
					thisResponseHeader += line; // we always capture the full header, as it has interesting fields (e.g. length of content which might exceed our memory capacity!)
				}
				if (endOfHeader) { thisResponse += line; } // always capture the response once we reach the end of the header

				if (line.length() <= 1) { // the first blank line found signifies the separation between header and content
					endOfHeader = true;
				}
			}
			else {
				Serial.print("!");
				thisResponse += CrLf + "Out of memory! " + CrLf;
				Serial.print("Out of memory! Heap=");
				Serial.println(ESP.getFreeHeap());
				isOutOfMemory = true;
				client.flush();
				return 4;
			}

			thisLineCount++;
		}
		//Serial.print("(");
		//Serial.print(countReadResponseAttempts);
		//Serial.print(")");
		countReadResponseAttempts--; // TODO - do we stil need to look for more data like this?
		// Serial.println("Next attempt!");
		delay(100); // give the OS a little breathing room when loading large documents
	}
	// END TIME SENSITIVE SECTION 

	Serial.print("Found Response Content Length = ");
	Serial.println(ResponseContentLength);
	Serial.print("Found Response Content Location = ");
	Serial.println(ResponseContentLocation);
	//
	// take ResponseLocation that looks like http://1.1.1.1/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html&ap_mac=00:11:22:33:44:55&client_mac=cc:11:22:33:44:55&wlan=Visitor%20WiFi&redirect=www.w3.org/
	// then strip the http & host name, and urlencode for the accessRedirect
	//
	if (ResponseLocation.length() > 138 ) { // make sure we have a minimum length REsponse Location string
		accessRedirect = ResponseLocation;  // should contain full URL:  http://1.1.1.1/path?switchurl=... but we only want path?switchurl=...
		accessRedirect.remove(0, 14);       // remove the first 14 characters that should be: http://1.1.1.1 TODO handle when different (longer?)
		accessRedirect.replace("&", "%26"); // urlencode the ampersands
	}
	else {
		// if there's no Location: value in header, we are likely not needing a redirect on the visitor WiFi
		//Serial.println("Warning accessRedirect too short.");
	}


	Serial.println("Response Header:");
	Serial.println("");
	Serial.println(thisResponseHeader);
	Serial.println("");
	if (myDebugLevel >= 2) { // only show the response content for debug level 2 or greater
		Serial.println("Response Payload Content:");
		Serial.println("");
		Serial.println(thisResponse);
		Serial.println("");
	}
	//Serial.println("Page Title=" + htmlTagValue(thisResponse, "TITLE") );
	Serial.println("");
	Serial.println("");
	Serial.print("Read done! Additional Read Responses:");
	Serial.println(countReadResponseAttempts);
	Serial.print("*** End of response. ");
	Serial.print(thisLineCount);
	Serial.println(" lines. ");
	Serial.println("");
	//Serial.print("thisResponse.indexOf(Web Authentication Redirect)");
	//Serial.println(thisResponse.indexOf("Web Authentication Redirect"));
	//Serial.println("");
	//Serial.print("thisResponseHeader.indexOf(Location: http://1.1.1.1/)");
	//Serial.println(thisResponseHeader.indexOf("Location: http://1.1.1.1/"));
	//Serial.println("");
	//Serial.print("htmlTagValue(thisResponse, TITLE) = ");
	//Serial.println(htmlTagValue(thisResponse,"TITLE"));
	//Serial.println("");


	//Serial.print("my MAC = ");
	//Serial.println(myMacAddressString());

	//Serial.println("Changing MAC...");
	//uint8_t  mac[] = { 0x77, 0x01, 0x02, 0x03, 0x04, 0x05 };
	//wifi_set_macaddr(STATION_IF, &mac[0]);
	//Serial.println("MAC Change Done!");

	//Serial.print("my MAC = ");
	//Serial.println(myMacAddressString());

	if (
		(thisResponseHeader.indexOf("Location: http://1.1.1.1/") > 0) // anytime the 1.1.1.1 address is in the header, we are doing a redirect
		  ||
		(htmlTagValue(thisResponse,"TITLE") == "Web Authentication Redirect") // anytime this text is found title tag, we are doing a redirect
		)
	{
		return 1; // success, but with redirect
	}
	else {
		return 0; // success! Internet access ready.
	}
	client.stopAll();
}

int doAcceptTermsAndConditions() {
	//**************************************************************************************************************
	// fetch the custom.js code; this is the interesting stuff that handles the web page button pressing
	// (this is what we need to emulate here, with no button and no button presser!)
	//
	// the script e3ssentially does this at load time:
	//
	//   document.forms[0].action = args.switch_url;
	//
	// then when the user presses the accept button, it does this:
	//
	//   redirectUrl = redirectUrl.substring(0,255);
	//   document.forms[0].redirect_url.value = redirectUrl;
	//   document.forms[0].buttonClicked.value = 4;
	//   document.forms[0].submit();
	//
	// 
	//**************************************************************************************************************

	//**************************************************************************************************************
	// fetch the custom.js
	// (this section is for reference only, and not needed to actually connect; there might be "interesting" data)
	//**************************************************************************************************************
	Serial.println("\r\n");
	Serial.println("Requesting custom.js: \r\n");
	//**************************************************************************************************************
	htmlString = "";
	htmlString += String("GET ") + "/fs/customwebauth/custom.js" + " HTTP/1.1\r\n";
	htmlString += "Host: " + String(accessHost) + "\r\n";
	htmlString += "Accept-Language: en-US\r\n";
	htmlString += String(wifiUserAgent) + "\r\n"; // "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; ASTE; rv:11.0) like Gecko\r\n";
	htmlString += "Connection: Keep-Alive\r\n\r\n";

	htmlSend(accessHost, 80, htmlString);


	//**************************************************************************************************************
	// we are pretending to be a browser, but we're not; so we need to manually redirect to the WiFi Access 
	// redirect, found in the META tag, above. (note we use accessRedirect, extracted from  ResponseLocation)
	//**************************************************************************************************************
	Serial.println("\r\n");
	Serial.println("Requesting URL redirect: \r\n");
	//**************************************************************************************************************
	htmlString = "";

	// GET /fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html%26ap_mac=00:11:22:33:44:55%26client_mac=cc:11:22:33:44:55%26wlan=Visitor%20WiFi%26redirect=www.google.com/ HTTP/1.1
	// htmlString += String("GET ") + String(wifiAccessRedirect) + " HTTP/1.1\r\n"; // TODO get the redirect from the prior http response
	htmlString += String("GET ") + accessRedirect + " HTTP/1.1\r\n"; 
	
	htmlString += "Host: " + String(accessHost) + "\r\n";
	htmlString += "Connection: Keep-Alive\r\n";
	htmlString += "\r\n"; // blank line before content

	htmlSend(accessHost, 80, htmlString);

	delay(5000); // here we are spending 5 seconds reading the terms and conditions.

    //**************************************************************************************************************

    //**************************************************************************************************************
	// The post back (note it is actually a GET, with query-string parameters; thanks fiddler app for this!)
	//**************************************************************************************************************
	// 
	PostData = "buttonClicked=4&redirect_url=" + String(internetHostCheck);
	Request = ""; // init request

	// note in the JavaScript to submit form, there's a line:  document.forms[0].buttonClicked.value = 4;
	Request += "GET http://" + String(accessHost) + "/login.html?buttonClicked=4&redirect_url=" + String(internetHostCheck) +" HTTP/1.1\r\n";
	Request += "Accept: text/html, application/xhtml+xml, */*\r\n";


	// Request += "Referer: http://" + String(accessHost) + "/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html&ap_mac=00:11:22:33:44:55&client_mac=cc:11:22:33:44:55&wlan=Visitor%20WiFi&redirect=" + String(internetHostCheck) + "\r\n";
	Request += "Referer: " + ResponseLocation + "\r\n";

	Serial.println("Header so far:\n\r:");
	Serial.println(Request);
	Serial.println("\n\r\n\r\n\r");
	Serial.println("accessRedirect:");
	Serial.println(accessRedirect);
	Serial.println("\n\r\n\r\n\r");
	Serial.println("\n\r\n\r\n\r");
	

	Request += "Accept-Language: en-US\r\n";
	Request += "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; ASTE; rv:11.0) like Gecko\r\n";
	Request += "DNT: 1\r\n";
	Request += "If-Modified-Since: Mon 31 Oct 2016 23:28:25 GMT\r\n";
	Request += "Content-Length: ";
	Request += PostData.length(); // we need to say how long our posted data is
	Request += "\n";              // linefeed after content length; not a blank line!
	Request += "Host: " + String(accessHost) + "\r\n";
	Request += "Connection: Keep-Alive\r\n";
	Request += "\r\n"; // this is the blank line separating the header from the postback content
	Request += PostData;

	//Serial.print("Debug Halt! Waiting");
	//while (1) {
	//	Serial.print(".");
	//	delay(90000000);
	//}

	// send the response, accepting the terms and conditions. the result should be we are granted access!
	//htmlSend(accessHost, 80, Request);


	Serial.println("\r\n");



	//PostData = "buttonClicked=4&redirect_url=www.google.com";
	//Request = "";
	//Request += "GET http://" + String(accessHost) + "/login.html?buttonClicked=4&redirect_url=www.google.com HTTP/1.1\r\n";
	//Request += "Host: " + String(accessHost) + "\r\n";
	//// Request += "Accept: text/html, application/xhtml+xml, */*\r\n";
	//// Request += "Referer: http://" + String(accessHost) + "/fs/customwebauth/login.html?switch_url=http://1.1.1.1/login.html&ap_mac=00:11:22:33:44:55&client_mac=cc:11:22:33:44:55&wlan=Visitor%20WiFi&redirect=www.google.com/" + "\r\n";
	//Request += "Referer: http://" + String(accessHost) + String(wifiAccessRedirect) + "\r\n";
	//
	//Request += "Accept-Language: en-US\r\n";
	//Request += String(wifiUserAgent) + "\r\n"; // "User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; ASTE; rv:11.0) like Gecko\r\n";
	//Request += "DNT: 1\r\n"; // Do Not Track (1 = tracking preference is enabled)
	//Request += "If-Modified-Since: Tue 1 Nov 2016 23:28:25 GMT\r\n"; // TODO put in recent current date
	//Request += "Content-Length: ";
	//Request += PostData.length();
	//Request += "\n";
	//Request += "Connection: close\r\n";
	//Request += "\r\n"; // blank line before content
	//Request += String(PostData);

	htmlSend(accessHost, 80, Request);

	//Serial.println("\r\n");



	//**************************************************************************************************************
	// we should now have full WiFi connectivity, and able to browse anywhere the access point allows.
	//
	// for reference, a Windows-based fiddler session shows a header like this:
	//
	//  GET http://google.com/ HTTP/1.1
	//  Host: google.com
	//  Connection: keep-alive
	//  Upgrade-Insecure-Requests: 1
	//  User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.71 Safari/537.36
	//  X-Client-Data: CIS...etc
	//  Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
	//  Accept-Encoding: gzip, deflate, sdch
	//  Accept-Language: en-US,en;q=0.8
	//  Cookie: OGPC=506...etc
	//
	//  "A server MUST NOT send transfer-codings to an HTTP/1.0 client. "  See http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html
	//  (however note that even though we may request 1.0, a HTTP/1.1 response comes back. TODO: why?)
	//  why would we do this? An attempt to avoid Chunked Transfer Encoding. TODO: how to fetch Chunked Transfer Encoded content
	//**************************************************************************************************************
	return 0;
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
  htmlString = String("GET http://") + String(internetHostCheck) + url + " HTTP/1.1\r\n" +
	  "Host: " + String(internetHostCheck) + "\r\n" +
	  "Content-Encoding: identity" + "\r\n" +
	  "Connection: Keep-Alive\r\n\r\n";

  int connectionStatus = htmlSend(internetHostCheck, 80, htmlString);
  if (connectionStatus == 0) {
	  Serial.println("Connected to internet!");
  }
  else if (connectionStatus == 1) {
	  Serial.println("Accepting Terms and Conditions...");

	  Serial.print("ResponseLocation=");
	  Serial.println(ResponseLocation);

	  Serial.println();

	  Serial.print("ap_mac queryStringValue=");
	  Serial.print(queryStringValue(ResponseLocation, "ap_mac"));

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
  client.stop();
  client.stopAll();
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
