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


//FILE *sysfb;
int sysfb;
void drawFrameBuffer(FILE *fh, color_t *c) {
	//sysfb = fopen("/dev/fb0", "w");
	//rewind(fh);
	//fwrite(c, 4, 160*128, fh);
	pwrite(sysfb, c, 160*128*4, 0);
	//fclose(sysfb);
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
	unsigned width, height;
	renderer->core->desiredVideoDimensions(renderer->core, &width, &height);


	int stride = 160 * 3;
	renderer->outputBuffer = malloc(width * height * BYTES_PER_PIXEL);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, width);

	//digitalWrite(P_PS, 1);
	//initDisplay();

	return true;
}

void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;

	//sysfb = fopen("/dev/fb0", "w");
	sysfb = open("/dev/fb0", "O_WRONLY);
	while (mCoreThreadIsActive(context)) {
		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			drawFrameBuffer(sysfb, renderer->outputBuffer);
		}

		mCoreSyncWaitFrameEnd(&context->impl->sync);
	}
	//fclose(sysfb);
	close(sysfb);
}

void mSDLSWDeinit(struct mSDLRenderer* renderer) {
	if (renderer->ratio > 1) {
		free(renderer->outputBuffer);
	}
}
