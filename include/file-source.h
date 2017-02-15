/*! \file file-source.h
 *
 * Class for retrieving data from a previously-recorded data file.
 *
 * (C) 2016 Benjamin Naecker bnaecker@stanford.edu
 */

#ifndef FILE_SOURCE_H_
#define FILE_SOURCE_H_

#include "base-source.h"

#include "libdatafile/include/datafile.h"

#include <QtCore>

#include <memory> // std::unique_ptr

class FileSource : public BaseSource {
	Q_OBJECT

	public:
		FileSource(const QString& filename, QObject *parent = nullptr);
		~FileSource();

		FileSource(const FileSource&) = delete;
		FileSource(FileSource&&) = delete;
		FileSource& operator=(const FileSource&) = delete;

	public slots:
		virtual void set(QString param, QVariant data) Q_DECL_OVERRIDE;
		virtual void initialize() Q_DECL_OVERRIDE;
		virtual void startStream() Q_DECL_OVERRIDE;
		virtual void stopStream() Q_DECL_OVERRIDE;

	private slots:
		void readDataFromFile();

	private:
		void getSourceInfo();
		virtual QVariantMap packStatus() Q_DECL_OVERRIDE;

		QString m_filename;
		std::unique_ptr<datafile::DataFile> m_datafile;
		QTimer *m_readTimer;
		quint64 m_currentSample;
};

#endif
