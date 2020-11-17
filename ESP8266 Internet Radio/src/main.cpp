#include <Arduino.h>
#include "main.h"
#include "tftHelper.h"
#include "WiFiHelper8266.h"

// MP3 player
VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

// Instantiate the web client
WiFiClient client;

void setup()
{
	Serial.begin(74880);
	delay(2000);
	Serial.println("Setup started.");

	// initialize SPI bus;
	Serial.println("Starting SPI");
	SPI.begin();

	//How much SRAM free (heap memory)
	Serial.printf("Free memory: %d bytes\n", ESP.getFreeHeap());

	// Initialise LITTLEFS system
	if (!LittleFS.begin())
	{
		Serial.println("LITTLEFS Mount Failed.");
		while (1)
			delay(1);
	}

	// VS1053 MP3 decoder
	Serial.println("Starting player");
	player.begin();

	// Wait for the player to be ready to accept data
	Serial.println("Waiting for VS1053 initialisation to complete.");
	while (!player.data_request())
	{
		delay(1);
	}

	// You MIGHT have to set the VS1053 to MP3 mode. No harm done if not required!
	Serial.println("Switching player to MP3 mode");
	player.switchToMp3Mode();

	// Set the volume here to MAX (100)
	player.setVolume(100);

	// Set the equivalent of "Loudness" (increased bass & treble)
	// Bits 15:12	treble control in 1.5dB steps (-8 to +7, 0 = off)
	// Bits 11:8	lower limit frequency in 1kHz steps (1kHz to 15kHz)
	// Bits 7:4		Bass Enhancement in 1dB steps (0 - 15, 0 = off)
	// Bits 3:0		Lower limit frequency in 10Hz steps (2 - 15)
	//char trebleCutOff = 3; // 3kHz cutoff
	//char trebleBoost = 3 << 4 | trebleCutOff;
	//char bassCutOff = 10; // times 10 = 100Hz cutoff
	//char bassBoost = 3 << 4 | bassCutOff;

	//uint16_t SCI_BASS = trebleBoost << 8 | bassBoost;
	// equivalent of player.setTone(0b0111001110101111);
	//player.setTone(SCI_BASS);

	// Some sort of startup message
	File file = LittleFS.open("/Intro.mp3", "r");
	if (file)
	{
		uint8_t audioBuffer[32] __attribute__((aligned(4)));
		while (file.available())
		{
			int bytesRead = file.read(audioBuffer, 32);
			player.playChunk(audioBuffer, bytesRead);
		}
		file.close();
	}
	else
	{
		Serial.println("Unable to open greetings file");
	}

	// Start WiFi
	ssid = getSSID();
	wifiPassword = getWiFiPassword();

	while (!WiFi.isConnected())
	{
		connectToWifi();
	}

	// Whether we want MetaData or not
	pinMode(ICYDATAPIN, INPUT_PULLUP);
	METADATA = digitalRead(ICYDATAPIN) == HIGH;

	// Get the station number that was previously playing TODO: change to LittleFS
	EEPROM.begin(64);
	currStnNo = EEPROM.read(0);
	if (currStnNo > stationCnt - 1)
	{
		currStnNo = 0;
	}
	prevStnNo = currStnNo;

	// Connect to that station
	Serial.printf("Current station number: %u\n", currStnNo);
	while (!stationConnect(currStnNo))
	{
		checkForStationChange();
	};

	//Now how much SRAM free (heap memory)
	Serial.printf("Free memory: %d bytes\n", ESP.getFreeHeap());
}

void loop()
{
	yield();

	// Data to read from mainBuffer?
	if (client.available())
	{
		populateRingBuffer();

		// If we've read all data between metadata bytes, check to see if there is track title to read
		if (bytesUntilmetaData == 0 && METADATA)
		{
			// TODO: if cant read valid metadata we need to reconnect
			if (!readMetaData())
			{
				while (!stationConnect(currStnNo))
				{
					checkForStationChange();
				};
			}

			// reset byte count for next bit of metadata
			bytesUntilmetaData = metaDataInterval;
		}
	}
	else
	{
		//Serial.println("No stream data available");

		// Sometimes we get randomly disconnected from WiFi BUG Why?
		if (!client.connected())
		{
			// TODO: if client suddenly not connected we will have to reconnect
			Serial.println("Client not connected.");
			connectToWifi();
			while (!stationConnect(currStnNo))
			{
				checkForStationChange();
			};
		}
	}

	// If we (no longer) need to buffer the streaming data (after a station change)
	// allow the buffer to be played
	if (canPlayMusicFromBuffer)
	{
		// If we failed to play 32b from the buffer (insufficient data) set the flag again
		if (!playMusicFromRingBuffer())
		{
			canPlayMusicFromBuffer = false;
			//player.setVolume(0);
			//volumeMax = false;
		};
	}
	else
	{
		// Otherwise, check whether we now have enough data to start playing
		checkBufferForPlaying();
	}

	// So how many bytes have we got in the buffer (should hover around 90%)
	drawBufferLevel(circBuffer.available());

	// Has CHANGE STATION button been pressed?
	checkForStationChange();
}

bool stationConnect(int stationNo)
{
	Serial.println("--------------------------------------");
	Serial.printf("        Connecting to station %d\n", stationNo);
	Serial.println("--------------------------------------");

	//How much SRAM free (heap memory)
	Serial.printf("Free memory: %d bytes\n", ESP.getFreeHeap());

	// Determine whether we want ICY metadata (LOW = NO ICY)
	METADATA = digitalRead(ICYDATAPIN) == HIGH;
	isConnected = false;

	// Set the metadataInterval value to zero we can detect that we found a valid one
	metaDataInterval = 0;

	// We try a few times to connect to the station
	bool connected = false;
	int connectAttempt = 0;

	while (!connected && connectAttempt < 5)
	{
		if (redirected)
		{
			Serial.printf("REDIRECTED URL DETECTED FOR STATION %d\n", stationNo);
		}

		connectAttempt++;
		Serial.printf("Host: %s Port:%d\n", radioStation[stationNo].host, radioStation[stationNo].port);

		//radioStation[stationNo].host
		if (client.connect(radioStation[stationNo].host, radioStation[stationNo].port))
		{
			connected = true;
		}
		else
		{
			delay(250);
		}
	}

	// If we could not connect (eg bad URL) just exit
	if (!connected)
	{
		Serial.printf("Could not connect to %s\n", radioStation[stationNo].host);
		return false;
	}
	else
	{
		Serial.printf("Connected to %s (%s%s)\n",
					  radioStation[stationNo].host, radioStation[stationNo].friendlyName,
					  redirected ? " - redirected" : "");
	}

	// Get the data stream plus any metadata (eg station name, track info between songs / ads)
	// TODO: Allow retries here (BBC Radio 4 very finicky before streaming).
	// We might also get a redirection URL given back.
	Serial.printf("Getting data from %s (%s Metadata)\n", radioStation[stationNo].path, (METADATA == 1 ? "WITH" : "WITHOUT"));
	client.print(
		String("GET ") + radioStation[stationNo].path + " HTTP/1.1\r\n" +
		"Host: " + radioStation[stationNo].host + "\r\n" +
		(METADATA ? "Icy-MetaData:1\r\n" : "") +
		"Connection: close\r\n\r\n");

	// Give the client a chance to connect
	int retryCnt = 30;
	while (client.available() == 0 && --retryCnt > 0)
	{
		Serial.println("No data yet from connection.");
		delay(500);
	}

	if (client.available() < 1)
		return false;

	// Keep reading until we read two LF bytes in a row.
	//
	// The ICY (I Can Yell, a precursor to Shoutcast) format:
	// In the http response we can look for icy-metaint: XXXX to tell us how far apart
	// the header information (in bytes) is sent.

	// Process all responses until we run out of header info (blank header)
	while (client.available())
	{
		yield();

		// Delimiter char is not included in data returned
		String responseLine = client.readStringUntil('\n');

		if (responseLine.indexOf("Status: 200 OK") > 0)
		{
			// TODO: we really should check we have a response of 200 before anything else
			Serial.println("200 - OK response");
			continue;
		}

		// If we have an EMPTY header (or just a CR) that means we had two linefeeds
		if (responseLine[0] == (uint8_t)13 || responseLine == "")
		{
			break;
		}

		// If the header is not empty process it
		Serial.print("HEADER:");
		Serial.println(responseLine);
		Serial.println("");

		// Critical value for this whole sketch to work: bytes between "frames"
		// Sometimes you can't get this first time round so we just reconnect
		if (responseLine.startsWith("icy-metaint"))
		{
			metaDataInterval = responseLine.substring(12).toInt();
			Serial.printf("NEW Metadata Interval:%d\n", metaDataInterval);
			continue;
		}

		// The bit rate of the transmission (FYI) eye candy
		if (responseLine.startsWith("icy-br:"))
		{
			bitRate = responseLine.substring(7).toInt();
			Serial.printf("Bit rate:%d\n", bitRate);
			continue;
		}
	}

	// Update the count of bytes until the next metadata interval (used in loop) and exit
	bytesUntilmetaData = metaDataInterval;

	// If we didn't find required metaDataInterval value in the headers, abort this connection
	if (bytesUntilmetaData == 0 && METADATA)
	{
		Serial.println("NO METADATA INTERVAL DETECTED - RECONNECTING");
		return false;
	}

	// All done here
	isConnected = true;
	return true;
}

// Populate ring buffer with streaming data
void populateRingBuffer()
{
	int bytesReadFromStream = 0;

	// Room in the ring buffer for (up to) X bytes?
	if (circBuffer.room() > 100)
	{
		// Read either the maximum available (max 100) or the number of bytes to the next meata data interval
		if (METADATA)
		{
			bytesReadFromStream = client.read((uint8_t *)readBuffer, min(100, (int)bytesUntilmetaData));
		}
		else
		{
			// If we are not worried about metadata then read a reasonable amount always
			bytesReadFromStream = client.read((uint8_t *)readBuffer, 100);
		}

		// If we get -1 here it means nothing could be read from the stream
		// TODO: find out why this might be. Remote server not streaming?
		if (bytesReadFromStream > 0)
		{
			// Add them to the circular buffer
			circBuffer.write(readBuffer, bytesReadFromStream);

			// Some radio stations (eg BBC Radio 4!!!) appear to limit the data stream. Why?
			if (bytesReadFromStream < 32 && bytesReadFromStream != bytesUntilmetaData)
			{
				Serial.printf("%db to circ\n", bytesReadFromStream);
			}
		}
		else
		{
			// There will be thousands of this message. Only for debugging.
			// Serial.println("Nothing read from Internet stream");
		}
	}
	else
	{
		// There will be thousands of this message. Only for debugging.
		//Serial.println("Circ buff full.");
	}

	// Subtract bytes actually read from incoming http data stream from the bytesUntilmetaData
	bytesUntilmetaData -= bytesReadFromStream;
}

// Copy streaming data to our ring buffer and check wheter there's enough to start playing yet
// This is only executed after a station connect to give the buffer a chance to fill up.
void checkBufferForPlaying()
{
	// If we have now got enough in the ring buffer
	if (circBuffer.available() >= 32)
	{
		// Reset the flag, allowing data to be played, won't get reset again until station change
		canPlayMusicFromBuffer = true;

		// Volume is set to zero before we have a valid connection. If we haven't
		// yet turned up the volume, now is the time to do that.
		//if (!volumeMax)
		{
			//player.setVolume(100);
			//volumeMax = true;
		}
	}
}

inline bool readMetaData()
{
	// The first byte is going to be the length of the metadata
	int metaDataLength = client.read();

	// Usually there is none as track/artist info is only updated when it changes
	// It may also return the station URL (not necessarily the same as we are using).
	// Example:
	//  'StreamTitle='Love Is The Drug - Roxy Music';StreamUrl='https://listenapi.planetradio.co.uk/api9/eventdata/62247302';'
	if (metaDataLength < 1) // includes -1
	{
		// Warning sends out about 4 times per second!
		//Serial.println("No metadata to read.");
		return "";
	}

	// The actual length is 16 times bigger to allow from 16 up to 4080 bytes (255 * 16) of metadata
	metaDataLength = (metaDataLength * 16);
	Serial.printf("Metadata block size: %d\n", metaDataLength);

	// Wait for the entire MetaData to become available in the stream
	// Serial.println("Waiting for METADATA");
	// while (client.available() < metaDataLength)
	// {
	// 	Serial.print(".");
	// 	delay(500);
	// }

	if (metaDataLength > 500){
		Serial.println("METADATA length ignored.");
		Serial.printf("METADATA interval %d\n",metaDataInterval);
		return false;
	}

	// Temporary buffer for the metadata
	char metaDataBuffer[metaDataLength + 1];

	// Initialise this temp buffer
	memset(metaDataBuffer, 0, metaDataLength + 1);

	// Populate it from the internet stream
	client.readBytes((char *)metaDataBuffer, metaDataLength);
	Serial.print("\nMetaData:");
	Serial.println(metaDataBuffer);

	for (auto cnt = 0; cnt < metaDataLength; cnt++)
	{
		if (metaDataBuffer[cnt] > 0 && metaDataBuffer[cnt] < 8)
		{
			Serial.printf("Corrupt METADATA found:%02X\n", metaDataBuffer[cnt]);
			// Terminate the string right after the corrupt char so we can print it
			metaDataBuffer[cnt + 1] = '\0';
			Serial.println(metaDataBuffer);
			return false;
		}
	}

	// Extract track Title/Artist from this string
	char *startTrack = NULL;
	char *endTrack = NULL;
	std::string streamArtistTitle = "";

	startTrack = strstr(metaDataBuffer, "StreamTitle");
	if (startTrack != NULL)
	{
		// We have found the streamtitle so just skip over "StreamTitle="
		startTrack += 12;

		// Now look for the end marker
		endTrack = strstr(startTrack, ";");
		if (endTrack == NULL)
		{
			// No end (very weird), so just set it as the string length
			endTrack = (char *)startTrack + strlen(startTrack);
		}

		// There might be an opening and closing quote so skip over those (reduce data width) too
		if (startTrack[0] == '\'')
		{
			startTrack += 1;
			endTrack -= 1;
		}

		// We MUST terminate the 'string' (character array) with a null to denote the end
		endTrack[0] = '\0';

		ptrdiff_t startIdx = startTrack - metaDataBuffer;
		ptrdiff_t endIdx = endTrack - metaDataBuffer;
		std::string streamInfo(metaDataBuffer, startIdx, endIdx);
		streamArtistTitle = streamInfo;
		Serial.printf("%s\n", streamArtistTitle.c_str());
		displayTrackArtist(toTitle(streamArtistTitle));
	}

	// All done
	return true;
}

void changeStation(int8_t btnValue)
{
	Serial.println("--------------------------------------");
	Serial.print("       Change Station ");
	Serial.println(btnValue > 0 ? "(NEXT)" : "(PREV)");
	Serial.println("--------------------------------------");

	// Make button inactive
	changeStnButton = false;

	// Reset any redirection flag
	redirected = false;

	// Get the next/prev station (in the list)
	nextStnNo = currStnNo + btnValue;
	if (nextStnNo > stationCnt - 1)
	{
		nextStnNo = 0;
	}
	else
	{
		if (nextStnNo < 0)
		{
			nextStnNo = stationCnt - 1;
		}
	}

	// Whether connected to this station or not, update the variable otherwise
	// we would 'stick' at old station
	currStnNo = nextStnNo;

	if (prevStnNo != nextStnNo)
	{
		prevStnNo = nextStnNo;
		//player.softReset();

		// Now actually connect to the new URL for the station
		bool isConnected = false;
		do
		{
			isConnected = stationConnect(nextStnNo);
		} while (!isConnected);

		// Next line might not be required for VS051
		player.switchToMp3Mode();

		// Store (new) current station in EEPROM
		EEPROM.write(0, nextStnNo);
		Serial.printf("Current station now stored: %u\n", nextStnNo);
		EEPROM.commit();

		// Button active again
		changeStnButton = true;
	}
}

// Change station button / screen button pressed?
void checkForStationChange()
{
	//Button(s) go LOW when active
	if (!digitalRead(stnChangePin) && changeStnButton == true)
	{
		changeStation(+1);
	}
	else
	{
		// If no actual button pressed check for screen button
		if (/*!digitalRead(tftTouchedPin) &&  */ changeStnButton == true)
		{
			if (getNextButtonPress())
			{
				changeStation(+1);
			}
			else
			{
				if (getPrevButtonPress())
				{
					changeStation(-1);
				}
			}
		}
	}
}

// Read the ringBuffer and give to VS1053 to play
bool playMusicFromRingBuffer()
{
	// Did we run out of data to send the VS1053 decoder?
	bool dataPanic = false;

	// Now read (up to) 32 bytes of audio data and play it
	if (circBuffer.available() >= 32)
	{
		// Does the VS1053 actually want any more data (yet)?
		if (player.data_request())
		{
			{
				// Read (up to) 32 bytes of data from the circular (ring) buffer
				int bytesRead = circBuffer.read((char *)mp3buff, 32);

				// If we didn't read the full 32 bytes, that's a worry!
				if (bytesRead != 32)
				{
					Serial.printf("Only read %db from ring buff\n", bytesRead);
					dataPanic = true;
				}

				// Actually send the data to the VS1053
				player.playChunk(mp3buff, bytesRead);
			}
		}
	}

	return !dataPanic;
}

// LITTLEFS card reader (done ONCE in setup)
// Format of data is:
//          #comment line
//          <KEY><DATA>
std::string readLITTLEFSInfo(char *itemRequired)
{
	char startMarker = '<';
	char endMarker = '>';
	char *receivedChars = new char[32];
	int charCnt = 0;
	char data;
	bool foundKey = false;

	Serial.printf("Looking for key '%s'.\n", itemRequired);

	// Get a handle to the file
	File configFile = LittleFS.open("/WiFiSecrets.txt", "r");
	if (!configFile)
	{
		// TODO: Display error on screen
		Serial.println("Unable to open file /WiFiSecrets.txt");
		while (1)
			;
	}

	// Look for the required key
	while (configFile.available())
	{
		charCnt = 0;

		// Read until start marker found
		while (configFile.available() && configFile.peek() != startMarker)
		{
			// Do nothing, ignore spurious chars
			data = configFile.read();
			//Serial.print("Throwing away preMarker:");
			//Serial.println(data);
		}

		// If EOF this is an error
		if (!configFile.available())
		{
			// Abort - no further data
			continue;
		}

		// Throw away startMarker char
		configFile.read();

		// Read all following characters as the data (key or value)
		while (configFile.available() && configFile.peek() != endMarker)
		{
			data = configFile.read();
			receivedChars[charCnt] = data;
			charCnt++;
		}

		// Terminate string
		receivedChars[charCnt] = '\0';

		// If we previously found the matching key then return the value
		if (foundKey)
			break;

		//Serial.printf("Found: '%s'.\n", receivedChars);
		if (strcmp(receivedChars, itemRequired) == 0)
		{
			//Serial.println("Found matching key - next string will be returned");
			foundKey = true;
		}
	}

	// Terminate file
	configFile.close();

	// Did we find anything
	Serial.printf("LITTLEFS parameter '%s'\n", itemRequired);
	if (charCnt == 0)
	{
		Serial.println("' not found.");
		return "";
	}
	else
	{
		//Serial.printf("': '%s'\n", receivedChars);
	}

	return receivedChars;
}