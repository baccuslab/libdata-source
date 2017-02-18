/*! \file mcs-source.cc
 *
 * Implementation of data source subclass managing an MCS array,
 * through the NIDAQmx library.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#include "mcs-source.h"

namespace datasource {
	
McsSource::McsSource(QObject *parent) :
	BaseSource("device", "mcs", 10000., 10, parent)
#ifndef Q_OS_WIN
{
}
#else
	,
	m_nchannels(64),
	m_acquisitionBlockSize(
			static_cast<float>(m_readInterval) * sampleRate / 1000.),
	m_acquisitionBufferSize(m_acquisitionBlockSize * m_nchannels);
{
	m_settableParameters.insert("analog-output");
	m_gettableParameters.insert("analog-output");
	m_settableParameters.insert("adc-range");
	m_gettableParameters.insert("adc-range");
	m_settableParameters.insert("trigger");
	m_gettableParameters.insert("trigger");
}
#endif

McsSource::~McsSource()
#ifndef Q_OS_WIN
{
}
#else
{
}
#endif

}; // end datasource namespace
