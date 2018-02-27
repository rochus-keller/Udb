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

#include "Global.h"
#include "Database.h"
#include "BtreeCursor.h"
#include "BtreeStore.h"
#include "DatabaseException.h"
using namespace Udb;

Global::Global(Database * db, QObject *parent) :
	QObject(parent), d_db(db), d_table(0)
{
	Q_ASSERT( db != 0 );
}

bool Global::open(quint32 id)
{
	d_db->checkOpen();
	if( id == d_db->d_meta.d_objTable || id == d_db->d_meta.d_dirTable ||
		id == d_db->d_meta.d_idxTable || id == d_db->d_meta.d_queTable ||
		id == d_db->d_meta.d_mapTable || id == d_db->d_meta.d_oixTable )
		return false; // id gehört einer internen Tabelle
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_db->getIdxTable(), false );
	if( cur.moveTo( Stream::DataCell().setId32( id ).writeCell() ) )
		return false; // id gehört einem Index
	d_oix.clear();
	d_table = id;
	return true;
}

quint32 Global::create()
{
	d_db->checkOpen();
	Database::TxnGuard lock( d_db );
	d_table = d_db->getStore()->createTable();
	d_oix.clear();
	return d_table;
}

void Global::setCell(const Global::KeyList &key, const Stream::DataCell &value)
{
	Stream::DataWriter k;
	for( int i = 0; i < key.size(); i++ )
		k.writeSlot( key[i] );
	setCell( k.getStream(), value.writeCell(false,true) );
}

void Global::getCell(const Global::KeyList &key, Stream::DataCell &value) const
{
	Stream::DataWriter k;
	for( int i = 0; i < key.size(); i++ )
		k.writeSlot( key[i] );
	QByteArray v;
	getCell( k.getStream(), v );
	value.readCell(v);
}

Stream::DataCell Global::getCell(const Global::KeyList &key) const
{
	Stream::DataCell v;
	getCell( key, v );
	return v;
}

void Global::setCell(const QByteArray &key, const QByteArray &value)
{
	checkOpen();
	Database::Lock lock( d_db );

	d_oix[ key ] = value;
}

void Global::getCell(const QByteArray &key, QByteArray &value) const
{
	checkOpen();
	value.clear();
	Database::Lock lock( d_db );
	Map::const_iterator i = d_oix.find( key );
	if( i != d_oix.end() )
	{
		value = i.value();
		return;
	}//else
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_table, false );
	if( cur.moveTo( key ) )
		value = cur.readValue();
}

QByteArray Global::getCell(const QByteArray &key) const
{
	QByteArray v;
	getCell( key, v );
	return v;
}

void Global::clearAllCells()
{
	checkOpen();
	Database::Lock lock( d_db );
	BtreeStore::WriteLock lock2( d_db->getStore() );
	d_db->getStore()->clearTable( d_table );
	d_oix.clear();
}

Git Global::findCells(const QByteArray &key) const
{
	checkOpen();
	Git m( this );
	if( m.seek( key ) )
		return m;
	else
		return Git();
}

Git Global::findCells(const Global::KeyList &key) const
{
	Stream::DataWriter k;
	for( int i = 0; i < key.size(); i++ )
		k.writeSlot( key[i] );
	return findCells( k.getStream() );
}

void Global::commit()
{
	checkOpen();
	Database::Lock lock( d_db );
	BtreeStore::WriteLock lock2( d_db->getStore() );
	BtreeCursor cur;
	cur.open( d_db->getStore(), d_table, true );
	Map::const_iterator j;
	for( j = d_oix.begin(); j != d_oix.end(); ++j )
	{
		if( j.value().isNull() )
		{
			if( cur.moveTo( j.key().d_ba ) )
				cur.removePos();
		}else
		{
			cur.insert( j.key().d_ba, j.value() );
		}
	}
	d_oix.clear();
}

void Global::rollback()
{
	checkOpen();
	Database::Lock lock( d_db );
	d_oix.clear();
}

void Global::checkOpen() const
{
	d_db->checkOpen();
	if( d_table == 0 )
		throw DatabaseException( DatabaseException::NotOpen );
}

BtreeStore *Global::getStore() const
{
	return d_db->getStore();
}

bool Git::seek(const QByteArray &key)
{
	checkNull();
	Database::Lock lock( d_global->d_db );
	d_key = key;
	d_cur.clear();
	if( d_key.isEmpty() )
		return firstKey();

	BtreeCursor cur;
	cur.open( d_global->getStore(), d_global->d_table );
	if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

void Git::getValue(QByteArray &v) const
{
	checkNull();
	v.clear();
	if( !d_cur.startsWith( d_key ) )
		return;

	Database::Lock lock( d_global->d_db );
	BtreeCursor cur;
	cur.open( d_global->getStore(), d_global->d_table, false );
	if( cur.moveTo( d_cur ) )
	{
		v = cur.readValue();
	}
}

QByteArray Git::getValue() const
{
	QByteArray v;
	getValue( v );
	return v;
}

QByteArray Git::getKey() const
{
	if( !d_cur.startsWith( d_key ) )
		return QByteArray();
	return d_cur;
}

Git::KeyList Git::getKeyAsList() const
{
	KeyList res;
	if( !d_cur.startsWith( d_key ) )
		return res;
	Stream::DataReader r( d_cur );
	Stream::DataReader::Token t = r.nextToken();
	while( t == Stream::DataReader::Slot )
	{
		Stream::DataCell v;
		r.readValue( v );
		res.append( v ); // erster ist Oid
		t = r.nextToken();
	}
	return res;
}

Stream::DataCell Git::getValueAsCell() const
{
	Stream::DataCell v;
	v.readCell( getValue() );
	return v;
}

bool Git::firstKey()
{
	checkNull();
	Database::Lock lock( d_global->d_db );
	d_cur.clear();
	BtreeCursor cur;
	cur.open( d_global->getStore(), d_global->d_table );
	if( d_key.isEmpty() )
	{
		if( cur.moveFirst() )
		{
			d_cur = cur.readKey();
			return true;
		}else
			return false;
	}else if( cur.moveTo( d_key, true ) )
	{
		d_cur = cur.readKey();
		return true;
	}else
		return false;
}

bool Git::nextKey()
{
	checkNull();
	Database::Lock lock( d_global->d_db );
	BtreeCursor cur;
	cur.open( d_global->getStore(), d_global->d_table );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.moveNext() )
	{
		d_cur = cur.readKey();
		return d_cur.startsWith( d_key );
	}else
		return false;
}

bool Git::prevKey()
{
	checkNull();
	Database::Lock lock( d_global->d_db );
	BtreeCursor cur;
	cur.open( d_global->getStore(), d_global->d_table );
	cur.moveTo( d_cur ); // zur letztbekannten oder neu verlangten Position
	if( cur.movePrev() )
	{
		d_cur = cur.readKey();
		return d_cur.startsWith( d_key );
	}else
		return false;
}

Git &Git::assign(const Git &r)
{
	if( d_global == r.d_global && d_cur == r.d_cur && d_key == r.d_key )
		return *this;
	d_global = r.d_global;
	d_cur = r.d_cur;
	d_key = r.d_key;
	return *this;
}

void Git::checkNull() const
{
	if( d_global == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "Git::checkNull");
	d_global->checkOpen();
}

Git::Git(const Global *g)
{
	d_global = g;
}
