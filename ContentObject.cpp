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
#include "ContentObject.h"
#include "Transaction.h"
using namespace Udb;
using namespace Stream;

quint32 ContentObject::AttrCreatedOn = 1;
quint32 ContentObject::AttrModifiedOn = 2;
quint32 ContentObject::AttrIdent = 3;
quint32 ContentObject::AttrAltIdent = 4;
quint32 ContentObject::AttrText = 5;
quint32 Folder::TID = 1;
quint32 TextDocument::TID = 2;
quint32 TextDocument::AttrBody = 6;
quint32 RootFolder::TID = 3;
quint32 ScriptSource::TID = 4;
quint32 ScriptSource::AttrSource = 7;
quint32 ScriptSource::AttrLang = 8;

const QUuid RootFolder::s_uuid = "{17d07d9c-8c1b-41f3-ab54-361291dfdd98}";

void ContentObject::setCreatedOn(const QDateTime & dt)
{
    if( dt.isValid() )
        setValue( AttrCreatedOn, DataCell().setDateTime( dt ));
    else
        setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ));
}

QDateTime ContentObject::getCreatedOn() const
{
    return getValue( AttrCreatedOn ).getDateTime();
}

void ContentObject::setModifiedOn(const QDateTime & dt)
{
    if( dt.isValid() )
        setValue( AttrModifiedOn, DataCell().setDateTime( dt ));
    else
        setValue( AttrModifiedOn, DataCell().setDateTime( QDateTime::currentDateTime() ));
}

QDateTime ContentObject::getModifiedOn() const
{
    return getValue( AttrModifiedOn ).getDateTime();
}

void ContentObject::setIdent(const QString & s)
{
    setString( AttrIdent, s );
}

QString ContentObject::getIdent() const
{
	return getString( AttrIdent, true );
}

QString ContentObject::getAltOrIdent() const
{
	QString id = getAltIdent();
	if( id.isEmpty() )
		id = getIdent();
	return id;
}

void ContentObject::setAltIdent(const QString & s)
{
    setString( AttrAltIdent, s );
}

QString ContentObject::getAltIdent() const
{
	return getString( AttrAltIdent, true );
}

void ContentObject::setText(const QString & s)
{
	setString( AttrText, s );
}

QString ContentObject::getText(bool strip) const
{
	if( isNull() )
		return QString();
	return getString( AttrText, strip );
}

void ContentObject::setTextHtml(const QString & s)
{
    setValue( AttrText, DataCell().setHtml( s ) );
}

QString ContentObject::getTextHtml() const
{
    return getValue( AttrText ).getStr();
}

void ContentObject::setTextBml(const QByteArray & s)
{
    setValue( AttrText, DataCell().setBml(s) );
}

QByteArray ContentObject::getTextBml() const
{
    return getValue( AttrText ).getBml();
}

void TextDocument::setBody(const QString & str)
{
	setString( AttrBody, str );
}

QString TextDocument::getBody() const
{
    return getString( AttrBody );
}

void TextDocument::setBodyHtml(const QString & s)
{
    setValue( AttrBody, DataCell().setHtml( s ) );
}

QString TextDocument::getBodyHtml() const
{
    return getValue( AttrBody ).getStr();
}

void TextDocument::setBodyBml(const QByteArray & s)
{
    setValue( AttrBody, DataCell().setBml(s) );
}

QByteArray TextDocument::getBodyBml() const
{
    return getValue( AttrBody ).getBml();
}

RootFolder RootFolder::getOrCreate(Transaction * txn)
{
    Q_ASSERT( txn != 0 );
	return txn->getOrCreateObject( s_uuid, TID );
}

void ScriptSource::setSource(const QString & s)
{
	setValue( AttrSource, DataCell().setString( s ) );
}

QString ScriptSource::getSource() const
{
	return getValue( AttrSource ).toString();
}

void ScriptSource::setSourceAscii(const QByteArray & s)
{
	setValue( AttrSource, DataCell().setAscii( s ) );
}

QByteArray ScriptSource::getSourceAscii() const
{
	return getValue( AttrSource ).getArr();
}

void ScriptSource::setLang(const QByteArray & s)
{
	setValue( AttrLang, DataCell().setTag( Stream::NameTag(s.data()) ) );
}

QByteArray ScriptSource::getLang() const
{
	QByteArray lang = getValue( AttrLang ).getTag().toByteArray();
	if( lang.isEmpty() )
		lang = "Lua";
	return lang;
}

static QString _formatTimeStamp( const QDateTime& dt )
{
	return dt.toString( QLatin1String("yyyy-MM-dd hh:mm") );
}

QString (*ContentObject::formatTimeStamp)( const QDateTime& ) = _formatTimeStamp;
