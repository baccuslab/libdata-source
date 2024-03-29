/*! \file base-source.h
 *
 * Description of base class for all data source objects. This should
 * not be used directly, but treated used as an interface, describing the 
 * public API for all data sources.
 *
 * The BaseSource class is designed to (but need not) live in a separate thread
 * from the main class managing it. As such, all state changes, parameter
 * changes, queries, etc. are to be done in a request-reply pattern. For example,
 * client code emits a request to set a parameter of the data source, and
 * receives a response to indicate whether the request succeeded.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef BASE_SOURCE_H_
#define BASE_SOURCE_H_

#ifdef COMPILE_LIBDATA_SOURCE
# define LIBDATA_SOURCE_VISIBILITY Q_DECL_EXPORT
#else
# define LIBDATA_SOURCE_VISIBILITY Q_DECL_IMPORT
#endif

#include "configuration.h"

#include <armadillo>
#include <QtCore>

namespace datasource {
	
/*! Type alias for a single frame of data.
 * This is declared inside its own namespace because the
 * Q_DECLARE_METATYPE macro must have the fully-qualified
 * name, but that macro itself must appear in the global
 * namespace.
 */
using Samples = arma::Mat<qint16>;
};

Q_DECLARE_METATYPE(datasource::Samples);

namespace datasource {

/*! \class BaseSource
 * The BaseSource class is the base class for all data sources in the Baccus Lab.
 * The class is not abstract, but should not be directly instantiated. It defines
 * a consistent API which all subclasses should use. This allows client code to
 * query and manipulate the device, and to retrieve data from it when it becomes
 * available.
 *
 * The source is designed to be, but need not be, placed in a background thread.
 * Qt's event-based system means that I/O and related methods should not block,
 * but it still may be useful to place the source in another thread, for example,
 * if the source object processes data as it is received.
 *
 * Because of this, the source object should be communicated with via signals and
 * slots. These are a request/reply pattern, in which the caller emits a signal
 * to request the source perform an action (e.g., setting a parameter), the source
 * tries to do so, and emits another signal indicating the result of that action.
 */
class LIBDATA_SOURCE_VISIBILITY BaseSource : public QObject {
	Q_OBJECT

	public:

		/*! Construct a BaseSource object.
		 * \param sourceType The type of source represented, i.e., "file" or "device".
		 * \param deviceType The type of the MEA device, e.g., "hidens" or "mcs".
		 * \param readInterval The interval (in ms) between reading chunks from the source.
		 * \param sampleRate Sampling rate of the data.
		 * \param parent The source's parent.
		 *
		 * The constructor stores the parameters in the appopriate data
		 * methods and computes any values based on them (e.g., the size of
		 * a frame of data), but otherwise performs no initialization.
		 */
		BaseSource(QString sourceType = "none", QString deviceType = "none", 
				int readInterval = 10, float sampleRate = qSNaN(),
				QObject *parent = Q_NULLPTR) :
			QObject(parent), 
			m_state("invalid"),
			m_sourceType(sourceType),
			m_deviceType(deviceType),
			m_startTime(QDateTime{}),
			m_configuration({}),
			m_readInterval(readInterval),
			m_sampleRate(sampleRate),
			m_frameSize(static_cast<int>(
						static_cast<float>(readInterval) * sampleRate / 1000)),
			m_gain(qSNaN()),
			m_adcRange(qSNaN()),
			m_nchannels(0),
			m_plug(-1),
			m_chipId(-1),
			m_trigger("none"),
			m_analogOutput({})
		{ 
			qRegisterMetaType<datasource::Samples>();
			m_gettableParameters = {
						"start-time",
						"state",
						"nchannels",
						"has-analog-output",
						"gain",
						"adc-range",
						"read-interval",
						"sample-rate",
						"source-type",
						"device-type"
					};
			m_settableParameters = {};
		}

		/*! Destroy a BaseSource object. */
		virtual ~BaseSource() { }

		BaseSource(const BaseSource&) = delete;
		BaseSource(BaseSource&&) = delete;
		BaseSource& operator=(const BaseSource&) = delete;

		/*! Return the interval in milliseconds between reads from the source. */
		int readInterval() const { return m_readInterval; }

	public:
		/*! Return a string representing the type of this source, e.g., "file" or "device". */
		const QString& sourceType() const { return m_sourceType; }

		/*! Return a string representing the type of the underlying
		 * data device represented by this source object, e.g., "hidens" or "mcs".
		 */
		const QString& deviceType() const { return m_deviceType; }

	public slots:

		/*! Perform any initialization setup needed before the source
		 * may be used by client code.
		 */
		virtual void initialize() {
			bool success;
			QString msg;
			if (m_state == "invalid") {
				m_state = "initialized";
				success = true;
			} else {
				success = false;
				msg = "Can only 'initialize' from 'invalid' state.";
			}
			emit initialized(success, msg);
		}

		/*! Start the data stream associated with the source.
		 *
		 * Subclasses should override this to implement the behavior desired to
		 * actually start the data stream for the source. If successful, this should
		 * make sure that the dataAvailable() signal will be emitted as new data
		 * becomes available.
		 */
		virtual void startStream() { 
			bool success;
			QString msg;
			if (m_state == "initialized") {
				m_state = "streaming";
				success = true;
			} else {
				success = false;
				msg = "Can only start stream from the 'initialized' state.";
			}
			emit streamStarted(success, msg);
		}

		/*! Stop the data stream associated with the source.
		 *
		 * Subclasses should override this to implement the behavior desired to
		 * actually stop the data stream for the source. If successful, this should
		 * make sure that the dataAvailable() signal should no longer be emitted as
		 * data becomes available.
		 */
		virtual void stopStream() { 
			bool success;
			QString msg;
			if (m_state == "streaming") {
				m_state = "initialized";
				success = true;
			} else {
				success = false;
				msg = "Can only stop stream from the 'streaming' state.";
			}
			emit streamStopped(success, msg);
		}

		/*! Attempt to set a named parameter of the source.
		 * \param param The name of the parameter to be set.
		 * \param value The underlying data representing the desired value of the parameter.
		 *
		 * Subclass overrides of this function should emit the setResponse() signal
		 * indicating whether the request to set the parameter was successful, with an
		 * error message indicating if not.
		 */
		virtual void set(QString param, QVariant /* value */) { 
			emit setResponse(param, false, "Base class implementation!");
		}

		/*! Attempt to get a named parameter.
		 * \param The name of the parameter to be retrieved.
		 *
		 * Subclass overrides of this function should not be needed. Instead of 
		 * overriding this function, in the constructor for new subclasses,
		 * define the parameters that are valid to get() for the class by
		 * storing them inside the member variable m_gettableParameters.
		 */
		virtual void get(QString param) { 
			bool valid = true;
			QVariant data;

			if (m_state == "invalid") {
				valid = false;
				data = QString("Can only get parameters in either "
						"'initialized' or 'streaming' state.");
			} else {
				if (!m_gettableParameters.contains(param)) {
					emit getResponse(param, false, 
							QString("The parameter \"%1\" is not valid "
							"for source %2").arg(param).arg(metaObject()->className()));
					return;
				}
				if (param == "trigger") {
					data = m_trigger;
				} else if (param == "connect-time") {
					data = m_connectTime.toString();
				} else if (param == "start-time") {
					data = m_startTime.toString();
				} else if (param == "state") {
					data = m_state;
				} else if (param == "nchannels") {
					data = m_nchannels;
				} else if (param == "analog-output") {
					data = QVariant::fromValue<QVector<double>>(m_analogOutput);
				} else if (param == "has-analog-output") {
					data = m_analogOutput.size() > 0;
				} else if (param == "gain") {
					data = m_gain;
				} else if (param == "adc-range") {
					data = m_adcRange;
				} else if (param == "plug") {
					data = m_plug;
				} else if (param == "chip-id") {
					data = m_chipId;
				} else if (param == "read-interval") {
					data = m_readInterval;
				} else if (param == "sample-rate") {
					data = m_sampleRate;
				} else if (param == "source-type") {
					data = m_sourceType;
				} else if (param == "device-type") {
					data = m_deviceType;
				} else if (param == "configuration") {
					data = QVariant::fromValue<QConfiguration>(m_configuration);
				} else if (param == "configuration-file") {
					data = m_configurationFile;
				} else if (param == "location") {
					data = m_sourceLocation;
				} else {
					valid = false;
					data = QString("No parameter named \"%1\" exists for the %2 device").
							arg(param).arg(m_deviceType);
				}
			}
			emit getResponse(param, valid, data);
		}

		/*! Pack the source's status information into a JSON object and emit
		 * it as a signal. This should be done in response to requests for
		 * the current status of the device.
		 */
		virtual void requestStatus() {
			emit status(packStatus());
		}

	signals:

		/*! Emitted in response to a request to set a parameter.
		 * \param param The name of the parameter requested to be set.
		 * \param success True if the parameter was successfully set, false otherwise.
		 * \param msg If the set request failed, this contains a message explaining why.
		 */
		void setResponse(QString param, bool success, QString msg = QString());

		/*! Emitted in response to a request to get a parameter.
		 * \param param The name of the parameter requested.
		 * \param valid True if the parameter exists for this source and 
		 * 	was successfully retrieved.
		 * \param data If the get request succeeded, this contains the data corresponding
		 * 	to the parameter. If the request failed, this contains an error message explaining
		 * 	why the request failed.
		 */
		void getResponse(QString param, bool valid, QVariant data = QVariant());

		/*! Emitted after the source has performed initialization needed and 
		 * 	ready to be used by client code.
		 * \param success True if the request succeeded.
		 * \param msg If the request failed, this contains an error message explaining why.
		 */
		void initialized(bool success, QString msg = QString());

		/*! Emitted in response to a request to start the source's data stream.
		 * \param success True if the request succeeded.
		 * \param msg If the request failed, this contains an error message explaining why.
		 */
		void streamStarted(bool success, QString msg = QString());

		/*! Emitted in response to a request to stop the source's data stream.
		 * \param success True if the request succeeded.
		 * \param msg If the request failed, this contains an error message explaining why.
		 */
		void streamStopped(bool success, QString msg = QString());

		/*! Emitted in response to a request for the full status of the source.
		 * \param status A map containing the key-value pairs for parameters and values.
		 */
		void status(QVariantMap status);

		/*! Emitted when new data is available from the source.
		 * \param samples An Armadillo matrix containing the new data. This is shaped as
		 * (nchannels, nsamples). Because Armadillo uses column-major ordering, the raw
		 * data is laid out with all samples from a single channel, followed by the next
		 * channel, etc.
		 */
		void dataAvailable(datasource::Samples samples);

		/*! Emitted when an error occurs on the source.
		 *
		 * This may happen if the source is unexpectedly disconnected, disappears, is
		 * unplugged, etc.
		 *
		 * \param msg Contains the error message explaining the error.
		 */
		void error(QString msg = QString());

	protected:

		/*! Pack all parameters indicating the status of the source into a map. */
		virtual QVariantMap packStatus() {
			return {
					{"state", m_state},
					{"source-type", m_sourceType},
					{"device-type", m_deviceType},
					{"start-time", m_startTime.toString()},
					{"read-interval", m_readInterval},
					{"sample-rate", m_sampleRate},
					{"gain", m_gain},
					{"adc-range", m_adcRange},
					{"nchannels", m_nchannels},
					{"has-analog-output", false},
					{"source-location", m_sourceLocation}
			};
		}

		/*! Deal with an error from the source.
		 *
		 * Subclasses should override this to define what must happen
		 * when an error occurs with the device. At a minimum, the 
		 * subclass should reset itself in some way, i.e., close network
		 * connections or files.
		 *
		 * Subclass overrides MUST emit the error() signal with an 
		 * appropriate message, or call this base class implementation,
		 * which emits the signal.
		 */
		virtual void handleError(const QString& message)
		{
			m_state = "invalid";
			m_startTime = QDateTime{};
			m_configuration = {};
			m_gain = qSNaN();
			m_adcRange = qSNaN();
			m_nchannels = 0;
			m_plug = -1;
			m_chipId = -1;
			m_trigger = "none";
			m_analogOutput = {};
			emit error(message);
		}

		/*! Current state of the source. */
		QString m_state;

		/*! Type of source, i.e., "file", "mcs", or "hidens". */
		QString m_sourceType;

		/*! The MEA device from which the data originated. This the type of
		 * the actual MEA on which the data was recorded, e.g., "hexagonal"
		 * or "hidens".
		 */
		QString m_deviceType;

		/*! The time at which the connection to the source was made. */
		QDateTime m_connectTime;

		/*! The time at which the data stream was started. */
		QDateTime m_startTime;

		/*! The configuration of the array, if this is a HiDens type. */
		QConfiguration m_configuration;

		/*! A file describing the configuration, to be sent to the chip. */
		QString m_configurationFile;

		/*! Any error messages */
		QString m_error;

		/*! Interval (in ms) between reading data from the source.
		 * This, with the sampling rate, defines the size of a chunk of
		 * data from the source.
		 */
		int m_readInterval;

		/*! Sampling rate of data from the source. */
		float m_sampleRate;

		/*! Size in samples of a single frame of data. */
		int m_frameSize;

		/*! Gain of the ADC conversion of the underlying MEA. */
		float m_gain;

		/*! Voltage range of the ADC of the underlying MEA. */
		float m_adcRange;

		/*! Number of data channels in the stream. */
		quint32 m_nchannels;

		/*! Neurolizer plug number for HiDens data sources. */
		quint32 m_plug;

		/*! ID number of the HiDens chip. */
		quint32 m_chipId;

		/*! Mechanism for triggering the start of the data stream,
		 * e.g., "photodiode" or "none".
		 */
		QString m_trigger;

		/*! Any analog output for the recording. */
		QVector<double> m_analogOutput;

		/*! Set of parameters that are valid in a get() call */
		QSet<QString> m_gettableParameters;

		/*! Set of parameters that can be set for this data source. */
		QSet<QString> m_settableParameters;

		/*! String identifier for this source. For subclasses, this can be
		 * used to identify the location, such as a filename or a remote hostname.
		 */
		QString m_sourceLocation;
};

}; // end datasource namespace

#endif
