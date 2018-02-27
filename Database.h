#ifndef __Udb_Database__
#define __Udb_Database__

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
#include <QHash>
#include <Udb/UpdateInfo.h>
#include <Udb/IndexMeta.h>

namespace Udb
{
	class BtreeStore;
	class Transaction;
	typedef quint32 Atom;
	typedef quint64 OID;

	// Hauptklasse für den Client-Zugriff.
	// Versteckt Btree. Wird von mehreren Threads parallel gebraucht.

	class Database : public QObject
	{ 
		Q_OBJECT
	public:
		class Lock // Um den Mutex zu sperren
		{
		public:
			Lock(Database*);
			~Lock();
		private:
			Database* d_db;
		};
		class TxnGuard // Um mehrere DB-Transaktionen in eine zusammenzufassen, was schneller geht
		{
		public:
			TxnGuard(Database*);
			~TxnGuard();
			void rollback();
			void commit();
		private:
			Database* d_db;
		};

		Database( QObject* = 0 );
		~Database(); // threadsafe

		void open( const QString& path, bool readOnly = false ); // threadsafe
		void close(); // threadsafe

		Index createIndex( const QByteArray& name, const IndexMeta& ); // threadsafe
		void removeIndex( const QByteArray& name ); // threadsafe
		Index findIndex( const QByteArray& name ); // threadsafe
		bool getIndexMeta( Index, IndexMeta& ); // threadsafe
		QList<Index> findIndexForAtom( Atom atom ); // threadsafe
		bool clearIndexContents( Index ); // threadsafe

		void presetAtom( const QByteArray& name, Atom atom ); // threadsafe
		Atom getAtom( const QByteArray& name ); // threadsafe
		QByteArray getAtomString( Atom ); // threadsafe

		OID getMaxOid(); // threadsafe

		// Post-Commit-Notification
		void addObserver( QObject*, const char* slot, bool asynch = true ); // threadsafe
		void removeObserver( QObject*, const char* slot ); // threadsafe

		QString getFilePath() const; // threadsafe
		QUuid getDbUuid(bool create = true); // threadsafe, GUID dieser DB-Datei
		bool isReadOnly() const;
		void setCacheSize( int numOfPages ); // threadsafe, default 100, min. 20
		void dumpAtoms();
        void registerDatabase();
		static void registerDatabase( const QUuid&, const QString& path );
        static QString findDatabase( const QUuid& );
        static bool runDatabaseApp( const QUuid&, const QStringList & moreArgs = QStringList() );
	signals:
		void notify( Udb::UpdateInfo ); 
	private: 
		friend class Transaction;
		friend class Qit;
		friend class Mit;
        friend class Xit;
		friend class Lock;
		friend class Extent;
		friend class Global;

		int getObjTable();
		int getDirTable();
		int getIdxTable();
		int getQueTable();
		int getMapTable();
        int getOixTable();
		void checkOpen() const;
		void loadMeta();
		void saveMeta();
		BtreeStore* getStore() const { return d_db; }
		OID getNextOid(bool persistent = true);
		quint32 getNextQueueNr(quint64 oid);
	private:
		BtreeStore* d_db;
#ifdef DATABASE_HAS_MUTEX
		QMutex d_lock; // jeder Zugriff auf public wird serialisiert
#endif
		QHash<quint32,Transaction*> d_objLocks;
		QList<quint32> d_objDeletes;

		struct Meta
		{
			Meta():d_objTable(0),d_dirTable(0),d_idxTable(0),d_queTable(0),
                d_mapTable(0),d_oixTable(0){}

			int d_objTable; // Btree mit ID->Record und UUID->ID
			int d_dirTable; // Btree mit Atom->Name und Name->Atom
			int d_idxTable; // Btree mit name->tableId, tableId->IndexMeta und Atom+tableId->tableId um Indizes aufzufinden
			int d_queTable; // Btree mit <oid> <nr> -> <cell>
			int d_mapTable; // Btree mit <oid> [ <cell> ]* -> <cell>
            int d_oixTable; // Btree mit <oid> <rawbytes> -> <cell>
		};
		Meta d_meta;

		QHash<QByteArray,Atom> d_dir;
		QHash<Atom,QByteArray> d_invDir;
		QHash<Index,IndexMeta> d_idxMeta; // Cache, bleibt im Speicher
		QHash<Atom,QList<Index> > d_idxAtoms; // Cache von Atom -> Idx
	};
}

#endif
