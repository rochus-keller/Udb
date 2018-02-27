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

#include "Obj.h"
#include "Transaction.h"
#include "Record.h"
#include "Database.h"
#include "DatabaseException.h"
#include "UpdateInfo.h"
#include <QtDebug>
#include <QMimeData>
using namespace Udb;
using namespace Stream;

const char* Obj::s_mimeObjectRefs = "application/udb/object-refs";
const char* Obj::s_xoidScheme = "xoid";

Obj::Obj( OID oid, Transaction* t ):d_oid(oid),d_txn(t)
{
}

Obj::Obj()
{
	d_oid = 0;
	d_txn = 0;
}

Obj::Obj( const Obj& lhs )
{
	d_oid = 0;
	d_txn = 0;
	*this = lhs;
}

bool Obj::isErased() const
{
	if( d_oid )
		return d_txn->isErased( d_oid );
	else
		return false;
}

void Obj::erase()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	if( d_txn->isErased( d_oid ) )
		return;
	Obj obj = getFirstObj();
	if( !obj.isNull() ) do
	{
		Obj o = obj;
		const bool hasNext = obj.next();
		o.erase();
		if( !hasNext )
			break;
	}while( true );
	deaggregateImp(true);
	const Atom type = getType();
	d_txn->erase( d_oid );

	UpdateInfo c;
	c.d_kind = UpdateInfo::ObjectErased;
	c.d_id = d_oid;
	c.d_name = type;
	d_txn->post( c );
}

Obj::operator Stream::DataCell() const
{
	Stream::DataCell v;
	if( d_oid )
		v.setOid( d_oid );
	else
		v.setNull();
	return v; 
}

Obj Obj::getFirstObj() const
{
	if( d_oid == 0 )
		return Obj();
	DataCell id;
	d_txn->getField( d_oid, Record::FieldFirstObj, id );
	return Obj( id.getOid(), d_txn );
}

Obj Obj::getLastObj() const
{
	if( d_oid == 0 )
		return Obj();
	DataCell id;
	d_txn->getField( d_oid, Record::FieldLastObj, id );
	return Obj( id.getOid(), d_txn );
}

bool Obj::next()
{
	if( d_oid == 0 )
		return false;
	DataCell id;
	d_txn->getField( d_oid, Record::FieldNextObj, id );
	if( id.isNull() )
		return false; 
	d_oid = id.getOid();
	return true;
}

Obj Obj::getNext() const
{
	if( d_oid == 0 )
		return Obj();
	DataCell id;
	d_txn->getField( d_oid, Record::FieldNextObj, id );
	if( id.isNull() )
		return Obj(); 
	else
		return d_txn->getObject( id.getOid() );
}

bool Obj::prev()
{
	if( d_oid == 0 )
		return false;
	DataCell id;
	d_txn->getField( d_oid, Record::FieldPrevObj, id );
	if( id.isNull() )
		return false; 
	d_oid = id.getOid();
	return true;
}

Obj Obj::getPrev() const
{
	if( d_oid == 0 )
		return Obj();
	DataCell id;
	d_txn->getField( d_oid, Record::FieldPrevObj, id );
	if( id.isNull() )
		return Obj(); 
	else 
		return d_txn->getObject( id.getOid() );
}

Obj Obj::getParent() const
{
	if( d_oid == 0 )
		return Obj();
	DataCell id;
	d_txn->getField( d_oid, Record::FieldParent, id );
	return Obj( id.getOid(), d_txn );
}

quint64 Obj::deaggregateImp(bool notify)
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	OID parent = getParent().getOid();
	if( parent == 0 )
		return 0;
	OID prev = d_txn->getIdField( d_oid, Record::FieldPrevObj );
	OID next = d_txn->getIdField( d_oid, Record::FieldNextObj );
	if( next == 0 || prev == 0 )
	{
		if( prev == 0 && next == 0 )
		{
			d_txn->setField( parent, Record::FieldFirstObj, DataCell().setNull() );
			d_txn->setField( parent, Record::FieldLastObj, DataCell().setNull() );
		}else if( prev == 0 )
		{
			d_txn->setField( parent, Record::FieldFirstObj, DataCell().setOid( next ) );
			d_txn->setField( next, Record::FieldPrevObj, DataCell().setNull() );
		}else
		{
			d_txn->setField( parent, Record::FieldLastObj, DataCell().setOid( prev ) );
			d_txn->setField( prev, Record::FieldNextObj, DataCell().setNull() );
		}
	}else
	{
		d_txn->setField( prev, Record::FieldNextObj, DataCell().setOid( next ) );
		d_txn->setField( next, Record::FieldPrevObj, DataCell().setOid( prev ) );
	}
	d_txn->setField( d_oid, Record::FieldPrevObj, DataCell().setNull() );
	d_txn->setField( d_oid, Record::FieldNextObj, DataCell().setNull() );
	d_txn->setField( d_oid, Record::FieldParent, DataCell().setNull() );
	if( notify && parent != 0 )
	{
		UpdateInfo c;
		c.d_kind = UpdateInfo::Deaggregated;
		c.d_id = d_oid;
		c.d_parent = parent;
		c.d_name2 = getType();
		d_txn->post( c );
	}
	return parent;
}

void Obj::aggregateTo(const Obj& parent, const Obj& before)
{
    checkNull();
	Database::Lock lock( d_txn->getDb() );

    // Deaggregate auch wenn parent.isNull(); Aggregat zu Null-Parent entspricht Deaggregate
	deaggregateImp(true);

	if( parent.isNull() )
		return;

	d_txn->setField( d_oid, Record::FieldParent, parent );

    if( !before.isNull() )
    {
        if( !before.getParent().equals( parent ) )
            // Wir haben zwar einen before, aber der gehört nicht zum gewünschten Parent
            throw DatabaseException( DatabaseException::WrongContext );

        const OID beforeBefore = d_txn->getIdField( before.d_oid, Record::FieldPrevObj );

		if( beforeBefore == 0 )
		{
			// Es gibt kein Aggregat vor Before. Füge this als erstes in Parent ein
			d_txn->setField( before.d_oid, Record::FieldPrevObj, DataCell().setOid( d_oid ) );
			d_txn->setField( d_oid, Record::FieldNextObj, DataCell().setOid( before.d_oid ) );
			d_txn->setField( parent.d_oid, Record::FieldFirstObj, DataCell().setOid( d_oid ) );
		}else
		{
			d_txn->setField( before.d_oid, Record::FieldPrevObj, DataCell().setOid( d_oid ) );
			d_txn->setField( beforeBefore, Record::FieldNextObj, DataCell().setOid( d_oid ) );
			d_txn->setField( d_oid, Record::FieldPrevObj, DataCell().setOid( beforeBefore ) );
			d_txn->setField( d_oid, Record::FieldNextObj, DataCell().setOid( before.d_oid ) );
		}

        UpdateInfo c;
        c.d_kind = UpdateInfo::Aggregated;
        c.d_id = d_oid;
        c.d_parent = parent.d_oid;
        c.d_before = before.d_oid;
        d_txn->post( c );
    }else
    {
        OID lastInParent = d_txn->getIdField( parent.d_oid, Record::FieldLastObj );
        if( lastInParent == 0 )
        {
            // parentOid hat noch keine Aggregate
            d_txn->setField( parent.d_oid, Record::FieldFirstObj, DataCell().setOid( d_oid ) );
            d_txn->setField( parent.d_oid, Record::FieldLastObj, DataCell().setOid( d_oid ) );
        }else
        {
            // parentOid hat bereits ein oder mehrere Aggregate
            d_txn->setField( lastInParent, Record::FieldNextObj, DataCell().setOid( d_oid ) );
            d_txn->setField( d_oid, Record::FieldPrevObj, DataCell().setOid( lastInParent ) );
            d_txn->setField( parent.d_oid, Record::FieldLastObj, DataCell().setOid( d_oid ) );
        }
        UpdateInfo c;
        c.d_kind = UpdateInfo::Aggregated;
        c.d_id = d_oid;
        c.d_parent = parent.d_oid;
        d_txn->post( c );
    }
}

Obj Obj::createAggregate( Atom type, const Obj& before, bool createUuid )
{
	checkNull();
	Obj sub = d_txn->createObject( type, createUuid );
	sub.aggregateTo( *this, before );
	return sub;
}

void Obj::deaggregate()
{
	deaggregateImp(true);
}

Obj Obj::getValueAsObj( Atom name ) const
{
	DataCell v = getValue( name );
    return Obj( v.getOid(), d_txn );
}

void Obj::setValueAsObj(Atom name, const Obj & o)
{
    Stream::DataCell v;
    if( !o.isNull() )
        v.setOid( o.getOid() );
    else
        v.setNull();
    setValue( name, v );
}

Obj& Obj::assign( const Obj& r )
{
	if( d_oid == r.d_oid )
		return *this;
	d_oid = r.d_oid;
	d_txn = r.d_txn;
	return *this;
}	

void Obj::checkNull() const
{
	if( d_oid == 0 || d_txn == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "Obj::checkNull");
}

void Obj::setValuePriv( const Stream::DataCell& cell, quint32 name )
{
	checkNull();
	UpdateInfo c;
	if( name == Record::FieldType )
	{
		Stream::DataCell v;
		d_txn->getField( d_oid, name, v );
		c.d_kind = UpdateInfo::TypeChanged;
		d_txn->setField( d_oid, name, cell );
		c.d_before = v.getAtom();
		c.d_name = cell.getAtom();
	}else
	{
		c.d_kind = UpdateInfo::ValueChanged;
		c.d_name = name;
		d_txn->setField( d_oid, name, cell );
	}
	c.d_id = d_oid;
	d_txn->post( c );
}

void Obj::setValue( quint32 name, const Stream::DataCell& cell )
{
	if( name >= Record::MinReservedField )
		throw DatabaseException(DatabaseException::ReservedName );
	setValuePriv( cell, name );
}

void Obj::clearValue( Atom name )
{
	setValue( name, Stream::DataCell().setNull() );
}

void Obj::setString( Atom name, const QString& str )
{
    setValue( name, DataCell().setString( str ) );
}

void Obj::setAscii(Atom name, const QByteArray & str)
{
    setValue( name, DataCell().setAscii( str ) );
}

void Obj::setLatin1(Atom name, const QByteArray & str)
{
    setValue( name, DataCell().setLatin1( str ) );
}

void Obj::setBool(Atom name, bool on)
{
    setValue( name, DataCell().setBool( on ) );
}

QString Obj::getString( Atom name, bool stripMarkup ) const
{
	return getValue( name ).toString( stripMarkup );
}

void Obj::setTimeStamp( quint32 name )
{
    setValue( name, DataCell().setDateTime( QDateTime::currentDateTime() ) );
}

quint32 Obj::incCounter(Atom name)
{
    quint32 val = getValue( name ).getUInt32();
    if( val == 0xffffffff )
        return val;
    val++;
    setValue( name, DataCell().setUInt32( val ) );
    return val;
}

quint32 Obj::decCounter(Atom name)
{
    quint32 val = getValue( name ).getUInt32();
    if( val == 0 )
        return val;
    val--;
    setValue( name, DataCell().setUInt32( val ) );
    return val;
}

void Obj::getValue( quint32 name, Stream::DataCell& v, bool forceOldValue ) const
{
	checkNull();
	if( name == 0 ) // kein gültigerName führt immer zu Null-Ergebnis
		v.setNull();
	else
		d_txn->getField( d_oid, name, v, forceOldValue );
}

bool Obj::hasValue( Atom name ) const
{
	checkNull();
	DataCell v;
	d_txn->getField( d_oid, name, v );
	return !v.isNull();
}

Stream::DataCell Obj::getValue( quint32 name, bool forceOldValue ) const
{
	Stream::DataCell v;
	getValue( name, v, forceOldValue );
	return v;
}

QUuid Obj::getUuid(bool create) const
{
	if( d_oid == 0 )
		return QUuid();
	else
		return d_txn->getUuid( d_oid, create );
}

Atom Obj::getType() const
{
	if( d_oid == 0 )
		return 0;
	else
		return getValue( Record::FieldType ).getAtom();
}

void Obj::setType( Atom t )
{
	if( t == 0 )
		setValuePriv( Stream::DataCell().setNull(), Record::FieldType );
	else
        setValuePriv( Stream::DataCell().setAtom( t ), Record::FieldType );
}

bool Obj::isNull(bool checkType, bool checkErased) const
{
     if( d_oid == 0 )
         return true;
     else if( checkType && getType() == 0 )
         return true;
     else if( checkErased && isErased() )
         return true;
     return false;
}

Obj::Names Obj::getNames() const
{
	checkNull();
	return d_txn->getUsedFields( d_oid );
}

Database* Obj::getDb() const
{
	checkNull();
    return d_txn->getDb();
}

bool Obj::equals(const Obj &rhs) const
{
    return d_oid != 0 && d_oid == rhs.d_oid && d_txn == rhs.d_txn;
}

Qit Obj::getFirstSlot() const
{
	if( d_oid == 0 )
		return Qit();
	Qit q( d_oid, d_txn, 0 );
	if( q.first() )
		return q;
	else
		return Qit();
}

Qit Obj::getLastSlot() const
{
	if( d_oid == 0 )
		return Qit();
	Qit q( d_oid, d_txn, 0 );
	if( q.last() )
		return q;
	else
		return Qit();
}

Qit Obj::appendSlot( const Stream::DataCell& cell )
{
	if( d_oid == 0 )
		return Qit();
	Database::Lock lock( d_txn->getDb() );
	const quint32 nr = d_txn->createQSlot( d_oid, cell );
	UpdateInfo c;
	c.d_kind = UpdateInfo::QueueAdded;
	c.d_id = nr;
	c.d_parent = d_oid;
	d_txn->post( c );
	return Qit( d_oid, d_txn, nr );
}

Qit Obj::getSlot( quint32 nr ) const
{
	if( d_oid == 0 )
		return Qit();
	DataCell maxnr;
	d_txn->getQSlot( d_oid, 0, maxnr );
	if( nr > maxnr.getId32() )
		return Qit();
	else
		return Qit( d_oid, d_txn, nr );
}


void Obj::setCell( const KeyList& key, const Stream::DataCell& value )
{
	Database::TxnGuard lock( d_txn->getDb() );
	d_txn->setCell( d_oid, key, value );
	UpdateInfo c;
	c.d_kind = UpdateInfo::MapChanged;
	c.d_id = d_oid;
	c.d_key = key; 
	d_txn->post( c );
}

void Obj::setCell( const QByteArray& key, const Stream::DataCell& value )
{
	Database::TxnGuard lock( d_txn->getDb() );
	d_txn->setCell( d_oid, key, value );
	UpdateInfo c;
	c.d_kind = UpdateInfo::MapChanged;
	c.d_id = d_oid;
	c.d_key << DataCell().setLob( key, false );
	d_txn->post( c );
}

void Obj::getCell( const KeyList& key, Stream::DataCell& value ) const
{
	value.setNull();
	if( d_oid == 0 )
		return;
	d_txn->getCell( d_oid, key, value );
}

void Obj::getCell( const QByteArray& key, Stream::DataCell& value ) const
{
	value.setNull();
	if( d_oid == 0 )
		return;
	d_txn->getCell( d_oid, key, value );
}

Stream::DataCell Obj::getCell( const KeyList& key ) const
{
	Stream::DataCell v;
	getCell( key, v );
	return v;	
}

Stream::DataCell Obj::getCell( const QByteArray& key ) const
{
	Stream::DataCell v;
	getCell( key, v );
	return v;
}

Mit Obj::findCells( const KeyList& key ) const
{
	// NOTE: mit findCells(KeyList()) erhält man alle Keys. Dagegen erhält man auch
	// Schrott, wenn man direkt Mit(Obj) und firstKey aufruft.
	if( d_oid == 0 )
		return Mit();
	Mit m( d_oid, d_txn );
	if( m.seek( key ) )
		return m;
	else
		return Mit();
}

Xit Obj::findCells( const QByteArray& key ) const
{
	if( d_oid == 0 )
		return Xit();
	Xit m( *this );
	if( m.seek( key ) )
		return m;
	else
		return Xit();
}

Obj Obj::getObject( OID oid ) const
{
	checkNull();
	return d_txn->getObject( oid );
}

Obj Obj::createObject( Atom type, bool createUuid ) const
{
	checkNull();
	return d_txn->createObject( type, createUuid );
}

Atom Obj::getAtom( const QByteArray& name ) const
{
	checkNull();
    return d_txn->getAtom( name );
}

Obj::ValueList Obj::unpackArray(const DataCell & v)
{
    Obj::ValueList res;
    if( !v.isBml() )
        return res;
    DataReader r( v );
    DataReader::Token t = r.nextToken();
    while( t == DataReader::Slot )
    {
        res.append( r.getValue() );
        t = r.nextToken();
    }
    return res;
}

DataCell Obj::packArray(const Obj::ValueList & a)
{
    DataWriter w;
    for( int i = 0; i < a.size(); i++ )
        w.writeSlot( a[i] );
    return w.getBml();
}

void Obj::commit()
{
	checkNull();
	d_txn->commit();
}

QList<Udb::Obj> Obj::readObjectRefs(const QMimeData *data, Udb::Transaction *txn)
{
    Q_ASSERT( txn != 0 );
    QList<Udb::Obj> res;
    if( !data->hasFormat( QLatin1String( Udb::Obj::s_mimeObjectRefs ) ) )
        return res;
    Stream::DataReader r( data->data(QLatin1String( Udb::Obj::s_mimeObjectRefs )) );
    Stream::DataReader::Token t = r.nextToken();
    if( t != Stream::DataReader::Slot || r.getValue().getUuid() != txn->getDb()->getDbUuid() )
        return res; // Objekte leben in anderer Datenbank; kein Move möglich.
    t = r.nextToken();
    while( t == Stream::DataReader::Slot && r.getValue().isOid() )
    {
        Udb::Obj o = txn->getObject( r.getValue() );
        if( !o.isNull(true,true) )
            res.append( o );
        t = r.nextToken();
    }
    return res;
}

void Obj::writeObjectRefs(QMimeData *data, const QList<Obj> & objs)
{
    if( objs.isEmpty() )
        return;
    Q_ASSERT( !objs.first().isNull() );
    // Schreibe ItemRefs
    Stream::DataWriter out;
    out.writeSlot( Stream::DataCell().setUuid( objs.first().getDb()->getDbUuid() ) );
    foreach( Udb::Obj o, objs )
        out.writeSlot( o );
    data->setData( s_mimeObjectRefs, out.getStream() );
}

bool Obj::isLocalObjectRefs(const QMimeData *data, Transaction *txn)
{
    Stream::DataReader r( data->data( s_mimeObjectRefs ) );
    return ( r.nextToken() == Stream::DataReader::Slot && r.getValue().isUuid() &&
        r.getValue().getUuid() == txn->getDb()->getDbUuid() );
}

QUrl Obj::objToUrl(const Obj & o, Atom id, Atom txt )
{
	if( o.isNull() )
		return QUrl();
	// URI-Schema gem. http://en.wikipedia.org/wiki/URI_scheme und http://tools.ietf.org/html/rfc3986 bzw. http://tools.ietf.org/html/rfc6068
	// Format: xoid:49203@348c2483-206a-40a6-835a-9b9753b60188?id=MO1425;txt=Dies ist ein Text
	QUrl url = oidToUrl( o.getOid(), o.getDb()->getDbUuid() );
	if( id != 0 || txt != 0 )
	{
		url.setQueryDelimiters( '=', ';' );
		QList<QPair<QString, QString> > items;
		if( id != 0 && o.hasValue( id ) )
			items.append( qMakePair( QString("id"), o.getString(id) ) );
		if( txt != 0 )
			items.append( qMakePair( QString("txt"), o.getString(txt) ) );
		url.setQueryItems( items );
	}
	return url;
}

QUrl Obj::oidToUrl(OID oid, const QUuid &dbid)
{
	QUrl url;
	url.setScheme( s_xoidScheme );
	QString uid = dbid.toString().mid(1);
	uid.chop(1);
	url.setAuthority( QString("%1@%2").arg( oid ).arg( uid ) );
	return url;
}

void Obj::writeObjectUrls(QMimeData *data, const QList<Obj> & objs, Atom id, Atom txt )
{
	if( objs.isEmpty() )
		return;
	QList<QUrl> urls;
	foreach( Udb::Obj o, objs )
		urls.append( objToUrl( o, id, txt ) );
	if( !urls.isEmpty() )
		data->setUrls( urls );
}

