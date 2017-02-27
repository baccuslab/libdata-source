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

namespace datasource {

/*! \class FileSource
 *
 * The FileSource class allows playing back previously-recorded data.
 * It presents almost identically to the original source from which the
 * data was recorded, making it useful for testing, debugging, and just
 * visualizating old data.
 */
class LIBDATA_SOURCE_VISIBILITY FileSource : public BaseSource {
	Q_OBJECT

	public:

		/*! Construct a FileSource.
		 *
		 * \param filename The name of the file from which to play data.
		 * \readInterval Interval at which data is retrieved from the file.
		 * \param parent The parent QObject.
		 */
		FileSource(const QString& filename, int readInterval = 10, 
				QObject *parent = nullptr);

		/*! Destroy a FileSource. */
		~FileSource();

		FileSource(const FileSource&) = delete;
		FileSource(FileSource&&) = delete;
		FileSource& operator=(const FileSource&) = delete;

	public slots:

		/*! Handle a request to set a named parameter.
		 * 
		 * \param param The name of the parameter to set.
		 * \param data The value to set the parameter to.
		 *
		 * This override of the method always fails, as it doesn't make
		 * sense to set the parameters of a device that doesn't currently
		 * exist.
		 */
		virtual void set(QString param, QVariant data) Q_DECL_OVERRIDE;

		/*! Handle a request to initialize the data source. */
		virtual void initialize() Q_DECL_OVERRIDE;

		/*! Handle a request to start the source's stream of data. */
		virtual void startStream() Q_DECL_OVERRIDE;

		/*! Handle a request to stop the source's stream of data. */
		virtual void stopStream() Q_DECL_OVERRIDE;

	private slots:
		/* Read the next chunk of data from the underlying file source. */
		void readDataFromFile();

	private:

		/* Read information about the data source from the file. */
		void getSourceInfo();

		/* Pack the device's status and parameters into a map. */
		virtual QVariantMap packStatus() Q_DECL_OVERRIDE;

		/* Name of the file from which the data is retrieved. */
		QString m_filename;

		/* Pointer to the actual DataFile object. */
		std::unique_ptr<datafile::DataFile> m_datafile;

		/* Timer used to read new data from the file. */
		QTimer *m_readTimer;

		/* Pointer to the current position in the file. */
		quint64 m_currentSample;
};

}; // end datasource namespace

#endif

