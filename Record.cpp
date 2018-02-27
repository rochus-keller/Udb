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

#include "Record.h"
#include "BtreeCursor.h"
#include "DatabaseException.h"
#include <Stream/DataWriter.h>
#include <cassert>
using namespace Udb;
using namespace Stream;

Record::Record()
{
}

void Record::writeField( BtreeCursor& cur, OID oid, Atom a, const Stream::DataCell& v )
{
	DataWriter w;
	w.writeSlot( DataCell().setOid( oid ) ); // Wir verwenden Multybyte64, wahrscheinlich genügt 32bit
	w.writeSlot( DataCell().setAtom( a ) );

	if( v.isNull() )
	{
		if( cur.moveTo( w.getStream() ) )
			cur.removePos();
	}else
		cur.insert( w.getStream(), v.writeCell() );
}

void Record::readField( BtreeCursor& cur, OID oid, Atom a, Stream::DataCell& v )
{
	DataWriter w;
	w.writeSlot( DataCell().setOid( oid ) ); // Wir verwenden Multybyte64, wahrscheinlich genügt 32bit
	w.writeSlot( DataCell().setAtom( a ) );

	if( cur.moveTo( w.getStream() ) )
		v.readCell( cur.readValue() );
	else
		v.setNull();
}

OID Record::findObject( BtreeCursor& cur, const QUuid& uuid )
{
	DataCell v;
	if( cur.moveTo( DataCell().setUuid( uuid ).writeCell() ) )
		v.readCell( cur.readValue() );
	return v.getOid();
}

void Record::eraseFields( BtreeCursor& cur, OID oid )
{
	const QByteArray key = DataCell().setOid( oid ).writeCell();
	if( cur.moveTo( key ) ) // Vergleich des ganzen Keys.
	{
		const QByteArray value = cur.readValue();
		// Record sollte nun auf Uuid zeigen
#ifdef _DEBUG
		DataCell v;
		v.readCell( value );
		if( !v.isUuid() )
			throw DatabaseException::DatabaseFormat;
#endif
		if( cur.moveTo( value ) )
			cur.removePos();
	}
	if( cur.moveTo( key, true ) ) do
	{
		cur.removePos();
	}while( cur.moveTo( key, true ) );
}

Record::Fields Record::getFields( BtreeCursor& cur, OID oid, bool all )
{
	Fields f;
	const QByteArray key = DataCell().setOid( oid ).writeCell();
	DataCell v;
	if( cur.moveTo( key, true ) ) do
	{
		const QByteArray atom = cur.readKey().mid( key.size() );
		if( !atom.isEmpty() )
		{
			v.readCell( atom );
			if( v.isAtom() )
			{
				if( all || v.getAtom() < MinReservedField )
					f.append( v.getAtom() );
			}
		}
	}while( cur.moveNext() && cur.readKey().startsWith( key ) );
	return f;
}

void Record::setUuid( BtreeCursor& cur, OID oid, const QUuid& uuid )
{
	const QByteArray o = DataCell().setOid( oid ).writeCell();
	const QByteArray u = DataCell().setUuid( uuid ).writeCell();
	if( cur.moveTo( o ) )
	{
		if( cur.moveTo( cur.readValue() ) )
			cur.removePos();
	}
	cur.insert( o, u );
	cur.insert( u, o );
}

QUuid Record::getUuid( BtreeCursor& cur, OID oid )
{
	const QByteArray o = DataCell().setOid( oid ).writeCell();
	if( cur.moveTo( o ) )
	{
		DataCell v;
		v.readCell( cur.readValue() );
		return v.getUuid();
	}
	return QUuid();
}
