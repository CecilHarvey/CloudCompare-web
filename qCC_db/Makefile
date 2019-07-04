CXX = g++
CC = gcc
AR = ar

OBJ = ./ccBBox.o \
	./ccLog.o \
	./ccGLMatrix.o \
	./ccGenericMesh.o \
	./ccObject.o \
	./ccDrawableObject.o \
	./ccGenericGLDisplay.o \
	./ccHObject.o \
	./ccHObjectCaster.o \
	./ccTimer.o \
	./ccBox.o \
	./ccGenericPrimitive.o \
	./ccMesh.o \
	./ccPlane.o \
	./ccGenericPointCloud.o \
	./ccPointCloud.o \
	./ccMaterial.o \
	./ccMaterialSet.o \
	./ccKdTree.o \
	./ccSphere.o \
	./ccMeshGroup.o \
	./ccScalarField.o \
	./cc2DLabel.o \
	./ccTorus.o \
	./ccCylinder.o \
	./ccCone.o \
	./ccClipBox.o \
	./ccPolyline.o \
	./ccColorScale.o \
	./ccGLUtils.o \
	./ccNormalVectors.o \
	./ccOctree.o \
	./ccColorScalesManager.o \
	./ccGBLSensor.o \
	./cc2DViewportLabel.o \
	./cc2DViewportObject.o \
	./ccDish.o \
	./ccExtru.o

SDL_CFLAGS = `sdl2-config --cflags`
GL_CFLAGS =
CFLAGS = -std=c++11 -O2 -g -I../CC/triangle -I../CC/include -I../CCFbo/include ${SDL_CFLAGS} ${GL_CFLAGS}

all: libqCC_db.a

libqCC_db.a: ${OBJ}
	${AR} rcs libqCC_db.a ${OBJ}

clean:
	-rm -f ${OBJ}
	-rm -f libqCC_db.a

%.o:    %.cpp
	${CXX} ${CFLAGS} -c $< -o $@

%.o:    %.c
	${CC} ${CFLAGS} -c $< -o $@
