#include "SDLStage.h"
#include <stdio.h>

SDLStage::SDLStage (int width, int height, int frameRate) {
    active = false;
    window = SDL_CreateWindow("OpenGL Test", 0, 0, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (window != NULL) {
        context = SDL_GL_CreateContext(window);
        previousTime = 0;
        ticksPerFrame = (int)(1000 / frameRate);
        active = true;
    }
}

SDLStage::~SDLStage () {
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void SDLStage::resize(int width, int height) {
    SDL_SetWindowSize(window, width, height);
}

void SDLStage::handleEvent (SDL_Event &event) {
    switch (event.type) {
    case SDL_QUIT:
        active = false;
        break;
    }

    if (eventListener != NULL) {
        (*eventListener) (event);
    }

    event.type = -1;

}


void SDLStage::render () {
    if (renderCallback != NULL) {
        (*renderCallback) ();
    }
    SDL_GL_SwapWindow(window);
}

void SDLStage::setEventListener (void (*listener) (SDL_Event&)) {
    eventListener = listener;
}

void SDLStage::setRenderCallback (void (*callback) (void)) {
    renderCallback = callback;
}

void SDLStage::setUpdateCallback (void (*callback) (int)) {
    updateCallback = callback;
}

void SDLStage::step () {
    SDL_Event event;

    while (SDL_PollEvent (&event)) {
        handleEvent (event);
        if (!active) {
            break;
        }
    }

    if (active) {
        int currentTime = SDL_GetTicks ();
        int deltaTime = currentTime - previousTime;

        update (currentTime - previousTime);
        render ();

#ifndef EMSCRIPTEN
        while (deltaTime < ticksPerFrame) {
            SDL_TimerID timer = SDL_AddTimer (ticksPerFrame - deltaTime, timer_onComplete, NULL);

            SDL_WaitEvent (&event);
            SDL_RemoveTimer (timer);

            if (event.type != SDL_USEREVENT) {
                handleEvent (event);
                deltaTime = SDL_GetTicks () - previousTime;
            } else {
                event.type = -1;
                break;
            }
        }
#endif
        previousTime = currentTime;
    }
}

void SDLStage::update (int deltaTime) {
    if (updateCallback != NULL) {
        (*updateCallback) (deltaTime);
    }
}

// Event Handlers

Uint32 timer_onComplete (Uint32 interval, void *param) {
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent (&event);

    return 0;
}
