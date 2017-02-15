/*! \file file-source.cc
 *
 * Implementation of BaseSource subclass which interacts with a 
 * previously-recorded file as if it were a live device.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#include "file-source.h"

#include "libdatafile/include/hidensfile.h"

FileSource::FileSource(const QString& filename, QObject *parent) :
	BaseSource("file", "none", qSNaN(), 10, parent),
	m_filename(filename),
	m_currentSample(0)
{
	if (!QFile::exists(m_filename)) {
		throw std::invalid_argument("The requested data file does not exist.");
	}

	/* 
	 * Create file of correct runtime type. This will throw a
	 * std::invalid_argument if the file cannot be created.
	 */
	if (datafile::array(m_filename.toStdString()) == "hidens") {
		m_datafile.reset(new hidensfile::HidensFile(m_filename.toStdString()));
		m_gettableParameters.insert("configuration");
		m_gettableParameters.insert("plug");
	} else {
		m_datafile.reset(new datafile::DataFile(m_filename.toStdString()));
		m_gettableParameters.insert("analog-output");
		m_gettableParameters.insert("has-analog-output");
	}
	m_deviceType = QByteArray::fromStdString(m_datafile->array());

	m_readTimer = new QTimer(this);
	m_readTimer->setInterval(m_readInterval);
	QObject::connect(m_readTimer, &QTimer::timeout,
			this, &FileSource::readDataFromFile);
}

FileSource::~FileSource()
{
}

void FileSource::getSourceInfo()
{

	/* Read basic information */
	m_sampleRate = m_datafile->sampleRate();
	m_frameSize = static_cast<int>(static_cast<float>(m_readInterval) * m_sampleRate / 1000);
	m_gain = m_datafile->gain();
	m_nchannels = m_datafile->nchannels();
	m_adcRange = m_datafile->offset();

	/* Read analog output */
	if (m_datafile->analogOutputSize()) {
		auto aout = m_datafile->analogOutput();
		m_analogOutput.resize(aout.size());
		std::memcpy(aout.memptr(), m_analogOutput.data(), aout.size() * sizeof(double));
	}

	/* Read Hidens-specific information */
	if (m_deviceType.startsWith("hidens")) {
		m_plug = 0;
		m_chipId = 1;
		auto* p = dynamic_cast<hidensfile::HidensFile*>(m_datafile.get());
		if (!p) {
			return;
		}
		m_configuration = QConfiguration::fromStdVector(p->configuration());
	}
}

void FileSource::set(QString param, QVariant /* value */)
{
	emit setResponse(param, false, "Cannot set parameters of a file data source.");
}

void FileSource::initialize()
{
	bool success;
	QString msg;
	if (m_state == "invalid") {
		m_state = "initialized";
		success = true;
		m_connectTime = QDateTime::currentDateTime();
		getSourceInfo();
	} else {
		msg = "Can only initialize from the 'invalid' state.";
		success = false;
	}
	emit initialized(success, msg);
}

void FileSource::startStream()
{
	bool success;
	QString msg;
	if (m_state == "initialized") {
		m_state = "streaming";
		m_startTime = QDateTime::currentDateTime();
		m_readTimer->start();
		success = true;
	} else {
		msg = "Can only start stream from 'initialized' state.";
		success = false;
	}
	emit streamStarted(success, msg);
}

void FileSource::stopStream()
{
	bool success;
	QString msg;
	if (m_state == "streaming") {
		m_readTimer->stop();
		m_state = "initialized";
		m_startTime = {};
		m_currentSample = 0;
		success = true;
	} else {
		msg = "Can only stop stream from 'streaming' state.";
		success = false;
	}
	emit streamStopped(success, msg);
}

void FileSource::readDataFromFile()
{
	Samples s;
	if (m_currentSample >= static_cast<decltype(m_currentSample)>(m_datafile->nsamples())) {
		/* Wrap data around to the beginning of the file. */
		m_currentSample = 0;
		m_readTimer->stop();
		m_state = "initialized";
		emit streamStopped(true, "Reached end of source data file.");
		return;
	}
	m_datafile->data(0, m_nchannels, m_currentSample, m_currentSample + m_frameSize, s);
	m_currentSample += m_frameSize;
	emit dataAvailable(s);
}

QVariantMap FileSource::packStatus()
{
	auto map = BaseSource::packStatus();
	if (m_datafile->array().find("hidens") != std::string::npos) {
		map.insert("configuration", configToJson(m_configuration));
		map.insert("plug", m_plug);
	} else {
		map.insert("trigger", m_trigger);
	}
	return map;
}

