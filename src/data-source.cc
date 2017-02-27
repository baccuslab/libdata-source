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
QByteArray serialize(const QString& param, const QVariant& value)
{
	QByteArray buffer;
	if ( (param == "trigger") ||
			(param == "connect-time") ||
			(param == "start-time") ||
			(param == "source-type") ||
			(param == "device-type") ||
			(param == "state") ||
			(param == "location") ||
			(param == "configuration-file") ){
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

QVariant deserialize(const QString& param, const QByteArray& buffer)
{
	QVariant data;
	if ( (param == "trigger") ||
			(param == "connect-time") ||
			(param == "start-time") ||
			(param == "source-type") ||
			(param == "device-type") ||
			(param == "state") ||
			(param == "location") ||
			(param == "configuration-file") ){
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

}; // end namespace

