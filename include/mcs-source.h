/*! \file mcs-source.h
 *
 * Source subclass for interacting with MCS arrays via the NIDAQ
 * data acquisition library.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef MCS_SOURCE_H_
#define MCS_SOURCE_H_

#ifdef __MINGW64__
# include "NIDAQmx-mingw64.h"
#endif

#include "base-source.h"

#include <QtCore>

namespace datasource {

/*! \class McsSource
 * The McsSource manages data recorded from the Multichannel Systems arrays
 * using the National Instruments NI-DAQmx driver library. This data source
 * can only be used on Windows machines.
 */
class VISIBILITY McsSource : public BaseSource {
	Q_OBJECT

#ifdef __MINGW64__

		/*! Default value for the device's name, used to 
		 * refer to the NIDAQ device.
		 */
		const QString DefaultDeviceName { "Dev1" };

		/*! Default mechanism for generating timing information about
		 * when to acquire samples.
		 */
		const QString DefaultTimingSource { "OnboardClock" };

		/*! Multiple of a single acquisition block which specifies the
		 * size of the NIDAQ runtime buffer. This means that the system
		 * buffers this many blocks for us before overwriting old samples.
		 */
		const int DefaultBufferMultiplier = 1000;

		/*! Default range of the ADC */
		const float DefaultAdcRange = 5.0;

		/*! Default edge (rising or falling) on which to trigger a recording. */
		const int32 DefaultTriggerEdge = DAQmx_Val_Falling;

		/*! Default level on which to trigger the start of a recording, in \a volts. */
		const float64 DefaultTriggerLevel = -0.1;

		/*! Default number of seconds to wait for a trigger before timing out. */
		const float64 DefaultTriggerTimeout = 60.0;

		/*! Default physical channel to use for the triggering. */
		const QString DefaultTriggerPhysicalChannel { "ai0" };

		/*! Default physical channel used to write analog output data. */
		const QString DefaultAnalogOutputPhysicalChannel { "ao0" };

		/*! Default mechanism for generating timing information about when
		 * to write analog output samples.
		 */
		const QString DefaultAnalogOutputClockSource { "SampleClock" };

		/*! Default physical channels (screw terminals) on the NIDAQ device
		 * which contain data from the MEA itself.
		 */
		const QString DefaultMeaPhysicalChannels { "ai16:75" };

		/*! Default description of how voltages are read from the MEA
		 * channels, e.g., referenced or non-referenced; single- or double-ended.
		 * The default is non-referenced single-ended (NRSE).
		 */
		const QString DefaultMeaWiringType { "NRSE" };

		/*! Default physical channel containing voltage data from the photodiode. */
		const QString DefaultPhotodiodePhysicalChannel { "ai0" };

		/*! Default wiring type for the photodiode channel. */
		const QString DefaultPhotodiodeWiringType { "RSE" };

		/*! List of "other" physical channels.
		 *
		 * In the current recording rig, these correspond to the intracellular
		 * voltage, intracellular current injection, and an unused channel.
		 */
		const QList<QString> DefaultOtherPhysicalChannels { "ai1", "ai2", "ai3" };

		/*! Wiring type for the "other" channels. */
		const QString DefaultOtherChannelWiringType { "RSE" };
#endif
	public:

		/*! Construct an McsSource.
		 *
		 * \param readInterval The interval at which data is read from the source,
		 * 	in milliseconds.
		 * \param parent The parent QObject.
		 */
		McsSource(int readInterval = 10, QObject *parent = nullptr);

		/*! Destroy an McsSource. */
		~McsSource();

		/* Copying is not allowed. */
		McsSource(const McsSource&) = delete;
		McsSource(McsSource&&) = delete;
		McsSource& operator=(const McsSource&) = delete;

#ifdef __MINGW64__

	public slots:
		/*! Method implementing requests to set a named parameter
		 * for the MCS data source.
		 *
		 * See \sa BaseSource::set for details.
		 */
		virtual void set(QString param, QVariant value) Q_DECL_OVERRIDE;

		/*! Metho implementing request to initialize the MCS data source.
		 *
		 * See \sa BaseSource::initialize for more details.
		 */
		virtual void initialize() Q_DECL_OVERRIDE;

		/*! Method implementing requests to start the data stream for
		 * an MCS data source.
		 *
		 * See \sa BaseSource::startStream for more details.
		 */
		virtual void startStream() Q_DECL_OVERRIDE;

		/*! Method implementing requests to stop the data stream for
		 * an MCS data source.
		 *
		 * See \sa BaseSource::startStream for more details.
		 */
		virtual void stopStream() Q_DECL_OVERRIDE;

	private slots:

		/* Read raw data from the NIDAQ device buffer and emit the new
		 * data in a signal.
		 */
		Q_INVOKABLE void readDeviceBuffer();

	private:

		virtual QVariantMap packStatus() Q_DECL_OVERRIDE;

		/* Setup the callback-base mechanism to read new data from
		 * the NIDAQ buffer as soon as it becomes available.
		 */
		int32 setupReadCallback();

		/* Look up an error by its code and return a string with
		 * the corresponding error message.
		 */
		QString getDaqmxError(int32);

		/* Clear all tasks, input and output, effectively re-initializing
		 * the data source.
		 */
		void resetTasks();

		/* Setup the analog input task with the current parameters.
		 *
		 * \return int32 The status of the setup.
		 */
		int32 setupAnalogInput();

		/* Setup the analog output task with the current parameters.
		 *
		 * \return int32 The status of the setup.
		 */
		int32 setupAnalogOutput();

		/* Configure the triggering mechanisms.
		 *
		 * \return int32 The status of the configuration.
		 */
		int32 configureTriggering();

		/* Reserve resources for the NIDAQmx runtime.
		 *
		 * This is used to finalize startup before streaming data, allowing
		 * the NIDAQ runtime to most efficiently use resources.
		 *
		 * \return int32 The status of the request.
		 */
		int32 finalizeTaskStartup();

		/* Read configuration file for parameter values.
		 *
		 * This will parse the file `resources/mcs-source.conf`, and will
		 * use the paramter values if they're valid, or the defaults defined
		 * at the top of this class if they're invalid.
		 */
		void readConfigurationFile();

		/* Override the default `QObject` event handler function.
		 *
		 * This class defines the custom `DataReadyEvent`, which allows 
		 * this class to be notified whenever new data is available for
		 * reading without waiting on a timer. This override handles all
		 * other events normally, and handles the `DataReadyEvent` by
		 * reading data from the buffer.
		 */
		virtual bool event(QEvent* ev) Q_DECL_OVERRIDE;

		/* A pointer to the DAQmx task handle object used to manage the
		 * analog input task.
		 */
		TaskHandle m_inputTask;

		/* A pointer to the DAQmx task handle object used to manage the
		 * analog output task.
		 */
		TaskHandle m_outputTask;

		/* A buffer into which new data is acquired. */
		Samples m_acqBuffer;

		/* Array storing the analog output for the current recording. */
		QVector<double> m_analogOutput;

		/* The size of a single block of data acquired by the device.
		 * This is the number of samples *per channel* acquired during
		 * each read.
		 */
		uInt32 m_acquisitionBlockSize;

		/* The size in samples of the full acquisition buffer. This is
		 * the number of samples in total, across all channels, during
		 * each acquisition from the device.
		 */
		uInt64 m_acquisitionBufferSize;

		/*! Name of the NIDAQ device to control. */
		QString m_deviceName;

		/*! Source of timing information, used to control when samples
		 * are acquired.
		 */
		QString m_timingSource;

		/*! Multiple of the block size for the device buffer internal to the
		 * NIDAQmx runtime, which buffers data on our behalf before we acquire
		 * it.
		 *
		 * \note Increase this multiplier if the NIDAQmx runtime library
		 * complains about not being able to keep up with data acquisition.
		 */
		int m_bufferMultiplier;

		/*! Edge on which to trigger, rising or falling. */
		int32 m_triggerEdge;

		/*! Triggering level, in *volts*.
		 *
		 * \note This dependent on both the Brownlee's gain and input selection.
		 * If that changes, this value becomes meaningless, and must be changed
		 * in the file `resources/mcs-source.conf` to reflect the wiring changes.
		 */
		float64 m_triggerLevel;

		/*! Time to wait before failing for a trigger, in seconds. */
		float m_triggerTimeout;

		/*! Channel used for triggering. */
		QString m_triggerPhysicalChannel;

		/*! Channel used for analog output. */
		QString m_analogOutputPhysicalChannel;

		/*! Clock source used for writing samples of the analog output. */
		QString m_analogOutputClockSource;

		/*! Physical channels corresponding to input data from the MEA. */
		QString m_meaPhysicalChannels;

		/*! Wiring type used for MEA channel inputs.
		 *
		 * See the NIDAQmx help documentation on wiring types for a more
		 * detailed explananation.
		 */
		QString m_meaWiringType;

		/*! Physical channel for the photodiode. */
		QString m_photodiodePhysicalChannel;

		/*! Wiring type for the photodiode channel. */
		QString m_photodiodeWiringType;

		/*! List of physical channels for any other data acquired.
		 *
		 * In the standard setup of the recording rig, this captures
		 * intracellular data (membrane voltage input and current output),
		 * with the third channel effectively unused. Only up to three
		 * physical channels may be specified in the configuration file.
		 */
		QList<QString> m_otherPhysicalChannels;

		/*! Wiring type for the other physical channels. */
		QString m_otherChannelWiringType;
		
		/*! Specification of how to acquire data. By default, data is
		 * acquired until we explicitly stop data (rather than a defined
		 * number of samples).
		 */
		const int32 m_deviceSampleMode = DAQmx_Val_ContSamps;

		/*! Specification of how to fill the acquisition buffer. Here
		 * we group all samples from a single channel together.
		 */
		const int32 m_deviceFillMode = DAQmx_Val_GroupByChannel;

		/*! Allowed range of A/D conversion system, in volts. */
		const QPair<float, float> m_adcRangeLimits { 0.1, 10. };

		/*! Integer specifiying the type number used to idenitify
		 * the `DataReadyEvent`. This is used by Qt's internal event
		 * notification system to identify custom events, and by this
		 * class to notify us when to read data from the device.
		 */
		int m_dataReadyEventType;


		const int32 NidaqTimeoutError = -200284;
		const int32 NidaqAbortedError = -88710;
		QSet<int32> NidaqDisconnectedErrors {
				-88708,
				-88709,
				-201003
			};

#endif // __MINGW64__

};

}; // end datasource namespace

#endif
