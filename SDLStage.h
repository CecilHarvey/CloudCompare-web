#ifndef SDLSTAGE_H
#define SDLSTAGE_H

#include "SDL.h"
#include "SDL_opengl.h"
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

using namespace std;

class SDLStage {
public:
    SDLStage (int width, int height, int frameRate);
    ~SDLStage ();

    bool active;

    void resize(int width, int height);
    void setEventListener (void (*listener) (SDL_Event&));
    void setRenderCallback (void (*callback) (void));
    void setUpdateCallback (void (*callback) (int));
    void step ();

private:
    void (*eventListener) (SDL_Event&);
    bool paused;
    int previousTime;
    void (*renderCallback) (void);
    int ticksPerFrame;
    void (*updateCallback) (int);
    SDL_Window *window;
    SDL_GLContext context;
    void handleEvent (SDL_Event &event);
    void render ();
    void update (int deltaTime);
};

Uint32 timer_onComplete (Uint32 interval, void *param);

#endif

