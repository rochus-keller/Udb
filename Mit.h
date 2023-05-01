#ifndef _Udb_Mit_
#define _Udb_Mit_

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
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <QVector>

namespace Udb
{
	class Record;
	class Transaction;
	class Database;
	class Obj;
	typedef quint64 OID;

	class Mit
	{
	public:
		// Vorsicht: Mit sieht bei Iteration nur die Keys in der Datenbank, nicht in der Transaktion!
		typedef QVector<Stream::DataCell> KeyList;

		Mit():d_oid(0),d_txn(0) {}
		Mit( const Mit& );
		~Mit();

		bool seek( const KeyList& keys );
		void getValue( Stream::DataCell& ) const;
		Stream::DataCell getValue() const;
		KeyList getKey() const;
		bool firstKey();
		bool nextKey(); // false..kein Nachfolger, unverändert
		bool prevKey(); // false..kein Vorgänger, unverängert

		Mit& operator=( const Mit& r ) { return assign( r ); }
		Mit& assign( const Mit& r );
		OID getOid() const { return d_oid; }
		Transaction* getTxn() const { return d_txn; }
		bool isNull() const { return d_oid == 0; }
	protected:
        friend class Obj;
        Mit( OID, Transaction* );
		void checkNull() const;
		OID d_oid;
		Transaction* d_txn;
		QByteArray d_cur;
		QByteArray d_key;
	};

    class Xit
	{
	public:
		// Vorsicht: Xit sieht bei Iteration nur die Keys in der Datenbank, nicht in der Transaktion!
		Xit():d_oid(0),d_txn(0) {}
		Xit( const Xit& );
		Xit( const Obj& );
		~Xit();

		bool seek( const QByteArray& key );
		void getValue( Stream::DataCell& ) const;
		Stream::DataCell getValue() const;
		QByteArray getKey() const;
		bool firstKey();
		bool nextKey(); // false..kein Nachfolger, unverändert
		bool prevKey(); // false..kein Vorgänger, unverängert

		Xit& operator=( const Xit& r ) { return assign( r ); }
		Xit& assign( const Xit& r );
		OID getOid() const { return d_oid; }
		Transaction* getTxn() const { return d_txn; }
		bool isNull() const { return d_oid == 0; }
	protected:
		Xit( OID, Transaction* );
		void checkNull() const;
		OID d_oid;
		Transaction* d_txn;
		QByteArray d_cur;
		QByteArray d_key;
	};
}

#endif
