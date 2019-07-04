//##########################################################################
//#                                                                        #
//#                            CLOUDCOMPARE                                #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#          COPYRIGHT: EDF R&D / TELECOM ParisTech (ENST-TSI)             #
//#                                                                        #
//##########################################################################

#include "ccGenericGLDisplay.h"

//CCLib
#include <CCConst.h>

ccViewportParameters::ccViewportParameters()
	: pixelSize(1.0f)
	, zoom(1.0f)
	, defaultPointSize(1)
	, defaultLineWidth(1)
	, perspectiveView(false)
	, objectCenteredView(true)
	, pivotPoint(0.0f)
	, cameraCenter(0.0f)
	, fov(30.0f)
	, aspectRatio(1.0f)
{
	viewMat.toIdentity();
}

ccViewportParameters::ccViewportParameters(const ccViewportParameters& params)
	: pixelSize(params.pixelSize)
	, zoom(params.zoom)
	, viewMat(params.viewMat)
	, defaultPointSize(params.defaultPointSize)
	, defaultLineWidth(params.defaultLineWidth)
	, perspectiveView(params.perspectiveView)
	, objectCenteredView(params.objectCenteredView)
	, pivotPoint(params.pivotPoint)
	, cameraCenter(params.cameraCenter)
	, fov(params.fov)
	, aspectRatio(params.aspectRatio)
{
}
