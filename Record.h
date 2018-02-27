#ifndef __Udb_Record__
#define __Udb_Record__

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

#include <QMap>
#include <QSet>
#include <Stream/DataCell.h>

namespace Udb
{
	class BtreeCursor;
	typedef quint32 Atom;
	typedef quint64 OID;

	class Record 
	{
	public:
        enum ReservedFields
		{
            MinReservedField = 0xffffff80,  // Die obersten 127 Zahlen im 32-Bit-Bereich gehören also Udb selber
			// Object:
            FieldParent,					// Object, das dieses Aggregat besitzt - Owner
            FieldPrevObj, FieldNextObj,     // Vorheriges/nächstes aggregiertes Object mit gleichem Owner.
            FieldFirstObj, FieldLastObj,    // Liste der im Aggregat enthaltenen Objects.
			// Mixed
            FieldType                       // Speichert den Type des Object als Atom; bei Types steht also der ganze
                                            // 32-Bit-Bereich zur Verfügung (bei Atoms nur bis MinReservedField, s.o.)
		};
		Record();

		virtual bool isDeleted() const { return false; }

		typedef QList<Atom> Fields;

		static void writeField( BtreeCursor&, OID oid, Atom, const Stream::DataCell& );
		static void readField( BtreeCursor&, OID oid, Atom, Stream::DataCell& );
		static void eraseFields( BtreeCursor&, OID oid );
		static void setUuid( BtreeCursor&, OID oid, const QUuid& );
		static QUuid getUuid( BtreeCursor&, OID oid );
		static OID findObject( BtreeCursor&, const QUuid& );
		static Fields getFields( BtreeCursor&, OID oid, bool all = true );
	};

	/* Format
	Records werden in Udb im Prinzip wie folgt gespeichert:
	<uuid> <atom> -> <cell>
	In Sqlite braucht das etwas viel Platz, aber in BDB gibt es Prefix-Compression.
	Darum bilden wir in Sqlite die <uuid> auf einen simplen quint ab:
	<uuid> -> <oid>
	<oid> -> <uuid>
	<oid> <atom> -> <cell>
	<null> -> <oid> : next id

	Queue:
	<oid> -> <id32> // next id
	<oid> <id32> -> <cell>

	Listenelemente: braucht es eigentlich nicht. Man kann ja dafür einfach Objects nehmen
	<oid> -> <id32> // next id
	<oid> <id32> -> <id32> <id32> [ <uuid> | null ] <cell>	// prev, next
	<uuid> -> <oid> <id32>
	*/
}

#endif
