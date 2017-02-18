/*! \file mcs-source.h
 *
 * Source subclass for interacting with MCS arrays via the NIDAQ
 * data acquisition library.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef MCS_SOURCE_H_
#define MCS_SOURCE_H_

#ifdef Q_OS_WIN
# include "NIDAQmx-mingw64.h"
#endif

#include "base-source.h"

#include <QtCore>

namespace datasource {

class McsSource : public BaseSource {
	Q_OBJECT

	public:
		McsSource(QObject *parent = nullptr);
		~McsSource();

		McsSource(const McsSource&) = delete;
		McsSource(McsSource&&) = delete;
		McsSource& operator=(const McsSource&) = delete;

#ifdef Q_OS_WIN

	public slots:
		virtual void set(QString param, QVariant value) Q_DECL_OVERRIDE;
		virtual void connect() Q_DECL_OVERRIDE;
		virtual void disconnect() Q_DECL_OVERRIDE;
		virtual void initialize() Q_DECL_OVERRIDE;
		virtual void deinitialize() Q_DECL_OVERRIDE;
		virtual void startStream() Q_DECL_OVERRIDE;
		virtual void stopStream() Q_DECL_OVERRIDE;

	private:

		QString getDaqmxError(int32);

		TaskHandle m_inputTask;
		TaskHandle m_outputTask;
		Samples m_acqBuffer;
		QVector<double> m_analogOutput;
		uInt32 m_acquisitionBlockSize;
		uInt64 m_acquisitionBufferSize;

		const char DeviceName[] = "Dev1";
		const int32 TriggerEdge = DAQmx_Val_Falling;
		const float64 TriggerLevel = 0.1;
		const QPair<float, float> AdcRangeLimits { 0.1, 10. };
		const bool32 BufferFillMode = DAQmx_Val_GroupByChannel;
		const int32 SampleMode = DAQmx_Val_ContSamps;
		const float64 DeviceTimeout = 60.; 	// seconds, used in triggering
		const int32 TerminalConfig = DAQmx_Val_NRSE;
		const char TimingSource[] = "OnboardClock";
		const uInt32 ErrorBufferSize = 2048;
		const char AnalogOutputPhysicalChannel[] = "Dev1/ao0";
		const char TriggerChannel[] = "Dev1/ai0";
		const char AnalogInputPhysicalChannels[] = "Dev1/ai0:63";
		const char AnalogOutputClockSource[] = "/Dev1/ai/SampleClock";
		const int32 AnalaogOutputUnits = DAQmx_Val_Volts;
		const int DeviceBufferMultiplier = 100; // Multiple of acquisition buffer size
												// that is buffered by the device itself.
		
		const int32 NidaqTimeoutError = -200284;
		const int32 NidaqAbortedError = -88710;
		QSet<int32> NidaqDisconnectedErrors {
				-88708,
				-88709,
				-201003
			};

#endif // Q_OS_WIN

};

}; // end datasource namespace

#endif
