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
#include "Database.h"
#include "Database.h"
#include "BtreeCursor.h"
#include "BtreeStore.h"
#include "DatabaseException.h"
#include "BtreeMeta.h"
#include <Stream/DataCell.h>
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <QBuffer>
#include <QtDebug>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>
#include <QProcess>
#include <cassert>
using namespace Udb;
using namespace Stream;

static const char* s_dbFormat = "{6D20986B-36ED-4571-AD5E-26734CCFB542}";

Database::Lock::Lock( Database* db ):d_db(db)
{
	assert( db );
#ifdef DATABASE_HAS_MUTEX
	db->d_lock.lock();
#endif
}

Database::Lock::~Lock()
{
#ifdef DATABASE_HAS_MUTEX
	d_db->d_lock.unlock();
#endif
}

Database::TxnGuard::TxnGuard( Database* db ):d_db(db)
{
	assert( db );
#ifdef DATABASE_HAS_MUTEX
	db->d_lock.lock();
#endif
#ifdef BTREESTORE_HAS_MUTEX
	db->d_db->d_lock.lock();
#endif
	if( !db->isReadOnly() )
		db->d_db->transBegin();
}

void Database::TxnGuard::rollback()
{
	if( d_db )
	{
		if( !d_db->isReadOnly() )
			d_db->d_db->transAbort();
#ifdef BTREESTORE_HAS_MUTEX
		d_db->d_db->d_lock.unlock();
#endif
#ifdef DATABASE_HAS_MUTEX
		d_db->d_lock.unlock();
#endif
		d_db = 0;
	}
}

void Database::TxnGuard::commit()
{
	if( d_db )
	{
		if( !d_db->isReadOnly() )
			d_db->d_db->transCommit();
#ifdef BTREESTORE_HAS_MUTEX
		d_db->d_db->d_lock.unlock();
#endif
#ifdef DATABASE_HAS_MUTEX
		d_db->d_lock.unlock();
#endif
		d_db = 0;
	}
}

Database::TxnGuard::~TxnGuard()
{
	commit();
}


Database::Database(QObject*p):QObject(p)
#ifdef DATABASE_HAS_MUTEX
	,d_lock(QMutex::Recursive)
#endif
{
	d_db = 0;
	qRegisterMetaType<Udb::UpdateInfo>();
}

Database::~Database()
{
	close();
}

void Database::addObserver( QObject* obj, const char* slot, bool asynch )
{
	connect( this, SIGNAL(notify( Udb::UpdateInfo )),
        obj, slot, (asynch)?Qt::QueuedConnection:Qt::DirectConnection );
		// NOTE: connect ist threadsafe
}

void Database::removeObserver( QObject* obj, const char* slot )
{
	disconnect( this, SIGNAL(notify( Udb::UpdateInfo )),obj, slot );
	// NOTE: disconnect ist threadsafe
}

void Database::open( const QString& path, bool readOnly )
{
	Lock lock( this );
	close();
	QFileInfo info( path );
	d_db = new BtreeStore( this );
    // NOTE: in Linux wird sonst der Pfad zum Symlink der DbPath
    d_db->open( (info.isSymLink())?info.symLinkTarget():info.absoluteFilePath(),
                !info.isWritable() && info.exists() || readOnly );
	loadMeta();
}

void Database::setCacheSize( int numOfPages )
{
	checkOpen();
	Lock lock( this );
	d_db->setCacheSize( numOfPages );
}

void Database::close()
{
	Lock lock( this );
	emit notify( UpdateInfo( UpdateInfo::DbClosing ) );
	if( d_db )
		delete d_db;
	d_db = 0;
	d_meta = Meta();
	// TODO: d_cache + Records löschen
}

void Database::loadMeta()
{
	// Header ist vorhanden
	BtreeMeta meta( d_db );
	DataReader reader( meta.read( DataCell().setNull().writeCell() ) );
	bool formatSeen = false;
	bool empty = true;

	for( DataReader::Token t = reader.nextToken(); DataReader::isUseful( t ); t = reader.nextToken() )
	{
		switch( t )
		{
		case DataReader::Slot:
			{
				const QByteArray name = reader.getName().getArr();
				DataCell value;
				reader.readValue( value );
				if( name == "objTable" )
					d_meta.d_objTable = value.getInt32();
				else if( name == "dirTable" )
					d_meta.d_dirTable = value.getInt32();
				else if( name == "idxTable" )
					d_meta.d_idxTable = value.getInt32();
				else if( name == "queTable" )
					d_meta.d_queTable = value.getInt32();
				else if( name == "mapTable" )
					d_meta.d_mapTable = value.getInt32();
                else if( name == "oixTable" )
					d_meta.d_oixTable = value.getInt32();
				else if( name == "dbFormat" )
				{
					QUuid uuid( s_dbFormat );
					if( uuid != value.getUuid() )
						throw DatabaseException( DatabaseException::DatabaseFormat );
					formatSeen = true;
				}
				empty = false;
			}
			break;
		default:
			throw DatabaseException( DatabaseException::DatabaseMeta );
		}
	}
	if( !empty && !formatSeen )
		throw DatabaseException( DatabaseException::DatabaseFormat );
}

void Database::saveMeta()
{
	if( d_db->isReadOnly() )
		return;
	DataWriter value;
	BtreeMeta meta( d_db );
	value.writeSlot( DataCell().setInt32( d_meta.d_objTable ), "objTable" );
	value.writeSlot( DataCell().setInt32( d_meta.d_dirTable ), "dirTable" );
	value.writeSlot( DataCell().setInt32( d_meta.d_idxTable ), "idxTable" );
	value.writeSlot( DataCell().setInt32( d_meta.d_queTable ), "queTable" );
	value.writeSlot( DataCell().setInt32( d_meta.d_mapTable ), "mapTable" );
    value.writeSlot( DataCell().setInt32( d_meta.d_oixTable ), "oixTable" );
	value.writeSlot( DataCell().setUuid( s_dbFormat ), "dbFormat" );
	meta.write( DataCell().setNull().writeCell(), value.getStream() );
}

void Database::checkOpen() const
{
	if( d_db == 0 )
		throw DatabaseException( DatabaseException::AccessDatabase, "database not open" );
}

int Database::getObjTable()
{
	checkOpen();
	if( d_meta.d_objTable == 0 )
	{
		BtreeStore::WriteLock txn( d_db );
		d_meta.d_objTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_objTable;
}

int Database::getDirTable()
{
	checkOpen();
	if( d_meta.d_dirTable == 0 )
	{
		BtreeStore::WriteLock txn( d_db );
		d_meta.d_dirTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_dirTable;
}

int Database::getQueTable()
{
	checkOpen();
	if( d_meta.d_queTable == 0 )
	{
		BtreeStore::WriteLock txn( d_db );
		d_meta.d_queTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_queTable;
}

int Database::getIdxTable()
{
	checkOpen();
	if( d_meta.d_idxTable == 0 )
	{
		BtreeStore::ReadLock txn( d_db );
		d_meta.d_idxTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_idxTable;
}

int Database::getMapTable()
{
	checkOpen();
	if( d_meta.d_mapTable == 0 )
	{
		BtreeStore::ReadLock txn( d_db );
		d_meta.d_mapTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_mapTable;
}

int Database::getOixTable()
{
	checkOpen();
	if( d_meta.d_oixTable == 0 )
	{
		BtreeStore::ReadLock txn( d_db );
		d_meta.d_oixTable = d_db->createTable();
		saveMeta();
	}
	return d_meta.d_oixTable;
}

QByteArray Database::getAtomString( quint32 a )
{
	if( a == 0 )
		return QByteArray();
#ifdef DATABASE_HAS_MUTEX
	QMutexLocker lock( &d_lock );
#endif
	QHash<quint32,QByteArray>::const_iterator i = d_invDir.find( a );
	if( i != d_invDir.end() )
		return i.value();
	BtreeCursor cur;
	cur.open( d_db, getDirTable(), false );
	if( cur.moveTo( DataCell().setAtom( a ).writeCell() ) )
	{
		DataCell s;
		s.readCell( cur.readValue() );
		d_dir[s.getArr()] = a;
		d_invDir[a] = s.getArr();
		return s.getArr();
	}else
		return QByteArray();
}

quint32 Database::getAtom( const QByteArray& name )
{
#ifdef DATABASE_HAS_MUTEX
	QMutexLocker lock( &d_lock );
#endif
	QHash<QByteArray,quint32>::const_iterator i = d_dir.find( name );
	if( i != d_dir.end() )
		return i.value();
	const QByteArray n = DataCell().setLatin1(name).writeCell();
	// Suche Atom in Db
	{
		BtreeCursor cur;
		cur.open( d_db, getDirTable(), false );
		if( cur.moveTo( n ) )
		{
			DataCell atom;
			atom.readCell( cur.readValue() );
			if( atom.getType() != DataCell::TypeAtom )
				throw DatabaseException( DatabaseException::DirectoryFormat );
			d_dir[name] = atom.getAtom();
			d_invDir[atom.getAtom()] = name;
			return atom.getAtom();
		}
	}
	// Atom existiert noch nicht, erzeuge es
	if( !d_db->isReadOnly() )
	{
		BtreeStore::WriteLock txn( d_db );
		BtreeCursor cur;
		cur.open( d_db, getDirTable(), true );
		quint32 atom = 0;
		const QByteArray null = DataCell().setNull().writeCell();
		if( cur.moveTo( null ) )
		{
			DataCell v;
			v.readCell( cur.readValue() );
			if( v.getType() != DataCell::TypeAtom )
				throw DatabaseException( DatabaseException::DirectoryFormat );
			atom = v.getAtom();
		}
		atom++;
		const QByteArray a = DataCell().setAtom( atom ).writeCell();
		cur.insert( null, a );
		cur.insert( n, a );
		cur.insert( a, n );
		d_dir[name] = atom;
		d_invDir[atom] = name; // name wird nur einmal gespeichert
		return atom;
	}else
		return 0;
}

void Database::presetAtom( const QByteArray& name, quint32 atom )
{
	checkOpen();
	// Suche den Wert im Cache und in der DB
#ifdef DATABASE_HAS_MUTEX
	QMutexLocker lock( &d_lock );
#endif
	QHash<QByteArray,quint32>::const_iterator i = d_dir.find( name );
	if( i != d_dir.end() )
	{
		// Name ist bereits im Cache. Prüfe nun ob ID übereinstimmt.
		if( i.value() != atom )
			throw DatabaseException( DatabaseException::DuplicateAtom );
		// Name wurde bereits im Cache registriert mit gewünschter Atom-ID
		return; 
	}
	const QByteArray n = DataCell().setLatin1(name).writeCell();
	QByteArray a;
	// Suche Atom in Db
	BtreeCursor cur;
	cur.open( d_db, getDirTable(), false );
	if( cur.moveTo( n ) )
	{
		a = cur.readValue();
		DataCell v;
		v.readCell( a );
		if( v.getType() != DataCell::TypeAtom )
			throw DatabaseException( DatabaseException::DirectoryFormat );
		if( v.getAtom() != atom )
			throw DatabaseException( DatabaseException::DuplicateAtom );
		// Prüfe nun, ob Atom-ID bereits in der DB vorhanden. 
		if( cur.moveTo( a ) )
		{
			if( cur.readValue() != n )
				// Darf in einer korrekten DB nicht vorkommen.
				throw DatabaseException( DatabaseException::DirectoryFormat );
		}
		// Name ist in Db vorhanden mit gewünschter Atom-ID
		return;
	}else
		a = DataCell().setAtom( atom ).writeCell();
	cur.close();
	// Füge Atom in Db ein
	if( !d_db->isReadOnly() )
	{
		BtreeStore::WriteLock txn( d_db );
		cur.open( d_db, getDirTable(), true );
		const QByteArray null = DataCell().setNull().writeCell();
		if( cur.moveTo( null ) )
		{
			DataCell v;
			v.readCell( cur.readValue() );
			if( v.getType() != DataCell::TypeAtom )
				throw DatabaseException( DatabaseException::DirectoryFormat );
			if( atom > v.getAtom() )
				// Erhöhe den Zähler auf mindestens die Preset-ID
				cur.insert( null, a );
		}else
			// Zähler war noch nicht vorhanden
			cur.insert( null, a );
		cur.insert( n, a );
		cur.insert( a, n );
	}
}

QString Database::getFilePath() const
{
	Lock lock( const_cast<Database*>(this) );
	return d_db->getPath();
}

OID Database::getMaxOid() 
{ 
	Lock lock( this );
	return getNextOid( false ); 
}

OID Database::getNextOid(bool persistent)
{
	checkOpen();
	OID id = 0;
	BtreeStore::WriteLock lock( d_db );
	BtreeCursor cur;
	cur.open( d_db, getObjTable(), !d_db->isReadOnly() );
	if( cur.moveTo( DataCell().setNull().writeCell() ) )
	{
		DataCell v;
		v.readCell( cur.readValue() );
		id = v.getOid();
	}
	id++;
	if( id == 0xffffffff )
		throw DatabaseException( DatabaseException::OidOutOfRange ); // RISK: zur Zeit nur auf 32 Bit ausgelegt
	if( persistent && !d_db->isReadOnly() )
		cur.insert( DataCell().setNull().writeCell(), DataCell().setOid( id ).writeCell() );
	return id;
}

quint32 Database::getNextQueueNr( OID oid )
{
	checkOpen();
	quint32 id = 0;
	BtreeStore::WriteLock txn( d_db );
	BtreeCursor cur;
	cur.open( d_db, getQueTable(), !d_db->isReadOnly() );
	const QByteArray key = DataCell().setOid(oid).writeCell();
	if( cur.moveTo( key ) )
	{
		DataCell v;
		v.readCell( cur.readValue() );
		id = v.getId32();
	}
	id++;
	if( !d_db->isReadOnly() )
		cur.insert( key, DataCell().setId32( id ).writeCell() );
	return id;
}

quint32 Database::findIndex( const QByteArray& name )
{
	Lock lock( this );
	checkOpen();
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), false );
	if( cur.moveTo( DataCell().setLatin1( name ).writeCell() ) )
	{
		DataCell id;
		id.readCell( cur.readValue() );
		return id.getId32();
	}else
		return 0;
}

static QByteArray writeIndexMeta( const IndexMeta& m )
{
	DataWriter w;
	w.writeSlot( DataCell().setUInt8( m.d_kind ), NameTag("kind") );
	for( int i = 0; i < m.d_items.size(); i++ )
	{
		w.startFrame( NameTag("item") );
		w.writeSlot( DataCell().setAtom( m.d_items[i].d_atom ), NameTag("atom") );
		w.writeSlot( DataCell().setBool( m.d_items[i].d_nocase ), NameTag("nc") );
		w.writeSlot( DataCell().setBool( m.d_items[i].d_invert ), NameTag("inv") );
		w.writeSlot( DataCell().setUInt8( m.d_items[i].d_coll ), NameTag("coll") );
		w.endFrame();
	}
	return w.getStream();
}

static void readIndexMeta( const QByteArray& data, IndexMeta& m )
{
	m = IndexMeta();
	IndexMeta::Item item;
	DataReader reader( data );
	bool inItem = false;
	for( DataReader::Token t = reader.nextToken(); DataReader::isUseful( t ); 
		t = reader.nextToken() )
	{
		switch( t )
		{
		case DataReader::Slot:
			{
				const NameTag name = reader.getName().getTag();
				DataCell value;
				reader.readValue( value );
				if( name == "atom" )
					item.d_atom = value.getAtom();
				else if( name == "nc" )
					item.d_nocase = value.getBool();
				else if( name == "inv" )
					item.d_invert = value.getBool();
				else if( name == "coll" )
					item.d_coll = value.getUInt8();
				else if( name == "kind" )
					m.d_kind = (IndexMeta::Kind)value.getUInt8();
			}
			break;
		case DataReader::BeginFrame:
			if( inItem )
				goto error;
			if( reader.getName().getTag() == "item" )
				inItem = true;
			break;
		case DataReader::EndFrame:
			if( !inItem )
				goto error;
			inItem = false;
			m.d_items.append( item );
			item = IndexMeta::Item();
			break;
		default:
			goto error;
		}
	}
	return;
error:
	throw DatabaseException( DatabaseException::DatabaseMeta, 
		"invalid index meta format" );
}

bool Database::isReadOnly() const
{
	checkOpen();
	return d_db->isReadOnly();
}

quint32 Database::createIndex( const QByteArray& name, const IndexMeta& meta )
{
	TxnGuard lock( this );
	checkOpen();
	if( d_db->isReadOnly() )
		return 0;
	if( findIndex( name ) != 0 )
		throw DatabaseException( DatabaseException::IndexExists );

	assert( !meta.d_items.isEmpty() );
	const quint32 table = d_db->createTable();
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), true );
	const QByteArray id = DataCell().setId32( table ).writeCell();
	// Write name->tableId
	cur.insert( DataCell().setLatin1( name ).writeCell(), id );
	// Write tableId->IndexMeta, 
	cur.insert( id, writeIndexMeta( meta ) );
	// Write atom + tableId -> tableId
	for( int i = 0; i < meta.d_items.size(); i++ )
		cur.insert( DataCell().setAtom( meta.d_items[i].d_atom ).writeCell() + id, id );
	d_idxMeta[table] = meta;
	return table;
}

void Database::removeIndex( const QByteArray& name )
{
	TxnGuard lock( this );
	checkOpen();
	Index idx = findIndex( name );
	if( idx == 0 )
		return;
	IndexMeta meta;
	if( !getIndexMeta( idx, meta ) )
		return;
	d_db->clearTable( idx );
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), true );
	const QByteArray id = DataCell().setId32( idx ).writeCell();
	if( cur.moveTo( DataCell().setLatin1( name ).writeCell() ) )
		cur.removePos();
	if( cur.moveTo( id ) )
		cur.removePos();
	for( int i = 0; i < meta.d_items.size(); i++ )
	{
		if( cur.moveTo( DataCell().setAtom( meta.d_items[i].d_atom ).writeCell() + id ) )
			cur.removePos();
	}
	d_idxMeta.remove( idx );
}

bool Database::getIndexMeta( quint32 id, IndexMeta& m )
{
	Lock lock( this );
	checkOpen();
	QHash<quint32,IndexMeta>::const_iterator i = d_idxMeta.find( id );
	if( i != d_idxMeta.end() )
	{
		m = i.value();
		return true;
	}
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), false );
	if( cur.moveTo( DataCell().setId32( id ).writeCell() ) )
	{
		readIndexMeta( cur.readValue(), m );
		d_idxMeta[id] = m;
		return true;
	}else
		return false;
}

QList<quint32> Database::findIndexForAtom( quint32 atom )
{
	Lock lock( this );
	checkOpen();
	QHash<quint32,QList<quint32> >::const_iterator i = d_idxAtoms.find( atom );
	if( i != d_idxAtoms.end() )
		return i.value();
	QList<quint32> idx;
	BtreeCursor cur;
	cur.open( d_db, getIdxTable() );
	const QByteArray key = DataCell().setAtom( atom ).writeCell();
	if( cur.moveTo( key, true ) ) do
	{
		DataCell id;
		id.readCell( cur.readValue() );
		if( id.getId32() )
			idx.append( id.getId32() );
	}while( cur.moveNext( key ) );
	d_idxAtoms[atom] = idx;
	return idx;
}

QUuid Database::getDbUuid(bool create)
{
	Lock lock( this );
	checkOpen();
	BtreeMeta meta( d_db );
	DataCell v; 
	const QByteArray key = DataCell().setTag( NameTag( "dbid" ) ).writeCell();
	v.readCell( meta.read( key ) );
	if( create && !v.isUuid() )
	{
		QUuid id = QUuid::createUuid();
		if( !d_db->isReadOnly() )
		{
			TxnGuard guard( this );
			v.setUuid( id );
			meta.write( key, v.writeCell() );
		}
		return id;
	}else
		return v.getUuid();
}

bool Database::clearIndexContents( Index id )
{
	checkOpen();
	BtreeCursor cur;
	cur.open( d_db, getIdxTable(), false );
	if( !cur.moveTo( DataCell().setId32( id ).writeCell() ) )
		return false; // id gehört nicht zu einem Index
	d_db->clearTable( id );
	return true;
}

void Database::dumpAtoms()
{
    BtreeCursor cur;
    cur.open( d_db, getDirTable(), false );
    if( cur.moveFirst() ) do
	{
		DataCell k;
		k.readCell( cur.readKey() );
		qDebug() << "Key=" << k.toPrettyString() << cur.readKey().toHex();
        DataCell atom;
        atom.readCell( cur.readValue() );
        qDebug() << "Value=" << atom.toPrettyString();
    }while( cur.moveNext() );
}

void Database::registerDatabase()
{
    if( d_db == 0 )
        return;
	registerDatabase( getDbUuid(), getFilePath() );
}

void Database::registerDatabase(const QUuid & uuid, const QString &path)
{
	QSettings set( tr("Dr. Rochus Keller"), tr("UdbDatabaseRegistry") );
	set.setValue( uuid.toString() + tr("/Db"), path );
	set.setValue( uuid.toString() + tr("/App"), QCoreApplication::applicationFilePath() );
}

QString Database::findDatabase(const QUuid & uid)
{
    QSettings set( tr("Dr. Rochus Keller"), tr("UdbDatabaseRegistry") );
    return set.value( QUuid(uid).toString() + tr("/Db") ).toString();
}

bool Database::runDatabaseApp(const QUuid & uid, const QStringList &moreArgs)
{
    QSettings set( tr("Dr. Rochus Keller"), tr("UdbDatabaseRegistry") );
    const QString dbPath = set.value( QUuid(uid).toString() + tr("/Db") ).toString();
    const QString appPath = set.value( QUuid(uid).toString() + tr("/App") ).toString();
    QStringList arguments;
    arguments.append( dbPath );
    arguments += moreArgs;
#ifdef _funktioniert_nicht_ //  Q_WS_WIN
    if( QProcess::startDetached( "cmd", QStringList() << "/C" << "start"
    << ( tr("\"%1 %2\"").arg( QDir::toNativeSeparators(dbPath) ).arg( moreArgs.join( QChar(' ') ) ) ) ) )
        return true;
#endif
    return QProcess::startDetached( appPath, arguments );
    // start command
    // oder ShellExecuteA wie in C:\Programme\Qt4.4\src\gui\util\qdesktopservices_win.cpp
    // http://stackoverflow.com/questions/13766969/how-to-launch-the-associated-application-for-a-file-directory-url

}
