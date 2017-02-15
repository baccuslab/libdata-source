/*! \file configuration.h
 *
 * Header describing a single electrode on the HiDens system
 * and a simple type alias for a configuration as a list of such
 * Electrode structs
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef LIBMEA_DEVICE_CONFIGURATION_H_
#define LIBMEA_DEVICE_CONFIGURATION_H_

#include <QtCore>

#include <vector>
#include <ostream>

/*! A single HiDens chip electrode */
struct Electrode {

	public:
		Electrode() :
			index(0),
			xpos(0),
			x(0),
			ypos(0),
			y(0),
			label(0) 
		{ }

		Electrode(quint32 index, quint32 xpos, quint16 x, 
				quint32 ypos, quint16 y, quint8 label) :
			index(index),
			xpos(xpos),
			x(x),
			ypos(ypos),
			y(y),
			label(label)
		{ }

		Electrode& operator=(Electrode other)
		{
			swap(*this, other);
			return *this;
		}

		friend void swap(Electrode& first, Electrode& second)
		{
			using std::swap;
			swap(first.index, second.index);
			swap(first.xpos, second.xpos);
			swap(first.x, second.x);
			swap(first.ypos, second.ypos);
			swap(first.y, second.y);
			swap(first.label, second.label);
		}

		bool operator==(const Electrode& other) const {
			return (index == other.index);
		}

		/*! The index number of the electrode on the HiDens chip */
		quint32 index;

		/*! The x-position on the chip, in microns */
		quint32 xpos;

		/*! The x-index on the chip. */
		quint16 x;

		/*! The y-position on the chip, in microns */
		quint32 ypos;

		/*! The y-index on the chip. */
		quint16 y;

		/*! A character label, used by the internal wiring of the HiDens system */
		quint8 label;

		/*! Encode an electrode to a JSON array */
		QJsonArray toJson() const {
			return QJsonArray {
					static_cast<qint64>(index),
					static_cast<qint64>(xpos),
					static_cast<qint64>(x),
					static_cast<qint64>(ypos),
					static_cast<qint64>(y),
					static_cast<qint64>(label),
			};
		}

		/*! Encode an electrode into a QVariant */
		QVariant toVariant() const {
			return QVariant::fromValue<Electrode>(*this);
		}
};

/*! A Qt-based configuration */
using QConfiguration = QVector<Electrode>;

/*! A STL configuration */
using Configuration = std::vector<Electrode>;

/*! Encode a QConfiguration to a JSON array */
inline QJsonArray configToJson(const QConfiguration& c) {
	QJsonArray config;
	for (auto& el : c) {
		config.append(el.toJson());
	}
	return config;
}

/*! Encode a QConfiguration as a QVector<Electrode> into a QVariant */
inline QVariant configToVariant(const QConfiguration& c) {
	return QVariant::fromValue<QConfiguration>(c);
}

/*! Print to a QDebug stream */
inline QDebug& operator<<(QDebug& stream, const Electrode& el) {
	return stream << "[ " << el.index << ", " << el.xpos << ", " << 
		el.x << ", " << el.ypos << ", " << el.y << " ]";
}

/*! Print to stdout/stderr */
inline std::ostream& operator<<(std::ostream& stream, const Electrode& el) {
	return stream << "[ " << el.index << ", " << el.xpos << ", " << 
		el.x << ", " << el.ypos << ", " << el.y << " ]";
}

/*! Write an Electrode to a QDataStream object.
 * This is used for writing a configuration to a remote client, for example.
 */
inline QDataStream& operator<<(QDataStream& stream, const Electrode& el)
{
	return stream << el.index << el.xpos << el.x << el.ypos << el.y << el.label;
}

/*! Write a full configuration to a QDataStream object.
 * This is used for writing a configuration to a remote client, for example.
 */
inline QDataStream& operator<<(QDataStream& stream, const QConfiguration& config)
{
	stream << static_cast<quint32>(config.size());
	for (auto& el : config)
		stream << el;
	return stream;
}

/*! Read an electrode from a QDataStream object. */
inline QDataStream& operator>>(QDataStream& stream, Electrode& el)
{
	return stream >> el.index >> el.xpos >> el.x >> el.ypos >> el.y >> el.label;
}

/*! Read a full configuration from a QDataStream object. */
inline QDataStream& operator>>(QDataStream& stream, QConfiguration& config)
{
	quint32 size;
	stream >> size;
	config.resize(size);
	for (auto& el : config)
		stream >> el;
	return stream;
}

Q_DECLARE_METATYPE(Electrode)
Q_DECLARE_METATYPE(QConfiguration)

#endif

