/*! \file base-source.h
 *
 * Description of base class for all data source objects. This should
 * not be used directly, but treated used as an interface, describing the 
 * public API for all data sources.
 *
 * The BaseSource class is designed to (but need not) live in a separate thread
 * from the main class managing it. As such, all state changes, parameter
 * changes, queries, etc. are to be done in a request-reply pattern. Client
 * code emits a request to connect to the underlying data source, the class
 * checks if it can be done, does it if it's possible, and then emits a 
 * signal indicating that the connection happened.
 *
 * The base class defines this interface for requests/replies, and implements
 * the basic mechanism for checking state transitions by ensuring that 
 * the correct source and destination states are specified. That is, the
 * base class enforces that client code can only start the device's stream
 * from the connected state, for example. Subclasses should override this
 * to enforce more specific requirements, for example, that certain parameters
 * have been set before the transition is allowed.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef BASE_SOURCE_H_
#define BASE_SOURCE_H_

#include "configuration.h"

#include <armadillo>
#include <QtCore>

/*! Type alias for a single frame of data. */
using Samples = arma::Mat<qint16>;
Q_DECLARE_METATYPE(Samples);

class BaseSource : public QObject {
	Q_OBJECT

	public:

		/*! Construct a BaseSource object.
		 * \param sourceType The type of source represented, i.e., "file" or "device".
		 * \param deviceType The type of the MEA device, e.g., "hidens" or "mcs".
		 * \param sampleRate Sampling rate of the data.
		 * \param readInterval The interval (in ms) between reading chunks from the source.
		 * \param parent The source's parent.
		 *
		 * The constructor stores the parameters in the appopriate data
		 * methods and computes any values based on them (e.g., the size of
		 * a frame of data), but otherwise performs no initialization.
		 */
		BaseSource(QString sourceType = "none", 
				QString deviceType = "none", float sampleRate = qSNaN(), 
				int readInterval = 10, QObject *parent = Q_NULLPTR) :
			QObject(parent), 
			m_state("disconnected"),
			m_sourceType(sourceType),
			m_deviceType(deviceType),
			m_connectTime(QDateTime{}),
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
			qRegisterMetaType<Samples>();
			m_gettableParameters = {
						"connect-time",
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

		/*! Connect to the device.
		 * The base class implementation checks that the state transition is valid,
		 * and emits the connected() signal with information about whether the connection
		 * succeeded.
		 *
		 * Subclasses should override this to implement the behavior desired to
		 * initialize a connection with the underlying data source.
		 */
		virtual void connect() {
			bool success = checkStateTransition("disconnected", "connected");
			QString msg;
			if (success) {
				m_connectTime = QDateTime::currentDateTime();
				m_state = "connected";
			} else {
				msg = "Can only connect from 'disconnected' state.";
			}
			emit connected(success, msg);
		}

		/*! Disconnect from the device.
		 * The base class implementation checks that the state transition is valid,
		 * and emits the disconnected() signal with information about whether the 
		 * disconnection succeeded.
		 *
		 * Subclasses should override this to implement the behavior desired to
		 * destroy a connection to the underlying data source.
		 */
		virtual void disconnect() { 
			bool success = checkStateTransition("connected", "disconnected");
			QString msg;
			if (success) {
				m_connectTime = QDateTime{};
				m_state = "disconnected";
			} else {
				msg = "Can only disconnect from 'connected' state.";
			}
			emit disconnected(success, msg);
		}

		/*! Start the data stream associated with the source.
		 * The base class implementation checks that the state transition is valid,
		 * and emits the streamStarted() signal with information about whether starting
		 * the stream succeeded.
		 *
		 * Subclasses should override this to implement the behavior desired to
		 * actually start the data stream for the source. If successful, this should
		 * make sure that the dataAvailable() signal will be emitted as new data
		 * becomes available.
		 */
		virtual void startStream() { 
			bool success = checkStateTransition("connected", "streaming");
			QString msg;
			if (success) {
				m_state = "streaming";
			} else {
				msg = "Can only start stream from 'connected' state.";
			}
			emit streamStarted(success, msg);
		}

		/*! Stop the data stream associated with the source.
		 * The base class implementation checks that the state transition is valid,
		 * and emits the streamStopped() signal with information about whether stopping
		 * the stream succeeded.
		 *
		 * Subclasses should override this to implement the behavior desired to
		 * actually stop the data stream for the source. If successful, this should
		 * make sure that the dataAvailable() signal should no longer be emitted as
		 * data becomes available.
		 */
		virtual void stopStream() { 
			bool success = checkStateTransition("streaming", "connected");
			QString msg;
			if (success) {
				m_state = "connected";
			} else {
				msg = "Can only stop stream from 'streaming' state.";
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
			} else {
				valid = false;
				data = QString("No parameter named \"%1\" exists for the %2 device").
						arg(param).arg(m_deviceType);
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
		 * \param valid True if the parameter exists for this source and was successfully retrieved.
		 * \param data If the get request succeeded, this contains the data corresponding
		 * to the parameter. If the request failed, this contains an error message explaining
		 * why the request failed.
		 */
		void getResponse(QString param, bool valid, QVariant data = QVariant());

		/*! Emitted in response to a request to connect to the source.
		 * \param success True if the request succeeded.
		 * \param msg If the request failed, this contains an error message explaining why.
		 */
		void connected(bool success, QString msg = QString());

		/*! Emitted in response to a request to disconnect from the source.
		 * \param success True if the request succeeded.
		 * \param msg If the request failed, this contains an error message explaining why.
		 */
		void disconnected(bool success, QString msg = QString());

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
		 * \param status A JSON object containing the full status.
		 */
		void status(QJsonObject status);

		/*! Emitted when new data is available from the source.
		 * \param samples An Armadillo matrix containing the new data. This is shaped as
		 * (nchannels, nsamples). Because Armadillo uses column-major ordering, the raw
		 * data is laid out with all samples from a single channel, followed by the next
		 * channel, etc.
		 */
		void dataAvailable(Samples samples);

		/*! Emitted when an error occurs on the source.
		 *
		 * This may happen if the source is unexpectedly disconnected, disappears, is
		 * unplugged, etc.
		 *
		 * \param msg Contains the error message explaining the error.
		 */
		void error(QString msg = QString());

	protected:

		virtual QJsonObject packStatus() {
			return QJsonObject{
					{"state", m_state},
					{"source-type", m_sourceType},
					{"device-type", m_deviceType},
					{"connect-time", m_connectTime.toString()},
					{"start-time", m_startTime.toString()},
					{"read-interval", m_readInterval},
					{"sample-rate", m_sampleRate},
					{"gain", m_gain},
					{"adc-range", m_adcRange},
					{"nchannels", static_cast<qint64>(m_nchannels)},
					{"has-analog-output", false}
			};
		}


		/* Check a state transition.
		 * \param from The source state from which the transition must originate.
		 * \param to The source state in which the transition should end.
		 *
		 * This enforces the basic state diagram of data sources. Subclass
		 * implementations of the various transition slots (e.g., connect()), 
		 * should call this method before performing any other checking specific
		 * to that subclass.
		 */
		virtual bool checkStateTransition(QString from, QString to) {
			if (m_state != from) {
				return false;
			}
			if (m_state == "disconnected") {
				return (to == "connected");
			} else if (m_state == "connected") { 
				return ( (to == "disconnected") || (to == "streaming") );
			} else if (m_state == "streaming") {
				return (to == "connected");
			} else {
				return false;
			}
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
			m_state = "disconnected";
			m_connectTime = QDateTime{};
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

		/* Current state of the source. */
		QString m_state;

		/* Type of source, i.e., "file" or "device" */
		QString m_sourceType;

		/* The device from which the data originated. This the type of
		 * the actual MEA on which the data was recorded, e.g., "hexagonal"
		 * or "hidens".
		 */
		QString m_deviceType;

		/* The time at which the connection to the source was made. */
		QDateTime m_connectTime;

		/* The time at which the data stream was started. */
		QDateTime m_startTime;

		/* The configuration of the array, if this is a HiDens type. */
		QConfiguration m_configuration;

		/* A file describing the configuration, to be sent to the chip. */
		QString m_configurationFile;

		/* Any error messages */
		QString m_error;

		/* Interval (in ms) between reading data from the source.
		 * This, with the sampling rate, defines the size of a chunk of
		 * data from the source.
		 */
		int m_readInterval;

		/* Sampling rate of data from the source. */
		float m_sampleRate;

		/* Size in samples of a single frame of data. */
		int m_frameSize;

		/* Gain of the ADC conversion of the underlying MEA. */
		float m_gain;

		/* Voltage range of the ADC of the underlying MEA. */
		float m_adcRange;

		/* Number of data channels in the stream. */
		quint32 m_nchannels;

		/* Neurolizer plug number for HiDens data sources. */
		quint32 m_plug;

		/* ID number of the HiDens chip. */
		quint32 m_chipId;

		/* Mechanism for triggering the start of the data stream,
		 * e.g., "photodiode" or "none".
		 */
		QString m_trigger;

		/* Any analog output for the recording. */
		QVector<double> m_analogOutput;

		/* Set of parameters that are valid in a get() call */
		QSet<QString> m_gettableParameters;

		/* Set of parameters that can be set for this data source. */
		QSet<QString> m_settableParameters;
};

#endif
