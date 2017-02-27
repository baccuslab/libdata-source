/*! \file hidens-source.h
 *
 * Class for interacting with the HiDens MEA device.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef BLDS_HIDENS_SOURCE_H_
#define BLDS_HIDENS_SOURCE_H_

#include "base-source.h"

#include <QtCore>
#include <QtNetwork>
#include <QtConcurrent>

namespace datasource {

/*! \class HidensSource
 *
 * The HidensSource class is used to represent data collected from a
 * Hidens MEA. In addition to all parameters defined by the BaseSource
 * class, this subclass allows getting and setting the Hidens electrode
 * configuration in several ways.
 *
 * Note that the current implementation is *blocking*. Despite the fact
 * that the server program with which this class communicates is over the
 * network, writing a non-blocking implementation of this isn't trivial
 * because of a lot of variability in the replies from that server. So 
 * this class imposes a blocing request/reply pattern on the communication
 * protocol. So this BaseSource subclass *really should* be in a background
 * thread.
 */
class LIBDATA_SOURCE_VISIBILITY HidensSource : public BaseSource {
	Q_OBJECT

	/*! The IP address for the Hidens ThreadedServer application. */
	const QString HidensAddr { "11.0.0.1" };

	/*! Port number at which to connect to the Hidens ThreadedServer. */
	const quint16 HidensPort { 11112 };

	/*! IP address for the FPGA, used to send configurations and other
	 * low-level driver commands to the Hidens chips.
	 */
	const QString FpgaAddr { "11.0.0.7" };

	/*! Port number to which the FPGA commands are sent. */
	const quint16 FpgaPort { 32124 };

	/*! Default time to wait for replies from the Hidens server. */
	const int RequestWaitTime { 100 };

	/*! Timeout to wait for replies in sending commands directly to the FPGA. */
	const int FpgaTimeout { 1000 };

	/*! The sample rate of the device. */
	static constexpr float SampleRate { 20000. };

	public:
		/*! Construct a HiDens data source.
		 * \param addr The IP address or hostname at which the HiDens ThreadedServer
		 * application is running.
		 * \param readInterval The interval at which data is retrieved from the source.
		 * \param parent Parent for this object.
		 */
		HidensSource(const QString& addr = "11.0.0.1", int readInterval = 10, 
				QObject *parent = nullptr);

		/*! Destroy a Hidens data source. */
		~HidensSource();

		HidensSource(const HidensSource&) = delete;
		HidensSource(HidensSource&&) = delete;
		HidensSource& operator=(const HidensSource&) = delete;

	public slots:
		/*! Method implementing requests to set a named parameter for
		 * the Hidens data source.
		 *
		 * See BaseSource::set() for details.
		 */
		virtual void set(QString param, QVariant value) Q_DECL_OVERRIDE;

		/*! Method implementing requests to initialize the Hidens data source.
		 *
		 * See BaseSource::initialize() for details.
		 */
		virtual void initialize() Q_DECL_OVERRIDE;

		/*! Method implementing requests to start the Hidens data stream.
		 *
		 * See BaseSource::stopStream() for details.
		 */
		virtual void startStream() Q_DECL_OVERRIDE;

		/*! Method implementing requests to stop the Hidens data stream.
		 *
		 * See BaseSource::stopStream() for details.
		 */
		virtual void stopStream() Q_DECL_OVERRIDE;

	private:
		/* Handle a connection attempt to the HiDens data server. */
		void handleConnectionMade(bool made);

		/* Handle an unexpected disconnection from HiDens data server. */
		void handleDisconnect();

		/* Send a full request to the HiDens data server. */
		void askHidens(const QByteArray& request);

		/* Receive a full reply from the HiDens data server. 
		 * Warning: This function will block for a short period of time.
		 */
		QByteArray getHidensReply();

		/* Request a frame of data from the server */
		void requestData(const QByteArray& method = "stream");

		/* Receive a frame of data from the HiDens server */
		void recvDataFrame();

		/* Verify that a reply is non-null or not an error. */
		bool verifyReply(const QByteArray& reply);

		/* Get the actual configuration from the HiDens server */
		void getConfigurationFromServer();

		/* Function run in the background to send a configuration to 
		 * the FPGA. Sending a configuration seems to require actually
		 * waiting a bit for the connection to be verified, so this
		 * function is run in the background using the QtConcurrent
		 * module.
		 *
		 * Have to use the QFuture/QFutureWatcher because this
		 * must be a static function.
		 */
		static QPair<bool, QString> sendConfigToFpga(
				QString file, QString addr, quint16 port);

		/*! Override of function for packing source status into a map */
		virtual QVariantMap packStatus() Q_DECL_OVERRIDE;

		/* The future representing the result of the above
		 * concurrent function.
		 */
		QFuture<QPair<bool, QString>> m_configFuture;

		/* A watcher that emits a signal when the future finishes. */
		QFutureWatcher<QPair<bool, QString>> m_configWatcher;

		/* Handler function called when the sendConfigToFpga() 
		 * function returns, using its result as stored in 
		 * m_configFuture. This emits a signal with the result
		 * of setting the configuration.
		 */
		void handleConfigSendResponse();

		/* Subclass override of the handleError() function. */
		virtual void handleError(const QString& msg) Q_DECL_OVERRIDE;

		/* TCP socket object for connection to HiDens data server. */
		QTcpSocket *m_socket;

		/* IP address or hostname of the HiDens data server. */
		QString m_addr;

		/* Port number for the HiDens data server. */
		quint16 m_port;

		/* Number of total data channels in the HiDens system.
		 * This is the actual number of possible valid channels containing data.
		 */
		const int m_ntotalChannels { 126 };
		
		/* Number of bytes in each frame of data as received from the HiDens
		 * system. This is per frame, i.e., a segment containing one sample
		 * from each channel.
		 */
		const int m_hidensFrameSize { 131 };

		/* Number of bytes from the HiDens system that must be available for
		 * us to read a full data frame for emission.
		 */
		const int m_bytesPerEmitFrame {
				static_cast<int>((static_cast<float>(m_readInterval) / 1000.) *
				m_sampleRate * m_hidensFrameSize)
			};

		/* Buffer into which raw data from the HiDens device is placed. */
		arma::Mat<uchar> m_acqBuffer;

		/* Indices of connected electrodes (-1 if not connected). */
		arma::Col<int> m_electrodeIndices;

		/* Array used to index into actual acquisition buffer to retrieve
		 * valid data for emission.
		 */
		arma::uvec m_channelIndices;

		/* Gain of the ADC converters on board the chip. This is the output
		 * of the "gain 0" command.
		 */
		float m_deviceGain;

		/* Timer used to request new data and read it from the server */
		QTimer* m_readTimer;
};

}; // end datasource namespace 

#endif
