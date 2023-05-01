#ifndef CONTENTOBJECT_H
#define CONTENTOBJECT_H

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

#include <Udb/Obj.h>

namespace Udb
{
    class ContentObject : public Obj
    { // Abstract, no TID
    public:
		static quint32 AttrCreatedOn;   // DateTime
		static quint32 AttrModifiedOn;  // DateTime
		static quint32 AttrIdent;       // String, wird bei einigen Klassen mit automatischer ID gesetzt
		static quint32 AttrAltIdent;    // String, der User kann seine eigene ID verwenden, welche die automatische übersteuert
		static quint32 AttrText;        // String, RTXT oder HTML, optional; anzeigbarer Titel/Text des Objekts

        ContentObject( const Udb::Obj& o ):Udb::Obj(o){}
		ContentObject(){}
		void setCreatedOn( const QDateTime& = QDateTime() ); // default now
        QDateTime getCreatedOn() const;
        void setModifiedOn( const QDateTime& = QDateTime() ); // default now
        QDateTime getModifiedOn() const;
        void setIdent( const QString& );
        QString getIdent() const;
		QString getAltOrIdent() const;
		void setAltIdent( const QString& );
        QString getAltIdent() const;
        void setText( const QString& );
		QString getText(bool strip = true) const;
        void setTextHtml( const QString& );
        QString getTextHtml() const;
        void setTextBml( const QByteArray& );
        QByteArray getTextBml() const;
		static QString (*formatTimeStamp)( const QDateTime& );
    };

    class Folder : public ContentObject
    {
    public:
        static quint32 TID; // 1
        Folder( const Udb::Obj& o ):ContentObject(o){}
		Folder(){}
	};

    class RootFolder : public Folder
    {
    public:
        static quint32 TID; // 3
        static const QUuid s_uuid;
        RootFolder( const Udb::Obj& o ):Folder(o){}
		RootFolder(){}
		static RootFolder getOrCreate( Transaction* );
    };

    class TextDocument : public ContentObject
    {
    public:
        static quint32 TID; // 2
		// AttrText ist der Titel
		static quint32 AttrBody;   // String, RTXT oder HTML, optional; Text des Dokuments
        TextDocument( const Udb::Obj& o ):ContentObject(o){}
		TextDocument(){}

        void setBody( const QString& );
        QString getBody() const;
        void setBodyHtml( const QString& );
        QString getBodyHtml() const;
        void setBodyBml( const QByteArray& );
        QByteArray getBodyBml() const;
    };

	class ScriptSource : public ContentObject
	{
	public:
		static quint32 TID; // 4
		// AttrText ist der Titel
		static quint32 AttrSource;     // String oder Ascii, optional; Source des Scripts
		static quint32 AttrLang;       // Tag; optional (es wird "Lua" angenommen, wenn fehlt)

		ScriptSource( const Udb::Obj& o ):ContentObject(o){}
		ScriptSource() {}

		void setSource( const QString& );
		QString getSource() const;
		void setSourceAscii( const QByteArray& );
		QByteArray getSourceAscii() const;
		void setLang( const QByteArray& );
		QByteArray getLang() const;
	};
}



#endif // CONTENTOBJECT_H
