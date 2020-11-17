#ifndef _ESP8266RADIO_
#define _ESP8266RADIO_

#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <VS1053.h>
#include <iostream>
#include <LITTLEFS.h>
#include <SPI.h>
#include <cbuf.h>
#include <EEPROM.h>

/* 
|  VS1053  | ESP8266  |
|----------|----------|
| SCK      | D5       |
| MISO     | D6       |
| MOSI     | D7       |
| XRST     | RST      |
| CS       | D1       |
| DCS      | D0       |
| DREQ     | D3       |
| 5V       | VU       |
| GND      | G        |
*/
// Wiring of VS1053 board (SPI connected in a standard way)
#define VS1053_CS 	D1
#define VS1053_DCS 	D0
#define VS1053_DREQ D3
#define VOLUME 100	   // volume level 0-100

// Do we want to connect with track/artist info (metadata)
bool METADATA = false;
#define ICYDATAPIN D2

// EEPROM writing routines (eg: remembers previous radio stn)
// Preferences preferences;
signed int currStnNo, prevStnNo;
signed int nextStnNo;
#define stnChangePin D4

// All connections are assumed insecure http:// not https://
const int8_t stationCnt = 8; // start counting from 1!

/* 
    Example URL: [port optional, 80 assumed]
        stream.antenne1.de[:80]/a1stg/livestream1.aac Antenne1 (Stuttgart)
*/

struct radioStationLayout
{
	char host[64];
	char path[128];
	int port;
	char friendlyName[64];
};

struct radioStationLayout radioStation[stationCnt] = {

    //0
    "stream.antenne1.de",
    "/a1stg/livestream1.aac",
    80,
    "Antenne1.de",

    //1
    "bbcmedia.ic.llnwd.net",
    "/stream/bbcmedia_radio4fm_mf_q", // also mf_p works
    80,
    "BBC Radio 4",

    //2
    "stream.antenne1.de",
    "/a1stg/livestream2.mp3",
    80,
    "Antenne1 128k",

    //3
    "listen.181fm.com",
    "/181-beatles_128k.mp3",
    80,
    "Beatles 128k",

    //4
    "stream-mz.planetradio.co.uk",
    "/magicmellow.mp3",
    80,
    "Mellow Magic (Redirected)",

    //5
    "edge-bauermz-03-gos2.sharp-stream.com",
    "/net2national.mp3",
    80,
    "Greatest Hits 112k (National)",

    //6
    "airspectrum.cdnstream1.com",
    "/1302_192",
    8024,
    "Mowtown Magic Oldies",

    //7
    "live-bauer-mz.sharp-stream.com",
    "/magicmellow.aac",
    80,
    "Mellow Magic (48k AAC)"

};

// Forward declarations
std::string readLITTLEFSInfo(char *itemRequired);
const char *wl_status_to_string(wl_status_t status);
std::string getWiFiPassword();
std::string getSSID();
void connectToWifi();
bool stationConnect(int stationNo);

// TFT routines
//void initDisplay();
//void setupDisplayModule();
//void displayStationName(char *stationName);
// void drawPrevButton();
// void drawNextButton();
void displayTrackArtist(std::string ArtistTrack);
void drawBufferLevel(size_t bufferLevel, bool override = false);

// Buffering routines
void populateRingBuffer();
bool playMusicFromRingBuffer();
void changeStation(int8_t plusOrMinus);
bool readMetaData() __always_inline;
void getRedirectedStationInfo(String header, int currStationNo);
bool getNextButtonPress();
bool getPrevButtonPress();
std::string toTitle(std::string s, const std::locale &loc = std::locale());

void checkForStationChange();
void checkBufferForPlaying();

// Global variables
bool redirected = false;
int bytesUntilmetaData = 0;
int bitRate = 0;
bool isConnected = false;

// The number of bytes between metadata (title track)
uint16_t metaDataInterval = 0; //bytes

// Dedicated 32-byte buffer for VS1053 aligned on 4-byte boundary for efficiency
uint8_t mp3buff[32] __attribute__((aligned(4)));

// Circular "Read Buffer" to stop stuttering on some stations
#define CIRCULARBUFFERSIZE 10000 
cbuf circBuffer(CIRCULARBUFFERSIZE);

// Internet stream buffer that we copy in chunks to the ring buffer
char readBuffer[100] __attribute__((aligned(4)));

// Pushbutton connected to this pin to change station
//int stnChangePin = 13;
//int tftTouchedPin = 15;

// Can we use the above button (not in the middle of changing stations)?
bool changeStnButton = true;
bool canPlayMusicFromBuffer = false;

#endif