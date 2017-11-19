/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include <mgba/internal/debugger/cli-debugger.h>

#ifdef USE_GDB_STUB
#include <mgba/internal/debugger/gdb-stub.h>
#endif
#ifdef USE_EDITLINE
#include "feature/editline/cli-el-backend.h"
#endif
#ifdef ENABLE_SCRIPTING
#include <mgba/core/scripting.h>

#ifdef ENABLE_PYTHON
#include "platform/python/engine.h"
#endif
#endif

#include <mgba/core/cheats.h>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/input.h>
#include <mgba/core/thread.h>
#include <mgba/internal/gba/input.h>

#include <mgba/feature/commandline.h>
#include <mgba-util/vfs.h>

#include <SDL.h>

#include <errno.h>
#include <signal.h>

#define PORT "sdl"

static bool mSDLInit(struct mSDLRenderer* renderer);
static void mSDLDeinit(struct mSDLRenderer* renderer);

static int mSDLRun(struct mSDLRenderer* renderer, struct mArguments* args);


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


void drawBuffer(uint8_t *buffer) {
    //FILE *sysfb;
    //uint8_t *buffer;

    //sysfb = fopen("/dev/fb0/", "r");
    //if (sysfb != NULL)
    //{
        //buffer = malloc (sizeof(uint8_t) * 160*128*4);

        //uint8_t result = fread(buffer, 160*128*4, 1, sysfb);

        //fclose (sysfb);

        screenWriteMemoryStart();
        for (int i = 0; i < (160*128); i++) {
            //uint8_t color = buffer[i+0] << 16 | buffer[i+1] << 8 | buffer[i+2];

            digitalWrite(P_CS, 0);
            digitalWrite(P_RS, 1);
            digitalWriteByte(buffer[(i*3)+0]);
            digitalWrite(P_RW, 0);
            digitalWrite(P_E, 1);
            digitalWrite(P_E, 0);
            digitalWrite(P_CS, 1);

            digitalWrite(P_CS, 0);
            digitalWrite(P_RS, 1);
            digitalWriteByte(buffer[(i*3)+1]);
            digitalWrite(P_RW, 0);
            digitalWrite(P_E, 1);
            digitalWrite(P_E, 0);
            digitalWrite(P_CS, 1);

            digitalWrite(P_CS, 0);
            digitalWrite(P_RS, 1);
            digitalWriteByte(buffer[(i*3)+2]);
            digitalWrite(P_RW, 0);
            digitalWrite(P_E, 1);
            digitalWrite(P_E, 0);
            digitalWrite(P_CS, 1);
        }

        //free(buffer);
    //}
}


uint8_t *sepsBuffer;

int main(int argc, char** argv) {

	//sepsBuffer = malloc (sizeof(uint8_t) * 160*128*3);

	struct mSDLRenderer renderer = {0};

	struct mCoreOptions opts = {
		.useBios = true,
		.rewindEnable = true,
		.rewindBufferCapacity = 600,
		.rewindSave = true,
		.audioBuffers = 1024,
		.videoSync = false,
		.audioSync = true,
		.volume = 0x100,
	};

	struct mArguments args;
	struct mGraphicsOpts graphicsOpts;

	struct mSubParser subparser;

	initParserForGraphics(&subparser, &graphicsOpts);
	bool parsed = parseArguments(&args, argc, argv, &subparser);
	if (!args.fname && !args.showVersion) {
		parsed = false;
	}
	if (!parsed || args.showHelp) {
		usage(argv[0], subparser.usage);
		freeArguments(&args);
		return !parsed;
	}
	if (args.showVersion) {
		version(argv[0]);
		freeArguments(&args);
		return 0;
	}

	renderer.core = mCoreFind(args.fname);
	if (!renderer.core) {
		printf("Could not run game. Are you sure the file exists and is a compatible game?\n");
		freeArguments(&args);
		return 1;
	}
	renderer.core->desiredVideoDimensions(renderer.core, &renderer.width, &renderer.height);

	mSDLSWCreate(&renderer);


	wiringPiSetup();
	//pinMode(P_CPU, OUTPUT);
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

	digitalWrite(P_PS, 1);

	initDisplay();


	renderer.ratio = graphicsOpts.multiplier;
	if (renderer.ratio == 0) {
		renderer.ratio = 1;
	}
	opts.width = renderer.width * renderer.ratio;
	opts.height = renderer.height * renderer.ratio;

	if (!renderer.core->init(renderer.core)) {
		freeArguments(&args);
		return 1;
	}

	struct mCheatDevice* device = NULL;
	if (args.cheatsFile && (device = renderer.core->cheatDevice(renderer.core))) {
		struct VFile* vf = VFileOpen(args.cheatsFile, O_RDONLY);
		if (vf) {
			mCheatDeviceClear(device);
			mCheatParseFile(device, vf);
			vf->close(vf);
		}
	}

	mInputMapInit(&renderer.core->inputMap, &GBAInputInfo);
	mCoreInitConfig(renderer.core, PORT);
	applyArguments(&args, &subparser, &renderer.core->config);

	mCoreConfigLoadDefaults(&renderer.core->config, &opts);
	mCoreLoadConfig(renderer.core);

	renderer.viewportWidth = renderer.core->opts.width;
	renderer.viewportHeight = renderer.core->opts.height;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer.player.fullscreen = renderer.core->opts.fullscreen;
	renderer.player.windowUpdated = 0;
#else
	renderer.fullscreen = renderer.core->opts.fullscreen;
#endif

	renderer.lockAspectRatio = renderer.core->opts.lockAspectRatio;
	renderer.lockIntegerScaling = renderer.core->opts.lockIntegerScaling;
	renderer.filter = renderer.core->opts.resampleVideo;

	if (!mSDLInit(&renderer)) {
		freeArguments(&args);
		renderer.core->deinit(renderer.core);
		return 1;
	}

	renderer.player.bindings = &renderer.core->inputMap;
	mSDLInitBindingsGBA(&renderer.core->inputMap);
	mSDLInitEvents(&renderer.events);
	mSDLEventsLoadConfig(&renderer.events, mCoreConfigGetInput(&renderer.core->config));
	mSDLAttachPlayer(&renderer.events, &renderer.player);
	mSDLPlayerLoadConfig(&renderer.player, mCoreConfigGetInput(&renderer.core->config));

//#if SDL_VERSION_ATLEAST(2, 0, 0)
//	renderer.core->setPeripheral(renderer.core, mPERIPH_RUMBLE, &renderer.player.rumble.d);
//#endif

	int ret;

	// TODO: Use opts and config
	ret = mSDLRun(&renderer, &args);
	mSDLDetachPlayer(&renderer.events, &renderer.player);
	mInputMapDeinit(&renderer.core->inputMap);

	if (device) {
		mCheatDeviceDestroy(device);
	}

	mSDLDeinit(&renderer);

	freeArguments(&args);
	mCoreConfigFreeOpts(&opts);
	mCoreConfigDeinit(&renderer.core->config);
	renderer.core->deinit(renderer.core);

	return ret;
}

int mSDLRun(struct mSDLRenderer* renderer, struct mArguments* args) {
	struct mCoreThread thread = {
		.core = renderer->core
	};
	if (!mCoreLoadFile(renderer->core, args->fname)) {
		return 1;
	}
	mCoreAutoloadSave(renderer->core);
	mCoreAutoloadCheats(renderer->core);
#ifdef ENABLE_SCRIPTING
	struct mScriptBridge* bridge = mScriptBridgeCreate();
#ifdef ENABLE_PYTHON
	mPythonSetup(bridge);
#endif
#endif

#ifdef USE_DEBUGGERS
	struct mDebugger* debugger = mDebuggerCreate(args->debuggerType, renderer->core);
	if (debugger) {
#ifdef USE_EDITLINE
		if (args->debuggerType == DEBUGGER_CLI) {
			struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
			CLIDebuggerAttachBackend(cliDebugger, CLIDebuggerEditLineBackendCreate());
		}
#endif
		mDebuggerAttach(debugger, renderer->core);
		mDebuggerEnter(debugger, DEBUGGER_ENTER_MANUAL, NULL);
 #ifdef ENABLE_SCRIPTING
		mScriptBridgeSetDebugger(bridge, debugger);
#endif
	}
#endif

	if (args->patch) {
		struct VFile* patch = VFileOpen(args->patch, O_RDONLY);
		if (patch) {
			renderer->core->loadPatch(renderer->core, patch);
		}
	} else {
		mCoreAutoloadPatch(renderer->core);
	}

	renderer->audio.samples = renderer->core->opts.audioBuffers;
	renderer->audio.sampleRate = 44100;

	bool didFail = !mCoreThreadStart(&thread);
	if (!didFail) {

// #if SDL_VERSION_ATLEAST(2, 0, 0)
// 		renderer->core->desiredVideoDimensions(renderer->core, &renderer->width, &renderer->height);
// 		unsigned width = renderer->width * renderer->ratio;
// 		unsigned height = renderer->height * renderer->ratio;
// 		if (width != (unsigned) renderer->viewportWidth && height != (unsigned) renderer->viewportHeight) {
// 			SDL_SetWindowSize(renderer->window, width, height);
// 			renderer->player.windowUpdated = 1;
// 		}
// 		mSDLSetScreensaverSuspendable(&renderer->events, renderer->core->opts.suspendScreensaver);
// 		mSDLSuspendScreensaver(&renderer->events);
// #endif
puts(">:3");
		if (mSDLInitAudio(&renderer->audio, &thread)) {

			renderer->runloop(renderer, &thread);

			mSDLPauseAudio(&renderer->audio);
			if (mCoreThreadHasCrashed(&thread)) {
				didFail = true;
				printf("The game crashed!\n");
			}
		} else {
			didFail = true;
			printf("Could not initialize audio.\n");
		}
// #if SDL_VERSION_ATLEAST(2, 0, 0)
// 		mSDLResumeScreensaver(&renderer->events);
// 		mSDLSetScreensaverSuspendable(&renderer->events, false);
// #endif

		mCoreThreadJoin(&thread);
	} else {
		printf("Could not run game. Are you sure the file exists and is a compatible game?\n");
	}
	renderer->core->unloadROM(renderer->core);

#ifdef ENABLE_SCRIPTING
	mScriptBridgeDestroy(bridge);
#endif

	return didFail;
}

static bool mSDLInit(struct mSDLRenderer* renderer) {
	/*
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Could not initialize video: %s\n", SDL_GetError());
		return false;
	}*/

	return renderer->init(renderer);
}

static void mSDLDeinit(struct mSDLRenderer* renderer) {
	mSDLDeinitEvents(&renderer->events);
	mSDLDeinitAudio(&renderer->audio);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DestroyWindow(renderer->window);
#endif

	renderer->deinit(renderer);

	SDL_Quit();
}
