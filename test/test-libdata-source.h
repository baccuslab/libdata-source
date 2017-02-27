/*! \file test-libdata-source.h
 *
 * Main test class for testing libdata-source.
 *
 * (C) 2017 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef TEST_LIBDATA_SOURCE_H
#define TEST_LIBDATA_SOURCE_H

#include "../include/data-source.h"

#include <QtTest/QtTest>

class TestLibDataSource : public QObject {
	Q_OBJECT

	signals:
		void requestInitialize();
		void requestStart();
		void requestStop();
		void requestSet(const QByteArray& param, const QVariant& data);
		void requestGet(const QByteArray& param);
		void requestStatus();

	private slots:
		void initTestCase();
		void testSignals();
		void testGetParameters();
		void testGetStatus();
		void testSetParameters();
		void cleanupTestCase();

	private:

		void createParameterList();

		QMap<QString, QPointer<datasource::BaseSource>> sources;
		QThread sourceThread;

		/* Store data about a parameter to ease testing of
		 * getting and setting.
		 */
		struct Parameter {

			/* Parameter name. */
			QString name;

			/* List of sources on which this should be settable. */
			QList<QString> settableSources;

			/* List of sources for which this parameter should
			 * be in both the status map and can be received
			 * in a `get` response.
			 */
			QList<QString> gettableSources;

			/* A valid value for which a get() request should succeed. */
			QVariant goodValue;

			/* A value for which a get() request should fail. */
			QVariant badValue;

			/* A byte-array giving the value serialized. */
			QByteArray serialized;
		};

		/* List of all parameters. This is created in the 
		 * initTestCase method.
		 */
		QList<Parameter> parameters;

		/* List of just parameter names. */
		QList<QString> paramNames;
};

#endif

