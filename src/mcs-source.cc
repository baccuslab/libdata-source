/*! \file mcs-source.cc
 *
 * Implementation of data source subclass managing an MCS array,
 * through the NIDAQmx library.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#include "mcs-source.h"

namespace datasource {

#ifdef Q_OS_WIN

/*! \class DataReadyEvent
 *
 * The DataReadyEvent class is used as a simple notification mechanism,
 * sent to the McsSource class when new data can be read from the
 * underlying NIDAQ device buffer.
 */
class  DataReadyEvent : public QEvent {
	public:
		DataReadyEvent() : QEvent(QEvent::User) { }
};
#endif // Q_OS_WIN
	
McsSource::McsSource(int readInterval, QObject *parent) :
	BaseSource("device", "mcs", readInterval, 10000., parent)
#ifndef Q_OS_WIN
{
	throw std::invalid_argument("Cannot create MCS sources on non-Windows machines.");
}
#else
	,
	m_nchannels(64),
	m_trigger("none"),
	m_inputTask(nullptr),
	m_outputTask(nullptr),
	m_acquisitionBlockSize(
			static_cast<float>(m_readInterval) * m_sampleRate / 1000.),
	m_acquisitionBufferSize(
			m_acquisitionBufferSize * m_nchannels),
	m_deviceName(DefaultDeviceName),
	m_timingSource(DefaultTimingSource),
	m_bufferMultiplier(DefaultBufferMultiplier),
	m_adcRange(DefaultAdcRange),
	m_triggerEdge(DefaultTriggerEdge),
	m_triggerLevel(DefaultTriggerLevel),
	m_triggerTimeout(DefaultTriggerTimeout),
	m_triggerPhysicalChannel(DefaultTriggerPhysicalChannel),
	m_analogOutputPhysicalChannel(DefaultAnalogOutputPhysicalChannel),
	m_analogOutputClockSource(DefaultAnalogOutputClockSource),
	m_meaPhysicalChannels(DefaultMeaPhysicalChannels),
	m_meaWiringType(DefaultMeaWiringType),
	m_photodiodePhysicalChannel(DefaultPhotodiodePhysicalChannel),
	m_photodiodeWiringType(DefaultPhotodiodeWiringType),
	m_otherPhysicalChannels(DefaultOtherPhysicalChannels),
	m_otherChannelWiringType(DefaultOtherChannelWiringType)
{

	/* Ask Qt to initialize its resources, which are stored in the
	 * `./resources/` folder, and in this case consist of the `mcs-sourc.conf`
	 * configuration file.
	 */
	Q_INIT_RESOURCES(resources);

	/* Initialize acquisition buffer */
	m_acqBuffer.set_size(m_acquisitionBlockSize, m_nchannels);
	m_acqBuffer.fill(0);
	m_gain = (m_adcRange * 2) / (1 << 16);

	/* Setup the MCS-specific parameters that can be manipulated
	 * and retrieved.
	 */
	m_settableParameters.insert("analog-output");
	m_gettableParameters.insert("analog-output");
	m_settableParameters.insert("adc-range");
	m_gettableParameters.insert("adc-range");
	m_settableParameters.insert("trigger");
	m_gettableParameters.insert("trigger");

	/* Get the runtime-constructed ID for the data-ready event. */
	m_dataReadyEventType = DataReadEvent{}.type();
}
#endif // Q_OS_WIN

McsSource::~McsSource()
#ifndef Q_OS_WIN
{
}
#else
{
	resetTasks();
}
#endif // Q_OS_WIN

#ifdef Q_OS_WIN

void McsSource::readConfigurationFile()
{
	if (!QFile::exists(":/mcs-source.conf")) {
		return;
	}
	QSettings settings(":/mcs-source.conf", QSettings::IniFormat);

	/* Load and validate all device settings, using defaults if they
	 * are not valid.
	 */

	auto name = settings.value("device/name").toString();
	if (!name.isNull()) {
		m_deviceName = name;
	}

	auto timingSource = settings.value("device/timing-source").toString();
	if (!timingSource.isNull()) {
		m_timingSource = timingSource;
	}

	auto multiplier = settings.value("device/buffer-multiplier").toInt();
	if (multiplier > 10 && multiplier < 10000) {
		m_bufferMultiplier = multiplier;
	}

	auto range = settings.value("device/adc-range").toFloat();
	if (range >= m_adcRangeLimits.first && range <= m_adcRangeLimits.second) {
		m_adcRange = range;
		m_gain = (m_adcRange * 2.0) / (1 << 16);
	}

	auto level = settings.value("trigger/level").toFloat();
	if (level != 0. && std::abs(level) <= (m_adcRange / 2.0)) {
		m_triggerLevel = level;
	}

	auto timeout = settings.value("trigger/timeout").toFloat();
	if (timeout >= 0. && timeout < 1000.) {
		m_triggerTimeout = timeout;
	}

	auto triggerChan = settings.value("trigger/physical-channel").toString();
	if (!triggerChan.isNull() && triggerChan.startsWith("ai")) {
		m_triggerPhysicalChannel = triggerChan;
	}

	auto edge = settings.value("trigger/edge").toString();
	if (!edge.isNull()) {
		if (edge == "falling") {
			m_triggerEdge = DAQmx_Val_Falling;
		} else if (edge == "rising") {
			m_triggerEdge = DAQmx_Val_Rising;
		}
	}

	auto aoutChan = settings.value("analog-output/physical-channel").toString();
	if (aoutChan.startsWith("ao")) {
		m_analogOutputPhysicalChannel = aoutChan;
	}

	auto aoutSource = settings.value("analog-output/clock-source").toString();
	if (!aoutSource.isNull()) {
		m_analogOutputClockSource = aoutSource;
	}

	auto meaChans = settings.value("mea-channels/physical-channels").toString();
	if (meaChans.starstWith("ai")) {
		m_meaPhysicalChannels = meaChans;
	}

	auto wireType = settings.value("mea-channels/wiring-type").toString();
	if (!wireType.isNull()) {
		if (wireType.toLower() == "nrse") {
			m_meaWiringType = "NRSE";
		} else if (wireType.toLower() == "rse") {
			m_meaWiringType = "RSE";
		}
	}

	auto pdchan = settings.value("photodiode/physical-channel").toString();
	if (!pdchan.isNull() && pdchan.startsWith("ai")) {
		m_photodiodePhysicalChannel = pdchan;
	}

	wireType = settings.value("photodiode/wiring-type").toString();
	if (!wireType.isNull()) {
		if (wireType.toLower() == "nrse") {
			m_photodiodeWiringType = "NRSE";
		} else if (wireType.toLower() == "rse") {
			m_photodiodeWiringType = "RSE";
		}
	}

	auto channels = settings.value("other-channels/physical-channels").toString();
	if (!channels.isNull()) {
		auto chans = channels.split(",");
		for (auto i = 0; 
				i < std::min(chans.size(), DefaultOtherPhysicalChannels.size()); 
				i++) {
			if (chans.at(i).startsWith("ai")) {
				m_otherPhysicalChannels[i] = chans.at(i);
			}
		}
	}

	wireType = settings.value("other-channels/wiring-type").toString();
	if (!wireType.isNull()) {
		if (wireType.toLower() == "nrse") {
			m_otherChannelWiringType = "NRSE";
		} else if (wireType.toLower() == "rse") {
			m_otherChannelWiringType = "RSE";
		}
	}
}

void McsSource::resetTasks()
{
	if (m_inputTask) {
		DAQmxClearTask(m_inputTask);
		m_inputTask = nullptr;
	}
	if (m_outputTask) {
		DAQmxClearTask(m_outputTask);
		m_outputTask = nullptr;
	}
}

int32 McsSource::setupAnalogOutput()
{
	/* Support clearing the analog output by setting the analog
	 * output array to size of 0.
	 */
	if (m_analogOutput.isEmpty()) {
		if (m_outputTask) {
			auto status = DAQmxClearTask(m_outputTask);
			m_outputTask = nullptr;
			return status;
		}
		return 0;
	}

	/* Create the task itself. */
	int32 status = DAQmxCreateTask("", &m_outputTask);
	if (status)
		return status;

	/* Create the aout channel. */
	status = DAQmxCreateAOVoltageChan(m_outputTask,
			(m_deviceName + "/" + m_analogOutputPhysicalChannel).toStdString().c_str(),
			nullptr, -m_adcRange, m_adcRange, DAQmx_Val_Volts, nullptr);
	if (status) {
		DAQmxClearTask(m_outputTask);
		m_outputTask = nullptr;
		return status;
	}

	/* Configure timing. */
	status = DAQmxCfgSamClkTiming(m_outputTask,
			m_analogOutputClockSource.toStdString().c_str(),
			m_sampleRate, m_triggerEdge, m_deviceSampleMode,
			m_analogOutput.size() * sizeof(double));
	if (status) {
		DAQmxClearTask(m_outputTask);
		m_outputTask = nullptr;
		return status;
	}

	/* Write actual analog output. */
	int32 nwritten = 0;
	status = DAQmxWriteAnalogF64(m_outputTask, m_analogOutput.size(),
			false, m_triggerTimeout, DAQmx_Val_GroupByScanNumber,
			m_analogOutput.data(), &nwritten, nullptr);
	if (status) {
		DAQmxClearTask(m_outputTask);
		m_outputTask = nullptr;
		return status;
	}
	if (nwritten != m_analogOutput.size()) {
		DAQmxClearTask(m_outputTask);
		m_outputTask = nullptr;
		return status;
	}
	return status;
}

int32 McsSource::setupAnalogInput()
{
	int32 status = DAQmxCreateTask("", &m_inputTask);
	if (status)
		return status;

	/* Create photodiode channel. */
	int32 termConfig = ((m_photodiodeWiringType == "NRSE") ?
			DAQmx_Val_NRSE : DAQmx_Val_RSE);
	status = DAQmxCreateAIVoltageCHan(m_inputTask,
			(m_deviceName + "/" + 
			 m_photodiodePhysicalChannel).toStdString().c_str(), nullptr,
			termConfig, -m_adcRange, m_adcRange, DAQmx_Val_Volts, nullptr);
	if (status) { 
		DAQmxClearTask(m_inputTask);
		m_inputTask = nullptr;
		return status;
	}

	/* Create other channels */
	termConfig = ((m_otherChannelWiringType == "NRSE") ?
			DAQmx_Val_NRSE : DAQmx_Val_RSE);
	for (auto& chan : m_otherPhysicalChannels) {
		status = DAQmxCreateAIVoltageChan(m_inputTask,
				(m_deviceName + "/" + chan).toStdString().c_str(),
				nullptr, termConfig, -m_adcRange, m_adcRange, 
				DAQmx_Val_Volts, nullptr);
		if (status) {
			DAQmxClearTask(m_inputTask);
			m_inputTask = nullptr;
			return status;
		}
	}

	/* Create MEA channels */
	termConfig = ((m_meaWiringType == "NRSE") ?
			DAQmx_Val_NRSE : DAQmx_Val_RSE);
	status = DAQmxCreateAIVoltageChan(m_inputTask,
			(m_deviceName + "/" + m_meaPhysicalChannels).toStdString().c_str(),
			nullptr, termConfig, -m_adcRange, m_adcRange, 
			DAQmx_Val_Volts, nullptr);
	if (status) {
		DAQmxClearTask(m_inputTask);
		m_inputTask = nullptr;
		return status;
	}

	/* Configure timing */
	status = DAQmxCfgSamClkTiming(m_inputTask,
			m_timingSource.toStdString().c_str(), m_sampleRate,
			m_triggerEdge, m_deviceSampleMode,
			m_bufferMultiplier * m_acquisitionBufferSize);
	if (status) {
		DAQmxClearTask(m_inputTask);
		m_inputTask = nullptr;
		return status;
	}
	return status;
}

int32 McsSource::configureTriggering()
{
	int32 status = 0;
	if (m_trigger.toLower() == "photodiode") {
		status = DAQmxCfgAnlgEdgeStartTrig(m_inputTask,
				(m_deviceName + "/" + m_triggerPhysicalChannel).toStdString().c_str(),
				m_triggerEdge, m_triggerLevel);
		if (status) {
			return status;
		}

		if (m_outputTask) {
			status = DAQmxCfgAnlgEdgeStartTrig(m_inputTask,
					(m_deviceName + "/" + m_triggerPhysicalChannel).toStdString().c_str(),
					m_triggerEdge, m_triggerLevel);
			if (status) {
				return status;
			}
		}
	} else {
		status = DAQmxDisableStartTrig(m_inputTask);
		if (status)
			return status;

		if (m_outputTask) {
			status = DAQmxDisableStartTrig(m_outputTask);
			if (status) 
				return status;
		}
	}
	return status;
}

void McsSource::initialize()
{
	bool success = false;
	QString msg;

	if (m_state == "invalid") {
		auto status = DAQmxSelfTestDevice(m_deviceName.toStdString().c_str());
		if (status) {
			success = false;
			msg = "The NIDAQ is not reachable or not working. Verify that it is powered.";
		} else {
			success = true;
			m_state = "initialized";
		}
	} else {
		success = false;
		msg = "Can only initialize from the 'invalid' state.";
	}
	emit initialized(success, msg);
}

void McsSource::startStream()
{
	bool success = false;
	QString msg;

	if (m_state == "initialized") {

		/* Setup the notification system for reading data from
		 * device as it becomes available.
		 */
		auto status = setupReadCallback();
		if (status) {
			msg = QString("Failed to initialize read callback: %1").arg(
					getDaqmxError(status));
			resetTasks();
			emit streamStarted(false, msg);
			return;
		}

		/* Setup the analog input task. */
		status = setupAnalogInput();
		if (status) {
			msg = QString("Failed to setup analog input task: %1").arg(
					getDaqmxError(status));
			resetTasks();
			emit streamStarted(false, msg);
			return;
		}

		/* Setup the analog output task. */
		status = setupAnalogOutput();
		if (status) {
			msg = QString("Failed to setup analog output task: %1").arg(
					getDaqmxError(status));
			resetTasks();
			emit streamStarted(false, msg);
			return;
		}

		/* Configure the triggering mechanisms. */
		status = configureTriggering();
		if (status) {
			msg = QString("Failed to configure task triggering: %1").arg(
					getDaqmxError(status));
			resetTasks();
			emit streamStarted(false, msg);
			return;
		}

		/* Reserve resources for the tasks. */
		status = finalizeTaskStartup();
		if (status) {
			msg = QString("Failed to finalize task startup: %1").arg(
					getDaqmxError(status));
			resetTasks();
			emit streamStarte(false, msg);
			return;
		}

		/* Everything setup correctly. */
		success = true;
		m_state = "streaming";

	} else {
		success = false;
		msg = "Can only start stream from the 'initialized' state.";
	}
	emit streamStarted(success, msg);
}

void McsSource::stopStream()
{
}

QString McsSource::getDaqmxError(int32 code)
{
	QString msg;
	if (NidaqDisconnectedErrors.contains(code)) {
		msg = "The NIDAQ device was disconnected.";
	} else if (code == NidaqTimeoutError) {
		msg = QString("The recording was not triggered "
				"within the timeout of %1 seconds.").arg(m_triggerTimeout, 'f', 1);
	} else if (code == NidaqAbortedError) {
		msg = "The task was aborted.";
	} else {
		QByteArray buffer(1024, '\0');
		DAQmxGetExtendedErrorInfo(buffer.data(), buffer.size());
		msg = QString::fromUtf8(buffer);
	}
	return msg;
}

int32 McsSource::finalizeTaskStartup()
{
	int32 status = 0;
	status = DAQmxTaskControl(m_inputTask, DAQmx_Val_Task_Reserve);
	if (!status && m_outputTask) {
		status = DAQmxTaskControl(m_outputTask, DAQmx_Val_Task_Reserve);
	}
	return status;
}

void McsSource::readDeviceBuffer()
{
	int32 nread = 0;
	int32 status = DAQmxReadBinaryI16(m_inputTask, m_acquisitionBlockSize,
			m_triggerTimeout, m_deviceFillMode, m_acqBuffer.memptr(),
			m_acquisitionBufferSize, &nread, nullptr);
	if (status) {
		resetTasks();
		emit error(QString("Qn error occurred reading data from the MCS"
					" source: %1").arg(getDaqmxError(status)));
		return;
	}

	if (nread != static_cast<int32>(m_acquisitionBlockSize)) {
		resetTasks();
		emit error("A short read occurred from the MCS source.");
		return;
	}

	emit dataAvailable(m_acqBuffer * static_cast<qint16>(-1));
}

bool McsSource::event(QEvent* event)
{
	if (event->type() == QEvent::User)
		readDeviceBuffer();
	return BaseSource::event(event);
}

/* Callback function that notifies the McsSource object that
 * new data is available to be read. This is defined as a
 * standalone function in order to operate correctly with
 * the NIDAQmx runtime library.
 */
int32 CVICALLBACK dataAvailableNotifier(TaskHandle /* task */,
		int32 /* eventType */, uInt32 /* nsamples */, void* data)
{
	auto source = reinterpret_cast<McsSource*>(data);
	if (source) {
		qApp->postEvent(source, new DataReadyEvent);
	}
	return 0;
}

int32 McsSource::setupReadCallback()
{
	/* Register the dataAvailableNotifier function to be run every time
	 * `m_acquisitionBlockSize`	samples have been acquired into the
	 * NIDAQmx runtime buffer. This generates a new `DataReadyEvent`
	 * every time the data can be read, which is then sent to this
	 * McsSource object.
	 */
	return DAQmxRegisterEveryNSamplesEvent(m_inputTask,
			DAQmx_Val_Acquired_Into_Buffer, m_acquisitionBlockSize, 0,
			dataAvailableNotifier, this);
}

QVariantMap McsSource::packStatus()
{
	auto status = BaseSource::packStatus();
	status.insert("analog-output", QVariant::fromValue(m_analogOutput));
	status.insert("analog-output-size", m_analogOutput.size());
	status.insert("trigger", m_trigger);
}

#endif // Q_OS_WIN

}; // end datasource namespace

