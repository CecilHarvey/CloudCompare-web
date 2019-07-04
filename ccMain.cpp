#include <ccIncludeGL.h>
#include "ccGLWindow.h"
#include "ccTimer.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <thread>
#include <mutex>

#include "SDLStage.h"

#include "ccSphere.h"
#include "ccBox.h"
#include "ccHObjectCaster.h"

SDLStage* stage = NULL;
std::mutex stage_mutex;

ccGLWindow *glWindow = NULL;
std::mutex gl_window_mutex;

ccHObject *root = NULL;
ccHObject *prev_selected = NULL;

int SCREEN_WIDTH = 1000;
int SCREEN_HEIGHT = 1000;

static bool initialized = false;

extern "C" void initialize_gl4es();

static bool initOpenGL() {
#ifdef __EMSCRIPTEN__
    initialize_gl4es();
#endif
    return true;
}

void handleEvent(SDL_Event &event) {
    if (event.type == SDL_QUIT) {
        exit(0);
    }
    std::lock_guard<std::mutex> _lock(gl_window_mutex);
    glWindow->handleEvent(&event);
}

void render() {
    std::lock_guard<std::mutex> _lock(gl_window_mutex);
    glWindow->paintGL();
}

void update(int deltaTime) {
}

extern "C" void step() {
    std::lock_guard<std::mutex> _lock(stage_mutex);
    if (stage != NULL) {
        stage->step();
    }
}

static void entitySelectionChanged(int uniqueID) {
    if (uniqueID < 0) {
        if (prev_selected) {
            prev_selected->setSelected(false);
            prev_selected = NULL;
        }
        return;
    }
    ccHObject *obj = root->find(uniqueID);
    if (obj == NULL) {
        fprintf(stderr, "entitySelectionChanged(): entity %d not found\n", uniqueID);
        return;
    }
    if (obj->isA(CC_BOX) || obj->isA(CC_SPHERE)) {
        if (prev_selected) {
            prev_selected->setSelected(false);
        }
        prev_selected = obj;
        obj->setSelected(true);
    } else if (prev_selected) {
        prev_selected->setSelected(false);
        prev_selected = NULL;
    }
}

int initialize() {
    ccTimer::Init();
    srand(time(NULL));

    stage = new SDLStage(SCREEN_WIDTH, SCREEN_HEIGHT, 30);

    if (!initOpenGL()) {
        fprintf(stderr, "Error initializing OpenGL\n");
        return -1;
    }

    stage->setEventListener(&handleEvent);
    stage->setRenderCallback(&render);
    stage->setUpdateCallback(&update);

    glWindow = new ccGLWindow();
    glWindow->entitySelectionChanged = entitySelectionChanged;
    glWindow->initializeGL();
    glWindow->resizeGL(SCREEN_WIDTH, SCREEN_HEIGHT);
    glWindow->setPickingMode(ccGLWindow::ENTITY_PICKING);

    root = new ccHObject("root");
    root->setDisplay(glWindow);
    glWindow->setSceneDB(root);

    return 0;
}

extern "C" int set_screen_size(int width, int height) {
    SCREEN_WIDTH = width;
    SCREEN_HEIGHT = height;
    std::lock_guard<std::mutex> _lock(stage_mutex);
    std::lock_guard<std::mutex> _lock2(gl_window_mutex);
    if (stage != NULL) {
        stage->resize(width, height);
        glWindow->resizeGL(width, height);
    }
    return 0;
}

#ifndef __EMSCRIPTEN__

void gameMain() {
    step();
}

int main(int argc, char *argv[]) {
    initialize();

    ccSphere *sphere = new ccSphere(30, NULL, "Sphere", 24);
    sphere->setDisplay(glWindow);
    sphere->showNameIn3D(true);
    root->addChild(sphere);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(gameMain, 0, 1);
#else
    while (true) {
        SDL_Delay(100);
        gameMain();
    }
#endif
    return 0;
}

#endif
