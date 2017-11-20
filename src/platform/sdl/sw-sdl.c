/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* SEPS525 display support added by Oskenso Kashi <contact@oskenso.com>
*
 */
#include "main.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/core/version.h>
#include <mgba-util/arm-algo.h>



void displaySend(uint8_t t, uint8_t d) {
	digitalWrite(P_CS, 0);
	digitalWrite(P_RS, t);
	digitalWriteByte(d);
    digitalWrite(P_RW, 0);
    digitalWrite(P_E, 1);
    digitalWrite(P_E, 0);
    digitalWrite(P_CS, 1);
}

void displaySetReg(uint8_t c, uint8_t d) {
	displaySend(COMMAND, c);
	displaySend(DATA, d);
}

void displaySetColumnAddress(uint8_t startX, uint8_t endX) {
	displaySetReg(0x17, startX);
	displaySetReg(0x18, endX);
}

void displaySetRowAddress(uint8_t startY, uint8_t endY) {
	displaySetReg(0x19, startY);
	displaySetReg(0x1A, endY);
}

//Begin write to RAM
void screenWriteMemoryStart() {
	displaySend(COMMAND, 0x22);
}

void initDisplay() {
	//digitalWrite(P_CPU, 0);
	//digitalWrite(P_PS, 0);

	digitalWrite(P_RES, 0);
	delay(100);
	digitalWrite(P_RES, 1);
	delay(100);

	//displaySend(COMMAND, 0x04);
	//displaySend(DATA, 0x03);
	//delay(2);

	//half driving current
	displaySetReg(0x04, 0x04);

	displaySetReg(0x3B, 0x00);
	displaySetReg(0x02, 0x01);//export1 pin at interal

	displaySetReg(0x03, 0x90);//120hz
	displaySetReg(0x80, 0x01);//ref voltage

	displaySetReg(0x08, 0x04);
	displaySetReg(0x09, 0x05);
	displaySetReg(0x0A, 0x05);

	displaySetReg(0x0B, 0x9D);
	displaySetReg(0x0C, 0x8C);
	displaySetReg(0x0D, 0x57);//pre charge of blue
	displaySetReg(0x10, 0x56);
	displaySetReg(0x11, 0x4D);
	displaySetReg(0x12, 0x46);
	displaySetReg(0x13, 0x00);//set color sequence

	displaySetReg(0x14, 0x01);// Set MCU Interface Mode
	displaySetReg(0x16, 0x76);// Set Memory Write Mode

	//shift mapping ram counter
	displaySetReg(0x20, 0x00);
	displaySetReg(0x21, 0x00);
	displaySetReg(0x28, 0x7F);// 1/128 Duty (0x0F~0x7F)
	displaySetReg(0x29, 0x00);// Set Mapping RAM Display Start Line (0x00~0x7F)
	displaySetReg(0x06, 0x01);// Display On (0x00/0x01)

	// Set All Internal Register Value as Normal Mode
	displaySetReg(0x05, 0x00); // Disable Power Save Mode

	// Set RGB Interface Polarity as Active Low
	displaySetReg(0x15, 0x00);

	displaySetColumnAddress(0, 159);
	displaySetRowAddress(0, 127);
}

void screenSetPos(uint8_t x, uint8_t y) {
	displaySetReg(0x20, x);
	displaySetReg(0x21, y);
}


void drawBuffer(color_t *pix) {
    screenWriteMemoryStart();

    for (int i = 0; i < (160*128); i++) {
		uint8_t buffer[BYTES_PER_PIXEL];
		buffer[0] = pix[i] >> 16;
		buffer[1] = pix[i] >> 8;
		buffer[2] = pix[i];
        digitalWrite(P_CS, 0);
        digitalWrite(P_RS, 1);
        digitalWriteByte(buffer[2]);
        digitalWrite(P_RW, 0);
        digitalWrite(P_E, 1);
        digitalWrite(P_E, 0);
        digitalWrite(P_CS, 1);

        digitalWrite(P_CS, 0);
        digitalWrite(P_RS, 1);
        digitalWriteByte(buffer[1]);
        digitalWrite(P_RW, 0);
        digitalWrite(P_E, 1);
        digitalWrite(P_E, 0);
        digitalWrite(P_CS, 1);

        digitalWrite(P_CS, 0);
        digitalWrite(P_RS, 1);
        digitalWriteByte(buffer[1]);
        digitalWrite(P_RW, 0);
        digitalWrite(P_E, 1);
        digitalWrite(P_E, 0);
        digitalWrite(P_CS, 1);
    }
}
/*
FILE *sysfb;
void drawFrameBuffer(color_t *c) {
	sysfb = fopen("/dev/fb0", "w");
	fwrite(c, 4, 160*128, sysfb);
	fclose(sysfb);
}
*/

static bool mSDLSWInit(struct mSDLRenderer* renderer);
static void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user);
static void mSDLSWDeinit(struct mSDLRenderer* renderer);

void mSDLSWCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLSWInit;
	renderer->deinit = mSDLSWDeinit;
	renderer->runloop = mSDLSWRunloop;



	wiringPiSetup();
	pinMode(P_PS, OUTPUT);
	pinMode(P_RES, OUTPUT);
	pinMode(P_RS, OUTPUT);

	pinMode(P_RW, OUTPUT);
	pinMode(P_E, OUTPUT);
	pinMode(P_CS, OUTPUT);
	pinMode(0, OUTPUT);
	pinMode(1, OUTPUT);
	pinMode(2, OUTPUT);
	pinMode(3, OUTPUT);
	pinMode(4, OUTPUT);
	pinMode(5, OUTPUT);
	pinMode(6, OUTPUT);
	pinMode(7, OUTPUT);

}


bool mSDLSWInit(struct mSDLRenderer* renderer) {
	unsigned width, height;
	renderer->core->desiredVideoDimensions(renderer->core, &width, &height);

	renderer->outputBuffer = malloc(width * height * BYTES_PER_PIXEL);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, width);

	digitalWrite(P_PS, 1);
	initDisplay();

	return true;
}

void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;

	//int fskip = 0;
	while (mCoreThreadIsActive(context)) {
		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
		//	if (fskip > 1)
				//drawBuffer(renderer->outputBuffer);
				drawBuffer(renderer->outputBuffer);
		//	fskip = 0;
		}
//		fskip++;

		mCoreSyncWaitFrameEnd(&context->impl->sync);
	}
}

void mSDLSWDeinit(struct mSDLRenderer* renderer) {
	if (renderer->ratio > 1) {
		free(renderer->outputBuffer);
	}
	displaySend(COMMAND, 0x04);//power save
	displaySend(DATA, 0x05);// 1/2 driving current, display off
}
