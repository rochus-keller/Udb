#ifndef Udb_InvQueueMdl_H
#define Udb_InvQueueMdl_H

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

#include <QAbstractItemModel>
#include <Udb/Obj.h>
#include <Udb/Qit.h>
#include <Udb/UpdateInfo.h>
#include <QList>

namespace Udb
{
	class InvQueueMdl : public QAbstractItemModel
	{
		Q_OBJECT
	public:
		enum Role { 
			// Defaultmässig wird die oid des Objekts zurückgeliefert
			SlotNrRole = Qt::UserRole + 1
		};

		InvQueueMdl(QObject *parent);
		~InvQueueMdl();

		void setQueue( const Obj& queue );
		const Obj& getQueue() const { return d_queue; }
		Udb::Obj getObj( const QModelIndex & ) const;
		Udb::Qit getItem( const QModelIndex & ) const;
        QModelIndex getIndex( quint32 nr, bool fetch = false ) const;
        QModelIndex getIndex( const Udb::Qit&, bool fetch = false ) const;
		void refill();

		// Overrides
        int columnCount( const QModelIndex & = QModelIndex() ) const { return 1; }
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
		Qt::ItemFlags flags(const QModelIndex &index) const;
		bool canFetchMore ( const QModelIndex & parent ) const;
		void fetchMore ( const QModelIndex & parent );
	protected:
		int fetch( int max, QList<quint32>* l = 0 ) const;
		// Calldowns
		virtual QVariant data( const Obj&, int role ) const;
        virtual QVariant data( const Stream::DataCell&, int role ) const;
	public slots:
		void onDbUpdate( Udb::UpdateInfo );
	private:
		Obj d_queue; // Geordnete Liste von Objekten; Queue-Value ist OID
		QList<quint32> d_slots; // Liste der slot-nr
	};
}

#endif // Udb_InvQueueMdl_H
