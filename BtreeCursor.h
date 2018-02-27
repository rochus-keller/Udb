#ifndef __Udb_BtreeCursor__
#define __Udb_BtreeCursor__
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

#include <QByteArray>

struct BtCursor;

namespace Udb
{
	class BtreeStore;

	// Interne Klasse
	// Ein BtreeCursor repräsentiert einen Sqlite-Btree-Table mit einem Qt-kompatiblen Interface
	class BtreeCursor // Value
	{
	public:
		BtreeCursor();
		~BtreeCursor();

		void open( BtreeStore*, int table, bool writing = false );
		void close();

		// Navigation
		bool moveFirst(); // false..empty table
		bool moveLast();  // false..empty table
		bool moveNext();  // false..passed last entry, not pointing on valid entry anymore
		bool movePrev();  // false..passed first entry, not pointing on valid entry anymore
		bool moveTo( const QByteArray& key, bool partial = false ); // partial=true: true..pos beginnt mit key
		bool moveNext( const QByteArray& key );  // partial, false..passed key or last entry
		bool isValidPos(); 

		// moveTo (egal ob partial oder nicht):
		// bei false ist Position entweder auf nächst grösserem Wert als key oder über
		// das Ende hinaus, bzw. !isValidPos.

		// Read/Write
		void insert( const QByteArray& key, const QByteArray& value ); // Unabhängig von Pos
		QByteArray readKey() const; // Pos
		QByteArray readValue() const; // Pos
		void removePos(); // VORSICHT: danach zeigt Cursor ins Kraut! Also nicht geeignet für Move-Loops!

		BtreeStore* getDb() const { return d_db; }
	protected:
		void checkOpen() const;
	private:
		int d_table;
		BtreeStore* d_db;
		BtCursor* d_cur;
	};
}

#endif
