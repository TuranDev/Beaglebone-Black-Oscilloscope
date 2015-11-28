#include <math.h>

#include "inputs.h"
#include "BBBiolib.h"
#include "display.h"
#include "processing.h"

//Oscilloscope Variables
int samples = 0;
int screenPos = 0;
int previousSample = 0;
int currentSample = 0;
int triggerLevel = 32;
int trigger = 0;
extern int voltageDiv;
extern int offset;
//Signal Generator Variables
extern int frequency[2];
extern int waveType[2];
float currentPhase[2] = { 0, 0 };
extern float amplitude[2];
extern int mode;
extern int sampleSkip;

void process(short *shortBuffer, int r, int channels, float sampleRate) {
	for (int n = 0; n < r * channels; n += channels) {
		//Checks if device in oscilloscope mode
		if (mode == 0) {
			//Have enough samples passed for required time divisions
			if (samples >= sampleSkip) {
				samples = 0;
				currentSample = (voltageDiv * (shortBuffer[n] / 1024) + 32) - offset; //normalises to between -32 and 32
				if (trigger == 0) {
					//Checks if sample is at trigger level and if its a rising edge
					if (triggerLevel == currentSample) {
						if (previousSample > currentSample) {
							trigger = 1;
							screenPos = 0;
						}
					}
				} else {

					clearColoumn(screenPos);
					drawLine(screenPos - 1, previousSample, screenPos,
							currentSample);
					screenPos++;
					if (screenPos > 127) {
						screenPos = 0;
						previousSample = 0;
						trigger = 0;
					}

				}

				previousSample = currentSample;
			}
			samples++;
			//Checks if device is in signal generator mode
		} else if (mode == 1) {
			for (int m = 0; m < channels; m++) {
				//Calculates Phase
				currentPhase[m] = currentPhase[m]
						+ (2.0 * M_PI * (float) frequency[m] / sampleRate);
				//Normalises Phase
				if (currentPhase[m] > 2.0 * M_PI)
					currentPhase[m] -= 2.0 * M_PI;

				//Checks Wavetype required then performs relevant action to gain output sample
				if (waveType[m] == 0) {
					shortBuffer[n + m] = amplitude[m]
							* (sin(currentPhase[m]) * 32768);
				} else if (waveType[m] == 1) {
					if (currentPhase[m] <= M_PI)
						shortBuffer[n + m] = amplitude[m] * 32768;
					else
						shortBuffer[n + m] = amplitude[m] * -32768;
				}
				//channel is off so assign 0
				else
					shortBuffer[n + m] = 0;
			}
		} else {
			shortBuffer[n] = 0;
			shortBuffer[n + 1] = 0;
		}
	}

}
