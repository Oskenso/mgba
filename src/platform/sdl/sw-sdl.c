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

#include "bilinear-resize.h"

int sysfb;
void drawFrameBuffer(int fh, color_t *c) {
	pwrite(fh, c, 160*144*4, 0);
}


static bool mSDLSWInit(struct mSDLRenderer* renderer);
static void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user);
static void mSDLSWDeinit(struct mSDLRenderer* renderer);

void mSDLSWCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLSWInit;
	renderer->deinit = mSDLSWDeinit;
	renderer->runloop = mSDLSWRunloop;
}

uint32_t * fbOut;

bool mSDLSWInit(struct mSDLRenderer* renderer) {
	unsigned width, height;
	renderer->core->desiredVideoDimensions(renderer->core, &width, &height);
	renderer->desiredWidth = width;
	renderer->desiredHeight = height;

	renderer->outputBuffer = malloc(width * height * BYTES_PER_PIXEL);
	fbOut = malloc(160 * 128 * BYTES_PER_PIXEL);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, width);

	sysfb = open("/dev/fb0", O_WRONLY);

	return true;
}

void osky_resize(uint32_t *in, uint32_t *out) {
	int offset = 0;
	int w = 160;
	int h = 128;
	for (int i = 0; i < w * h; i++) {
		int line = floor(i / w);
		if ( ((i % w) == 0) && ((line % 8) == 1) && (line != 0)) {
			offset += w;
		}
		uint8_t r,g,b;
		if (offset >= w) {
			r = (((in[i+offset] >> 16) & 0xFF) + ((in[i+offset-160] >> 16) & 0xFF)) / 2;
			g = (((in[i+offset] >> 8) & 0xFF) + ((in[i+offset-160] >> 8) & 0xFF)) / 2;
			b = (((in[i+offset]) & 0xFF) + ((in[i+offset-160]) & 0xFF)) / 2;
		} else {
			r = (((in[i+offset] >> 16) & 0xFF) );
			g = (((in[i+offset] >> 8) & 0xFF) ) ;
			b = (((in[i+offset]) & 0xFF));
		}

		out[i] = r << 16 | g << 8 | b;
	}
}

void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;

	while (mCoreThreadIsActive(context)) {

		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			//resize_bilinear(renderer->outputBuffer, fbOut, renderer->desiredWidth, renderer->desiredHeight, 160, 128);
			osky_resize(renderer->outputBuffer, fbOut);
			drawFrameBuffer(sysfb, fbOut);
		}

		mCoreSyncWaitFrameEnd(&context->impl->sync);
		SDL_Delay(1);
	}


}

void mSDLSWDeinit(struct mSDLRenderer* renderer) {
	if (renderer->ratio > 1) {
		free(renderer->outputBuffer);
		free(fbOut);
	}
	close(sysfb);
}
