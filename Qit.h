#ifndef _Udb_Qit_
#define _Udb_Qit_

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

#include <Stream/DataCell.h>
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>


namespace Udb
{
	class Transaction;
	class Database;
	typedef quint64 OID;

	class Qit
	{
	public:
		Qit():d_oid(0),d_txn(0),d_nr(0) {}
		Qit( OID, Transaction*, quint32 nr );
		Qit( const Qit& );
		~Qit();

		void setValue( const Stream::DataCell& );
		void getValue( Stream::DataCell& ) const;
		Stream::DataCell getValue() const;
		void erase();
		bool first();
		bool last();
		bool next(); // false..kein Nachfolger, unverändert
		bool prev(); // false..kein Vorgänger, unverängert

		Qit& operator=( const Qit& r ) { return assign( r ); }
		Qit& assign( const Qit& r );
		quint32 getSlotNr() const { return d_nr; }
		OID getOid() const { return d_oid; }
		Transaction* getTxn() const { return d_txn; }
		bool isNull() const { return d_oid == 0; }
	protected:
		void checkNull() const;
		quint32 d_oid;
		Transaction* d_txn;
		quint32 d_nr;
	};
}

#endif
