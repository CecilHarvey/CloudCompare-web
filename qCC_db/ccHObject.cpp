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

#include "ccHObject.h"

//Local
#include "ccIncludeGL.h"
#include "ccTimer.h"
#include "ccLog.h"

//Objects handled by factory
#include "ccBox.h"
#include "ccSphere.h"
#include "ccPointCloud.h"
#include "ccMesh.h"
#include "ccMaterialSet.h"
#include "cc2DLabel.h"
#include "ccTorus.h"
#include "ccCylinder.h"
#include "ccCone.h"
#include "ccGBLSensor.h"
#include "cc2DViewportLabel.h"
#include "cc2DViewportObject.h"
#include "ccMeshGroup.h"
#include "ccPolyline.h"
#include "ccPlane.h"
#include "ccAdvancedTypes.h"
#include "ccDish.h"
#include "ccExtru.h"

//CCLib
#include <CCShareable.h>

//System
#include <stdint.h>
#include <assert.h>

ccHObject::ccHObject(std::string name/*=""*/)
	: ccObject(name)
	, ccDrawableObject()
	, m_parent(0)
	, m_lastModificationTime_ms(0)
	, m_selectionBehavior(SELECTION_AA_BBOX)
{
	setVisible(false);
	lockVisibility(true);
	updateModificationTime();
	m_glTransHistory.toIdentity();
}

ccHObject::~ccHObject()
{
	removeAllChildren();
}

ccHObject* ccHObject::New(unsigned objectType, const char* name/*=0*/)
{
	switch(objectType)
	{
	case CC_HIERARCHY_OBJECT:
		return new ccHObject(name);
	case CC_BOX:
		return new ccBox(name);
	case CC_POINT_CLOUD:
		return new ccPointCloud(name);
	case CC_SPHERE:
		return new ccSphere(name);
	case CC_MESH:
		//warning: no associated vertices --> retrieved later
		return new ccMesh(0);
	case CC_MATERIAL_SET:
		return new ccMaterialSet();
	case CC_2D_LABEL:
		return new cc2DLabel(name);
	case CC_TORUS:
		return new ccTorus(name);
	case CC_CYLINDER:
		return new ccCylinder(name);
	case CC_CONE:
		return new ccCone(name);
	case CC_GBL_SENSOR:
		//warning: default sensor type set in constructor (see CCLib::GroundBasedLidarSensor::setRotationOrder)
		return new ccGBLSensor();
	case CC_POLY_LINE:
		//warning: no associated vertices --> retrieved later
		return new ccPolyline(0);
	case CC_MESH_GROUP:
		//warning: no associated vertices --> retrieved later
		return new ccMeshGroup(0);
	case CC_NORMALS_ARRAY:
		return new NormsTableType();
	case CC_NORMAL_INDEXES_ARRAY:
		return new NormsIndexesTableType();
	case CC_RGB_COLOR_ARRAY:
		return new ColorsTableType();
	case CC_TEX_COORDS_ARRAY:
		return new TextureCoordsContainer();
	case CC_2D_VIEWPORT_OBJECT:
		return new cc2DViewportObject(name);
	case CC_2D_VIEWPORT_LABEL:
		return new cc2DViewportLabel(name);
	case CC_PLANE:
		return new ccPlane(name);
	case CC_DISH:
		return new ccDish(name);
	case CC_EXTRU:
		return new ccExtru(name);
	case CC_POINT_OCTREE:
	case CC_POINT_KDTREE:
		//construction this way is not supported (yet)
		ccLog::ErrorDebug("[ccHObject::New] This object (type %i) can't be constructed this way (yet)!",objectType);
		break;

	default:
		//unhandled ID
		ccLog::ErrorDebug("[ccHObject::New] Invalid object type (%i)!",objectType);
		break;
	}

	return 0;
}

void ccHObject::addChild(ccHObject* anObject, bool dependant/*=true*/, int insertIndex/*=-1*/)
{
	if (!anObject)
		return;

	if (isLeaf())
	{
		ccLog::ErrorDebug("[ccHObject::addChild] Leaf objects shouldn't have any child!");
		return;
	}

	if (insertIndex<0 || insertIndex>=(int)m_children.size())
		m_children.push_back(anObject);
	else
		m_children.insert(m_children.begin()+insertIndex,anObject);

	if (dependant)
	{
		anObject->setParent(this);
		anObject->setFlagState(CC_FATHER_DEPENDANT,dependant);
		if (anObject->isShareable())
			dynamic_cast<CCShareable*>(anObject)->link();
	}
}

ccHObject* ccHObject::find(int uniqueID)
{
	//now, we are going to test each object in the database!
	//(any better idea ?)
	ccHObject::Container toTest;
	toTest.push_back(this);

	while (!toTest.empty())
	{
		ccHObject* obj = toTest.back();
		toTest.pop_back();

		if (obj->getUniqueID() == uniqueID)
			return obj;

		for (unsigned i=0;i<obj->getChildrenNumber();++i)
			toTest.push_back(obj->getChild(i));
	}

	return NULL;
}

unsigned ccHObject::filterChildren(Container& filteredChildren, bool recursive/*=false*/, CC_CLASS_ENUM filter /*= CC_OBJECT*/) const
{
	for (Container::const_iterator it = m_children.begin(); it!=m_children.end(); ++it)
	{
		if ((*it)->isKindOf(filter))
			//warning: we have to handle unicity as a sibling may be in the same container as its parent!
			if (std::find(filteredChildren.begin(),filteredChildren.end(),*it) == filteredChildren.end()) //not yet in output vector?
				filteredChildren.push_back(*it);

		if (recursive)
			(*it)->filterChildren(filteredChildren, true, filter);
	}

	return (unsigned)filteredChildren.size();
}

int ccHObject::getChildIndex(const ccHObject* aChild) const
{
	for (unsigned i=0; i<m_children.size(); ++i)
	{
		if (m_children[i] == aChild)
			return (int)i;
	}

	return -1;
}

void ccHObject::detachFromParent()
{
	ccHObject* parent = getParent();
	if (!parent)
		return;

	setFlagState(CC_FATHER_DEPENDANT,false);
	parent->removeChild(this);
}

void ccHObject::transferChild(unsigned index, ccHObject& newParent)
{
	ccHObject* child = getChild(index);
	if (!child)
	{
		assert(false);
		return;
	}

	//remove link from old parent
	bool fatherDependent = child->getFlagState(CC_FATHER_DEPENDANT);
	if (fatherDependent)
		child->setFlagState(CC_FATHER_DEPENDANT,false);
	removeChild(index);
	newParent.addChild(child,fatherDependent);
}

void ccHObject::transferChildren(ccHObject& newParent, bool forceFatherDependent/*=false*/)
{
	for (Container::iterator it = m_children.begin(); it != m_children.end(); ++it)
	{
		//remove link from old parent
		bool fatherDependent = (*it)->getFlagState(CC_FATHER_DEPENDANT) || forceFatherDependent;
		if (fatherDependent)
			(*it)->setFlagState(CC_FATHER_DEPENDANT,false);
		newParent.addChild(*it,fatherDependent);
	}

	m_children.clear();
}

void ccHObject::swapChildren(unsigned firstChildIndex, unsigned secondChildIndex)
{
	assert(firstChildIndex<m_children.size());
	assert(secondChildIndex<m_children.size());

	std::swap(m_children[firstChildIndex],m_children[secondChildIndex]);
}

int ccHObject::getIndex() const
{
	return (m_parent ? m_parent->getChildIndex(this) : -1);
}

bool ccHObject::isAncestorOf(const ccHObject *anObject) const
{
	assert(anObject);
	ccHObject* parent = anObject->getParent();
	if (!parent)
		return false;

	if (parent == this)
		return true;

	return isAncestorOf(parent);
}

ccBBox ccHObject::getBB(bool relative/*=true*/, bool withGLfeatures/*=false*/, const ccGenericGLDisplay* display/* = NULL*/)
{
	ccBBox box;

	//if (!isEnabled())
	//    return box;

	if (!display || m_currentDisplay==display)
		box = (withGLfeatures ? getDisplayBB() : getMyOwnBB());

	Container::iterator it = m_children.begin();
	for (;it!=m_children.end();++it)
	{
		if ((*it)->isEnabled())
			box += ((*it)->getBB(false, withGLfeatures, display));
	}

	//apply GL transformation afterwards!
	if (!display || m_currentDisplay==display)
		if (box.isValid() && !relative && m_glTransEnabled)
			box *= m_glTrans;

	return box;
}

ccBBox ccHObject::getMyOwnBB()
{
	return ccBBox();
}

ccBBox ccHObject::getDisplayBB()
{
	//by default, this is the same bbox as the "geometrical" one
	return getMyOwnBB();
}

CCVector3 ccHObject::getCenter()
{
	ccBBox box = getBB(true,false,m_currentDisplay);

	return box.getCenter();
}

void ccHObject::drawNameIn3D(CC_DRAW_CONTEXT& context)
{
	if (!context._win)
		return;

	//we display it in the 2D layer in fact!
    ccBBox bBox = getBB(true,false,m_currentDisplay);
	if (bBox.isValid())
	{
		const double* MM = context._win->getModelViewMatd(); //viewMat
		const double* MP = context._win->getProjectionMatd(); //projMat
		int VP[4];
		context._win->getViewportArray(VP);

		GLdouble xp,yp,zp;
		CCVector3 C = bBox.getCenter();
		gluProject(C.x,C.y,C.z,MM,MP,VP,&xp,&yp,&zp);

		context._win->displayText(getName(),(int)xp,(int)yp,ccGenericGLDisplay::ALIGN_HMIDDLE | ccGenericGLDisplay::ALIGN_VMIDDLE,75);
	}
}

bool ccHObject::isDisplayed() const
{
	return isBranchEnabled() && isVisible() && (getDisplay() != 0);
}

bool ccHObject::isBranchEnabled() const
{
	if (!isEnabled())
		return false;
	
	if (m_parent)
		return m_parent->isBranchEnabled();

	return true;
}

void ccHObject::draw(CC_DRAW_CONTEXT& context)
{
	if (!isEnabled())
		return;

	//are we currently drawing objects in 2D or 3D?
	bool draw3D = MACRO_Draw3D(context);
	
	//the entity must be either visible and selected, and of course it should be displayed in this context
	bool drawInThisContext = ((m_visible || m_selected) && m_currentDisplay == context._win);

	//no need to display anything but clouds and meshes in "element picking mode"
	drawInThisContext &= (( !MACRO_DrawPointNames(context) || isKindOf(CC_POINT_CLOUD) ) || 
		                  ( !MACRO_DrawTriangleNames(context) || isKindOf(CC_MESH) ));

	//apply 3D 'temporary' transformation (for display only)
	if (draw3D && m_glTransEnabled)
	{
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glMultMatrixf(m_glTrans.data());
	}

	//draw entity
	if (m_visible && drawInThisContext)
	{
		if (( !m_selected || !MACRO_SkipSelected(context) ) &&
			( m_selected || !MACRO_SkipUnselected(context) ))
		{
			//apply default color (in case of)
			glColor3ubv(context.pointsDefaultCol);

			drawMeOnly(context);

			//draw name in 3D (we display it in the 2D foreground layer in fact!)
			if (m_showNameIn3D && MACRO_Draw2D(context) && MACRO_Foreground(context) && !MACRO_DrawNames(context))
				drawNameIn3D(context);
		}
	}

	//draw entity's children
	for (Container::iterator it = m_children.begin(); it!=m_children.end(); ++it)
		(*it)->draw(context);

	//if the entity is currently selected, we draw its bounding-box
	if (m_selected && draw3D && drawInThisContext && !MACRO_DrawNames(context))
	{
		switch (m_selectionBehavior)
		{
		case SELECTION_AA_BBOX:
			drawBB(context.bbDefaultCol);
			break;
		case SELECTION_FIT_BBOX:
			{
				ccGLMatrix trans;
				ccBBox box = getFitBB(trans);
				if (box.isValid())
				{
					glMatrixMode(GL_MODELVIEW);
					glPushMatrix();
					glMultMatrixf(trans.data());
					box.draw(context.bbDefaultCol);
					glPopMatrix();
				}
			}
			break;
		case SELECTION_IGNORED:
			break;
		default:
			assert(false);
		}
	}

	if (draw3D && m_glTransEnabled)
		glPopMatrix();
}

void ccHObject::applyGLTransformation(const ccGLMatrix& trans)
{
	m_glTransHistory = trans * m_glTransHistory;
}

void ccHObject::applyGLTransformation_recursive(ccGLMatrix* trans/*=NULL*/)
{
	ccGLMatrix* _trans = NULL;

	if (m_glTransEnabled)
	{
		if (!trans)
		{
			//if no transformation is provided (by father)
			//we initiate it with the current one
			trans = _trans = new ccGLMatrix(m_glTrans);
		}
		else
		{
			*trans *= m_glTrans;
		}
	}

	if (trans)
	{
		applyGLTransformation(*trans);
		updateModificationTime();
	}

	for (Container::iterator it = m_children.begin(); it!=m_children.end(); ++it)
		(*it)->applyGLTransformation_recursive(trans);

	if (_trans)
		delete _trans;

	if (m_glTransEnabled)
		razGLTransformation();
}

bool ccHObject::getAbsoluteGLTransformation(ccGLMatrix& trans) const
{
        trans.toIdentity();
        bool hasGLTrans = false;

        //recurse among ancestors to get the absolute GL transformation
        const ccHObject* obj = this;
        while (obj)
        {
                if (obj->isGLTransEnabled())
                {
                        trans = trans * obj->getGLTransformation();
                        hasGLTrans = true;
                }
                obj = obj->getParent();
        }

        return hasGLTrans;
}

//void ccHObject::setDisplay_recursive(ccGenericGLDisplay* win)
//{
//	setDisplay(win);
//
//	for (Container::iterator it = m_children.begin(); it!=m_children.end(); ++it)
//		(*it)->setDisplay_recursive(win);
//}
//
//void ccHObject::setSelected_recursive(bool state)
//{
//	setSelected(state);
//
//	for (Container::iterator it = m_children.begin(); it!=m_children.end(); ++it)
//		(*it)->setSelected_recursive(state);
//}
//
//
//void ccHObject::removeFromDisplay_recursive(ccGenericGLDisplay* win)
//{
//	removeFromDisplay(win);
//
//	for (Container::iterator it = m_children.begin(); it!=m_children.end(); ++it)
//		(*it)->removeFromDisplay_recursive(win);
//}
//
//void ccHObject::refreshDisplay_recursive()
//{
//	refreshDisplay();
//
//	for (Container::iterator it = m_children.begin(); it!=m_children.end(); ++it)
//		(*it)->refreshDisplay_recursive();
//}
//
//void ccHObject::prepareDisplayForRefresh_recursive()
//{
//	prepareDisplayForRefresh();
//
//	for (Container::iterator it = m_children.begin(); it!=m_children.end(); ++it)
//		(*it)->prepareDisplayForRefresh_recursive();
//}

void ccHObject::removeChild(const ccHObject* anObject, bool preventAutoDelete/*=false*/)
{
	assert(anObject);

	int pos = getChildIndex(anObject);

	if (pos>=0)
		removeChild(pos,preventAutoDelete);
}

void ccHObject::removeChild(int pos, bool preventAutoDelete/*=false*/)
{
	assert(pos>=0 && unsigned(pos)<m_children.size());

	ccHObject* child = m_children[pos];
	if (child->getFlagState(CC_FATHER_DEPENDANT) && !preventAutoDelete)
	{
		//delete object
		if (child->isShareable())
			dynamic_cast<CCShareable*>(child)->release();
		else
			delete child;
	}
	else
	{
		//detach object
		if (child->getParent() == this)
			child->setParent(0);
	}

	//version "swap"
	/*m_children[pos]=m_children.back();
	m_children.pop_back();
	//*/

	//version "shift"
	m_children.erase(m_children.begin()+pos);
}

void ccHObject::removeAllChildren()
{
	while (!m_children.empty())
	{
		ccHObject* child = m_children.back();
		m_children.pop_back();
		if (child->getParent()==this && child->getFlagState(CC_FATHER_DEPENDANT))
		{
			if (child->isShareable())
				dynamic_cast<CCShareable*>(child)->release();
			else
				delete child;
		}
	}
}

int ccHObject::getLastModificationTime_recursive() const
{
	int t = getLastModificationTime();

	for (Container::const_iterator it = m_children.begin();it!=m_children.end();++it)
	{
		int child_t = (*it)->getLastModificationTime_recursive();
		t = std::max(t,child_t);
	}

	return t;
}

static int s_lastModificationTime_ms = 0;
void ccHObject::updateModificationTime()
{
	m_lastModificationTime_ms = ccTimer::Msec();
	//to be sure that the clock is increasing, whatever its precision!
	if (m_lastModificationTime_ms <= s_lastModificationTime_ms)
		m_lastModificationTime_ms = s_lastModificationTime_ms+1;

	s_lastModificationTime_ms = m_lastModificationTime_ms;
}
