#ifndef __Udb_BtreeMeta__
#define __Udb_BtreeMeta__

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

#include <QByteArray>

namespace Udb
{
	class BtreeStore;

	// Zugriff muss zwischen Threads serialisiert werden. Wir verwenden nicht SQLITE_THREADSAFE.
	// Darum ist sowohl für lesen als auch schreiben ein vorgelagerter Mutex erforderlich.
	class BtreeMeta // Value
	{
	public:
		BtreeMeta( BtreeStore* db ):d_db(db) {}

		// Meta-Access
		void write( const QByteArray& key, const QByteArray& val );
		QByteArray read( const QByteArray& key ) const;
		void erase( const QByteArray& key );
	private:
		BtreeStore* d_db;
	};
}

#endif
