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
BaseSource* LIBDATA_SOURCE_VISIBILITY create(const QString& type, const QString& location, 
		int readInterval = 10);

/*! Serialize a parameter to raw bytes.
 *
 * \param param The name of the parameter to be serialized.
 * \param value The data for the parameter.
 *
 * \note This is intended to be used by the BLDS application to communicate
 * with remote clients.
 *
 * \note This method is defined in this header file, rather than
 * the src/data-source.cc implementation file, so that client code
 * need only include this file and not require linking to the
 * actual shared library if they only want serialization functionality.
 */
QByteArray serialize(const QString& param, const QVariant& value)
{
	QByteArray buffer;
	if ( (param == "trigger") ||
			(param == "connect-time") ||
			(param == "start-time") ||
			(param == "source-type") ||
			(param == "device-type") ||
			(param == "state") ||
			(param == "location") ){
		/* String, serialized as UTF8 byte array. */
		buffer = value.toByteArray();
	} else if ( (param == "nchannels") ||
			(param == "plug") ||
			(param == "chip-id") ||
			(param == "read-interval") ){
		/* Unsigned integer types, serialized as uint32_t. */
		quint32 x = value.toUInt();
		buffer.resize(sizeof(x));
		std::memcpy(buffer.data(), &x, sizeof(x));
	} else if (param == "has-analog-output") {
		/* Boolean */
		buffer.resize(sizeof(bool));
		auto val = value.toBool();
		std::memcpy(buffer.data(), &val, sizeof(bool));
	} else if (param == "analog-output") {
		/* Analog output is QVector<double>, serialize size as
		 * uint32_t followed by raw samples.
		 */
		auto aout = value.value<QVector<double>>();
		quint32 size = aout.size();
		buffer.resize(sizeof(size) + sizeof(double) * size);
		std::memcpy(buffer.data(), &size, sizeof(size));
		std::memcpy(buffer.data() + sizeof(size), aout.data(), size * sizeof(double));
	} else if ( (param == "gain") ||
			(param == "adc-range") ||
			(param == "sample-rate") ){
		/* Floating point types. */
		float x = value.toFloat();
		buffer.resize(sizeof(x));
		std::memcpy(buffer.data(), &x, sizeof(x));
	} else if (param == "configuration") {
		/* Configuration is a vector of Electrode structs. Serialize
		 * size as uint32_t followed by raw data.
		 */
		auto config = value.value<QConfiguration>();
		quint32 size = config.size();
		buffer.resize(sizeof(size) + size * sizeof(Electrode));
		std::memcpy(buffer.data(), &size, sizeof(size));
		std::memcpy(buffer.data() + sizeof(size), config.data(), 
				size * sizeof(Electrode));
	}
	return buffer;
}
/*! Deserialize a parameter from raw bytes.
 * 
 * \param param The name of the parameter to be deserialized.
 * \param buffer The raw bytes in which the parameter is encoded.
 *
 * \note This is intended to be used by the BLDS application to communicate
 * with remote clients.
 *
 * \note This method is defined in this header file, rather than
 * the src/data-source.cc implementation file, so that client code
 * need only include this file and not require linking to the
 * actual shared library if they only want serialization functionality.
 */
QVariant deserialize(const QString& param, const QByteArray& buffer)
{
	QVariant data;
	if ( (param == "trigger") ||
			(param == "connect-time") ||
			(param == "start-time") ||
			(param == "source-type") ||
			(param == "device-type") ||
			(param == "state") ||
			(param == "location") ){
		data = buffer;
	} else if ( (param == "nchannels") ||
			(param == "plug") ||
			(param == "chip-id") ||
			(param == "read-interval") ){
		quint32 x = 0;
		std::memcpy(&x, buffer.data(), sizeof(x));
		data = x;
	} else if (param == "analog-output") {
		quint32 size = 0;
		std::memcpy(&size, buffer.data(), sizeof(size));
		QVector<double> aout(size);
		std::memcpy(aout.data(), buffer.data() + sizeof(size), 
				size * sizeof(double));
		data = QVariant::fromValue<decltype(aout)>(aout);
	} else if ( (param == "gain") ||
			(param == "adc-range") ||
			(param == "sample-rate") ){
		float x = 0.0;
		std::memcpy(&x, buffer.data(), sizeof(x));
		data = x;
	} else if (param == "configuration") {
		quint32 size = 0;
		std::memcpy(&size, buffer.data(), sizeof(size));
		QConfiguration config(size);
		std::memcpy(config.data(), buffer.data() + sizeof(size),
				size * sizeof(Electrode));
		data = QVariant::fromValue<decltype(config)>(config);
	}
	return data;
}

}; // end datasource namespace

#endif

