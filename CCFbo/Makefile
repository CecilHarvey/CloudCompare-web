CXX = g++
CC = gcc
AR = ar

OBJ = ./src/ccBilateralFilter.o \
	./src/ccFBOUtils.o \
	./src/ccFrameBufferObject.o \
	./src/ccShader.o

SDL_CFLAGS = `sdl2-config --cflags`
GL_CFLAGS =
CFLAGS = -O2 -g -I./include ${SDL_CFLAGS} ${GL_CFLAGS}

all: libccfbo.a

libccfbo.a: ${OBJ}
	${AR} rcs libccfbo.a ${OBJ}

clean:
	-rm -f ${OBJ}
	-rm -f libccfbo.a

%.o:    %.cpp
	${CXX} ${CFLAGS} -c $< -o $@

%.o:    %.c
	${CC} ${CFLAGS} -c $< -o $@
