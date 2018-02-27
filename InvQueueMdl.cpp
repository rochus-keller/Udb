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

#include "InvQueueMdl.h"
#include <Udb/Transaction.h>
#include <QtDebug>
#include <cassert>
using namespace Udb;

InvQueueMdl::InvQueueMdl(QObject *parent)
	: QAbstractItemModel(parent)
{
}

InvQueueMdl::~InvQueueMdl()
{

}

void InvQueueMdl::setQueue( const Udb::Obj& queue )
{
	d_queue = queue;
	refill();
}

int InvQueueMdl::rowCount ( const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		return d_slots.size();
	}else
		return 0;
}

QModelIndex InvQueueMdl::index ( int row, int column, const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		if( row < d_slots.size() && column == 0 )
		{
			return createIndex( row, column, row );
		}
	}
	return QModelIndex();
}

QModelIndex InvQueueMdl::getIndex( const Udb::Qit& i, bool fetch ) const
{
    return getIndex( i.getSlotNr(), fetch );
}

QModelIndex InvQueueMdl::getIndex( quint32 nr, bool fetch ) const
{
	if( d_queue.isNull() )
		return QModelIndex();
	const int i = d_slots.indexOf( nr );
	if( i != -1 )
		return createIndex( i, 0, i );
    else if( !fetch )
		return QModelIndex();
    else
    {
        int max = d_slots.size();
        do
        {
            const_cast<InvQueueMdl*>( this )->fetchMore( QModelIndex() );
            const int i = d_slots.indexOf( nr, max );
            if( i != -1 )
                return createIndex( i, 0, i );
            qDebug() << d_slots[max] << d_slots.last() << nr;
            max = d_slots.size();
        }while( canFetchMore( QModelIndex() ) );
        return QModelIndex();
    }
}

QModelIndex InvQueueMdl::parent(const QModelIndex & index) const
{
    Q_UNUSED(index);
	return QModelIndex();
}

Qt::ItemFlags InvQueueMdl::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant InvQueueMdl::data ( const QModelIndex & index, int role ) const
{
	if( !index.isValid() || d_queue.isNull() )
		return QVariant();

	if( index.row() < d_slots.size() && index.column() == 0 )
	{
		Udb::Qit i = d_queue.getSlot( d_slots[index.row()] );
		if( !i.isNull() )
		{
			if( role == SlotNrRole )
				return d_slots[index.row()];
			else
			{
				Udb::Obj o = d_queue.getObject( i.getValue().getOid() );
				if( !o.isNull() )
					return data( o, role );
                else
                    return data( i.getValue(), role );
			}
		}
	}
	return QVariant();
}

Udb::Obj InvQueueMdl::getObj( const QModelIndex & index  ) const
{
	Udb::Qit i = getItem( index );
	if( !i.isNull() )
	{
		Udb::Obj o = d_queue.getObject( i.getValue().getOid() );
		if( !o.isNull() )
			return o;
	}
	return Udb::Obj();
}

Udb::Qit InvQueueMdl::getItem( const QModelIndex & index ) const
{
	if( !index.isValid() || d_queue.isNull() )
		return Udb::Qit();
	if( index.row() < d_slots.size() )
	{
		return d_queue.getSlot( d_slots[index.row()] );
	}
	return Udb::Qit();
}

int InvQueueMdl::fetch( int max, QList<quint32>* l ) const
{
	assert( !d_queue.isNull() );
	Udb::Qit i;
	if( d_slots.isEmpty() )
	{
		i = d_queue.getLastSlot();
		if( i.isNull() )
			return 0;
	}else
	{
		i = d_queue.getSlot( d_slots.last() );
		if( !i.prev() )
			return 0;
	}
	// Wir gehen vom letzten (neusten) Slot richtung erstem (ältestem) und 
	// fügen die Slots an das Ende der Liste
	int n = 0;
	if( !i.isNull() ) do
	{
		if( l )
			l->append( i.getSlotNr() );
		n++;
	}while( i.prev() && ( max == 0 || n < max ) );
	return n;
}

bool InvQueueMdl::canFetchMore ( const QModelIndex & parent ) const
{
    Q_UNUSED(parent);
	if( d_queue.isNull() )
		return false;
	const int n = fetch( 1 );
	return n > 0;
}

void InvQueueMdl::fetchMore ( const QModelIndex & parent )
{
	if( d_queue.isNull() )
		return;
	QList<quint32> l;
    fetch( 100, &l );
	if( l.isEmpty() )
		return;

	beginInsertRows( parent, d_slots.size(), d_slots.size() + l.size() - 1 );
	d_slots += l;
	endInsertRows();
}

void InvQueueMdl::onDbUpdate( Udb::UpdateInfo info )
{
	if( d_queue.isNull() )
		return;
	if( info.d_kind == Udb::UpdateInfo::QueueAdded && info.d_parent == d_queue.getOid() )
	{
		// in d_slots sind die jüngsten Slots zuoberst
		beginInsertRows( QModelIndex(), 0, 0 );
		d_slots.prepend( info.d_id );
		endInsertRows();
	}else if( info.d_kind == Udb::UpdateInfo::QueueErased && info.d_parent == d_queue.getOid() )
	{
		// TODO: effizienter, ev. Quicksearch
		for( int i = 0; i < d_slots.size(); i++ )
		{
			if( d_slots[i] == info.d_id )
			{		
				beginRemoveRows( QModelIndex(), i, i );
				d_slots.removeAt( i );
				endRemoveRows();
			}
		}
	}else if( info.d_kind == Udb::UpdateInfo::ObjectErased && d_queue.getOid() == info.d_id )
	{
		d_queue = Udb::Obj();
		refill();
	}
}

void InvQueueMdl::refill()
{
	d_slots.clear();
	reset();
}

QVariant InvQueueMdl::data( const Udb::Obj&, int role ) const
{
    return QVariant();
}

QVariant InvQueueMdl::data(const Stream::DataCell &, int role) const
{
    Q_UNUSED(role);
    return QVariant();
}
