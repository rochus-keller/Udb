#ifndef __Udb_Extent__
#define __Udb_Extent__

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

#include <Stream/DataCell.h>
#include <Udb/IndexMeta.h>
#include <Udb/Obj.h>

namespace Udb
{
	class Transaction;

	class Extent
	{
	public:
		Extent():d_txn(0),d_oid(0) {}
		Extent( Transaction* t ):d_txn(t),d_oid(0) {}
		bool first();
		bool next();
		Obj getObj() const;		

		static void eraseOrphans( Transaction* ); // Bereinigt den Bug in Record::eraseFields
		static QString checkDb( Transaction*, bool fix = false );
	protected:
		void checkNull() const;
	private:
		Transaction* d_txn;
		quint64 d_oid;
	};
}

#endif
