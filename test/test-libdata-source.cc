/*! \file test-libdata-source.cc
 *
 * Implementation of testing code for libdata-source.
 *
 * (C) 2017 Benjamin Naecker bnaecker@stanford.edu
 */

#include "test-libdata-source.h"

using namespace datasource;

void TestLibDataSource::initTestCase()
{
	sources.insert("base", new BaseSource);
	try { 
		sources.insert("mcs", new McsSource);
	} catch (std::invalid_argument&) {
		sources.take("mcs");
		qWarning() << "Cannot test MCS source on this machine.";
	}
	/*
	sources.insert("hidens", new HidensSource);
	*/
	sources.insert("file", new FileSource("test-file.h5"));
	for (auto& sourceName : sources.keys()) {
		sources[sourceName]->initialize();
		sources[sourceName]->moveToThread(&sourceThread);
	}
	sourceThread.start();

	createParameterList();
}

void TestLibDataSource::createParameterList()
{
	parameters << Parameter {
			"trigger",
			{ "mcs" },
			{ "mcs" },
			"photodiode",
			"invalid",
			"photodiode",
	};

	auto time = QDateTime::currentDateTime();
	parameters << Parameter {
			"start-time",
			{ },
			{ "base", "mcs", "file", "hidens" },
			time.toString(),
			"",
			time.toString().toUtf8()
	};

	parameters << Parameter {
			"state",
			{ },
			{ "base", "mcs", "file", "hidens" },
			"initialized",
			"invalid-state",
			"initialized"
	};

	parameters << Parameter {
			"nchannels",
			{ },
			{ "base", "mcs", "file", "hidens" },
			64,
			-1,
			"@\x00\x00\x00"
	};

	QVector<double> validAout { 0.0, 1.0, 2.0 };
	QVector<double> invalidAout { 100., 100., 100. };
	parameters << Parameter {
			"analog-output",
			{ "mcs" },
			{ "mcs" },
			QVariant::fromValue(validAout),
			QVariant::fromValue(invalidAout),
			"\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\xf0?\x00\x00\x00\x00\x00\x00\x00@"
	};

	parameters << Parameter {
			"has-analog-output",
			{ },
			{ "base", "mcs", "file", "hidens" },
			true,
			true,
			"\x00"
	};

	parameters << Parameter {
			"analog-output-size",
			{ },
			{ "mcs" },
			5,
			-1,
			"\x05\x00\x00\x00"
	};

	parameters << Parameter {
			"gain",
			{ }, 
			{ "base", "mcs", "file", "hidens" },
			0.01f,
			100000.0f,
			"\n\xd7#<"
	};

	parameters << Parameter {
			"adc-range",
			{ "mcs" },
			{ "base", "mcs", "file", "hidens" },
			1.0,
			100000.0f,
			"\x00\x00\x80?"
	};

	parameters << Parameter {
			"plug",
			{ "hidens" },
			{ "hidens" },
			1,
			100,
			"\x01\x00\x00\x00"
	};

	parameters << Parameter {
			"chip-id",
			{ },
			{ "hidens" },
			1234,
			100000,
			"\xd2\x04\x00\x00"
	};

	parameters << Parameter {
			"read-interval",
			{ },
			{ "base", "mcs", "file", "hidens" },
			10,
			-1,
			"\n\x00\x00\x00"
	};

	parameters << Parameter {
			"sample-rate",
			{ },
			{ "base", "mcs", "file", "hidens" },
			10000.,
			1.0,
			"\x00@\x1c" "F" // must separate F to not be escaped by previous \x
	};

	parameters << Parameter {
			"source-type",
			{ },
			{ "base", "mcs", "file", "hidens" },
			"file",
			"invalid",
			"file"
	};

	parameters << Parameter {
			"device-type",
			{ },
			{ "base", "mcs", "file", "hidens" },
			"mcs",
			"invalid",
			"mcs"
	};

	parameters << Parameter {
			"configuration",
			{ "hidens" },
			{ "hidens" },
			"./test-config.cmdraw.nrk2",
			"invalid",
			{ } // not sure yet how to test
	};

	parameters << Parameter {
			"location",
			{ },
			{ "hidens", "file" },
			"/path/to/a/file",
			"invalid",
			"/path/to/a/file"
	};

	for (auto& p : parameters) {
		paramNames.append(p.name);
	}
}

void TestLibDataSource::testGetStatus()
{
	/* Loop over parameters. */
	for (auto& param : parameters) {

		for (auto& sourceName : sources.keys()) {
			/* Only test sources for which this parameter should be listed
			 * in the status map.
			 */
			if (param.gettableSources.contains(sourceName)) {
				bool contains = false;
				QObject::connect(this, &TestLibDataSource::requestStatus,
						sources[sourceName], &BaseSource::requestStatus);
				QObject::connect(sources[sourceName], &BaseSource::status, 
						[this,&contains,&param](QVariantMap status) {
							contains = status.contains(param.name);
						});
				QSignalSpy spy(sources[sourceName], &BaseSource::status);
				emit requestStatus();
				QVERIFY(spy.wait(1000));
				QVERIFY(contains);
				QObject::disconnect(sources[sourceName], 0, 0, 0);
			}
		}
	}
}

void TestLibDataSource::testGetParameters()
{
	for (auto& param : parameters) {
		for (auto& sourceName : sources.keys()) {

			/* Only test sources for which this parameter should be
			 * listed in the get request.
			 */
			if  (param.gettableSources.contains(sourceName)) {
				bool ok = false;
				QObject::connect(this, &TestLibDataSource::requestGet,
						sources[sourceName], &BaseSource::get);
				QObject::connect(sources[sourceName], &BaseSource::getResponse, 
						[this,&ok,&param](QString name, bool valid, QVariant) {
							ok = (name == param.name) && valid;
						});

				/* Actually request the parameter and verify its OK. */
				QSignalSpy spy(sources[sourceName], &BaseSource::getResponse);
				emit requestGet(param.name.toUtf8());
				QVERIFY(spy.wait(1000));
				QVERIFY(ok);
				QObject::disconnect(this, &TestLibDataSource::requestGet,
						sources[sourceName], &BaseSource::get);
			}
		}
	}
}

void TestLibDataSource::testSetParameters()
{

	for (auto& sourceName : sources.keys()) {
		for (auto& param : parameters) {
			/* If this parameter is settable for this source, try to
			 * set it and verify the request.
			 */
			if (param.settableSources.contains(sourceName)) {

				/* Connect thie request method and source's set method. */
				QObject::connect(this, &TestLibDataSource::requestSet,
						sources[sourceName], &BaseSource::set);

				/* Connect source's response method and the tester functor. */
				QString p;
				bool ok = false;
				auto tester = [&ok,&p](QString param, bool success) -> void {
					p = param;
					ok = success;
				};
				QObject::connect(sources[sourceName], &BaseSource::setResponse, tester);

				QSignalSpy spy(sources[sourceName], &BaseSource::setResponse);

				/* Set a valid value for the source, verify it succeeds. */
				emit requestSet(param.name.toUtf8(), 
						QVariant::fromValue(param.goodValue));
				QVERIFY(spy.wait(1000));
				QVERIFY(p == param.name);
				QVERIFY(ok == true);

				/* Set an invalid value for the source, verify it fails. */
				emit requestSet(param.name.toUtf8(), 
						QVariant::fromValue(param.badValue));
				QVERIFY(spy.wait(1000));
				QVERIFY(p == param.name);
				QVERIFY(ok == false);
			}
		}
		QObject::disconnect(this, &TestLibDataSource::requestSet,
				sources[sourceName], &BaseSource::set);
		QObject::disconnect(sources[sourceName], &BaseSource::setResponse, 0, 0);
	}
}

void TestLibDataSource::testSignals()
{
	for (auto& source : sources) {
		QObject::connect(this, &TestLibDataSource::requestInitialize,
				source, &BaseSource::initialize);
		QObject::connect(this, &TestLibDataSource::requestStatus,
				source, &BaseSource::requestStatus);
		QObject::connect(this, &TestLibDataSource::requestGet,
				source, &BaseSource::get);
		QObject::connect(this, &TestLibDataSource::requestSet,
				source, &BaseSource::set);
		QObject::connect(this, &TestLibDataSource::requestStart,
				source, &BaseSource::startStream);
		QObject::connect(this, &TestLibDataSource::requestStop,
				source, &BaseSource::stopStream);

		QSignalSpy initSpy(source, &BaseSource::initialized);
		emit requestInitialize();
		QVERIFY(initSpy.wait(1000));

		QSignalSpy statusSpy(source, &BaseSource::status);
		emit requestStatus();
		QVERIFY(statusSpy.wait(1000));

		QSignalSpy getSpy(source, &BaseSource::getResponse);
		emit requestGet("");
		QVERIFY(getSpy.wait(1000));

		QSignalSpy setSpy(source, &BaseSource::setResponse);
		emit requestSet("", {});
		QVERIFY(setSpy.wait(1000));

		QSignalSpy startSpy(source, &BaseSource::streamStarted);
		emit requestStart();
		QVERIFY(startSpy.wait(1000));

		QSignalSpy stopSpy(source, &BaseSource::streamStopped);
		emit requestStop();
		QVERIFY(stopSpy.wait(1000));

		QObject::disconnect(this, 0, source, 0);
	}
}

void TestLibDataSource::cleanupTestCase()
{
	for (auto& source : sources)
		source->deleteLater();
	sourceThread.quit();
}

QTEST_MAIN(TestLibDataSource)
