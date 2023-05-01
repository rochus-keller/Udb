/*
* Copyright 2010-2017 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the CrossLine Udb library.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.ch.
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

#include "Transaction.h"
#include "BtreeCursor.h"
#include "BtreeStore.h"
#include "Record.h"
#include "Database.h"
#include "DatabaseException.h"
#include "Idx.h"
#include <cassert>
#include <QtDebug>
using namespace Udb;
using namespace Stream;

bool Transaction::ByteArrayHolder::operator <( const ByteArrayHolder& rhs ) const
{
	// Korrigiert am 2016-01-04: es wurde direkt ret und rhs-lhs zurueckgegeben statt bool

	// aus qstrcmp(const QByteArray &, const QByteArray &)
	const int lhsLen = d_ba.length();
	const int rhsLen = rhs.d_ba.length();
	const int ret = memcmp( d_ba.data(), rhs.d_ba.data(), qMin(lhsLen, rhsLen));
	// ret < 0 -> lhs < rhs
	// ret > 0 -> lhs > rhs
	if( ret != 0 )
		return ( ret < 0 );
	// they matched qMin(l1, l2) bytes
	// so the longer one is lexically after the shorter one
	return ( rhsLen - lhsLen ) > 0; // rhs is laenger als lhs und damit gilt "lhs < rhs"
}

Transaction::Transaction( Database* db, QObject* p ):
	QObject(p),d_db(db),d_commitLock(false),d_individualNotify(true)
{
	assert( db );
}

Transaction::~Transaction()
{
	if( isActive() )
		rollback();
}

Atom Transaction::getAtom( const QByteArray& name ) const
{
	return d_db->getAtom( name );
}

QByteArray Transaction::getAtomString(Atom a) const
{
	return d_db->getAtomString( a );
}

bool Transaction::isReadOnly() const
{
	return d_db->isReadOnly();
}

void Transaction::checkLock( OID oid )
{
	// NOTE: Caller ist für Database::Lock verantwortlich
	QHash<quint32,Transaction*>::const_iterator i = d_db->d_objLocks.find( oid );
	if( i != d_db->d_objLocks.end() )
	{
		// Objekt ist bereits gelockt
		if( i.value() != this )
			// Objekt ist bereits durch andere Transaktion gelockt
			throw DatabaseException( DatabaseException::RecordLocked ); 
	}else
	{
		// Objekt ist noch nicht gelockt
		d_db->d_objLocks[oid] = this;
	}
}

void Transaction::setField( OID oid, Atom a, const Stream::DataCell& v )
{
	Database::Lock lock( d_db );
	
	checkLock( oid );
	// Lock ist ab hier zu unseren Gunsten vorhanden.

	if( d_db->d_objDeletes.contains( oid ) )
		throw DatabaseException( DatabaseException::RecordDeleted, "cannot write to deleted record" );

	d_changes[ qMakePair(quint32(oid),a) ] = v;
}

void Transaction::getField( OID oid, Atom a, Stream::DataCell& v, bool forceOld ) const
{
    if( !forceOld )
    {
        Changes::const_iterator i = d_changes.find( qMakePair(quint32(oid),a) );
        if( i != d_changes.end() )
        {
            v = i.value(); // Solange Delete nicht vollzogen ist, darf noch gelesen werden.
            return;
        }//else
    }

    Database::Lock lock( d_db );
    BtreeCursor cur;
    cur.open( d_db->getStore(), d_db->getObjTable(), false );
    Record::readField( cur, oid, a, v );
}

void Transaction::erase( OID oid )
{
	Database::Lock lock( d_db );
	
	checkLock( oid );
	// Lock ist ab hier zu unseren Gunsten vorhanden.

	if( d_db->d_objDeletes.contains( oid ) )
		throw DatabaseException( DatabaseException::RecordDeleted );
	d_db->d_objDeletes.append( oid );
	// Da commit deletes nur erkennt, wenn d_changes mind. einen Eintrag hat.
	Stream::DataCell& v = d_changes[ qMakePair(quint32(oid),Atom(0)) ];
	if( !v.isNull() )
		v.setNull(); 
}

bool Transaction::isErased( OID oid ) const
{
	Database::Lock lock( d_db );
	return d_db->d_objDeletes.contains( oid );
}

OID Transaction::create()
{
	Database::Lock lock( d_db );
	return d_db->getNextOid();
}

BtreeStore* Transaction::getStore() const
{
	return d_db->getStore();
}

typedef QMap<QPair<quint32,Atom>,Stream::DataCell> Changes;

static void _saveQueue( const Changes& queue, BtreeCursor& cur )
{

	Changes::const_iterator j;
	for( j = queue.begin(); j != queue.end(); ++j )
	{
		const QByteArray oid = DataCell().setOid( j.key().first ).writeCell();
		const QByteArray nr = DataCell().setId32( j.key().second ).writeCell();
		if( j.value().isNull() )
		{
			if( cur.moveTo( oid + nr ) )
				cur.removePos();
		}else
		{
			cur.insert( oid + nr, j.value().writeCell( false, true ) ); // RISK: compression
		}
	}
}

static void _saveMap( const Transaction::Map& m, BtreeCursor& cur )
{
	Transaction::Map::const_iterator j;
	for( j = m.begin(); j != m.end(); ++j )
	{
		if( j.value().isNull() )
		{
			if( cur.moveTo( j.key().d_ba ) )
				cur.removePos();
		}else
		{
			cur.insert( j.key().d_ba, j.value().writeCell( false, true ) ); // RISK: compression
		}
	}
}

static void _eraseQueue( OID id, BtreeCursor& cur, Changes& queue )
{
	const QByteArray oid = DataCell().setOid( id ).writeCell();
	if( cur.moveTo( oid, true ) ) do
	{
		cur.removePos();
	}while( cur.moveTo( oid, true ) );
	Changes::iterator j = queue.lowerBound( qMakePair( quint32(id), quint32(0) ) );
	while( j != queue.end() && j.key().first == id )
	{
		j = queue.erase( j );
	}
}

static void _eraseMap( OID id, BtreeCursor& cur, Transaction::Map& m )
{
	const QByteArray oid = DataCell().setOid( id ).writeCell();
	if( cur.moveTo( oid, true ) ) do
	{
		cur.removePos();
	}while( cur.moveTo( oid, true ) );
	Transaction::Map::iterator j = m.lowerBound( oid );
	while( j != m.end() && j.key().d_ba.startsWith( oid ) )
	{
		j = m.erase( j );
	}
}

void Transaction::commit()
{
	if( !isActive() )
		return;
	if( d_db->isReadOnly() )
	{
		rollback();
		return;
	}
    if( d_commitLock )
        return;
    d_commitLock = true;
    try
	{
		doNotify( UpdateInfo( UpdateInfo::PreCommit ) );
	}catch( ... )
	{
		// RISK: ev. Exceptions abfangen
	}
	Database::Lock lock( d_db );
	{
		BtreeStore::WriteLock lock2( d_db->getStore() );
		
		quint32 oid = 0;
		bool skip = false;
		Changes::const_iterator i;
		BtreeCursor objCur;
		objCur.open( d_db->getStore(), d_db->getObjTable(), true );
		BtreeCursor qCur;
		qCur.open( d_db->getStore(), d_db->getQueTable(), true );
		BtreeCursor mCur;
		mCur.open( d_db->getStore(), d_db->getMapTable(), true );
        BtreeCursor xCur;
		xCur.open( d_db->getStore(), d_db->getOixTable(), true );
		// Changes beinhaltet pro Objekt und geändertem Feld einen Record.
		for( i = d_changes.begin(); i != d_changes.end(); ++i )
		{
			if( i.key().first != oid )
			{
				// Wir sind in einem neuen Objekt angelangt.
				skip = false;
				oid = i.key().first;
				// Lösche das Objekt falls nötig
				if( d_db->d_objDeletes.contains( oid ) )
				{
					// Lösche Record mit allen Bestandteilen aus Store und Indizes
					d_db->d_objDeletes.removeAll( oid );
					Record::Fields f = Record::getFields( objCur, oid );
					for( int j = 0; j < f.size(); j++ )
						removeFromIndex( oid, f[j], objCur );
					Record::eraseFields( objCur, oid );
					// TODO: OID an Freelist hängen
					// Allfällige weitere geänderte Felder werden nach löschen ignoriert
					_eraseQueue( oid, qCur, d_queue );
					_eraseMap( oid, mCur, d_map );
                    _eraseMap( oid, xCur, d_oix );
					skip = true;
				}
				// Entferne den Lock
				// Es kann sein dass Objekt gar nicht gelockt ist.
				d_db->d_objLocks.remove( oid );
			}
			if( !skip )
			{
				if( i.key().second )
				{
					removeFromIndex( oid, i.key().second, objCur ); // alles alte Werte
					Record::writeField( objCur, oid, i.key().second, i.value() );
					addToIndex( oid, d_changes, i.key().second, objCur ); // neue Werte, soweit vorhanden; rest alte
				}else if( i.value().isUuid() )
					Record::setUuid( objCur, oid, i.value().getUuid() );
			}
		}
		// Speichere Bestandteile auf Record-Ebene
		_saveMap( d_map, mCur );
        _saveMap( d_oix, xCur );
		_saveQueue( d_queue, qCur );
		d_changes.clear();
		d_queue.clear();
		d_map.clear();
        d_oix.clear();
		d_uuidCache.clear();
	}
	for( int i = 0; i < d_notify.size(); i++ )
	{
		try
		{
			emit d_db->notify( d_notify[i] );
		}catch( ... )
		{
			// RISK: ev. Exceptions abfangen
		}
	}
	d_notify.clear();
	try
	{
		doNotify( UpdateInfo( UpdateInfo::Commit ) );
	}catch( ... )
	{
		// RISK: ev. Exceptions abfangen
	}
    d_commitLock = false;
}

void Transaction::rollback()
{
	if( !isActive() )
		return;
    if( d_commitLock )
        return;
    d_commitLock = true;
    try
	{
		doNotify( UpdateInfo( UpdateInfo::PreRollback ) );
	}catch( ... )
	{
		// RISK: ev. Exceptions abfangen
	}
	Database::Lock lock( d_db );
	quint32 oid = 0;
	Changes::const_iterator i;
	for( i = d_changes.begin(); i != d_changes.end(); ++i )
	{
		if( i.key().first != oid )
		{
			oid = i.key().first;
			// Entferne die Löschung
			d_db->d_objDeletes.removeAll( oid );
			// Entferne den Lock (falls vorhanden; nicht bei new)
			d_db->d_objLocks.remove( oid );
		}
		// TODO: OIDs von neuen Objekten an Freelist hängen
	}
	d_changes.clear();
	d_queue.clear();
	d_map.clear();
    d_oix.clear();
	d_notify.clear();
	d_uuidCache.clear();
	try
	{
		doNotify( UpdateInfo( UpdateInfo::Rollback ) );
	}catch( ... )
	{
		// RISK: ev. Exceptions abfangen
	}
    d_commitLock = false;
}

OID Transaction::getIdField( OID oid, Atom a ) const
{
	Stream::DataCell v;
	getField( oid, a, v );
	return v.getOid();
}

Obj Transaction::createObject( Atom type, bool createUuid )
{
	OID oid = create();

	if( type && type >= Record::MinReservedField )
		throw DatabaseException(DatabaseException::ReservedName );
	if( type )
		d_changes[qMakePair(quint32(oid),Atom(Record::FieldType)) ].setAtom( type );

	if( createUuid )
	{
		QUuid u = QUuid::createUuid();
		d_changes[qMakePair(quint32(oid),Atom(0)) ].setUuid( u );
		d_uuidCache[u] = oid;
	}

	UpdateInfo c;
	c.d_kind = UpdateInfo::ObjectCreated;
	c.d_id = oid;
	c.d_name = type;
	d_notify.append( c );

	return Obj( oid, this );
}

Obj Transaction::getObject( OID oid ) const
{
	return Obj( oid, const_cast<Transaction*>( this ) );
}

Obj Transaction::getObject( const Stream::DataCell& v ) const
{
	if( v.isOid() )
		return getObject( v.getOid() );
	else if( v.isUuid() )
		return getObject( v.getUuid() );
	else
		return Obj();
}

Obj Transaction::getObject( const QUuid& uuid ) const
{
	OID oid = d_uuidCache.value( uuid );
	if( oid )
		return Obj( oid, const_cast<Transaction*>(this) );
	Database::Lock lock( d_db );
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_db->getObjTable(), false );
	oid = Record::findObject( cur, uuid );
	return Obj( oid, const_cast<Transaction*>(this) );
}

Obj Transaction::createObject( const QUuid& uuid, Atom type )
{
	// TODO: sicherstellen, dass nicht bereits ein Objekt mit der gegebenen uuid existiert
	Obj o = createObject( type );
	d_changes[qMakePair(quint32(o.getOid()),Atom(0)) ].setUuid( uuid );
	d_uuidCache[uuid] = o.getOid();
	return o;
}

Obj Transaction::getOrCreateObject( const QUuid& uuid, Atom type )
{
	Obj o = getObject( uuid );
	if( o.isNull() )
		o = createObject( uuid, type );
	return o;
}

QUuid Transaction::getUuid( OID oid, bool create )
{
	Changes::const_iterator i = d_changes.find( qMakePair(quint32(oid),Atom(0)) );
	if( i != d_changes.end() &&  i.value().isUuid() )
		return i.value().getUuid(); // Solange Delete nicht vollzogen ist, darf noch gelesen werden.
	else
	{
		Database::Lock lock( d_db );
		BtreeCursor cur;
		cur.open( d_db->getStore(), d_db->getObjTable(), false );
		QUuid u = Record::getUuid( cur, oid );
		if( !u.isNull() )
			return u;
		if( !create )
			return QUuid();
		// Es gibt noch keine Uuid
		u = QUuid::createUuid();
		if( isActive() )
		{
			// Wir sind in Transaktion. Schreibe erst bei Commit in die DB.
			d_changes[qMakePair(quint32(oid),Atom(0)) ].setUuid( u );
			d_uuidCache[u] = oid;
		}else
		{
			// Wir sind nicht in Transaktion. Schreibe sofort in die DB.
			BtreeStore::WriteLock lock( d_db->getStore() );
			cur.close();
			cur.open( d_db->getStore(), d_db->getObjTable(), true );
			Record::setUuid( cur, oid, u );
		}
		return u;
	}
}

void Transaction::removeFromIndex(OID id, Atom a, BtreeCursor& objCur)
{
	if( a == 0 )
		return;
	const QByteArray idstr = DataCell().setOid( id ).writeCell();
	QByteArray key;
	QList<quint32> idx = d_db->findIndexForAtom( a );
	// Gehe durch alle Indizes, in denen das Atom vertreten
	for( int i = 0; i < idx.size(); i++ )
	{
		key.clear();
		key.reserve( 255 ); // RISK
		IndexMeta meta;
		if( d_db->getIndexMeta( idx[i], meta ) )
		{
			if( meta.d_kind == IndexMeta::Value || meta.d_kind == IndexMeta::Unique )
			{
				// RISK: zur Zeit wird im Falle von Indizes mit mehreren Feldern dieser Code für jedes
				// geänderte Feld pro Record einmal ausgeführt, was Rechenzeit kostet.
				DataCell value;
				bool allNull = true;
				for( int j = 0; j < meta.d_items.size(); j++ )
				{
					// Gehe durch alle Felder des Index und erzeuge den Schlüsse anhand der aktuellen Werte in der DB.
					// Der Änderungsspeicher ist hier irrelevant, da wir den bestehenden Indexeintrag löschen wollen,
					// der vor der Änderung bestand.
					Record::readField( objCur, id, meta.d_items[j].d_atom, value );
					if( !value.isNull() )
						allNull = false;
					Idx::addElement( key, meta.d_items[j], value );
				}
				if( !key.isEmpty() && !allNull )
				{
					if( meta.d_kind == IndexMeta::Value )
						key += idstr;
					
					BtreeCursor cur;
					cur.open( d_db->getStore(), idx[i], true );
					if( cur.moveTo( key ) )
					{
						// Bei Unique Index nur die Indizes für die eigene ID entfernen.
						if( meta.d_kind != IndexMeta::Unique || cur.readValue() != idstr )
							cur.removePos();
					}
				}
			}
		}
	}
}

void Transaction::addToIndex(OID id, const Changes& all, Atom a, BtreeCursor& objCur )
{
	if( a == 0 )
		return;
	const QByteArray idstr = DataCell().setOid( id ).writeCell();
	QByteArray key;
	QList<quint32> idx = d_db->findIndexForAtom( a );
	// Gehe durch alle Indizes, in denen das Atom vorhanden
	for( int i = 0; i < idx.size(); i++ )
	{
		key.clear();
		key.reserve( 255 ); // RISK
		IndexMeta meta;
		if( d_db->getIndexMeta( idx[i], meta ) )
		{
			if( meta.d_kind == IndexMeta::Value || meta.d_kind == IndexMeta::Unique )
			{
				// RISK: zur Zeit wird im Falle von Indizes mit mehreren Feldern dieser Code für jedes
				// geänderte Feld pro Record einmal ausgeführt, was Rechenzeit kostet.
				DataCell value;
				bool allNull = true; 
				for( int j = 0; j < meta.d_items.size(); j++ )
				{
					// Gehe durch alle Felder des Index und prüfe, ob das Feld entweder im Änderungsspeicher "all"
					// vorhanden ist, oder ob es aus der DB gelesen werden muss.
					// Seit 5.9.10 werden auch Null-Werte in den Index geschrieben, wenn wenigstens ein Element nicht null ist.
					Changes::const_iterator it = all.find( qMakePair( quint32(id), meta.d_items[j].d_atom ) );
					if( it == all.end() )
					{
						// Das Feld wurde nicht geändert, sondern muss im Originalzustand aus der DB geholt werden.
						Record::readField( objCur, id, meta.d_items[j].d_atom, value );
						if( !value.isNull() )
							allNull = false;
						Idx::addElement( key, meta.d_items[j], value );
					}else
					{
						// Das Feld liegt im Änderungsspeicher vor.
						if( !it.value().isNull() )
							allNull = false;
						Idx::addElement( key, meta.d_items[j], it.value() );
					}
				}
				if( !key.isEmpty() && !allNull ) 
				{
					// Wir wollen den Key im Index, sobald mindestens ein Feld im Index einen Wert hat.
					// Die darauf folgenden Felder können null sein; der Eintrag wird trotzdem angelegt.
					// Wenn das nicht so ist, werden z.B. Personen ohne Vornahmen nicht im Namen-Vornamen-Index angelegt.
					if( meta.d_kind == IndexMeta::Value )
						key += idstr; // OID ist Teil des Strings, damit sich mehrere gleiche Values unterscheiden lassen.
					
					BtreeCursor cur;
					cur.open( d_db->getStore(), idx[i], true );
					cur.insert( key, idstr );
				}
			}
		}
	}
}

Obj::Names Transaction::getUsedFields( OID oid ) const
{
	// TODO: teuer
	Database::Lock lock( d_db );
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_db->getObjTable(), false );
	Obj::Names names = Record::getFields( cur, oid, false );
	Changes::const_iterator i;
	for( i = d_changes.lowerBound( qMakePair( quint32(oid), Atom(0) ) ); 
		i != d_changes.end() && i.key().first == oid; ++i )
	{
		if( i.key().second < Record::MinReservedField && !names.contains( i.key().second ) )
			names.append( i.key().second );
	}
	return names;
}

quint32 Transaction::createQSlot( OID oid, const Stream::DataCell& v )
{
	Database::Lock lock( d_db );
	const quint32 nr = d_db->getNextQueueNr( oid );
	setQSlot( oid, nr, v );
	return nr;
}

void Transaction::getQSlot( OID oid, quint32 nr, Stream::DataCell& v ) const
{
	v.setNull();
	if( oid == 0 )
		return;
	Database::Lock lock( d_db );
	if( nr != 0 )
	{
		Changes::const_iterator i = d_queue.find( qMakePair(quint32(oid),nr) );
		if( i != d_queue.end() )
		{
			v = i.value(); // Solange Delete nicht vollzogen ist, darf noch gelesen werden.
			return;
		}//else
	}
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_db->getQueTable(), false );
	DataWriter w;
	w.writeSlot( DataCell().setOid( oid ) );
	if( nr != 0 )
		w.writeSlot( DataCell().setId32( nr ) );
	// nr == 0 gibt als Wert die maximale bisher vergebene nr zurück
	if( cur.moveTo( w.getStream() ) )
	{
		v.readCell( cur.readValue() );
	}
}

void Transaction::setQSlot( OID oid, quint32 nr, const Stream::DataCell& v )
{
	Database::Lock lock( d_db );
	
	checkLock( oid );
	// Lock ist ab hier zu unseren Gunsten vorhanden.

	if( d_db->d_objDeletes.contains( oid ) )
		throw DatabaseException( DatabaseException::RecordDeleted, "cannot write to deleted record" );

	d_queue[ qMakePair(quint32(oid),nr) ] = v;
}

void Transaction::getCell( OID oid, const Obj::KeyList& key, Stream::DataCell& v ) const
{
	v.setNull();
	if( oid == 0 )
		return;
	Database::Lock lock( d_db );
	DataWriter k;
	k.writeSlot( DataCell().setOid( oid ) );
	for( int i = 0; i < key.size(); i++ )
		k.writeSlot( key[i] );
	Map::const_iterator i = d_map.find( k.getStream() );
	if( i != d_map.end() )
	{
		v = i.value(); // Solange Delete nicht vollzogen ist, darf noch gelesen werden.
		return;
	}//else
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_db->getMapTable(), false );
	if( cur.moveTo( k.getStream() ) )
	{
		v.readCell( cur.readValue() );
	}
}

void Transaction::setCell( OID oid, const Obj::KeyList& key, const Stream::DataCell& v )
{
	Database::Lock lock( d_db );
	
	checkLock( oid );
	// Lock ist ab hier zu unseren Gunsten vorhanden.

	if( d_db->d_objDeletes.contains( oid ) )
		throw DatabaseException( DatabaseException::RecordDeleted, "cannot write to deleted record" );

	DataWriter k;
	k.writeSlot( DataCell().setOid( oid ) );
	for( int i = 0; i < key.size(); i++ )
		k.writeSlot( key[i] );
	QByteArray b = k.getStream();
	d_map[ b ] = v;
}

void Transaction::getCell( OID oid, const QByteArray& key, Stream::DataCell& v ) const
{
	v.setNull();
	if( oid == 0 )
		return;
	Database::Lock lock( d_db );
    QByteArray b = DataCell().setOid( oid ).writeCell();
    b += key;
	Map::const_iterator i = d_oix.find( b );
	if( i != d_oix.end() )
	{
		v = i.value(); // Solange Delete nicht vollzogen ist, darf noch gelesen werden.
		return;
	}//else
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_db->getOixTable(), false );
	if( cur.moveTo( b ) )
	{
		v.readCell( cur.readValue() );
	}
}

void Transaction::setCell( OID oid, const QByteArray& key, const Stream::DataCell& v )
{
	Database::Lock lock( d_db );

	checkLock( oid );
	// Lock ist ab hier zu unseren Gunsten vorhanden.

	if( d_db->d_objDeletes.contains( oid ) )
		throw DatabaseException( DatabaseException::RecordDeleted, "cannot write to deleted record" );

    QByteArray b = DataCell().setOid( oid ).writeCell();
    b += key;
	d_oix[ b ] = v;
}

void Transaction::post( const UpdateInfo& info )
{
	d_notify.append( info );
	try
	{
        // NOTE: wenn möglich auf notify verzichten; stattdessen sollen die Adressaten 
        // bei PreCommit die getPendingNotifications durchsehen
		if( d_individualNotify )
			doNotify( info ); // Pre-Commit
	}catch( ... )
	{
		// RISK: ev. Exceptions abfangen
	}
}

void Transaction::doNotify(const UpdateInfo & info)
{
	foreach( Callback cb, d_callbacks )
		cb( this, info );
	emit notify( info );
}

void Transaction::addObserver( QObject* obj, const char* slot, bool asynch )
{
	connect( this, SIGNAL(notify( Udb::UpdateInfo )),
        obj, slot, (asynch)?Qt::QueuedConnection:Qt::DirectConnection );
		// NOTE: connect ist threadsafe
}

void Transaction::removeObserver( QObject* obj, const char* slot )
{
	disconnect( this, SIGNAL(notify( Udb::UpdateInfo )),obj, slot );
	// NOTE: disconnect ist threadsafe
}

void Transaction::addCallback( Callback cb)
{
	d_callbacks.append(cb);
}

void Transaction::removeCallback(Callback cb)
{
	d_callbacks.removeAll(cb);
}
