#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#include "inputs.h"
#include "BBBiolib.h"
#include "display.h"
#include "processing.h"

//Stores inputs from the 5 potentiometers inputs (10 samples per potentiometer)
int analougeInputs[5][10];

//Stores number of samples to skip for a specific time division
int sampleSkip = 0;

extern int gShouldStop;

//Initialises all inputs
void inputInitialise() {
	iolib_init();
	//Sets direction of inputs
	iolib_setdir(CS, OUTPUT);
	iolib_setdir(RST, OUTPUT);
	iolib_setdir(A0, OUTPUT);
	iolib_setdir(SCLK, OUTPUT);
	iolib_setdir(SID, OUTPUT);
	iolib_setdir(scrollButton, INPUT);
	iolib_setdir(selectButton, INPUT);
	iolib_setdir(button1, INPUT);
	iolib_setdir(button2, INPUT);

	BBBIO_module_ctrl(1);

	BBBIO_channel_ctrl(BBBIO_ADC_AIN6, BBBIO_ADC_STEP_MODE_SW_CONTINUOUS, 0, 1,
	BBBIO_ADC_STEP_AVG_1, analougeInputs[0], 10);

	BBBIO_channel_ctrl(BBBIO_ADC_AIN5, BBBIO_ADC_STEP_MODE_SW_CONTINUOUS, 0, 1,
	BBBIO_ADC_STEP_AVG_1, analougeInputs[1], 10);

	BBBIO_channel_ctrl(BBBIO_ADC_AIN4, BBBIO_ADC_STEP_MODE_SW_CONTINUOUS, 0, 1,
	BBBIO_ADC_STEP_AVG_1, analougeInputs[2], 10);

	BBBIO_channel_ctrl(BBBIO_ADC_AIN1, BBBIO_ADC_STEP_MODE_SW_CONTINUOUS, 0, 1,
	BBBIO_ADC_STEP_AVG_1, analougeInputs[3], 10);

	BBBIO_channel_ctrl(BBBIO_ADC_AIN0, BBBIO_ADC_STEP_MODE_SW_CONTINUOUS, 0, 1,
	BBBIO_ADC_STEP_AVG_1, analougeInputs[4], 10);

	//Enables analogue inputs
	BBBIO_channel_enable(BBBIO_ADC_AIN6);
	BBBIO_channel_enable(BBBIO_ADC_AIN5);
	BBBIO_channel_enable(BBBIO_ADC_AIN4);
	BBBIO_channel_enable(BBBIO_ADC_AIN1);
	BBBIO_channel_enable(BBBIO_ADC_AIN0);
}

void *inputCheck(void *data) {
	// Variables to be assigned values
	extern int frequency[];
	extern float amplitude[];
	extern float timeDiv;
	extern int triggerLevel;
	extern int voltageDiv;
	extern int waveType[2];
	extern int offset;
	extern int menu;
	extern int mode;

	//Stores latest value of buttons
	int latestValue[4];
	//Stores previous values for buttons
	int previousValue[4] = { 1, 1, 1, 1 };

	while (!gShouldStop) {

		//Handles Scroll Button Input
		if (is_high(scrollButton))
			latestValue[0] = 1;
		else
			latestValue[0] = 0;
		//Cycles through menu
		if (latestValue[0] == 0 && previousValue[0] == 1) {
			if (mode == -1) {
				menu++;
				menu %= 3;
				memcpy(displayBuffer, &menuBuffer[menu], 1024);
			}
		}
		previousValue[0] = latestValue[0];

		//Handles Select Button Input
		if (is_high(selectButton))
			latestValue[1] = 1;
		else
			latestValue[1] = 0;

		if (latestValue[1] == 0 && previousValue[1] == 1) {
			//Selects menu or goes back to menu
			if (mode == -1) {
				if (menu == 0) {
					mode = 0;
					memcpy(displayBuffer, &oscilliscopeBuffer, 1024);
				} else if (menu == 1) {
					mode = 1;
					memcpy(displayBuffer, &signalGenBuffer, 1024);
				} else if (menu == 2) {
					mode = 2;
					memcpy(displayBuffer, &aboutBuffer, 1024);
				}
			} else {
				mode = -1;
				memcpy(displayBuffer, &menuBuffer[menu], 1024);
			}
		}
		previousValue[1] = latestValue[1];

		//Handles Wave Type For Channel 1 or Offset for oscilloscope
		if (is_high(button1))
			latestValue[2] = 1;
		else
			latestValue[2] = 0;

		if (latestValue[2] == 0 && previousValue[2] == 1) {
			if (mode == 1) {
				if (waveType[0] == 0) {
					waveType[0] = 1;
					drawCross(26, 57, 0);
					drawCross(58, 57, 1);
				} else if (waveType[0] == 1) {
					waveType[0] = 2;
					drawCross(26, 57, 0);
					drawCross(58, 57, 0);
				} else {
					waveType[0] = 0;
					drawCross(26, 57, 1);
				}
			} else if (mode == 0) {
				offset++;
				if (offset > 30)
					offset = 30;
			}
		}
		previousValue[2] = latestValue[2];

		//Handles Wave Type For Channel 2 or Offset for oscilloscope
		if (is_high(button2))
			latestValue[3] = 1;
		else
			latestValue[3] = 0;

		if (latestValue[3] == 0 && previousValue[3] == 1) {
			if (mode == 1) {
				if (waveType[1] == 0) {
					waveType[1] = 1;
					drawCross(89, 57, 0);
					drawCross(121, 57, 1);
				} else if (waveType[1] == 1) {
					waveType[1] = 2;
					drawCross(89, 57, 0);
					drawCross(121, 57, 0);
				} else {
					waveType[1] = 0;
					drawCross(89, 57, 1);
				}
			} else if (mode == 0) {
				offset--;
				if (offset < 1)
					offset = 1;
			}
		}

		previousValue[3] = latestValue[3];

		//Retrieves 10 analouge samples
		BBBIO_work(10);

		//Averages the retrieved smaples
		int sampleAverage[5] = { 0, 0, 0, 0, 0 };
		for (int n = 0; n < 10; n++) {
			for (int m = 0; m < 5; m++) {
				sampleAverage[m] += analougeInputs[m][n];
			}
		}
		for (int n = 0; n < 5; n++)
			sampleAverage[n] /= 10;
		//End of averaging

		//checks if device in oscilliscope mode
		if (mode == 0) {

			triggerLevel = 1 + (sampleAverage[2] * (62 - 1) / 4096); // Normalises trigger level to between 1 and 62

			voltageDiv = 1 + (sampleAverage[1] * (5 - 1) / 4096); // normalise to between 0 and 5 for voltage division

			sampleAverage[0] = (sampleAverage[0] * 6 / 4096); // normalise to between 0 and 6 for time divisions

			//10 pixels per division hence sampleSkip=sampleRate X timeDiv /10
			switch (sampleAverage[0]) {
			case 0:
				timeDiv = 0.1; //0.1mS
				sampleSkip = 1; //0.96
				break;
			case 1:
				timeDiv = 0.5; //0.5mS
				sampleSkip = 5; //4.8
				break;
			case 2:
				timeDiv = 1; //1mS
				sampleSkip = 10; //9.6
				break;
			case 3:
				timeDiv = 2; //2mS
				sampleSkip = 19; //19.2
				break;
			case 4:
				timeDiv = 5; //5mS
				sampleSkip = 48; //48
				break;
			case 5:
				timeDiv = 10; //10mS
				sampleSkip = 96; //96
				break;
			}

			printf(
					"\rTime Div = %.1f mS, Voltage Div = %.2f v,TriggerLevel= %d ",
					timeDiv, (float) voltageDiv / 4, triggerLevel);

			fflush(stdout);
			//Checks if device in signal generator mode
		} else if (mode == 1) {

			frequency[0] = ((50 + (sampleAverage[3] * (20000 - 50) / 4096)) / 50
					* 50); // normalise to 50-20000 and assigns to frequency1
			frequency[1] = ((50 + (sampleAverage[4] * (20000 - 50) / 4096)) / 50
					* 50); // normalise to 50-20000 and assigns to frequency2

			drawNumber(18, 19, frequency[0], 4);
			drawNumber(82, 19, frequency[1], 4);

			amplitude[0] = (float) sampleAverage[1] / 4096;
			amplitude[1] = (float) sampleAverage[2] / 4096;
			drawNumber(42, 30, sampleAverage[1] * (100) / 4096, 1);
			drawNumber(106, 30, sampleAverage[2] * (100) / 4096, 1);
		}

		updateDisplay();
		usleep(30000);
	}
	pthread_exit(NULL);

}

void inputCleanup() {
	iolib_free();
}
