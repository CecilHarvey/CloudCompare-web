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

#ifndef CC_SPHERE_PRIMITIVE_HEADER
#define CC_SPHERE_PRIMITIVE_HEADER

#include "ccGenericPrimitive.h"

//CCLib
#include <CCGeom.h>

//! Sphere (primitive)
/** 3D sphere primitive
**/
#ifdef QCC_DB_USE_AS_DLL
#include "qCC_db_dll.h"
class QCC_DB_DLL_API ccSphere : public ccGenericPrimitive
#else
class ccSphere : public ccGenericPrimitive
#endif
{
public:

	//! Default constructor
	/** \param radius sphere radius
		\param transMat optional 3D transformation (can be set afterwards with ccDrawableObject::setGLTransformation)
		\param name name
		\param precision drawing precision (angular step = 360/precision)
	**/
	ccSphere(PointCoordinateType radius,
				const ccGLMatrix* transMat = NULL,
				std::string name = "Sphere",
				unsigned precision = 24);

	//! Simplified constructor
	/** For ccHObject factory only!
	**/
	ccSphere(std::string name = "Sphere");

	//! Returns class ID
	virtual CC_CLASS_ENUM getClassID() const { return CC_SPHERE; }

	//inherited from ccGenericPrimitive
	virtual std::string getTypeName() const { return "Sphere"; }
	virtual bool hasDrawingPrecision() const { return true; }
	virtual ccGenericPrimitive* clone() const;

	//! Returns radius
	PointCoordinateType getRadius() const { return m_radius; }

protected:
    
    //inherited from ccGenericPrimitive
	virtual bool buildUp();

	//inherited from ccHObject
	virtual void drawNameIn3D(CC_DRAW_CONTEXT& context);

	//! Radius
	PointCoordinateType m_radius;
};

#endif //CC_SPHERE_PRIMITIVE_HEADER
