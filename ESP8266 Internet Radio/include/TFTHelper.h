// All things TFT go here
#include <Arduino.h>
#include "main.h"

void displayTrackArtist(std::string ArtistTrack){
	Serial.println(ArtistTrack.c_str());
}

// Draw percentage of buffering (eg 10000 buffer that has 8000 bytes = 80%)
void drawBufferLevel(size_t bufferLevel, bool override)
{
	static unsigned long prevMillis = millis();
	static int prevBufferPerCent = 0;

	// Only do this infrequently and if the buffer % changes
	if (millis() - prevMillis > 500 || override)
	{
		// Capture current values for next time
		prevMillis = millis();

		// Calculate the percentage (ESP32 has FP processor so shoud be efficient)
		float bufLevel = (float)bufferLevel;
		float arraySize = (float)(CIRCULARBUFFERSIZE);
		int bufferPerCent = (bufLevel / arraySize) * 100.0;

		// Only update the screen on real change (avoids flicker & saves time)
		if (bufferPerCent != prevBufferPerCent || override)
		{
			// Track the buffer percentage
			prevBufferPerCent = bufferPerCent;
			Serial.printf("Buffer:%d\n", bufferPerCent);
			/*
			Print at specific rectangular place
			TODO: These should not be magic numbers
			uint16_t bgColour, fgColour;
			switch (bufferPerCent)
			{
			case 0 ... 30:
				bgColour = TFT_RED;
				fgColour = TFT_WHITE;
				break;

			case 31 ... 74:
				bgColour = TFT_ORANGE;
				fgColour = TFT_BLACK;
				break;

			case 75 ... 100:
				bgColour = TFT_DARKGREEN;
				fgColour = TFT_WHITE;
				break;

			default:
				bgColour = TFT_WHITE;
				fgColour = TFT_BLACK;
			};

			tft.fillRoundRect(245, 170, 60, 30, 5, bgColour);
			tft.setTextColor(fgColour, bgColour);
			tft.setFreeFont(&FreeSans9pt7b);
			tft.setTextSize(1);
			tft.setCursor(255, 191);
			tft.printf("%d%%\n", bufferPerCent);
			*/
		}
	}
}

// Some track titles are ALL IN UPPER CASE, so ugly, so let's convert them
// Stackoverflow: https://stackoverflow.com/a/64128715
std::string toTitle(std::string s, const std::locale &loc)
{
	// Is the current character a word delimiter (a space)?
	bool last = true;

	// Process each character in the supplied string
	for (char &c : s)
	{
		// If the previous character was a word delimiter (space)
		// then upper case current word start, else make lower case
		c = last ? std::toupper(c, loc) : std::tolower(c, loc);

		// Is this character a space?
		last = std::isspace(c, loc);
	}
	return s;
}

// On screen NEXT button (proof of concept) There is a library that encapsulates all this.
bool getNextButtonPress()
{
	// To store the touch coordinates
	// uint16_t t_x = 0, t_y = 0;

	// // Get current touch state and coordinates
	// boolean pressed = tft.getTouch(&t_x, &t_y, 200);

	// if (pressed)
	// {
	// 	Serial.printf("Touch at x:%d y:%d\n", t_x, t_y);
	// 	if (t_x > REDBUTTON_X && (t_x < (REDBUTTON_X + REDBUTTON_W)))
	// 	{
	// 		//if ((t_y > 240 - (REDBUTTON_Y + REDBUTTON_H)) && (t_y >= (REDBUTTON_Y + REDBUTTON_H)))
	// 		if (t_y > 30 && t_y < 70)
	// 		{
	// 			Serial.println("Next");
	// 			return true;
	// 		}
	// 	}
	// }

	return false;
}

// On screen PREV button (proof of concept) There is a library that encapsulates all this.
bool getPrevButtonPress()
{
	// To store the touch coordinates
	// uint16_t t_x = 0, t_y = 0;

	// // Get current touch state and coordinates
	// boolean pressed = tft.getTouch(&t_x, &t_y, 200);

	// if (pressed)
	// {
	// 	Serial.printf("Touch at x:%d y:%d\n", t_x, t_y);
	// 	if (t_x > REDBUTTON_X - 100 && (t_x < (REDBUTTON_X - 100 + REDBUTTON_W)))
	// 	{
	// 		if (t_y > 30 && t_y < 70)
	// 		{
	// 			Serial.println("Prev!");
	// 			return true;
	// 		}
	// 	}
	// }

	return false;
}