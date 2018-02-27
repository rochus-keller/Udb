#ifndef Udb_ObjChildMdl_H
#define Udb_ObjChildMdl_H

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
#include <Udb/UpdateInfo.h>
#include <QList>

namespace Udb
{
	class ObjChildMdl : public QAbstractItemModel
	{
		Q_OBJECT
	public:
		enum Role { OidRole = Qt::UserRole + 1 };

		ObjChildMdl(QObject *parent);
		virtual ~ObjChildMdl();

		void setRoot( const Obj& );
		void setInverted( bool );
		const Obj& getRoot() const { return d_root; }
		Udb::Obj getObj( const QModelIndex & ) const;
		QModelIndex getIndex( const Udb::Obj&, bool fetch = false ) const;
		void refill();
		bool isReadOnly() const;

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
		int fetch( int max, QList<Udb::Obj>* l = 0 ) const;
		// Calldowns
		virtual QVariant data( const Obj&, int section, int role ) const;
		virtual bool isSupportedType( quint32 ) const;
	public slots:
		virtual void onDbUpdate( const Udb::UpdateInfo& );
	private:
		Obj d_root;
		QList<Udb::Obj> d_items;
		bool d_inverted;
	};
}

#endif // Udb_ObjChildMdl_H
