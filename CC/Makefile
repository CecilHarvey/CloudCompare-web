CXX = g++
CC = gcc
AR = ar

OBJ = ./src/AutoSegmentationTools.o \
	./src/CCMiscTools.o \
	./src/CCShareable.o \
	./src/ChamferDistanceTransform.o \
	./src/ChunkedPointCloud.o \
	./src/CloudSamplingTools.o \
	./src/DebugProgressCallback.o \
	./src/Delaunay2dMesh.o \
	./src/DgmOctree.o \
	./src/DgmOctreeReferenceCloud.o \
	./src/DistanceComputationTools.o \
	./src/ErrorFunction.o \
	./src/FastMarching.o \
	./src/FastMarchingForPropagation.o \
	./src/GeometricalAnalysisTools.o \
	./src/KdTree.o \
	./src/LocalModel.o \
	./src/ManualSegmentationTools.o \
	./src/MeshSamplingTools.o \
	./src/Neighbourhood.o \
	./src/NormalDistribution.o \
	./src/PointProjectionTools.o \
	./src/Polyline.o \
	./src/ReferenceCloud.o \
	./src/RegistrationTools.o \
	./src/ScalarField.o \
	./src/ScalarFieldTools.o \
	./src/SimpleCloud.o \
	./src/SimpleMesh.o \
	./src/StatisticalTestingTools.o \
	./src/TrueKdTree.o \
	./src/WeibullDistribution.o \
	./triangle/triangle.o

SDL_CFLAGS = `sdl2-config --cflags`
GL_CFLAGS =
CFLAGS = -DTRILIBRARY=1 -O2 -g -I./include -I./triangle ${SDL_CFLAGS} ${GL_CFLAGS}

all: libcc.a

libcc.a: ${OBJ}
	${AR} rcs libcc.a ${OBJ}

clean:
	-rm -f ${OBJ}
	-rm -f libcc.a

%.o:    %.cpp
	${CXX} ${CFLAGS} -c $< -o $@

%.o:    %.c
	${CC} ${CFLAGS} -c $< -o $@
