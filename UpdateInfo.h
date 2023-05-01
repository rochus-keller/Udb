#ifndef __Udb_UpdateInfo__
#define __Udb_UpdateInfo__

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

#include <QMetaType>
#include <Stream/DataCell.h>
#include <QVector>

namespace Udb
{
	struct UpdateInfo
	{
		enum Kind
		{
			ObjectCreated,	// id=oid, name=type
			ValueChanged,	// id=oid, name=field, für Obj::setValue
			TypeChanged,	// id=oid, name=new_type, before=old_type, für Obj::setType
			Aggregated,		// id=object parent=oid, before (0==last)
			Deaggregated,	// id=object parent=oid name2=type
			ObjectErased,	// id=oid, name=type
			QueueAdded,		// id=nr, parent=oid
			QueueChanged,	// id=nr, parent=oid
			QueueErased,	// id=nr, parent=oid
			MapChanged,		// id=oid, key
            OixChanged,		// id=oid, key als Element 0 lob
			DbClosing,
            PreCommit,      // Commit unmittelbar bevorstehend; nur via Pre-Commit-Notification
			PreRollback,    // Rollback unmittelbar bevorstehend; nur via Pre-Commit-Notification
			Commit,			// Commit Completed; nur via Pre-Commit-Notification
            Rollback		// Rollback Completed; nur via Pre-Commit-Notification
		};
        static const char* s_kindName[];
		quint8 d_kind;
		quint32 d_id;
		union
		{
			quint32 d_name;
			quint32 d_parent;
		};
		union
		{
			quint32 d_before;
			quint32 d_name2;
		};
		QVector<Stream::DataCell> d_key;
		UpdateInfo(quint8 k = 0):d_kind(k),d_name(0),d_id(0),d_before(0){}
        QString toString() const;
        static QString toString( const QVector<Stream::DataCell>&);
	};
}
Q_DECLARE_METATYPE(Udb::UpdateInfo)

#endif
