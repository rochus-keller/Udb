#ifndef _Udb_Global_H
#define _Udb_Global_H

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

#include <QObject>
#include <Udb/Transaction.h>
#include <QVector>

namespace Udb
{
	class Database;
	class Global;

	// Global Iterator Git
	class Git
	{
	public:
		// Vorsicht: Git sieht bei Iteration nur die Keys in der Datenbank, nicht in der Transaktion!
		Git():d_global(0) {}
		Git( const Global* );

		bool seek( const QByteArray& key );
		void getValue( QByteArray& ) const;
		QByteArray getValue() const;
		QByteArray getKey() const;
		typedef QVector<Stream::DataCell> KeyList;
		KeyList getKeyAsList() const; // throws exception if key is not BML
		Stream::DataCell getValueAsCell() const;
		bool firstKey();
		bool nextKey(); // false..kein Nachfolger, unverändert
		bool prevKey(); // false..kein Vorgänger, unverängert

		Git& operator=( const Git& r ) { return assign( r ); }
		Git& assign( const Git& r );
		const Global* getGlobal() const { return d_global; }
		bool isNull() const { return d_global == 0; }
	private:
		friend class Global;
		void checkNull() const;
		const Global* d_global;
		QByteArray d_cur;
		QByteArray d_key;
	};

	// Nach dem Vorbild von Mumps
	class Global : public QObject
	{
	public:
		typedef QVector<Stream::DataCell> KeyList;
		explicit Global(Database*, QObject *parent = 0);
		bool isOpen() const { return d_table != 0; }
		quint32 getId() const { return d_table; }
		bool open( quint32 id );
		quint32 create();
		void setCell( const KeyList& key, const Stream::DataCell& value );
		void getCell( const KeyList& key, Stream::DataCell& value ) const;
		Stream::DataCell getCell( const KeyList& key ) const;
		void setCell( const QByteArray& key, const QByteArray& value );
		void getCell( const QByteArray& key, QByteArray& value ) const;
		QByteArray getCell( const QByteArray& key ) const;
		void clearAllCells();
		Git findCells( const QByteArray& key ) const;
		Git findCells( const KeyList& key ) const;
		void commit(); // läuft nicht über Transaction, sondern direkt
		void rollback();
	protected:
		void checkOpen() const;
		BtreeStore* getStore() const;
	private:
		friend class Git;
		Database* d_db;
		quint32 d_table;
		typedef QMap<Transaction::ByteArrayHolder,QByteArray> Map;
		Map d_oix;
	};
}


#endif // _Udb_Global_H
