/*! \file hidens-source.cc
 *
 * Implementation of data source class that interacts with the
 * HiDens data server/array.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#include "hidens-source.h"

#include <algorithm> 	// for std::for_each
#include <cmath>		// std::isnan

namespace datasource {

HidensSource::HidensSource(const QString& addr, int readInterval, QObject *parent) :
	BaseSource("hidens", "hidens", readInterval, SampleRate, parent),
	m_addr(addr),
	m_port(HidensPort),
	m_electrodeIndices(m_hidensFrameSize)
{
	m_electrodeIndices.fill(-1);
	m_electrodeIndices(m_hidensFrameSize - 1) = 1; // photodiode channel
	m_acqBuffer.set_size(m_hidensFrameSize, 
			static_cast<int>(m_sampleRate * 
			static_cast<float>(m_readInterval) / 1000.));
	m_sourceLocation = addr;

	m_socket = new QTcpSocket(this);

	m_readTimer = new QTimer(this);
	m_readTimer->setInterval(m_readInterval);
	QObject::connect(m_readTimer, &QTimer::timeout, 
			this, [&]() {
				recvDataFrame();
				requestData("stream");
		});

	m_gettableParameters.insert("configuration");
	m_settableParameters.insert("configuration");
	m_gettableParameters.insert("configuration-file");
	m_settableParameters.insert("configuration-file");
	m_gettableParameters.insert("plug");
	m_settableParameters.insert("plug");
}

HidensSource::~HidensSource()
{
	QObject::disconnect(m_socket, 0, 0, 0);
	m_socket->disconnectFromHost();
	m_socket->deleteLater();
}

void HidensSource::initialize()
{
	if (m_state == "invalid") {
		QObject::connect(m_socket, &QAbstractSocket::connected,
				this, [&] { handleConnectionMade(true); });
		QObject::connect(m_socket, 
				static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),
				this, [&] { handleConnectionMade(false); });
		m_socket->connectToHost(m_addr, m_port);
	} else {
		emit initialized(false, "Can only initialize from 'invalid' state.");
	}
}

void HidensSource::startStream()
{
	bool valid = false;
	QString msg;
	if (m_state == "initialized") {
		if (m_plug > 4) {
			msg = QString("Cannot start HiDens data stream with source plug = %1").arg(m_plug);
			emit streamStarted(false, msg);
			return;
		}
		if (m_configuration.size() == 0) {
			msg = "Cannot initialize HiDens source with empty configuration.";
			emit streamStarted(false, msg);
			return;
		}
		if (std::isnan(m_gain) || 
				( (m_gain < 0.) || (m_gain > 10000.) )) {
			msg = QString("Cannot initialize HiDens source with gain = %1").arg(m_gain);
			emit streamStarted(false, msg);
			return;
		}
		m_state = "streaming";
		requestData("live");
		m_readTimer->start();
		valid = true;
	} else {
		msg = "Can only start stream from the 'connected' state.";
		valid = false;
	}
	emit streamStarted(valid, msg);
}

void HidensSource::stopStream()
{
	bool valid = false;
	QString msg;
	if (m_state == "streaming") {
		m_state = "initialized";
		m_readTimer->stop(); // should probably flush remaining data
		valid = true;
	} else {
		msg = "Can only stop stream from the 'streaming' state.";
	}
	emit streamStopped(valid, msg);
}

void HidensSource::set(QString param, QVariant value)
{
	if (!m_settableParameters.contains(param)) {
		emit setResponse(param, false,
				QString("Cannot set parameter \"%1\" for HidensSource.").arg(param));
		return;
	}

	if (m_state != "initialized") {
		emit setResponse(param, false, 
				"Can only set parameters while in the 'initialized' state.");
		return;
	}

	if (param == "plug") {
		bool ok;
		auto plug = value.toUInt(&ok);
		if (!ok || (plug > 4) ) {
			m_plug = -1;
			emit setResponse(param, false, 
					"The plug value was not an integer or outside the allowed range [0, 4].");
			return;
		}
		askHidens("select " + QByteArray::number(plug));
		if (!verifyReply(getHidensReply())) {
			m_plug = -1;
			emit setResponse(param, false, "The requested plug does not contain a chip.");
			return;
		}

		/* Verify a chip is plugged in to the requested slot */
		askHidens("id");
		auto reply = getHidensReply();
		auto id = reply.toUInt(&ok);
		if (!ok || (id == 65535)) {
			emit setResponse(param, false, "The chip in the requested plug appears invalid.");
			return;
		}
		
		/* Valid plug number and valid chip id in that plug. */
		m_plug = plug;
		m_chipId = id;
		emit setResponse(param, true);

		/* Get configuration for the connected chip */
		getConfigurationFromServer();
		return;

	} else if (param == "configuration") {
		emit setResponse(param, false, "Setting Hidens configurations directly from "
				"the command bytes is not yet supported. Set it via the 'configuration-file' "
				"parameter until this is implemented");
	} else if (param == "configuration-file") {

		if (m_plug == static_cast<unsigned>(-1)) {
			emit setResponse(param, false,
					"Must select a Neurolizer plug before setting configuration.");
			return;
		}

		m_configurationFile = value.toString();
		if (!m_configurationFile.endsWith(".cmdraw.nrk2")) {
			emit setResponse(param, false, 
					QString("Configuration files must be in \"*.cmdraw.nrk2\" format"));
			m_configurationFile.clear();
			return;
		}

		if (!QFile::exists(m_configurationFile)) {
			emit setResponse(param, false,
					QString("Configuration file \"%1\" does not exist.").arg(
					m_configurationFile));
			m_configurationFile.clear();
			return;
		}

		m_configFuture = QtConcurrent::run(sendConfigToFpga, 
				m_configurationFile, FpgaAddr, FpgaPort);
		m_configWatcher.setFuture(m_configFuture);
		QObject::connect(&m_configWatcher, 
				&decltype(m_configWatcher)::finished,
				this, &HidensSource::handleConfigSendResponse);
	} else {
		emit setResponse(param, false, 
				"The requested parameter is not supported for HiDens sources.");
	}
}

void HidensSource::handleConnectionMade(bool made)
{
	QObject::disconnect(m_socket, 0, 0, 0);
	if (made) {

		/* Set some communication parameters */
		askHidens("setbytes " + QByteArray::number(m_hidensFrameSize));
		if (!verifyReply(getHidensReply())) {
			m_socket->disconnectFromHost();
			qDebug() << "Could not setbytes with HiDens server.";
			emit initialized(false, "Error initializing communication with HiDens data server.");
			return;
		}

		askHidens("header_frameno off");
		if (!verifyReply(getHidensReply())) {
			m_socket->disconnectFromHost();
			qDebug() << "Could not set header_frameno off with HiDens server.";
			emit initialized(false, "Error initializing communication with HiDens data server.");
			return;
		}

		askHidens("client_name blds");
		if (!verifyReply(getHidensReply())) {
			m_socket->disconnectFromHost();
			qDebug() << "Could not set client name with HiDens server.";
			emit initialized(false, "Error initializing communication with HiDens data server.");
			return;
		}

		askHidens("sr");
		auto reply = getHidensReply();
		bool ok;
		m_sampleRate = reply.toFloat(&ok);
		if (!ok) {
			m_socket->disconnectFromHost();
			QString msg { "Could not retrieve sampling rate from HiDens server. "
					"Make sure the server is running and a chip is plugged into the Neurolizer." };
			qDebug().noquote() << msg;
			emit initialized(false, msg);
			return;
		}

		askHidens("gain 0");
		reply = getHidensReply();
		auto gain = reply.toFloat(&ok);
		if (!ok) {
			m_socket->disconnectFromHost();
			QString msg { "Could not retrieve gain from HiDens server. "
					"Make sure the server is running and a chip is plugged into the Neurolizer." };
			qDebug().noquote() << msg;
			emit initialized(false, msg);
			return;
		}
		m_deviceGain = gain;

		askHidens("adc_range");
		reply = getHidensReply();
		auto adcRange = reply.toFloat(&ok);
		if (!ok) {
			m_socket->disconnectFromHost();
			QString msg { "Could not retrieve ADC range from HiDens server. "
					"Make sure the server is running and a chip is plugged into the Neurolizer." };
			qDebug().noquote() << msg;
			emit initialized(false, msg);
			return;
		}
		m_adcRange = adcRange;
		m_gain = m_adcRange / static_cast<float>(1 << 8) / m_deviceGain;

		QObject::connect(m_socket, &QAbstractSocket::disconnected,
				this, &HidensSource::handleDisconnect);

		m_state = "initialized";
		m_connectTime = QDateTime::currentDateTime();
		emit initialized(true);

	} else {
		qDebug() << "Could not connect to HiDens data server.";
		m_socket->disconnectFromHost();
		m_connectTime = QDateTime{};
		emit initialized(false, "Could not connect to HiDens data server.");
	}
}

void HidensSource::handleDisconnect()
{
	handleError("Unexpectedly disconnected from HiDens data server.");
}

void HidensSource::askHidens(const QByteArray& cmd)
{
	qint64 nwritten = 0;
	do {
		auto tmp = m_socket->write(cmd.data() + nwritten, cmd.size() - nwritten);
		if (tmp == -1) {
			QObject::disconnect(m_socket, 0, 0, 0);
			m_socket->disconnectFromHost();
			handleError("Error sending request to HiDens data server.");
		} else {
			nwritten += tmp;
		}
	} while (nwritten < cmd.size());
}

QByteArray HidensSource::getHidensReply()
{
	QByteArray reply;
	if (!m_socket->waitForReadyRead(RequestWaitTime)) {
		handleError("Communication with the HiDens data server timed out.");
	} else {
		reply = m_socket->readLine();
		if (reply.endsWith('\n')) {
			reply.chop(1);
		}
	}
	return reply;
}

bool HidensSource::verifyReply(const QByteArray& reply)
{
	return (!reply.isNull() && !reply.startsWith("Error"));
}

void HidensSource::requestData(const QByteArray& method)
{
	askHidens(method + " " + QByteArray::number(m_readInterval));
}

void HidensSource::recvDataFrame()
{
	if (m_socket->bytesAvailable() < m_bytesPerEmitFrame) {
		return;
	}

	/* Read all avaialable frames */
	while (m_socket->bytesAvailable() >= m_bytesPerEmitFrame) {

		/* Read until a full frame has been received. */
		qint64 nread = 0, ret = 0;
		do {
			ret = m_socket->read(reinterpret_cast<char*>(m_acqBuffer.memptr()) + nread,
					m_bytesPerEmitFrame - nread);
			if (ret == -1) {
				emit error("Error reading data from HiDens server!");
				return;
			}
			nread += ret;
		} while (nread < m_bytesPerEmitFrame);

		/* Convert photodiode signal.
		 *
		 * The photodiode is carried across a couple of distributed channels. The bits we
		 * care about are the 4th bit in the last byte. Note that this is entirely dependent
		 * on which pin on the LVDS adapter the output from the Arduino is connected to.
		 * This code assumes that the digital output is connected to the 4th pin from the
		 * top on the left. If it moves, see the below URL to get the new value for the
		 * bit-twiddling.
		 *
		 * See https://wiki-bsse.ethz.ch/display/DBSSECMOSMEA/HiDens+Neurolizer+LVDS+Adapter
		 * for more information.
		 */
		m_acqBuffer.row(m_hidensFrameSize - 1).for_each(
				[](uchar& x) { 
					x = ( x & 0x08 ) ? 255 : 0; // set elements with 4th bit a 1 to 255
				});
		emit dataAvailable(arma::conv_to<datasource::Samples>::from(
				m_acqBuffer.rows(m_channelIndices).t() * static_cast<qint16>(-1)));
	};
}

void HidensSource::getConfigurationFromServer()
{
	askHidens("ch 0-125");
	if (!m_socket->waitForReadyRead(RequestWaitTime)) {
		handleError("Communication with the HiDens data server timed out.");
		return;
	}
	auto bytes = m_socket->readAll();
	if (!verifyReply(bytes)) {
		handleError("Could not retrieve configuration from HiDens server.");
		return;
	}
	auto originalChannelReply = QString(bytes);

	/* Get actual connected electrodes in a list */
	auto stripFunction = [](QString& s) { if (s.endsWith(" ")) { s.chop(1); } };
	auto channelStringList = originalChannelReply.split("\n");
	std::for_each(channelStringList.begin(), channelStringList.end(), stripFunction);
	m_electrodeIndices.fill(-1);
	m_electrodeIndices(m_hidensFrameSize - 1) = 1;
	m_nchannels = 0;
	int n = 0;
	for (auto& each : channelStringList) {
		if (each.size()) {
			m_electrodeIndices(n) = each.toInt();
			m_nchannels += 1; // Counts connected channels
		}
		n += 1;	// Counts possible channels
	}

	/* Convert from channel number to data buffer column index, keeping photodiode */
	m_channelIndices = arma::find(m_electrodeIndices > 0);

	/* 
	 * Read the electrode positions from the resource file, rather than parsing
	 * from the server.
	 */
	QFile electrodeFile(":/electrode-list.txt");
	if (!electrodeFile.exists()) {
		m_configuration = {};
		handleError("Electrode configuration file 'electrode-list.txt' is missing!");

		/* Should really just get this from the server. */
		return;
	}
	electrodeFile.open(QIODevice::ReadOnly);
	auto electrodeList = QString(electrodeFile.readAll()).split("\n");
	electrodeFile.close();

	/* Parse configuration */
	m_configuration.clear();
	m_configuration.reserve(m_nchannels);
	QRegularExpression re("\\s|[xyp]");
	for (int i = 0; i < m_ntotalChannels; i++) {
		if (m_electrodeIndices(i) == -1) {
			continue; // Skip unconnected channels
		}
		Electrode el;
		el.index = m_electrodeIndices(i);

		auto positionList = electrodeList.at(m_electrodeIndices(i)).split(re);
		el.xpos = positionList[0].toInt();
		el.ypos = positionList[1].toInt();
		el.x = positionList[3].toInt();
		el.y = positionList[4].toInt();
		el.label = positionList[5][0].toLatin1();
		m_configuration << el;
	}
}

QPair<bool, QString> HidensSource::sendConfigToFpga(
		QString file, QString addr, quint16 port) 
{
	QTcpSocket* fpga = new QTcpSocket();
	fpga->connectToHost(addr, port);
	if (!fpga->waitForConnected(10000)) {
		// Can block as this is run in another thread.
		fpga->deleteLater();
		return {false, file};
	}

	/* Open configuration file */
	QFile configFile(file);
	if (!configFile.exists()) {
		fpga->deleteLater();
		return {false, file};
	}
	configFile.open(QIODevice::ReadOnly);
	auto configData = configFile.readAll();
	configFile.close();

	/* Write data to FPGA */
	qint64 nwritten = 0, tmp = 0;
	bool success = true;
	QString msg;
	do {
		tmp = fpga->write(configData.data() + nwritten, 
				configData.size() - nwritten);
		if (tmp == -1) {
			success = false;
			break;
		}
		nwritten += tmp;
	} while (nwritten < configData.size());
	fpga->waitForBytesWritten(10000);
	fpga->deleteLater();
	return {success, file};
}

void HidensSource::handleConfigSendResponse()
{
	QObject::disconnect(&m_configWatcher, 0, 0, 0);
	auto result = m_configFuture.result();
	QString msg;
	if (result.first) {
		m_configurationFile = result.second;
		getConfigurationFromServer();
		getConfigurationFromServer(); // actually needed twice for some reason
	} else {
		m_configurationFile.clear();
		msg = "Could not send the configuration to the server.";
	}
	emit setResponse("configuration", result.first, msg);
}

void HidensSource::handleError(const QString& msg)
{
	QObject::disconnect(m_socket, 0, 0, 0);
	m_socket->disconnectFromHost();
	BaseSource::handleError(msg);
}

QVariantMap HidensSource::packStatus() 
{
	auto map = BaseSource::packStatus();
	map.insert("location", m_sourceLocation);
	map.insert("configuration", configToVariant(m_configuration));
	map.insert("configuration-file", m_configurationFile.toUtf8());
	map.insert("plug", m_plug);
	return map;
}

}; // end datasource namespace

