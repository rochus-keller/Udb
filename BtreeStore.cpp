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

#include "BtreeStore.h"
#include "DatabaseException.h"
#include <Sqlite3/sqlite3.h>
#include "Private.h"
#include <QBuffer>
#include <cassert>
using namespace Udb;

static const int s_pageSize = SQLITE_DEFAULT_PAGE_SIZE;
static const int s_schema = 15;

BtreeStore::ReadLock::ReadLock( BtreeStore* db ):d_db(db)
{
	assert( db );
#ifdef BTREESTORE_HAS_MUTEX
	d_db->d_lock.lock();
#endif
}

BtreeStore::ReadLock::~ReadLock()
{
#ifdef BTREESTORE_HAS_MUTEX
	d_db->d_lock.unlock();
#endif
}

BtreeStore::WriteLock::WriteLock( BtreeStore* db ):d_db(db)
{
	assert( db );
#ifdef BTREESTORE_HAS_MUTEX
	d_db->d_lock.lock();
#endif
	if( !d_db->isReadOnly() )
		d_db->transBegin();
}

void BtreeStore::WriteLock::rollback()
{
	if( d_db )
	{
		if( !d_db->isReadOnly() )
			d_db->transAbort();
#ifdef BTREESTORE_HAS_MUTEX
		d_db->d_lock.unlock();
#endif
		d_db = 0;
	}
}

BtreeStore::WriteLock::~WriteLock()
{
	if( d_db )
	{
		if( !d_db->isReadOnly() )
			d_db->transCommit();
#ifdef BTREESTORE_HAS_MUTEX
		d_db->d_lock.unlock();
#endif
	}
}

BtreeStore::BtreeStore( QObject* owner ):
	QObject( owner ), d_db(0), d_metaTable(0), d_txnLevel( 0)
#ifdef BTREESTORE_HAS_MUTEX
		,d_lock(QMutex::Recursive)
#endif
{
}

BtreeStore::~BtreeStore()
{
	close();
}

void BtreeStore::setCacheSize( int numOfPages )
{
	checkOpen();
	// keine wesentliche Wirkung: 
	// Default-Grösse ist 100. Minimalgrösse ist 10.
	sqlite3BtreeSetCacheSize( getBt(), numOfPages );
	// Es gilt SQLITE_MAX_PAGE_COUNT, zur Zeit 1'073'741'823
}

void BtreeStore::open( const QString& path, bool readOnly )
{
	ReadLock lock( this );
	close();
	d_path = path;
	int res = sqlite3_open_v2( path.toUtf8(), // ":memory:"
		&d_db, 
		(readOnly)?SQLITE_OPEN_READONLY:SQLITE_OPEN_READWRITE 
		| SQLITE_OPEN_CREATE 
		//| SQLITE_OPEN_EXCLUSIVE
		, 0 );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::OpenDbFile, ::sqlite3ErrStr( res ) );

	if( readOnly )
	{
		ReadLock guard( this );

		// riskant bei schreiben, keine Wirkung bei lesen: sqlite3BtreeSetSafetyLevel( getBt(), 1, 0 );

		unsigned int tmp;
		res = sqlite3BtreeGetMeta( getBt(), s_schema, &tmp );
		if( res != SQLITE_OK )
		{
			close();
			throw DatabaseException( DatabaseException::AccessMeta, ::sqlite3ErrStr( res ) );
		}

		if( tmp == 0 )
		{
			close();
			throw DatabaseException( DatabaseException::AccessMeta, "cannot create meta table in readonly database" );
		}else
			d_metaTable = tmp;
	}else
	{
		WriteLock guard( this );

		// keine wesentliche Wirkung: sqlite3BtreeSetCacheSize( getBt(), 10000000 );
		// riskant bei schreiben, keine Wirkung bei lesen: sqlite3BtreeSetSafetyLevel( getBt(), 1, 0 );

		unsigned int tmp;
		res = sqlite3BtreeGetMeta( getBt(), s_schema, &tmp );
		if( res != SQLITE_OK )
		{
			guard.rollback();
			close();
			throw DatabaseException( DatabaseException::AccessMeta, ::sqlite3ErrStr( res ) );
		}

		if( tmp == 0 )
		{
			d_metaTable = createTable();
			res = sqlite3BtreeUpdateMeta( getBt(), s_schema, d_metaTable );
			if( res != SQLITE_OK )
			{
				guard.rollback();
				close();
				throw DatabaseException( DatabaseException::AccessMeta, ::sqlite3ErrStr( res ) );
			}
		}else
			d_metaTable = tmp;
	}
}

bool BtreeStore::isReadOnly() const
{
	checkOpen();
	return getBt()->pBt->readOnly;
}

void BtreeStore::close()
{
	ReadLock lock( this );
	if( d_db )
		sqlite3_close( d_db );
	d_db = 0;
	d_metaTable = 0;
}

void BtreeStore::checkOpen() const
{
	if( d_db == 0 )
		throw DatabaseException( DatabaseException::AccessDatabase, "database not open" );
}

void BtreeStore::transBegin()
{
	checkOpen();
	if( d_txnLevel == 0 && !isReadOnly() )
	{
		int res = sqlite3BtreeBeginTrans( getBt(), 1 );
		if( res != SQLITE_OK )
			throw DatabaseException( DatabaseException::StartTrans, sqlite3ErrStr( res ) );
	}
	d_txnLevel++;
}

void BtreeStore::transCommit()
{
	checkOpen();
	if( d_txnLevel == 1 && !isReadOnly() )
	{
		int res = sqlite3BtreeCommit( getBt() );
		if( res != SQLITE_OK )
			throw DatabaseException( DatabaseException::StartTrans, sqlite3ErrStr( res ) );
	}
	if( d_txnLevel > 0 )
		d_txnLevel--;
}

void BtreeStore::transAbort()
{
	checkOpen();
	d_txnLevel = 0; // Breche sofort ab
	if( !isReadOnly() )
		sqlite3BtreeRollback( getBt() );
}

int BtreeStore::createTable(bool noData)
{
	ReadLock lock( this );
	checkOpen();
	int table;
	WriteLock guard( this );
	int res = sqlite3BtreeCreateTable( getBt(), &table, (noData)? BTREE_ZERODATA:0 );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::CreateTable, sqlite3ErrStr( res ) );
	return table;
}

void BtreeStore::dropTable( int table )
{
	ReadLock lock( this );
	checkOpen();
	WriteLock guard( this );
	int res = sqlite3BtreeDropTable( getBt(), table, 0 );
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::RemoveTable, sqlite3ErrStr( res ) );
}

void BtreeStore::clearTable( int table )
{
	ReadLock lock( this );
	checkOpen();
	WriteLock guard( this );
	int res = sqlite3BtreeClearTable( getBt(), table);
	if( res != SQLITE_OK )
		throw DatabaseException( DatabaseException::ClearTable, sqlite3ErrStr( res ) );
}

Btree* BtreeStore::getBt() const
{
	if( d_db == 0 )
		return 0;
	return d_db->aDb->pBt;
}

