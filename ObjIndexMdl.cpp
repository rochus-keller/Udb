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

#include "ObjIndexMdl.h"
#include <Udb/Transaction.h>
#include <QtDebug>
using namespace Udb;


OID ObjIndexMdl::getFirstInList() const
{
	if( d_ids.isEmpty() )
		return 0;
	else
        return d_ids.front();
}

void ObjIndexMdl::setInverted(bool on)
{
    if( d_inverted == on )
        return;
    d_inverted = on;
    refill();
}

void ObjIndexMdl::setIdx( const Udb::Idx& idx )
{
	d_idx = idx;
	refill();
}

void ObjIndexMdl::seek( const Stream::DataCell& key )
{
	if( d_idx.isNull() )
		return;
	d_idx.seek( key );
	refill();
}

int ObjIndexMdl::rowCount ( const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		return d_ids.size();
	}else
		return 0;
}

QVariant ObjIndexMdl::data ( const QModelIndex & index, int role ) const
{
	if( !index.isValid() || d_idx.isNull() )
		return QVariant();

	if( index.row() < d_ids.size() && index.column() == 0 )
	{
		switch( role )
		{
		case Qt::DisplayRole:
			return d_ids[index.row()];
		}
	}
	return QVariant();
}

OID ObjIndexMdl::getOid(const QModelIndex & index) const
{
	if( index.isValid() && index.row() < d_ids.size() )
		return d_ids[ index.row() ];
	else
        return 0;
}

QModelIndex ObjIndexMdl::getIndex(OID oid) const
{
    const int row = d_ids.indexOf( oid );
    if( row == -1 )
        return QModelIndex();
    else
        return index( row, 0 );
}

QModelIndex ObjIndexMdl::index ( int row, int column, const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		if( row < d_ids.size() )
		{
			return createIndex( row, column, row );
		}
	}
	return QModelIndex();
}

QModelIndex ObjIndexMdl::parent(const QModelIndex & index) const
{
	return QModelIndex();
}

Qt::ItemFlags ObjIndexMdl::flags(const QModelIndex &index) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool ObjIndexMdl::canFetchMore ( const QModelIndex & parent ) const
{
	if( parent.isValid() || d_idx.isNull() )
		return false;
	if( d_refetch )
	{
		return d_idx.firstKey();
	}else
	{
		const QByteArray cur = d_idx.getCur();
		const bool res = d_idx.nextKey();
		d_idx.gotoCur( cur );
		return res;
	}
}

void ObjIndexMdl::fetchMore ( const QModelIndex & parent )
{
	if( parent.isValid() || d_idx.isNull() )
		return;
	Udb::Qit i;
	// QTBUG: eigenartigerweise wird fetchMore auch aufgerufen, wenn canFetchMore false retourniert
	int n = 0;
	if( d_refetch )
	{
		d_refetch = false;
		if( !d_idx.firstKey() )
			return;
        if( !filtered( d_idx.getOid() ) )
        {
            d_ids.append( d_idx.getOid() );
            n++;
        }
	}
	const int maxBatch = 50; // TODO: ev. variabel
	if( d_idx.nextKey() ) do
	{
        if( !filtered( d_idx.getOid() ) )
        {
            d_ids.append( d_idx.getOid() );
            n++;
        }
	}while( n < maxBatch && d_idx.nextKey() );
    // Zuerst n<max prüfen, da sonst nextKey aufgerufen auch wenn Abbruch; damit geht Datensatz verloren
    if( n > 0 )
    {
        beginInsertRows( parent, d_ids.size() - n, d_ids.size() - 1 );
        endInsertRows();
    }
}

void ObjIndexMdl::refill()
{
	d_ids.clear();
	d_refetch = true;
	reset();
	fetchMore( QModelIndex() ); // Hole die ersten paar, damit nach seek gleich schon was da ist
}

void ObjIndexMdl::onDbUpdate( const UpdateInfo &info )
{
	if( d_idx.isNull() )
		return;
	if( info.d_kind == Udb::UpdateInfo::ObjectErased )
	{
		// TODO: effizienter, ev. Quicksearch
		for( int i = 0; i < d_ids.size(); i++ )
		{
			if( d_ids[i] == info.d_id )
			{		
				beginRemoveRows( QModelIndex(), i, i );
				d_ids.removeAt( i );
				endRemoveRows();
			}
		}
    }
}

bool ObjIndexMdl::filtered(OID)
{
    return false;
}
