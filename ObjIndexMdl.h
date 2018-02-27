#ifndef OBJINDEXMDL_H
#define OBJINDEXMDL_H

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
#include <Udb/UpdateInfo.h>
#include <Udb/Idx.h>
#include <QList>
#include <QSet>

namespace Udb
{
	class ObjIndexMdl : public QAbstractItemModel
	{
		Q_OBJECT

	public:
		ObjIndexMdl(QObject *parent):QAbstractItemModel(parent),d_inverted(false),d_refetch(false) {}

		Udb::OID getOid(const QModelIndex &) const;
        QModelIndex getIndex( Udb::OID ) const;
		void setIdx( const Udb::Idx& );
		void seek( const Stream::DataCell& );
		void refill();
		Udb::OID getFirstInList() const;
        Udb::Transaction* getTxn() const { return d_idx.getTxn(); }
        bool getInverted() const { return d_inverted; }
        void setInverted( bool );

		// Overrides
		int columnCount( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
		Qt::ItemFlags flags(const QModelIndex &index) const;
		bool canFetchMore ( const QModelIndex & parent ) const;
		void fetchMore ( const QModelIndex & parent );
	protected slots:
		virtual void onDbUpdate( const Udb::UpdateInfo& );
    protected:
        // To override
        virtual bool filtered( Udb::OID );
	private:
	    mutable Udb::Idx d_idx;
		QList<Udb::OID> d_ids;
        bool d_inverted; // Geht nicht, da idx.lastKey sehr teuer. Besser Index invertieren.
		bool d_refetch;
	};
}

#endif // OBJINDEXMDL_H
