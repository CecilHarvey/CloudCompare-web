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

#include "ccObject.h"

//System
#include <string.h>
#include <assert.h>
#include <stdint.h>

/** Versions:
   V1.0 = prior to 05/04/2012 = old version
   V2.0 - 05/04/2012 - upgrade to serialized version with version tracking
   V2.1 - 07/02/2012 - points & 2D labels upgraded
   V2.2 - 11/26/2012 - object name is now a QString
   V2.3 - 02/07/2013 - attribute 'm_selectionBehavior' added to ccHObject class
   v2.4 - 02/22/2013 - per-cloud point size + whether name is displayed in 3D or not
   v2.5 - 03/16/2013 - ccViewportParameters structure modified
   v2.6 - 04/03/2013 - strictly positive scalar field removed and 'hidden' values marker is now NaN
   v2.7 - 04/12/2013 - Customizable color scales
**/
const unsigned c_currentDBVersion = 27; //2.7
unsigned g_unique_id = 0;

unsigned ccObject::GetCurrentDBVersion()
{
	return c_currentDBVersion;
}

void ccObject::ResetUniqueIDCounter()
{
	g_unique_id = 0;
}

unsigned ccObject::GetNextUniqueID()
{
    unsigned lastID = GetLastUniqueID();
	++lastID;
	UpdateLastUniqueID(lastID);

	return lastID;
}

unsigned ccObject::GetLastUniqueID()
{
	return g_unique_id;
}

void ccObject::UpdateLastUniqueID(unsigned lastID)
{
	g_unique_id = lastID;
}

ccObject::ccObject(std::string name)
{
	m_dirIsConfirmed = false;
    m_flags = CC_ENABLED;
    m_uniqueID = GetNextUniqueID();
    setName(name.length() == 0 ? "unnamed" : name);
}

std::string ccObject::getName() const
{
    return m_name;
}

void ccObject::setName(const std::string& name)
{
	m_name = name;
}

unsigned ccObject::getUniqueID() const
{
    return m_uniqueID;
}

void ccObject::setUniqueID(unsigned ID)
{
	m_uniqueID = ID;

	//updates last unique ID
	if (m_uniqueID>GetLastUniqueID())
		UpdateLastUniqueID(m_uniqueID);
}

bool ccObject::getFlagState(CC_OBJECT_FLAG flag) const
{
    return (m_flags & flag);
}

void ccObject::setFlagState(CC_OBJECT_FLAG flag, bool state)
{
    if (state)
        m_flags |= unsigned(flag);
    else
        m_flags &= (~unsigned(flag));
}

bool ccObject::isEnabled() const
{
    return getFlagState(CC_ENABLED);
}

void ccObject::setEnabled(bool state)
{
    setFlagState(CC_ENABLED,state);
}

bool ccObject::isLocked() const
{
    return getFlagState(CC_LOCKED);
}

void ccObject::setLocked(bool state)
{
    setFlagState(CC_LOCKED,state);
}

std::string ccObject::getMetaData(std::string key) const
{
	auto it = m_metaData.find(key);
	if (it == m_metaData.end()) {
		return "";
	}
	return it->second;
}

bool ccObject::removeMetaData(std::string key)
{
	auto it = m_metaData.find(key);
	if (it == m_metaData.end()) {
		return false;
	}
	m_metaData.erase(it);
	return true;
}

void ccObject::setMetaData(std::string key, std::string data)
{
    m_metaData[key] = data;
}
