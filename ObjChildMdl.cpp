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

#include "ObjChildMdl.h"
#include <Udb/Transaction.h>
#include <QtDebug>
#include <cassert>
using namespace Udb;

ObjChildMdl::ObjChildMdl(QObject *parent)
	: QAbstractItemModel(parent), d_inverted(false)
{
}

ObjChildMdl::~ObjChildMdl()
{

}

void ObjChildMdl::setRoot( const Udb::Obj& root )
{
	if( d_root.equals( root ) )
		return;
	d_root = root;
	refill();
}

void ObjChildMdl::setInverted(bool on)
{
	if( d_inverted == on )
		return;
	d_inverted = on;
	refill();
}

int ObjChildMdl::rowCount ( const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		return d_items.size();
	}else
		return 0;
}

QModelIndex ObjChildMdl::index ( int row, int column, const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		if( row < d_items.size() && column < columnCount(parent) )
		{
			return createIndex( row, column, quint32(d_items[row].getOid()) );
		}
	}
	return QModelIndex();
}

QModelIndex ObjChildMdl::getIndex( const Obj & o, bool fetch ) const
{
	if( d_root.isNull() )
		return QModelIndex();
	const int i = d_items.indexOf( o );
	if( i != -1 )
		return createIndex( i, 0, quint32(o.getOid()) );
    else if( !fetch )
		return QModelIndex();
    else
    {
		int max = d_items.size();
        do
        {
            const_cast<ObjChildMdl*>( this )->fetchMore( QModelIndex() );
			const int i = d_items.indexOf( o, max );
            if( i != -1 )
				return createIndex( i, 0, quint32(o.getOid()) );
			max = d_items.size();
        }while( canFetchMore( QModelIndex() ) );
        return QModelIndex();
    }
}

QModelIndex ObjChildMdl::parent(const QModelIndex & index) const
{
    Q_UNUSED(index);
	return QModelIndex();
}

Qt::ItemFlags ObjChildMdl::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant ObjChildMdl::data ( const QModelIndex & index, int role ) const
{
	if( !index.isValid() || d_root.isNull() )
		return QVariant();

	if( index.row() < d_items.size() )
	{
		if( role == OidRole )
			return d_items[index.row()].getOid();
		else
			return data( d_items[index.row()], index.column(), role );
	}
	return QVariant();
}

Udb::Obj ObjChildMdl::getObj( const QModelIndex & index  ) const
{
	if( !index.isValid() || index.row() >= d_items.size() )
		return Udb::Obj();
	return d_items[index.row()];
}

int ObjChildMdl::fetch( int max, QList<Obj> *l ) const
{
	assert( !d_root.isNull() );
	Udb::Obj item;
	bool useIt = true;
	if( d_items.isEmpty() )
		item = (d_inverted)? d_root.getLastObj() : d_root.getFirstObj();
	else
	{
		item = d_items.last();
		useIt = false;
	}
	// Wir gehen vom letzten (neusten) Slot richtung erstem (ltestem) und
	// fgen die Slots an das Ende der Liste
	int n = 0;
	if( !item.isNull() ) do
	{
		if( useIt && isSupportedType( item.getType() ) )
		{
			if( l )
				l->append( item );
			n++;
		}
		useIt = true;
	}while( ( (d_inverted)? item.prev() : item.next() ) && ( max == 0 || n < max ) );
	return n;
}

bool ObjChildMdl::canFetchMore ( const QModelIndex & parent ) const
{
    Q_UNUSED(parent);
	if( d_root.isNull() )
		return false;
	const int n = fetch( 1 );
	return n > 0;
}

void ObjChildMdl::fetchMore ( const QModelIndex & parent )
{
	if( d_root.isNull() )
		return;
	QList<Obj> l;
    fetch( 100, &l );
	if( l.isEmpty() )
		return;

	beginInsertRows( parent, d_items.size(), d_items.size() + l.size() - 1 );
	d_items += l;
	endInsertRows();
}

void ObjChildMdl::onDbUpdate( const Udb::UpdateInfo& info )
{
	if( d_root.isNull() )
		return;
	if( info.d_kind == Udb::UpdateInfo::ObjectErased && d_root.getOid() == info.d_id )
	{
		d_root = Udb::Obj();
		refill();
	}else if( info.d_kind == UpdateInfo::Aggregated && info.d_parent == d_root.getOid() )
	{
		Udb::Obj item = d_root.getObject( info.d_id );
		if( isSupportedType( item.getType() ) )
		{
			// RISK: richtigen Ort finden zum einfuegen
			if( d_inverted )
			{
				beginInsertRows( QModelIndex(), 0, 0 );
				d_items.prepend( item );
				endInsertRows();
			}else
			{
				beginInsertRows( QModelIndex(), d_items.size(), d_items.size() );
				d_items.append( item );
				endInsertRows();
			}
		}
	}else if( info.d_kind == UpdateInfo::Deaggregated && info.d_parent == d_root.getOid() )
	{
		Udb::Obj item = d_root.getObject( info.d_id );
		if( isSupportedType( info.d_name2 ) )
		{
			const int pos = d_items.indexOf(item);
			if( pos != -1 )
			{
				beginRemoveRows( QModelIndex(), pos, pos );
				d_items.removeAt(pos);
				endRemoveRows();
			}
		}
	}
}

void ObjChildMdl::refill()
{
	d_items.clear();
	reset();
}

bool ObjChildMdl::isReadOnly() const
{
	return d_root.isNull() || d_root.getTxn()->isReadOnly();
}

QVariant ObjChildMdl::data( const Udb::Obj&, int section, int role ) const
{
	return QVariant();
}

bool ObjChildMdl::isSupportedType(quint32) const
{
	return false;
}

