#ifndef __Udb_Idx__
#define __Udb_Idx__

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

namespace Udb
{
	class Transaction;
	typedef quint64 OID;

	// Udb Table Index Class
	class Idx // Value Class
	{
	public:
        typedef QList<Stream::DataCell> Keys;

		Idx():d_txn(0),d_idx(0){}
		Idx( Transaction*, int idx );
		Idx( Transaction*, const QByteArray& name );
		Idx( const Idx& );

		bool first();
		bool last();
		bool firstKey();
		bool seek( const Stream::DataCell& key );
		bool seek( const Stream::DataCell& key1, const Stream::DataCell& key2 );
		bool seek( const Keys& keys );
		bool gotoCur( const QByteArray& );
		bool next();
		bool nextKey();
		bool prev();
		bool prevKey();
        bool isOnKey() const;
        bool isOnIndex() const;
		OID getOid();

		void rebuildIndex(); // RISK: erstelle Index neu
		void clearIndex(); // RISK: lösche Index-Inhalt

		bool isNull() const { return d_idx == 0; }
		Transaction* getTxn() const { return d_txn; }
		const QByteArray& getCur() const { return d_cur; }
		const QByteArray& getKey() const { return d_key; }
		Idx& operator=( const Idx& r );

        // Helper für Indexbau
        static void addElement( QByteArray&, const IndexMeta::Item&, const Stream::DataCell& );
        // Values see IndexMeta::Collation
		static void collate( QByteArray&, quint8 collation, const QString& );
	protected:
		void checkNull() const;
	private:
		friend class Transaction;
		// NOTE: Hier würde Database genügen. Da aber alle Txn benötigen, 
		// wird hier Txn-Pointer gespeichert
		Transaction* d_txn;
		int d_idx;
		QByteArray d_cur;
		QByteArray d_key;
	};
}

#endif
