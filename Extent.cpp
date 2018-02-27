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

#include "Extent.h"
#include "DatabaseException.h"
#include "BtreeStore.h"
#include "BtreeCursor.h"
#include "Transaction.h"
#include "Database.h"
#include "Record.h"
#include <cassert>
#include <QtDebug>
using namespace Udb;

Obj Extent::getObj() const
{
	if( d_txn && d_oid )
		return Obj( d_oid, d_txn );
	else
		return Obj();
}

bool Extent::first()
{
	checkNull();
	Database::Lock lock( d_txn->getDb());
	BtreeCursor cur;
	cur.open( d_txn->getStore(), d_txn->getDb()->getObjTable() );
	bool run = cur.moveFirst();
	Stream::DataCell v;
	while( run )
	{
		v.readCell( cur.readKey() );
		if( v.isOid() )
		{
			d_oid = v.getOid();
			return true;
		}
		run = cur.moveNext();
	}
	return false;
}

bool Extent::next()
{
	checkNull();
	if( d_oid == 0 )
		return false;
	Database::Lock lock( d_txn->getDb());
	BtreeCursor cur;
	cur.open( d_txn->getStore(), d_txn->getDb()->getObjTable() );

	Stream::DataCell v;
	v.setOid( d_oid );
	if( !cur.moveTo( v.writeCell(), true ) )
		return false;
	bool run = cur.moveNext();
	while( run )
	{
		v.readCell( cur.readKey() );
		if( v.isOid() && v.getOid() != d_oid )
		{
			d_oid = v.getOid();
			return true;
		}
		run = cur.moveNext();
	}
	return false;
}

void Extent::checkNull() const
{
	if( d_txn == 0 )
		throw DatabaseException(DatabaseException::AccessRecord, "Extent::checkNull");
}

void Extent::eraseOrphans( Transaction* t )
{
	Extent e( t );
	if( !e.first() ) 
		return;
	Database::TxnGuard lock( t->getDb() );
	while( true )
	{
		Obj o = e.getObj();
		bool run = e.next();

		Obj p = o.getParent();
		if( !p.isNull() )
		{
			Obj sub = p.getFirstObj(); 
			bool contained = false;
			if( !sub.isNull() ) do
			{
				if( sub.equals( o ) )
					contained = true;
			}while( sub.next() );
			if( !contained )
			{
				qDebug() << "Erasing OID " << o.getOid();
				t->setField( o.getOid(), Udb::Record::FieldParent, Stream::DataCell().setNull() ); // Sonst wird Transaktion nicht aktiv
				t->erase( o.getOid() );
			}
		}

		if( !run )
		{
			t->commit();
			return;
		}
	}
}

// Debug Helper
#include "ContentObject.h"

static inline QString text( const ContentObject& o )
{
	return o.getText(true).simplified();
}

QString Extent::checkDb(Transaction * t, bool fix)
{
	QString res;
	QTextStream out(&res);
	Extent e( t );
	Database::TxnGuard lock( t->getDb() );
	QList<Obj> orphans;
	typedef QList<QPair<ContentObject,ContentObject> > OpairList;
	OpairList reparent;
	OpairList fixFirstObj;
	OpairList fixLastObj;
	if( e.first() ) do
	{
		ContentObject super = e.getObj();
		ContentObject sub = super.getFirstObj();
		ContentObject prev;
		if( !sub.isNull() ) do
		{
			OpairList _reparent;
			OpairList _fixFirstObj;
			OpairList _fixLastObj;
			bool toFix = false;
			const ContentObject parent = sub.getParent();
			if( !parent.equals( super ) )
			{
				out << QString("Obj %1/%2 '%3' is contained in %4/%5 '%6' but references %7/%8 '%9' as parent")
							.arg(sub.getOid()).arg(sub.getType()).arg(text(sub))
							.arg(super.getOid()).arg(super.getType()).arg(text(super))
							.arg(parent.getOid()).arg(parent.getType()).arg(text(parent)) << endl;
				toFix = true;
				_reparent.append( qMakePair( sub, super ) );
			}
			ContentObject other = sub.getPrev();
			if( prev.isNull() && !other.isNull() )
			{
				out << QString("Obj %1/%2 '%3' is first in list but points to %4/%5 '%6' as predecessor")
							.arg(sub.getOid()).arg(sub.getType()).arg(text(sub))
							.arg(other.getOid()).arg(other.getType()).arg(text(other)) << endl;
				while( !other.isNull() && other.getParent().equals( parent ) )
				{
					out << QString("Obj %1/%2 '%3' seems to belong to %4/%5 '%6' too")
								.arg(other.getOid()).arg(other.getType()).arg(text(other))
								.arg(super.getOid()).arg(super.getType()).arg(text(super)) << endl;
					_reparent.append( qMakePair( other, super ) );
					ContentObject temp = other;
					other = other.getPrev();
					if( other.isNull() )
						_fixFirstObj.append( qMakePair( temp, super ) );
				}
				if( !other.isNull() )
				{
					out << QString("Obj %1/%2 '%3' is still not the first in list but points to another parent %4/%5")
								.arg(other.getOid()).arg(other.getType()).arg(text(other))
						   .arg(other.getParent().getOid()).arg(other.getParent().getType()) << endl;
					toFix = false;
				}
			}else if( !prev.isNull() && !other.equals(prev) )
			{
				out << QString("Obj %1/%2 '%3' is successor of %4/%5 '%6' but points to %7/%8 '%9' as predecessor")
							.arg(sub.getOid()).arg(sub.getType()).arg(text(sub))
							.arg(prev.getOid()).arg(prev.getType()).arg(text(prev))
							.arg(other.getOid()).arg(other.getType()).arg(text(other)) << endl;
				toFix = false;
			}
			other = sub.getNext();
			if( super.getLastObj().equals(sub) && !other.isNull() )
			{
				out << QString("Obj %1/%2 '%3' is last in list but points to %4/%5 '%6' as successor")
							.arg(sub.getOid()).arg(sub.getType()).arg(text(sub))
							.arg(other.getOid()).arg(other.getType()).arg(text(other)) << endl;
				while( !other.isNull() && other.getParent().equals( parent ) )
				{
					out << QString("Obj %1/%2 '%3' seems to belong to %4/%5 '%6' too")
								.arg(other.getOid()).arg(other.getType()).arg(text(other))
								.arg(super.getOid()).arg(super.getType()).arg(text(super)) << endl;
					_reparent.append( qMakePair( other, super ) );
					ContentObject temp = other;
					other = other.getNext();
					if( other.isNull() )
						_fixLastObj.append( qMakePair( temp, super ) );
				}
				if( !other.isNull() )
				{
					out << QString("Obj %1/%2 '%3' is still not the last in list but points to another parent %4/%5")
								.arg(other.getOid()).arg(other.getType()).arg(text(other))
						   .arg(other.getParent().getOid()).arg(other.getParent().getType()) << endl;
					toFix = false;
				}
			}
			if( fix && toFix )
			{
				reparent += _reparent;
				fixLastObj += _fixLastObj;
				fixFirstObj += _fixFirstObj;
			}
			prev = sub;
		}while( sub.next() );
		if( super.getParent().isNull(true,true) )
		{
			bool toDelete = true;
			ContentObject prev = super.getPrev();
			if( !prev.isNull() )
			{
				out << QString("Obj %1/%2 '%3' has no parent but points to %4/%5 '%6' as predecessor")
							.arg(super.getOid()).arg(super.getType()).arg(text(super))
							.arg(prev.getOid()).arg(prev.getType()).arg(text(prev)) << endl;
				toDelete = false;
			}
			ContentObject next = super.getNext();
			if( !next.isNull() )
			{
				out << QString("Obj %1/%2 '%3' has no parent but points to %4/%5 '%6' as successor")
							.arg(super.getOid()).arg(super.getType()).arg(text(super))
							.arg(next.getOid()).arg(next.getType()).arg(text(next)) << endl;
				toDelete = false;
			}
			if( super.getFirstObj().isNull() && super.getUuid(false).isNull() )
			{
				out << QString("Obj %1/%2 '%3' has no parent, no uuid and no children")
							.arg(super.getOid()).arg(super.getType()).arg(text(super));
				if( super.getFirstSlot().isNull() )
					out << QString(" ,also has no slots");
				else
					toDelete = false;
				if( super.findCells(Obj::KeyList()).isNull() && super.findCells(QByteArray()).isNull() )
					out << QString(" ,also has no map");
				else
					toDelete = false;
				out << endl;
			}else
				toDelete = false;
			if( fix && toDelete )
				orphans.append(super);
		}
	}while( e.next() );
	for( int i = 0; i < fixFirstObj.size(); i++ )
	{
		out << QString("Fix: set first object of %1/%2 to %3/%4")
					.arg(fixFirstObj[i].second.getOid()).arg(fixFirstObj[i].second.getType())
					.arg(fixFirstObj[i].first.getOid()).arg(fixFirstObj[i].first.getType()) << endl;
		Obj parent = fixFirstObj[i].first.getParent();
		if( parent.getFirstObj().equals( fixFirstObj[i].first ) )
			t->setField( parent.getOid(), Udb::Record::FieldFirstObj, Stream::DataCell().setNull() );
		t->setField( fixFirstObj[i].second.getOid(), Udb::Record::FieldFirstObj, fixFirstObj[i].first );
	}
	for( int i = 0; i < fixLastObj.size(); i++ )
	{
		out << QString("Fix: set last object of %1/%2 to %3/%4")
					.arg(fixLastObj[i].second.getOid()).arg(fixLastObj[i].second.getType())
					.arg(fixLastObj[i].first.getOid()).arg(fixLastObj[i].first.getType()) << endl;
		Obj parent = fixLastObj[i].first.getParent();
		if( parent.getLastObj().equals( fixLastObj[i].first ) )
			t->setField( parent.getOid(), Udb::Record::FieldFirstObj, Stream::DataCell().setNull() );
		t->setField( fixLastObj[i].second.getOid(), Udb::Record::FieldLastObj, fixLastObj[i].first );
	}
	for( int i = 0; i < reparent.size(); i++ )
	{
		out << QString("Fix: set parent of %1/%2 to %3/%4")
					.arg(reparent[i].first.getOid()).arg(reparent[i].first.getType())
					.arg(reparent[i].second.getOid()).arg(reparent[i].second.getType()) << endl;
		Obj parent = reparent[i].first.getParent();
		if( parent.getFirstObj().equals( reparent[i].first ) )
			t->setField( parent.getOid(), Udb::Record::FieldFirstObj, Stream::DataCell().setNull() );
		if( parent.getLastObj().equals( reparent[i].first ) )
			t->setField( parent.getOid(), Udb::Record::FieldLastObj, Stream::DataCell().setNull() );
		t->setField( reparent[i].first.getOid(), Udb::Record::FieldParent, reparent[i].second );
	}
	foreach( Obj o, orphans )
	{
		out << QString("Fix: erasing orphan %1/%2")
			   .arg(o.getOid()).arg(o.getType()) << endl;
		o.erase();
	}
	t->commit();
	return res;
}
