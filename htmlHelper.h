// htmlHelper.h

#ifndef _HTMLHELPER_h
#define _HTMLHELPER_h
#include <ESP8266WiFi.h>

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

String queryStringValue(String urlString, String keyString);


String getHeaderValue(String keyWord, String str);
void getHeaderValue(String keyWord, String str, uint& OutValue);
void getHeaderValue(String keyWord, String str, String& OutValue);

int htmlSend(const char* thisHost, int thisPort, String sendHeader);

int doAcceptTermsAndConditions();

#endif

