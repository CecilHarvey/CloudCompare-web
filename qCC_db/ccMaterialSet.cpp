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

#include "ccMaterialSet.h"

//Local
#include "ccGenericGLDisplay.h"
#include "ccLog.h"

//System
#include <string.h>
#include <assert.h>

ccMaterialSet::ccMaterialSet(std::string name/*=""*/)
	: std::vector<ccMaterial>()
	, CCShareable()
	, ccHObject(name)
	, m_display(0)
{
	setFlagState(CC_LOCKED,true);
	setFlagState(CC_FATHER_DEPENDANT,false);
}

ccMaterialSet::~ccMaterialSet()
{
	associateTo(0);
}

int ccMaterialSet::findMaterial(std::string mtlName)
{
	unsigned i=0;
	ccMaterialSet::const_iterator it = begin();
	for (; it != end(); ++it,++i)
		if (it->name == mtlName)
			return (int)i;

	return -1;
}

bool ccMaterialSet::addMaterial(const ccMaterial& mat)
{
	if (findMaterial(mat.name)>=0)
		return false;

	push_back(mat);
	return  true;
}

#define MTL_LOADER_WHITESPACE " \t\n\r"

//! Max number of characters per line in an ASCII file
const int MAX_ASCII_FILE_LINE_LENGTH	=	4096;

//MTL PARSER INSPIRED FROM KIXOR.NET "objloader" (http://www.kixor.net/dev/objloader/)
bool ccMaterialSet::ParseMTL(std::string path, const std::string& filename, ccMaterialSet &materials, std::list<std::string>& errors)
{
	return false; // TODO
#if 0
	char current_line[MAX_ASCII_FILE_LINE_LENGTH];
	unsigned current_line_index = 0;
	int current_mtl_index = -1;
	ccMaterial currentMaterial;

	// open scene
	std::string fullName = path+"/"+filename;
	FILE *fp = fopen(fullName.c_str(), "r");
	if(fp == 0)
	{
		errors.push_back(std::string("Error reading file: ") + filename);
		return false;
	}

	while( fgets(current_line, MAX_ASCII_FILE_LINE_LENGTH, fp) )
	{
		++current_line_index;

		std::list<std::string> tokens;// = QString(current_line).split(QRegExp("\\s+"),QString::SkipEmptyParts);
		char *p = &current_line[0];
		char *pBegin = &current_line[0];
		while (*p != '\0') {
			if (*p == ' ' || *p == '\t' || *p = '\n') {
				*p = '\0';
				if (pBegin < p) {
					tokens.push_back(pBegin);
				}
				pBegin = p + 1;
			}
			p++;
		}
		if (pBegin < p) {
			tokens.push_back(pBegin);
		}

		//skip comments & empty lines
		if( tokens.empty() || tokens.front()[0] == '/' || tokens.front()[0] == '#')
			continue;

		//start material
		if (tokens.front() == "newmtl")
		{
			//push the precedent material
			if (current_mtl_index>=0)
				materials.addMaterial(currentMaterial);

			++current_mtl_index;
			currentMaterial = ccMaterial();
			materials.resize(materials.size()+1);
			// get the name
			currentMaterial.name = (tokens.size()>1 ? tokens[1] : "undefined");

		}
		else if (current_mtl_index>=0) //we already have a "current" material
		{
			//ambient
			if (tokens.front() == "Ka")
			{
				if (tokens.size() > 3)
				{
					currentMaterial.ambient[0] = tokens[1].toFloat();
					currentMaterial.ambient[1] = tokens[2].toFloat();
					currentMaterial.ambient[2] = tokens[3].toFloat();
				}
			}

			//diff
			else if (tokens.front() == "Kd")
			{
				if (tokens.size() > 3)
				{
					currentMaterial.diffuseFront[0] = tokens[1].toFloat();
					currentMaterial.diffuseFront[1] = tokens[2].toFloat();
					currentMaterial.diffuseFront[2] = tokens[3].toFloat();
					//duplicate
					memcpy(currentMaterial.diffuseBack,currentMaterial.diffuseFront,sizeof(float)*3);
				}
			}

			//specular
			else if (tokens.front() == "Ks")
			{
				if (tokens.size() > 3)
				{
					currentMaterial.specular[0] = tokens[1].toFloat();
					currentMaterial.specular[1] = tokens[2].toFloat();
					currentMaterial.specular[2] = tokens[3].toFloat();
				}
			}
			//shiny
			else if (tokens.front() == "Ns")
			{
				if (tokens.size() > 1)
					currentMaterial.setShininess(tokens[1].toFloat());
			}
			//transparent
			else if (tokens.front() == "d" || tokens.front() == "Tr")
			{
				if (tokens.size() > 1)
					currentMaterial.setTransparency(tokens[1].toFloat());
			}
			//reflection
			else if (tokens.front() == "r")
			{
				//ignored
				//if (tokens.size() > 1)
				//	currentMaterial.reflect = tokens[1].toFloat();
			}
			//glossy
			else if (tokens.front() == "sharpness")
			{
				//ignored
				//if (tokens.size() > 1)
				//	currentMaterial.glossy = tokens[1].toFloat();
			}
			//refract index
			else if (tokens.front() == "Ni")
			{
				//ignored
				//if (tokens.size() > 1)
				//	currentMaterial.refract_index = tokens[1].toFloat();
			}
			// illumination type
			else if (tokens.front() == "illum")
			{
				//ignored
			}
			// texture map
			else if (tokens.front() == "map_Ka"
					|| tokens.front() == "map_Kd"
					|| tokens.front() == "map_Ks")
			{
				std::string texture_filename = std::string(current_line+7).trimmed();
				std::string fullTexName = path+"/"+texture_filename;
				QImage image;
				image.load(fullTexName);
				if (!image.load(fullTexName))
				{
					errors << QString("Failed to load texture file: %1").arg(fullTexName);
				}
				else
				{
					currentMaterial.texture = image.mirrored(); //mirrored = WTF?!
				}
			}
			else
			{
				errors << QString("Unknown command '%1' at line %2").arg(tokens.front()).arg(current_line_index);
			}
		}
	}

	//push the last material
	if (current_mtl_index>=0)
		materials.addMaterial(currentMaterial);

	fclose(fp);
	fp=0;

	return true;
#endif
}

void ccMaterialSet::associateTo(ccGenericGLDisplay* display)
{
	if (m_display == display)
		return;

	ccMaterialSet::iterator it = begin();
	for (;it!=end();++it)
		if (!it->texture.isNull())
		{
			//we release texture from old display (if any)
			if (m_display)
				m_display->releaseTexture(it->texID);
			//we register texture in the new one (if any)
			//it->texID = 0;
			it->texID = 0;//(display ? display->getTexture(it->texture) : 0);
		}

	m_display = display;
}

const ccGenericGLDisplay* ccMaterialSet::getAssociatedDisplay()
{
	return m_display;
}

bool ccMaterialSet::append(const ccMaterialSet& source)
{
	try
	{
		reserve(size()+source.size());
	}
	catch(.../*const std::bad_alloc&*/) //out of memory
	{
		ccLog::Warning("[ccMaterialSet::append] Not enough memory");
		return false;
	}
	
	for (ccMaterialSet::const_iterator it = source.begin(); it!=source.end(); ++it)
	{
		push_back(*it);
		//back().texture.detach();
	}

	return true;
}


ccMaterialSet* ccMaterialSet::clone() const
{
	ccMaterialSet* cloneSet = new ccMaterialSet(getName());
	if (!cloneSet->append(*this))
	{
		ccLog::Warning("[ccMaterialSet::clone] Not enough memory");
		cloneSet->release();
		cloneSet=0;
	}

	return cloneSet;
}
