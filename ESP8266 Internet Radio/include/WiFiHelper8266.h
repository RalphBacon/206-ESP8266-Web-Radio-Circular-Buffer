// All things WiFi go here
#include "Arduino.h"
#include <ESP8266WiFi.h>

#define WIFITIMEOUTSECONDS 20
bool wiFiDisconnected = true;

std::string ssid, wifiPassword;

// Connect to WiFi
void connectToWifi()
{
	Serial.printf("ESP8266 Connecting to SSID: %s\n", ssid.c_str());

	// Ensure we disconnect WiFi first to stop connection problems
	if (WiFi.status() == WL_CONNECTED){
		Serial.println("Disconnecting from WiFi");
		WiFi.disconnect(false);
	}

	// WE want to be a client not a server
	WiFi.mode(WIFI_STA);

	// Don't store the SSID and password
	WiFi.persistent(false);

	// Don't allow WiFi sleep
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	WiFi.setAutoReconnect(true);

	// Lock down the WiFi stuff
	IPAddress local_IP(192, 168, 1, 104);
	IPAddress gateway(192, 168, 1, 254);
	IPAddress subnet(255, 255, 255, 0);
	IPAddress DNS1(8, 8, 8, 8);
	IPAddress DNS2(8, 8, 4, 4);
	WiFi.config(local_IP, gateway, subnet, DNS1, DNS2);

	//Connect to the required WiFi
	WiFi.begin(ssid.c_str(), wifiPassword.c_str());
	delay(2000);

	// Try to connect a few times before timing out
	int timeout = WIFITIMEOUTSECONDS * 4;
	while (WiFi.status() != WL_CONNECTED && (timeout-- > 0))
	{
		delay(500);
		//WiFi.reconnect();
		Serial.print(".");
	}

	// Successful connection?
	wl_status_t wifiStatus = WiFi.status();
	if (wifiStatus != WL_CONNECTED)
	{
		Serial.println("\nFailed to connect, exiting.");
		Serial.printf("WiFi Status: %s\n", wl_status_to_string(wifiStatus));
		return;
	}

	Serial.printf("\nWiFi connected with (local) IP address of: %s\n", WiFi.localIP().toString().c_str());
	wiFiDisconnected = false;
}


// Get the WiFi SSID
std::string getSSID()
{
	std::string currSSID = readLITTLEFSInfo((char *)"SSID");
	if (currSSID == "")
	{
		return "Benny";
	}
	else
	{
		return currSSID;
	}
}

// Get the WiFi Password
std::string getWiFiPassword()
{
std::string currWiFiPassword = readLITTLEFSInfo((char *)"WiFiPassword");
	if (currWiFiPassword == "")
	{
		return "BestCatEver";
	}
	else
	{
		return currWiFiPassword;
	}
}

// Convert the WiFi (error) response to a string we can understand
const char *wl_status_to_string(wl_status_t status)
{
	switch (status)
	{
	case WL_NO_SHIELD:
		return "WL_NO_SHIELD";
	case WL_IDLE_STATUS:
		return "WL_IDLE_STATUS";
	case WL_NO_SSID_AVAIL:
		return "WL_NO_SSID_AVAIL";
	case WL_SCAN_COMPLETED:
		return "WL_SCAN_COMPLETED";
	case WL_CONNECTED:
		return "WL_CONNECTED";
	case WL_CONNECT_FAILED:
		return "WL_CONNECT_FAILED";
	case WL_CONNECTION_LOST:
		return "WL_CONNECTION_LOST";
	case WL_DISCONNECTED:
		return "WL_DISCONNECTED";
	default:
		return "UNKNOWN";
	}
}