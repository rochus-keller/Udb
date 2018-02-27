#ifndef __Udb_IndexMeta__
#define __Udb_IndexMeta__

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


#include <QList>

namespace Udb
{
	typedef quint32 Index;

	struct IndexMeta
	{
        enum { AttrParent = 0xffffff81,
             AttrType = 0xffffff86 }; // Damit man nach Parent und Type indizieren kann

		// TODO: Index optional auf gewisse Types Eingrenzen
		// TODO: Index optional für alle Elemente NonNull fordern

		// Die Enums sind persistent. Vorsicht bei Änderung.
		enum Kind 
		{ 
			Value = 1,	// Mehrere Items zulässig. Diese werden einfach binär hintereinandergefügt.
			Unique = 2 // Value, aber Wert der Items wird ohne nachgestellte ID gespeichert. Nur für Value
		};
		Kind d_kind;

		enum Collation 
		{ 
			None = 0, // Ergebnis UTF-8
			NFKD_CanonicalBase = 1 // Mache QChar::decompose und verwende bei QChar::Canonical nur Basiszeichen für Vergleich, sonst alles, Ergebnis UTF-8
		};

		struct Item
		{
			quint32 d_atom; // Feld, das indiziert wird
			bool d_nocase; // true..transformiere in Kleinbuchstaben vor Vergleich (falls Text)
			bool d_invert; // true..invertiere Daten so dass absteigend sortiert wird

			quint8 d_coll; // Collation

			Item( quint32 atom = 0, Collation c = None, bool nc = true, bool inv = false ):
				d_atom(atom),d_nocase(nc),d_invert(inv),d_coll(c) {}
		};
		QList<Item> d_items;

		IndexMeta(Kind k = Value):d_kind(k) {}
	};
}

#endif
