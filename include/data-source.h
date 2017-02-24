/*! \file data-source.h
 *
 * General include file for libdata-source.
 *
 * (C) 2017 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef DATA_SOURCE_H
#define DATA_SOURCE_H

#include "base-source.h"
#include "file-source.h"
#include "mcs-source.h"
#include "hidens-source.h"

#include <QtCore>

namespace datasource {

/*! Factory method to create a source type from its name and a location.
 *
 * \param type The type of source to create.
 * \param location The location identifier for the source.
 * \param readInterval The interval at which data is retrieved from the source,
 * 	in milliseconds.
 *
 * This method will throw an std::invalid_argument if either the requested
 * type is unknown or if the source could  not be created for some reason
 * (e.g., 'mcs' sources on non-Windows machines).
 */
BaseSource* VISIBILITY create(const QString& type, const QString& location, 
		int readInterval = 10);

/*! Serialize a parameter to raw bytes.
 *
 * \param param The name of the parameter to be serialized.
 * \param data The data for the parameter.
 *
 * This is intended to be used by the BLDS application to communicate
 * with remote clients.
 */
QByteArray VISIBILITY serialize(const QString& param, const QVariant& data);

/*! Deserialize a parameter from raw bytes.
 * 
 * \param param The name of the parameter to be deserialized.
 * \param buffer The raw bytes in which the parameter is encoded.
 *
 * This is intended to be used by the BLDS application to communicate
 * with remote clients.
 */
QVariant VISIBILITY deserialize(const QString& param, const QByteArray& buffer);

}; // end datasource namespace

#endif

