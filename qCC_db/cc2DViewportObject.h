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

#ifndef CC_2D_VIEWPORT_OBJECT_HEADER
#define CC_2D_VIEWPORT_OBJECT_HEADER

//Local
#include "ccHObject.h"
#include "ccGLMatrix.h"
#include "ccGenericGLDisplay.h"

#include <string>
#include <stdio.h>

//! 2D viewport object
#ifdef QCC_DB_USE_AS_DLL
#include "qCC_db_dll.h"
class QCC_DB_DLL_API cc2DViewportObject : public ccHObject
#else
class cc2DViewportObject : public ccHObject
#endif
{
public:

	//! Default constructor
	cc2DViewportObject(std::string name="");

	//inherited from ccHObject
    virtual CC_CLASS_ENUM getClassID() const {return CC_2D_VIEWPORT_OBJECT;};

	//! Sets perspective view state
	void setParameters(const ccViewportParameters& params) { m_params = params; }

	//! Gets parameters
	const ccViewportParameters& getParameters() const { return m_params; }

protected:

	//! Viewport parameters
	ccViewportParameters m_params;
};

#endif //CC_2D_VIEWPORT_OBJECT_HEADER
