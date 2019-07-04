OBJ = ./qCC_glWindow/ccRenderingTools.o \
	./qCC_glWindow/ccGLWindow.o \
	./qCC_glWindow/ccGuiParameters.o \
	./ccMain.o \
	./SDLStage.o

ifeq "${EMSCRIPTEN}" "1"

CXX = em++
CC = emcc
AR = emar

PWD = $(shell pwd)
SDL_CFLAGS = -s USE_SDL=2
SDL_LIBS = -s USE_SDL=2
GL_CFLAGS = -s FULL_ES2=1 -D__EMSCRIPTEN__=1 -Wno-macro-redefined -I${PWD}/gl4es/include
GL_LIBS = -s FULL_ES2=1 -s WASM=1 -s SINGLE_FILE=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s ALLOW_MEMORY_GROWTH=1 -s GL_MAX_TEMP_BUFFER_SIZE=8388608 -L${PWD}/gl4es -lGL -lGLU
LDFLAGS = -O2 -s EXTRA_EXPORTED_RUNTIME_METHODS="['cwrap']" -s EXPORTED_FUNCTIONS="['_step', '_set_screen_size']" -s "BINARYEN_TRAP_MODE='clamp'"

OUT = minicc.js

else

CXX = g++
CC = gcc
AR = ar

UNAME_S := $(shell uname -s)

SDL_CFLAGS = `sdl2-config --cflags`
SDL_LIBS = `sdl2-config --libs`
GL_CFLAGS = -Wno-deprecated-declarations
ifeq ($(UNAME_S),Darwin)
GL_LIBS = -framework OpenGL
else
GL_LIBS = -lGL -lGLU
endif
LDFLAGS = -O2 -g

OUT = minicc

endif

CFLAGS = -DTRILIBRARY=1 -O2 -g -I./CC/include -I./qCC_glWindow -I./CCFbo/include -I./qCC_db ${SDL_CFLAGS} ${GL_CFLAGS}

all: ${OUT}

${OUT}: ${OBJ} qCC_db/libqCC_db.a CC/libcc.a CCFbo/libccfbo.a
	${CXX} ${LDFLAGS} -o ${OUT} ${OBJ} qCC_db/libqCC_db.a CC/libcc.a CCFbo/libccfbo.a ${SDL_LIBS} ${GL_LIBS}

clean:
	-rm -f ${OBJ}
	-rm -f minicc minicc.js minicc.wasm minicc.html minicc.wast
	-cd qCC_db && make clean
	-cd CC && make clean
	-cd CCFbo && make clean

%.o:    %.cpp
	${CXX} ${CFLAGS} -c $< -o $@

%.o:    %.c
	${CC} ${CFLAGS} -c $< -o $@

qCC_db/libqCC_db.a:
	cd qCC_db && make CXX=${CXX} CC=${CC} AR=${AR} SDL_CFLAGS="${SDL_CFLAGS}" GL_CFLAGS="${GL_CFLAGS}"

CC/libcc.a:
	cd CC && make CXX=${CXX} CC=${CC} AR=${AR} SDL_CFLAGS="${SDL_CFLAGS}" GL_CFLAGS="${GL_CFLAGS}"

CCFbo/libccfbo.a:
	cd CCFbo && make CXX=${CXX} CC=${CC} AR=${AR} SDL_CFLAGS="${SDL_CFLAGS}" GL_CFLAGS="${GL_CFLAGS}"
