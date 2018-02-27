#ifndef __Udb_BtreeStore__
#define __Udb_BtreeStore__

/*
* Copyright 2010-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the CrossLine Udb library.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.info.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <QObject>
#include <QMutex>

struct Btree;
struct sqlite3;

namespace Udb
{
	class System;

	// Wir verwenden nicht SQLITE_THREADSAFE.
	// Darum wird hier ein Mutex verwendet zur Serialisierung aller Lese- und Schreibzugriffe.
	class BtreeStore : public QObject
	{
	public:
		// ReadLock und WriteLock können rekursiv und auch verschachtelt aufgerufen werden.
		class ReadLock // Serialisiert Lese/Schreibzugriff auf Store
		{
		public:
			ReadLock( BtreeStore* );
			~ReadLock();
		private:
			BtreeStore* d_db;
		};
		class WriteLock // Automatisiert aufruf von transBegin/transCommit und führt auch Lock durch
		{
		public:
			WriteLock( BtreeStore* );
			~WriteLock();
			void rollback();
		private:
			BtreeStore* d_db;
		};

		BtreeStore( QObject* owner = 0 );
		~BtreeStore(); // threadsafe

		void open( const QString& path, bool readOnly = false ); // threadsafe
		void close();	// threadsafe

		void transBegin(); 
		void transCommit();
		void transAbort();

		int createTable(bool noData = false); // threadsafe
		void dropTable( int table ); // threadsafe
		void clearTable( int table ); // threadsafe

		Btree* getBt() const;
		int getMetaTable() const { return d_metaTable; }
		bool isTrans() const { return d_txnLevel > 0; }
		const QString& getPath() const { return d_path; }
		bool isReadOnly() const;
		void setCacheSize( int numOfPages );
	protected:
		void checkOpen() const;
	private:
		friend class Locker;
		friend class WriteLock;
		sqlite3* d_db;
		qint32 d_txnLevel;
		int d_metaTable;
		QString d_path; 
#ifdef BTREESTORE_HAS_MUTEX
		QMutex d_lock;
#endif
	};
}


#endif
