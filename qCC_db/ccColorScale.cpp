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

#include "ccColorScale.h"
#include <algorithm>
#include <stdlib.h>
//Local
#include "ccLog.h"

ccColorScale::Shared ccColorScale::Create(std::string name)
{
	return std::make_shared<ccColorScale>(name);
}

ccColorScale::ccColorScale(std::string name, std::string uuid/*=""*/)
	: m_name(name)
	, m_uuid(uuid)
	, m_updated(false)
	, m_relative(true)
	, m_locked(false)
	, m_absoluteMinValue(0.0)
	, m_absoluteRange(1.0)
{
	if (m_uuid.length() == 0)
		generateNewUuid();
}

void ccColorScale::generateNewUuid()
{
	const std::string CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	m_uuid = std::string(36,' ');
	int rnd = 0;
	int r = 0;

	m_uuid[8] = '-';
	m_uuid[13] = '-';
	m_uuid[18] = '-';
	m_uuid[23] = '-';

	m_uuid[14] = '4';

	for (int i = 0; i < 36; i++) {
		if (i != 8 && i != 13 && i != 18 && i != 14 && i != 23) {
			if (rnd <= 0x02) {
				rnd = 0x2000000 + (rand() * 0x1000000) | 0;
			}
			rnd >>= 4;
			m_uuid[i] = CHARS[(i == 19) ? ((rnd & 0xf) & 0x3) | 0x8 : rnd & 0xf];
		}
	}
}

ccColorScale::~ccColorScale()
{
}

void ccColorScale::insert(const ccColorScaleElement& step, bool autoUpdate/*=true*/)
{
	if (m_locked)
	{
		ccLog::Warning(std::string("[ccColorScale::insert] Scale [") + m_name + "] is locked!");
		return;
	}

	m_steps.push_back(step);

	m_updated = false;

	if (autoUpdate && m_steps.size() >= (int)MIN_STEPS)
		update();
}

void ccColorScale::clear()
{
	if (m_locked)
	{
		ccLog::Warning(std::string("[ccColorScale::clear] Scale [") + m_name + "] is locked!");
		return;
	}

	m_steps.clear();

	m_updated = false;
}

void ccColorScale::remove(int index, bool autoUpdate/*=true*/)
{
	if (m_locked)
	{
		ccLog::Warning(std::string("[ccColorScale::remove] Scale [") + m_name + "] is locked!");
		return;
	}

	m_steps.erase(m_steps.begin() + index);
	m_updated = false;

	if (autoUpdate)
		update();
}

void ccColorScale::sort()
{
	std::sort(m_steps.begin(), m_steps.end(), ccColorScaleElement::IsSmaller);
}

void ccColorScale::update()
{
	if (m_steps.size() >= (int)MIN_STEPS)
	{
		sort();

		unsigned stepCount = static_cast<unsigned>(m_steps.size());
		assert(stepCount>=2);
		assert(m_steps.front().getRelativePos() == 0.0);
		assert(m_steps.back().getRelativePos() == 1.0);
		if (m_steps.front().getRelativePos() != 0.0 || m_steps.back().getRelativePos() != 1.0)
		{
			//invalid scale: paint it black ;)
			memset(m_rgbaScale,0,sizeof(colorType)*4*MAX_STEPS);
			ccLog::Warning(std::string("[ccColorScale] Scale '") + getName() + "' is invalid! (boundaries are not [0.0-1.0]");
			return;
		}

		colorType* _scale = m_rgbaScale;

		unsigned j = 0; //current intervale
		for (unsigned i=0; i<MAX_STEPS; ++i)
		{
			double relativePos = (double)i/(double)(MAX_STEPS-1);

			//forward to the right intervale
			while (j+2 < stepCount && m_steps[j+1].getRelativePos() < relativePos)
				++j;

			// linear interpolation
			CCVector3d colBefore (	m_steps[j].getColor().r / 255.0,
									m_steps[j].getColor().g / 255.0,
									m_steps[j].getColor().b / 255.0 );
			CCVector3d colNext (	m_steps[j+1].getColor().r / 255.0,
									m_steps[j+1].getColor().g / 255.0,
									m_steps[j+1].getColor().b / 255.0 );

			//interpolation coef
			double alpha = (relativePos - m_steps[j].getRelativePos())/(m_steps[j+1].getRelativePos() - m_steps[j].getRelativePos());

			CCVector3d interpCol = colBefore + (colNext-colBefore) * alpha;

			*_scale++ = static_cast<colorType>(interpCol.x * (double)MAX_COLOR_COMP);
			*_scale++ = static_cast<colorType>(interpCol.y * (double)MAX_COLOR_COMP);
			*_scale++ = static_cast<colorType>(interpCol.z * (double)MAX_COLOR_COMP);
			*_scale++ = MAX_COLOR_COMP; //do not dream: no transparency ;)
		}

		m_updated = true;
	}
	else //invalid scale: paint it black ;)
	{
		ccLog::Warning(std::string("[ccColorScale] Scale '") + getName() + "' is invalid! (not enough elements)");
		memset(m_rgbaScale,0,sizeof(colorType)*4*MAX_STEPS);
	}
}

void ccColorScale::setAbsolute(double minVal, double maxVal)
{
	assert(maxVal >= minVal);

	m_relative = false;

	m_absoluteMinValue = minVal;
	m_absoluteRange = maxVal - minVal;

	//as 'm_absoluteRange' is used for division, we make sure it is not left to 0!
	m_absoluteRange = std::max(m_absoluteRange, 1e-12);
}

void ccColorScale::getAbsoluteBoundaries(double& minVal, double& maxVal) const
{
	minVal = m_absoluteMinValue;
	maxVal = m_absoluteMinValue + m_absoluteRange;
}
