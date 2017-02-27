/*! \file data-source.cc
 *
 * Implementation of main public routines in the datasource namespace.
 *
 * (C) 2017 Benjamin Naecker bnaecker@stanford.edu
 */

#include "data-source.h"

namespace datasource {

BaseSource* create(const QString& type, const QString& location, int readInterval)
{
	if (type == "mcs") {
#ifdef Q_OS_WIN
		return new McsSource(readInterval);
#else
		throw std::invalid_argument("MCS sources can only be created on Windows machines.");
#endif
	} else if (type == "hidens") {
		return new HidensSource(location, readInterval);
	} else if (type == "file") {
		return new FileSource(location, readInterval);
	} else {
		throw std::invalid_argument("Unknown source type: " + type.toStdString());
	}
}

}; // end namespace

