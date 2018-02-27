#ifndef __Udb_Transaction__
#define __Udb_Transaction__

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
#include <QHash>
#include <QMap>
#include <Stream/DataCell.h>
#include <Udb/UpdateInfo.h>
#include <Udb/Obj.h>

namespace Udb
{
	class Database;
	class BtreeStore;
	class BtreeCursor;
	typedef quint32 Atom;
	typedef quint64 OID;

	// Wird von genau einem Thread verwendet. Die Klasse ist also nicht threadsafe
	class Transaction : public QObject
	{
		Q_OBJECT
	public:
		typedef QMap<QPair<quint32,Atom>,Stream::DataCell> Changes; // Wir koennen nicht QHash nehmen. Es muss geordnet sein.
		struct ByteArrayHolder
		{
			// Dieser Trick ist noetig, da QByteArray::operator< intern qstrcmp verwendet, das mit 0-Zeichen scheitert
			ByteArrayHolder( const QByteArray& ba ):d_ba(ba) {}
			bool operator<( const ByteArrayHolder& rhs ) const;
			QByteArray d_ba;
		};
		typedef QMap<ByteArrayHolder,Stream::DataCell> Map; // key: oid (vector<cell>) -> value

		Transaction( Database*, QObject* owner = 0 );
		~Transaction();

		// begin() gibt es nicht. Txn wird automatisch gestartet bei lock, create, set, erase
		void commit();
		void rollback();
		bool isActive() const { return !d_changes.isEmpty() || !d_notify.isEmpty(); }

		Obj createObject( Atom type = 0, bool createUuid = false );
		Obj createObject( const QUuid&, Atom type = 0 );
		Obj getObject( OID oid ) const;
		Obj getObject( const Stream::DataCell& ) const;
		Obj getObject( const QUuid& ) const;
		Obj getOrCreateObject( const QUuid&, Atom type = 0 ); 
		QUuid getUuid( OID oid, bool create );

		// Pre-Commit-Notification
		void addObserver( QObject*, const char* slot, bool asynch = true ); 
		void removeObserver( QObject*, const char* slot ); 
		typedef void (*Callback)(Transaction*, const Udb::UpdateInfo& );
		void addCallback( Callback );
		void removeCallback( Callback );
        const QList<UpdateInfo>& getPendingNotifications() const { return d_notify; }
		void setIndividualNotify(bool on) { d_individualNotify = on; }

		Database* getDb() const { return d_db; }
		Atom getAtom( const QByteArray& name ) const; // convenience for Database
		QByteArray getAtomString( Atom ) const; // convenience for Database
		bool isReadOnly() const; // convenience für Database
		const Changes& getChanges() const { return d_changes; }
	signals:
		void notify( Udb::UpdateInfo );  // Pre-Commit Notify
	private:
		friend class Obj;
		friend class Idx;
		friend class Qit;
		friend class Extent;
		void setField( OID oid, Atom, const Stream::DataCell& );
		void getField( OID oid, Atom, Stream::DataCell&, bool forceOld = false ) const;
		Obj::Names getUsedFields( OID ) const;
		OID getIdField( OID, Atom id ) const; // Helper for getField
		void erase( OID oid );
		bool isErased( OID oid ) const;
		OID create();
		void removeFromIndex(OID id, Atom atom, BtreeCursor&);
		void addToIndex( OID id, const Changes& all, Atom atom, BtreeCursor& );
		BtreeStore* getStore() const;
		quint32 createQSlot( OID, const Stream::DataCell& );
		void getQSlot( OID, quint32 nr, Stream::DataCell& ) const;
		void setQSlot( OID, quint32 nr, const Stream::DataCell& );
		void getCell( OID, const Obj::KeyList& key, Stream::DataCell& value ) const;
		void setCell( OID, const Obj::KeyList& key, const Stream::DataCell& value );
        void getCell( OID, const QByteArray& key, Stream::DataCell& value ) const;
		void setCell( OID, const QByteArray& key, const Stream::DataCell& value );
		void post( const UpdateInfo& );
		void doNotify( const UpdateInfo& );
	protected:
		void checkLock( OID oid );
	private:
		Changes d_changes; // oid+Atom-> Geaenderter Wert, oid+0 -> uuid | null
		Changes d_queue;	// oid+nr->Geaenderter Wert
		QMap<QUuid, quint32> d_uuidCache; // uuid->oid
		Map d_map; 
        Map d_oix;
		QList<UpdateInfo> d_notify;
		QList<Callback> d_callbacks;
		Database* d_db;
        bool d_commitLock; // Gegen doppelte Commit-Calls aus Pre-Commit-Notification
        bool d_individualNotify;
	};
}

#endif
