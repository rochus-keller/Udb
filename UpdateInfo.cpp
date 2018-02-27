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

#include "UpdateInfo.h"
using namespace Udb;

const char* UpdateInfo::s_kindName[] =
{
    "ObjectCreated",
    "ValueChanged",
    "TypeChanged",
    "Aggregated",
    "Deaggregated",
    "ObjectErased",
    "QueueAdded",
    "QueueChanged",
    "QueueErased",
    "MapChanged",
    "OixChanged",
    "DbClosing",
    "PreCommit",
    "PreRollback",
    "Commit",
    "Rollback",
};

QString UpdateInfo::toString() const
{
    QString res = s_kindName[d_kind];
    res += ": ";
    switch( d_kind )
    {
    case ObjectCreated:
        res += QString("oid=%1 type=%2").arg(d_id).arg(d_name);
        break;
    case ValueChanged:
        res += QString("oid=%1 field=%2").arg(d_id).arg(d_name);
        break;
    case TypeChanged:
        res += QString("oid=%1 new=%2 old=%3").arg(d_id).arg(d_name).arg(d_before);
        break;
    case Aggregated:
        res += QString("oid=%1 parent=%2 before=%3").arg(d_id).arg(d_parent).arg(d_before);
        break;
    case Deaggregated:
        res += QString("oid=%1 parent=%2").arg(d_id).arg(d_parent);
        break;
    case ObjectErased:
        res += QString("oid=%1 type=%2").arg(d_id).arg(d_name);
        break;
    case QueueAdded:
    case QueueChanged:
    case QueueErased:
        res += QString("nr=%1 parent=%2").arg(d_id).arg(d_parent);
        break;
    case MapChanged:
    case OixChanged:
        res += QString("oid=%1 key=%2").arg(d_id).arg(toString(d_key));
        break;
    case DbClosing:
    case PreCommit:
    case PreRollback:
    case Commit:
    case Rollback:
        break;
    }
    return res;
}

QString UpdateInfo::toString(const QVector<Stream::DataCell> & key)
{
    QString res;
    foreach( const Stream::DataCell& v, key )
    {
        res += v.toPrettyString();
        res += " ";
    }
    return res;
}
