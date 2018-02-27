#ifndef _Udb_Obj_
#define _Udb_Obj_

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

#include <Stream/DataCell.h>
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <Udb/UpdateInfo.h>
#include <Udb/Qit.h>
#include <Udb/Mit.h>
#include <QList>
#include <QVector>

class QMimeData;

namespace Udb
{
	class Transaction;
	class Database;
	class Global;
	typedef quint64 OID;
	typedef quint32 Atom;

	// Value
	class Obj
	{
	public:
        static const char* s_mimeObjectRefs;
		static const char* s_xoidScheme;
		typedef QList<OID> OidList;
		typedef QVector<Stream::DataCell> KeyList;
        typedef QVector<Stream::DataCell> ValueList;
		typedef QList<Atom> Names;

		Obj();
		Obj( OID oid, Transaction* );
		Obj( const Obj& );

		// Objektzugriff via Attribute
		Obj getValueAsObj( Atom name ) const;
        void setValueAsObj( Atom name, const Obj& );
        void setValue( Atom name, const Stream::DataCell& );
		void clearValue( Atom name );
		void setString( Atom name, const QString& );
        void setAscii( Atom name, const QByteArray& );
        void setLatin1( Atom name, const QByteArray& );
        void setBool( Atom name, bool );
		void setTimeStamp( Atom name );
        quint32 incCounter( Atom name ); // verwendet Value als UInt32
        quint32 decCounter( Atom name ); // verwendet Value als UInt32
		void getValue( Atom name, Stream::DataCell&, bool forceOldValue = false ) const;
		bool hasValue( Atom name ) const; // teuer
		Stream::DataCell getValue( Atom name, bool forceOldValue = false ) const;
		QString getString( Atom name, bool stripMarkup = false ) const;
		Names getNames() const;
		// -

		// Identitaet
		OID getOid() const { return d_oid; }
		QUuid getUuid(bool create = true) const;
		Atom getType() const;
        void setType( Atom ); // alle 32 Bits stehen uneingeschrnkt zur Verfgung
		bool isNull(bool checkType = false, bool checkErased = false ) const;
		Transaction* getTxn() const { return d_txn; }
		Database* getDb() const;
		bool equals( const Obj& rhs ) const;
		// -

		// CRUD
		void erase();
		bool isErased() const;
		Obj& operator=( const Obj& r ) { return assign( r ); }
		Obj& assign( const Obj& r );
		operator Stream::DataCell() const;
		//bool operator==( const Obj& rhs ) const { return equals( rhs ); }
		// -

		// Objektzugriff als Queue
		Qit appendSlot( const Stream::DataCell& );
		Qit getFirstSlot() const;
		Qit getLastSlot() const;
		Qit getSlot( quint32 nr ) const;
		// -

		// Unterobjekte
		Obj getParent() const;
		void aggregateTo(const Obj& parent, const Obj& before = Obj()); // before.isNull -> Append
                                                    // !before.isNull -> before wird Successor von this
		Obj createAggregate( Atom type = 0, const Obj& before = Obj(), bool createUuid = false );
		void deaggregate(); // Befreie dieses Objekt aus dem Aggregat und mache es selbstndig
		Obj getFirstObj() const;
		Obj getLastObj() const;
		bool next(); // false..kein Nachfolger, unverndert
		Obj getNext() const;
		bool prev(); // false..kein Vorgnger, unverngert
		Obj getPrev() const;
		// -

		// Objektzugriff als Map bzw. MUMPS Global bzw. Sparse Array
		void setCell( const KeyList& key, const Stream::DataCell& value );
		void getCell( const KeyList& key, Stream::DataCell& value ) const;
		Stream::DataCell getCell( const KeyList& key ) const;
		Mit findCells( const KeyList& key ) const;
        // Wie Map, aber mit ByteArray als key, dessen Inhalt frei ist und partial Match zulsst
        void setCell( const QByteArray& key, const Stream::DataCell& value );
		void getCell( const QByteArray& key, Stream::DataCell& value ) const;
		Stream::DataCell getCell( const QByteArray& key ) const;
		Xit findCells( const QByteArray& key ) const;
		// -

		// Convenience
		Obj getObject( OID ) const;
		Obj createObject( Atom = 0, bool createUuid = false ) const;
		Atom getAtom( const QByteArray& name ) const;
        static ValueList unpackArray( const Stream::DataCell& );
        static Stream::DataCell packArray( const ValueList& );
        static QList<Udb::Obj> readObjectRefs(const QMimeData *data, Udb::Transaction* txn );
        static void writeObjectRefs(QMimeData *data, const QList<Udb::Obj>& );
        static bool isLocalObjectRefs( const QMimeData *data, Udb::Transaction* txn );
		static QUrl objToUrl(const Obj & o, Atom id = 0, Atom txt = 0 );
		static QUrl oidToUrl(OID oid, const QUuid& dbid );
		static void writeObjectUrls(QMimeData *data, const QList<Obj> & objs, Atom id = 0, Atom txt = 0 );
        void commit();
	protected:
		void checkNull() const;
	private:
		quint64 deaggregateImp(bool notify); // returns parent or null
		void setValuePriv( const Stream::DataCell&, Atom name );
	private:
		quint32 d_oid;
		Transaction* d_txn;
	};
	inline bool operator==(const Udb::Obj& lhs, const Udb::Obj& rhs) { return lhs.equals(rhs); }
	inline uint qHash( const Udb::Obj& o ) { return o.getOid(); }
}

#endif
