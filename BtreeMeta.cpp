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

#include "BtreeMeta.h"
#include "BtreeCursor.h"
#include "BtreeStore.h"
using namespace Udb;


void BtreeMeta::write( const QByteArray& key, const QByteArray& val )
{
	BtreeStore::WriteLock guard( d_db );
	BtreeCursor cur;
	cur.open( d_db, d_db->getMetaTable(), true );
	cur.insert( key, val );
}

QByteArray BtreeMeta::read( const QByteArray& key ) const
{
	BtreeCursor cur;
	cur.open( d_db, d_db->getMetaTable() );
	if( cur.moveTo( key ) )
		return cur.readValue();
	else
		return QByteArray();
}

void BtreeMeta::erase( const QByteArray& key )
{
	BtreeStore::WriteLock guard( d_db );
	BtreeCursor cur;
	cur.open( d_db, d_db->getMetaTable(), true );
	if( cur.moveTo( key ) )
		cur.removePos();
}
