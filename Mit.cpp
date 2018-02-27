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

#include "Mit.h"
#include "Record.h"
#include "DatabaseException.h"
#include "Transaction.h"
#include "Database.h"
#include <Udb/BtreeCursor.h>
#include <Stream/DataCell.h>
#include <Stream/BmlRecord.h>
using namespace Udb;
using namespace Stream;

Mit::Mit( OID oid, Transaction* txn )
{
	d_oid = oid;
	d_txn = txn;
}

Mit::Mit( const Mit& lhs )
{
	d_oid = 0;
	d_txn = 0;
	d_cur.clear();
	d_key.clear();
	*this = lhs;
}

bool Mit::seek( const KeyList& key )
{
	// RISK: sucht nur in Db, nicht in Transaktion
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	d_key.clear();
	d_cur.clear();
	DataWriter w;
	w.writeSlot( DataCell().setOid( d_oid ) );
	for( int i = 0; i < key.size(); i++ )
		w.writeSlot( key[i] );
	d_key = w.getStream();

	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable() );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

Mit::~Mit()
{
}

Mit& Mit::assign( const Mit& r )
{
	if( d_oid == r.d_oid && d_cur == r.d_cur && d_key == r.d_key )
		return *this;
	d_oid = r.d_oid;
	d_txn = r.d_txn;
	d_cur = r.d_cur;
	d_key = r.d_key;
	return *this;
}	

void Mit::checkNull() const
{
	if( d_oid == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "Mit::checkNull");
}

void Mit::getValue( Stream::DataCell& v ) const
{
	checkNull();
	v.setNull();
	if( !d_cur.startsWith( d_key ) )
		return;

	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable(), false );
	if( cur.moveTo( d_cur ) )
	{
		v.readCell( cur.readValue() );
	}
}

Stream::DataCell Mit::getValue() const
{
	Stream::DataCell v;
	getValue( v );
	return v;
}

Mit::KeyList Mit::getKey() const
{
	KeyList res;
	if( !d_cur.startsWith( d_key ) )
		return res;
	DataReader r( d_cur );
	DataReader::Token t = r.nextToken();
	bool first = true;
	while( t == DataReader::Slot )
	{
		Stream::DataCell v;
		r.readValue( v );
		if( !first )
			res.append( v ); // erster ist Oid
		first = false;
		t = r.nextToken();
	}
	return res;
}

bool Mit::firstKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	d_cur.clear();
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable() );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Mit::nextKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable() );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.moveNext() )
	{
		d_cur = cur.readKey();
		return d_cur.startsWith( d_key );
	}else
		return false;
}

bool Mit::prevKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getMapTable() );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.movePrev() )
	{
		d_cur = cur.readKey();
		return d_cur.startsWith( d_key );
	}else
		return false;
}

Xit::Xit( OID oid, Transaction* txn )
{
	d_oid = oid;
	d_txn = txn;
}

Xit::Xit( const Xit& lhs )
{
	d_oid = 0;
	d_txn = 0;
	d_cur.clear();
	d_key.clear();
	*this = lhs;
}

Xit::Xit(const Obj & o)
{
	d_oid = o.getOid();
	d_txn = o.getTxn();
}

bool Xit::seek( const QByteArray& key )
{
	// RISK: sucht nur in Db, nicht in Transaktion
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	d_key.clear();
	d_cur.clear();
    d_key = DataCell().setOid( d_oid ).writeCell();
    d_key += key;

	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getOixTable() );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

Xit::~Xit()
{
}

Xit& Xit::assign( const Xit& r )
{
	if( d_oid == r.d_oid && d_cur == r.d_cur && d_key == r.d_key )
		return *this;
	d_oid = r.d_oid;
	d_txn = r.d_txn;
	d_cur = r.d_cur;
	d_key = r.d_key;
	return *this;
}

void Xit::checkNull() const
{
	if( d_oid == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "Xit::checkNull");
}

void Xit::getValue( Stream::DataCell& v ) const
{
	checkNull();
	v.setNull();
	if( !d_cur.startsWith( d_key ) )
		return;

	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getOixTable(), false );
	if( cur.moveTo( d_cur ) )
	{
		v.readCell( cur.readValue() );
	}
}

Stream::DataCell Xit::getValue() const
{
	Stream::DataCell v;
	getValue( v );
	return v;
}

QByteArray Xit::getKey() const
{
	if( !d_cur.startsWith( d_key ) )
        return QByteArray();
    const QByteArray b = DataCell().setOid( d_oid ).writeCell();
    return d_cur.mid( b.size() );
}

bool Xit::firstKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	d_cur.clear();
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getOixTable() );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Xit::nextKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getOixTable() );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.moveNext() )
	{
		d_cur = cur.readKey();
		return d_cur.startsWith( d_key );
	}else
		return false;
}

bool Xit::prevKey()
{
	checkNull();
	Database::Lock lock( d_txn->getDb() );
	BtreeCursor cur;
	cur.open( d_txn->getDb()->getStore(), d_txn->getDb()->getOixTable() );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.movePrev() )
	{
		d_cur = cur.readKey();
		return d_cur.startsWith( d_key );
	}else
		return false;
}
