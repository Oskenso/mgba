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

void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;

	while (mCoreThreadIsActive(context)) {

		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			resize_bilinear(renderer->outputBuffer, fbOut, renderer->desiredWidth, renderer->desiredHeight, 160, 128);
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
