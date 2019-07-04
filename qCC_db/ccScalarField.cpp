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

#include "ccScalarField.h"

//Local
#include "ccColorScalesManager.h"
#include "ccColorRampShader.h"
#include "ccLog.h"

//CCLib
#include <CCConst.h>

#include <algorithm>

using namespace CCLib;

//! Default number of classes for associated histogram
const unsigned MAX_HISTOGRAM_SIZE = 512;

ccScalarField::ccScalarField(const char* name/*=0*/)
	: ScalarField(name)
	, m_showNaNValuesInGrey(true)
	, m_symmetricalScale(false)
	, m_logScale(false)
	, m_alwaysShowZero(false)
	, m_colorScale(0)
	, m_colorRampSteps(0)
{
	setColorRampSteps(ccColorScale::DEFAULT_STEPS);
	setColorScale(ccColorScalesManager::GetUniqueInstance()->getDefaultScale(ccColorScalesManager::BGYR));
}

ScalarType ccScalarField::normalize(ScalarType d) const
{
	if (/*!ValidValue(d) || */!m_displayRange.isInRange(d)) //NaN values are also rejected by 'isInRange'!
		return (ScalarType)-1.0;

	//most probable path first!
	if (!m_logScale)
	{
		if (!m_symmetricalScale)
		{
			if (d <= m_saturationRange.start())
				return 0;
			else if (d >= m_saturationRange.stop())
				return (ScalarType)1.0;
			return (d - m_saturationRange.start()) / m_saturationRange.range();
		}
		else //symmetric scale
		{
			if (fabs(d) <= m_saturationRange.start())
				return (ScalarType)0.5;
			
			if (d >= 0)
			{
				if (d >= m_saturationRange.stop())
					return (ScalarType)1.0;
				return ((ScalarType)1.0 + (d - m_saturationRange.start()) / m_saturationRange.range()) / (ScalarType)2.0;
			}
			else
			{
				if (d <= -m_saturationRange.stop())
					return (ScalarType)0.0;
				return ((ScalarType)1.0 + (d + m_saturationRange.start()) / m_saturationRange.range()) / (ScalarType)2.0;
			}
		}
	}
	else //log scale
	{
        ScalarType dLog = log10(std::max( (ScalarType) fabs(d),(ScalarType)ZERO_TOLERANCE));
		if (dLog <= m_logSaturationRange.start())
			return 0;
		else if (dLog >= m_logSaturationRange.stop())
			return (ScalarType)1.0;
		return (dLog - m_logSaturationRange.start()) / m_logSaturationRange.range();
	}

	//can't get here normally!
	assert(false);
	return (ScalarType)-1.0;
}

void ccScalarField::setColorScale(ccColorScale::Shared scale)
{
	if (m_colorScale != scale)
	{
		bool wasAbsolute = (m_colorScale && !m_colorScale->isRelative());
		bool isAbsolute = (scale && !scale->isRelative());

		m_colorScale = scale;

		if (isAbsolute)
			m_symmetricalScale = false;

		if (isAbsolute || wasAbsolute != isAbsolute)
			updateSaturationBounds();
	}
}

void ccScalarField::setSymmetricalScale(bool state)
{
	if (m_symmetricalScale != state)
	{
		m_symmetricalScale = state;
		updateSaturationBounds();
	}
}

void ccScalarField::setLogScale(bool state)
{
	if (m_logScale != state)
	{
		m_logScale = state;
		if (m_logScale && m_minVal < 0)
		{
			ccLog::Warning("[ccScalarField] Scalar field contains negative values! Log scale will only consider absolute values...");
		}
	}
}

void ccScalarField::computeMinAndMax()
{
	ScalarField::computeMinAndMax();

	m_displayRange.setBounds(m_minVal,m_maxVal);

	//update histogram
	{
		if (m_displayRange.maxRange() == 0 || currentSize() == 0)
		{
			//can't build histogram of a flat field
			m_histogram.clear();
		}
		else
		{
			unsigned count = currentSize();
			unsigned numberOfClasses = (unsigned)ceil(sqrt((double)count));
			numberOfClasses = std::max<unsigned>(std::min<unsigned>(numberOfClasses,MAX_HISTOGRAM_SIZE),4);

			m_histogram.maxValue = 0;

			//reserve memory
			try
			{
				m_histogram.resize(numberOfClasses);
			}
			catch(std::bad_alloc)
			{
				ccLog::Warning("[ccScalarField::computeMinAndMax] Failed to update associated histogram!");
				m_histogram.clear();
			}

			if (!m_histogram.empty())
			{
				std::fill(m_histogram.begin(),m_histogram.end(),0);

				//compute histogram
				{
					for (unsigned i=0; i<count; ++i)
					{
						const ScalarType& val = getValue(i);

						unsigned bin = static_cast<unsigned>(floor((val-m_displayRange.min())*(ScalarType)numberOfClasses/m_displayRange.maxRange()));
						++m_histogram[std::min(bin,numberOfClasses-1)];
					}
				}

				//update 'maxValue'
				m_histogram.maxValue = *std::max_element(m_histogram.begin(),m_histogram.end());
			}
		}
	}

	updateSaturationBounds();
}

void ccScalarField::updateSaturationBounds()
{
	if (!m_colorScale || m_colorScale->isRelative()) //Relative scale (default)
	{
		ScalarType minAbsVal = ( m_maxVal < 0 ? std::min(-m_maxVal,-m_minVal) : std::max<ScalarType>(m_minVal,0) );
		ScalarType maxAbsVal = std::max(fabs(m_minVal),fabs(m_maxVal));

		if (m_symmetricalScale)
		{
			m_saturationRange.setBounds(minAbsVal,maxAbsVal);
		}
		else
		{
			m_saturationRange.setBounds(m_minVal,m_maxVal);
		}

		//log scale (we always update it even if m_logScale is not enabled!)
		//if (m_logScale)
		{
			ScalarType minSatLog = log10(std::max(minAbsVal,(ScalarType)ZERO_TOLERANCE));
			ScalarType maxSatLog = log10(std::max(maxAbsVal,(ScalarType)ZERO_TOLERANCE));
			m_logSaturationRange.setBounds(minSatLog,maxSatLog);
		}
	}
	else //absolute scale
	{
		//DGM: same formulas as for the 'relative scale' case but we use the boundaries
		//defined by the scale itself instead of the current SF boundaries...
		double minVal=0, maxVal=0;
		m_colorScale->getAbsoluteBoundaries(minVal,maxVal);

		m_saturationRange.setBounds(minVal,maxVal);

		//log scale (we always update it even if m_logScale is not enabled!)
		//if (m_logScale)
		{
			ScalarType minAbsVal = ( maxVal < 0 ? std::min(-maxVal,-minVal) : std::max<ScalarType>(minVal,0) );
			ScalarType maxAbsVal = (ScalarType)std::max(fabs(minVal),fabs(maxVal));
			ScalarType minSatLog = log10(std::max(minAbsVal,(ScalarType)ZERO_TOLERANCE));
			ScalarType maxSatLog = log10(std::max(maxAbsVal,(ScalarType)ZERO_TOLERANCE));
			m_logSaturationRange.setBounds(minSatLog,maxSatLog);
		}
	}
}

void ccScalarField::setSaturationStart(ScalarType val)
{
	if (m_logScale)
	{
		m_logSaturationRange.setStart(val/*log10(std::max(val,(ScalarType)ZERO_TOLERANCE))*/);
	}
	else
	{
		m_saturationRange.setStart(val);
	}
}

void ccScalarField::setSaturationStop(ScalarType val)
{
	if (m_logScale)
	{
		m_logSaturationRange.setStop(val/*log10(std::max(val,(ScalarType)ZERO_TOLERANCE))*/);
	}
	else
	{
		m_saturationRange.setStop(val);
	}
}

void ccScalarField::setColorRampSteps(unsigned steps)
{
	if (steps > ccColorScale::MAX_STEPS)
		m_colorRampSteps = ccColorScale::MAX_STEPS;
	else if (steps < ccColorScale::MIN_STEPS)
        m_colorRampSteps = ccColorScale::MIN_STEPS;
    else
        m_colorRampSteps = steps;
}
