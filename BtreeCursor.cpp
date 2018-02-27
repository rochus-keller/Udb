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

#include "BtreeCursor.h"
#include "BtreeStore.h"
#include "DatabaseException.h"
#include "Private.h"
#include <QtDebug>
using namespace Udb;

BtreeCursor::BtreeCursor():d_db(0),d_table(0)
{
}

BtreeCursor::~BtreeCursor()
{
	close();
}

void BtreeCursor::open( BtreeStore* db, int table, bool writing )
{
	assert( db );
	BtreeStore::ReadLock lock( db );
	close();
	d_db = db;
	d_table = table;
	int res = sqlite3BtreeCursor( db->getBt(), table, writing, 0, 0, &d_cur );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::CreateBtCursor, ::sqlite3ErrStr( res ) );
}

void BtreeCursor::close()
{
	if( d_db )
	{
		BtreeStore::ReadLock lock( d_db );
		sqlite3BtreeCloseCursor( d_cur );
		d_db = 0;
		d_table = 0;
	}
}

void BtreeCursor::checkOpen() const
{
	if( d_db == 0 )
		throw DatabaseException( DatabaseException::AccessCursor, "cursor not open" );
}

void BtreeCursor::insert( const QByteArray& key, const QByteArray& value )
{
	checkOpen();
	BtreeStore::WriteLock guard( d_db );
	if( key.isEmpty() )
		qWarning( "BtreeCursor::insert funktioniert nicht richtig mit leeren Keys" );
	int res = sqlite3BtreeInsert( d_cur, key.data(), key.size(), 
		value.data(), value.size(), 0, 0 );
	if( res != SQLITE_OK )
	{
		guard.rollback();
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	}
}

QByteArray BtreeCursor::readKey() const
{
	checkOpen();
	BtreeStore::ReadLock lock( d_db );
	i64 size;
	int res = sqlite3BtreeKeySize( d_cur, &size );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	QByteArray data;
	data.resize( size );
	res = sqlite3BtreeKey( d_cur, 0, size, data.data() );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	return data;
}

QByteArray BtreeCursor::readValue() const
{
	checkOpen();
	BtreeStore::ReadLock lock( d_db );
	u32 size;
	int res = sqlite3BtreeDataSize( d_cur, &size );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	QByteArray data;
	data.resize( size );
	res = sqlite3BtreeData( d_cur, 0, size, data.data() );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	return data;
}

bool BtreeCursor::moveFirst()
{
	checkOpen();
	BtreeStore::ReadLock lock( d_db );
	int empty = 0;
	int res = sqlite3BtreeFirst( d_cur, &empty );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	return empty == 0;
}

bool BtreeCursor::moveLast()
{
	checkOpen();
	BtreeStore::ReadLock lock( d_db );
	int empty = 0;
	int res = sqlite3BtreeLast( d_cur, &empty );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	return empty == 0;
}

bool BtreeCursor::moveNext()
{
	checkOpen();
	BtreeStore::ReadLock lock( d_db );
	int eof = 0;
	int res = sqlite3BtreeNext( d_cur, &eof );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	// falls eof befindet sich der Cursor nicht mehr auf einem gültigen Eintrag
	return eof == 0;
}

bool BtreeCursor::movePrev()
{
	checkOpen();
	BtreeStore::ReadLock lock( d_db );
	int bof = 0;
	int res = sqlite3BtreePrevious( d_cur, &bof );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	// falls eof befindet sich der Cursor nicht mehr auf einem gültigen Eintrag
	return bof == 0;
}

bool BtreeCursor::isValidPos()
{
	checkOpen();
	BtreeStore::ReadLock lock( d_db );
	return sqlite3BtreeEof( d_cur ) == 0;
}

bool BtreeCursor::moveTo( const QByteArray& key, bool partial )
{
	checkOpen();
	BtreeStore::ReadLock lock( d_db );
	int compare;
	int res = sqlite3BtreeMoveto( d_cur, key, key.size(), 0, &compare );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
	if( partial )
	{
		// Ziel ist die Position auf den ersten Wert zu setzen, von startsWith(key) erfüllt ist.
		// bei false ist Position entweder auf nächst grösserem Wert als key oder über Rand hinaus.
		if( compare == 0 )
			return true;
		else if( compare > 0 )
			return readKey().startsWith( key );
		else
		{
			if( !moveNext() ) // moveNext verschiebt den Cursor allenfalls über den Schluss hinaus
				return false;
			else
				return readKey().startsWith( key );
		}
	}else
	{
		if( compare < 0 )
			moveNext();
		return compare == 0; // 0..genauer match
	}
}

bool BtreeCursor::moveNext(const QByteArray& key)
{
	if( moveNext() )
		return readKey().startsWith( key );
	else
		return false;
}

void BtreeCursor::removePos()
{
	checkOpen();
	BtreeStore::WriteLock guard( d_db );
	int res = sqlite3BtreeDelete( d_cur );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::AccessCursor, sqlite3ErrStr( res ) );
}
