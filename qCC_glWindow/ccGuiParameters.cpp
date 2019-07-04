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

#include "ccGuiParameters.h"
#include <ccBasicTypes.h>

//System
#include <string.h>

//! Static unique instance of ccGui
static ccGui* s_gui = 0;

const int c_fColorArraySize = 4*sizeof(float);
const int c_ubColorArraySize = 3*sizeof(unsigned char);

const ccGui::ParamStruct& ccGui::Parameters()
{
    if (!s_gui)
    {
        s_gui = new ccGui();
    }

    return s_gui->params;
}

void ccGui::ReleaseInstance()
{
    if (s_gui)
        delete s_gui;
    s_gui=0;
}

void ccGui::Set(const ParamStruct& params)
{
    if (!s_gui)
        s_gui = new ccGui();

    s_gui->params = params;
}

ccGui::ParamStruct::ParamStruct()
{
    reset();
}

void ccGui::ParamStruct::reset()
{
    memcpy(lightAmbientColor,   ccColor::darkest,			    c_fColorArraySize);
    memcpy(lightSpecularColor,  ccColor::darker,				c_fColorArraySize);
    memcpy(lightDiffuseColor,   ccColor::bright,				c_fColorArraySize);
    memcpy(meshFrontDiff,       ccColor::defaultMeshFrontDiff,	c_fColorArraySize);
    memcpy(meshBackDiff,        ccColor::defaultMeshBackDiff,	c_fColorArraySize);
    memcpy(meshSpecular,        ccColor::middle,				c_fColorArraySize);
    memcpy(pointsDefaultCol,    ccColor::defaultColor,			c_ubColorArraySize);
    memcpy(textDefaultCol,      ccColor::defaultColor,			c_ubColorArraySize);
    memcpy(backgroundCol,       ccColor::defaultBkgColor,		c_ubColorArraySize);
    memcpy(histBackgroundCol,	ccColor::defaultHistBkgColor,	c_ubColorArraySize);
    memcpy(labelCol,			ccColor::defaultLabelColor,		c_ubColorArraySize);
    memcpy(bbDefaultCol,        ccColor::yellow,				c_ubColorArraySize);

    drawBackgroundGradient      = true;
    decimateMeshOnMove          = true;
    decimateCloudOnMove         = true;
    displayCross                = true;

	pickedPointsSize = 4;

	colorScaleShowHistogram		= true;
	colorScaleUseShader			= false;
	colorScaleShaderSupported	= false;
	colorScaleRampWidth			= 50;
	
	defaultFontSize				= 10;
	displayedNumPrecision		= 6;
	labelsTransparency			= 50;
}

ccGui::ParamStruct& ccGui::ParamStruct::operator =(const ccGui::ParamStruct& params)
{
    memcpy(lightDiffuseColor,   params.lightDiffuseColor,   c_fColorArraySize);
    memcpy(lightAmbientColor,   params.lightAmbientColor,   c_fColorArraySize);
    memcpy(lightSpecularColor,  params.lightSpecularColor,  c_fColorArraySize);
    memcpy(meshFrontDiff,       params.meshFrontDiff,       c_fColorArraySize);
    memcpy(meshBackDiff,        params.meshBackDiff,        c_fColorArraySize);
	memcpy(meshSpecular,		params.meshSpecular,        c_fColorArraySize);
    memcpy(pointsDefaultCol,    params.pointsDefaultCol,    c_ubColorArraySize);
    memcpy(textDefaultCol,      params.textDefaultCol,      c_ubColorArraySize);
    memcpy(backgroundCol,       params.backgroundCol,       c_ubColorArraySize);
    memcpy(histBackgroundCol,	params.histBackgroundCol,	c_ubColorArraySize);
    memcpy(labelCol,			params.labelCol,			c_ubColorArraySize);
    memcpy(bbDefaultCol,        params.bbDefaultCol,        c_ubColorArraySize);

    drawBackgroundGradient      = params.drawBackgroundGradient;
    decimateMeshOnMove          = params.decimateMeshOnMove;
    decimateCloudOnMove         = params.decimateCloudOnMove;
    displayCross                = params.displayCross;
	pickedPointsSize			= params.pickedPointsSize;
	colorScaleShowHistogram		= params.colorScaleShowHistogram;
	colorScaleUseShader			= params.colorScaleUseShader;
	colorScaleShaderSupported	= params.colorScaleShaderSupported;
	colorScaleRampWidth			= params.colorScaleRampWidth;
	defaultFontSize				= params.defaultFontSize;
	displayedNumPrecision		= params.displayedNumPrecision;
	labelsTransparency			= params.labelsTransparency;

    return *this;
}
