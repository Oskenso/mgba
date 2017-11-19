/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/core/version.h>
#include <mgba-util/arm-algo.h>
#include <stdio.h>


#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>
#define P_RS 22 //D/C
#define P_RW 23
#define P_E 24
#define P_RES 25
#define P_PS 27
#define P_CS 28

#define COMMAND 0
#define DATA 1
#define CHANNEL 1

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

//Begin write to RAM
void screenWriteMemoryStart() {
	displaySend(COMMAND, 0x22);
}

void screenSetPos(uint8_t x, uint8_t y) {
	displaySetReg(0x20, x);
	displaySetReg(0x21, y);
}


void drawBuffer(color_t *pix) {
    screenWriteMemoryStart();

    for (int i = 0; i < (160*128); i++) {
		uint8_t buffer[BYTES_PER_PIXEL];
		buffer[0] = pix[i*BYTES_PER_PIXEL] >> 16;
		buffer[1] = pix[i*BYTES_PER_PIXEL] >> 8;
		buffer[2] = pix[i*BYTES_PER_PIXEL];
        digitalWrite(P_CS, 0);
        digitalWrite(P_RS, 1);
        digitalWriteByte(buffer[0]);
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
        digitalWriteByte(buffer[2]);
        digitalWrite(P_RW, 0);
        digitalWrite(P_E, 1);
        digitalWrite(P_E, 0);
        digitalWrite(P_CS, 1);
    }
}



static bool mSDLSWInit(struct mSDLRenderer* renderer);
static void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user);
static void mSDLSWDeinit(struct mSDLRenderer* renderer);

void mSDLSWCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLSWInit;
	renderer->deinit = mSDLSWDeinit;
	renderer->runloop = mSDLSWRunloop;
}

bool mSDLSWInit(struct mSDLRenderer* renderer) {
// #if !SDL_VERSION_ATLEAST(2, 0, 0)
// #ifdef COLOR_16_BIT
// 	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_DOUBLEBUF | SDL_HWSURFACE);
// #else
// 	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);
// #endif
// #endif

	unsigned width, height;
	renderer->core->desiredVideoDimensions(renderer->core, &width, &height);


	int stride = 160 * 3;
	renderer->outputBuffer = malloc(width * height * BYTES_PER_PIXEL);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, width);

// #if SDL_VERSION_ATLEAST(2, 0, 0)
// 	renderer->window = SDL_CreateWindow(projectName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderer->viewportWidth, renderer->viewportHeight, SDL_WINDOW_OPENGL | (SDL_WINDOW_FULLSCREEN_DESKTOP * renderer->player.fullscreen));
// 	SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
// 	renderer->player.window = renderer->window;
// 	renderer->sdlRenderer = SDL_CreateRenderer(renderer->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
// #ifdef COLOR_16_BIT
// #ifdef COLOR_5_6_5
// 	renderer->sdlTex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, width, height);
// #else
// 	renderer->sdlTex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, width, height);
// #endif
// #else
// 	renderer->sdlTex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, width, height);
// #endif
//
// 	int stride;
// 	SDL_LockTexture(renderer->sdlTex, 0, (void**) &renderer->outputBuffer, &stride);
// 	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, stride / BYTES_PER_PIXEL);
// #else
// 	SDL_Surface* surface = SDL_GetVideoSurface();
// 	SDL_LockSurface(surface);
//
// 	if (renderer->ratio == 1) {
// 		renderer->core->setVideoBuffer(renderer->core, surface->pixels, surface->pitch / BYTES_PER_PIXEL);
// 	} else {
// #ifdef USE_PIXMAN
// 		renderer->outputBuffer = malloc(width * height * BYTES_PER_PIXEL);
// 		renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, width);
// #ifdef COLOR_16_BIT
// #ifdef COLOR_5_6_5
// 		pixman_format_code_t format = PIXMAN_r5g6b5;
// #else
// 		pixman_format_code_t format = PIXMAN_x1b5g5r5;
// #endif
// #else
// 		pixman_format_code_t format = PIXMAN_x8b8g8r8;
// #endif
// 		renderer->pix = pixman_image_create_bits(format, width, height,
// 		    renderer->outputBuffer, width * BYTES_PER_PIXEL);
// 		renderer->screenpix = pixman_image_create_bits(format, renderer->viewportWidth, renderer->viewportHeight, surface->pixels, surface->pitch);
//
// 		pixman_transform_t transform;
// 		pixman_transform_init_identity(&transform);
// 		pixman_transform_scale(0, &transform, pixman_int_to_fixed(renderer->ratio), pixman_int_to_fixed(renderer->ratio));
// 		pixman_image_set_transform(renderer->pix, &transform);
// 		pixman_image_set_filter(renderer->pix, PIXMAN_FILTER_NEAREST, 0, 0);
// #else
// 		return false;
// #endif
// 	}
// #endif

	return true;
}

void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;
// #if !SDL_VERSION_ATLEAST(2, 0, 0)
// 	SDL_Surface* surface = SDL_GetVideoSurface();
// #endif

	while (mCoreThreadIsActive(context)) {
		// while (SDL_PollEvent(&event)) {
		// 	mSDLHandleEvent(context, &renderer->player, &event);
		// }

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, 160);
			drawBuffer(renderer->outputBuffer);
// #if SDL_VERSION_ATLEAST(2, 0, 0)
// 			SDL_UnlockTexture(renderer->sdlTex);
// 			SDL_RenderCopy(renderer->sdlRenderer, renderer->sdlTex, 0, 0);
// 			SDL_RenderPresent(renderer->sdlRenderer);
// 			int stride;
// 			SDL_LockTexture(renderer->sdlTex, 0, (void**) &renderer->outputBuffer, &stride);
// 			renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, stride / BYTES_PER_PIXEL);
// #else
// #ifdef USE_PIXMAN
// 			if (renderer->ratio > 1) {
// 				pixman_image_composite32(PIXMAN_OP_SRC, renderer->pix, 0, renderer->screenpix,
// 				    0, 0, 0, 0, 0, 0,
// 				    renderer->viewportWidth, renderer->viewportHeight);
// 			}
// #else
// 			switch (renderer->ratio) {
// #if defined(__ARM_NEON) && COLOR_16_BIT
// 			case 2:
// 				_neon2x(surface->pixels, renderer->outputBuffer, width, height);
// 				break;
// 			case 4:
// 				_neon4x(surface->pixels, renderer->outputBuffer, width, height);
// 				break;
// #endif
// 			case 1:
// 				break;
// 			default:
// 				abort();
// 			}
// #endif
// 			SDL_UnlockSurface(surface);
// 			SDL_Flip(surface);
// 			SDL_LockSurface(surface);
// #endif
		}
		mCoreSyncWaitFrameEnd(&context->impl->sync);
	}
}

void mSDLSWDeinit(struct mSDLRenderer* renderer) {
	if (renderer->ratio > 1) {
		free(renderer->outputBuffer);
	}
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_UnlockSurface(surface);
#ifdef USE_PIXMAN
	pixman_image_unref(renderer->pix);
	pixman_image_unref(renderer->screenpix);
#endif
#endif
}
