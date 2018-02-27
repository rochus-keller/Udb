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

#include "LuaBinding.h"
#include <Script2/QtValue.h>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <Txt/TextInStream.h>
#include <Txt/TextOutHtml.h>
#include <Udb/Transaction.h>
#include <Udb/ContentObject.h>
using namespace Udb;
using namespace Stream;
using namespace Lua;

struct _RichText
{
	static int toLuaString( lua_State * L)
	{
		if( lua_isuserdata( L, 1 ) )
		{
			LuaBinding::RichText* obj = ValueBinding<LuaBinding::RichText>::check( L, 1 );
			bool stripMarkup = true;
			if( lua_isboolean( L, 2 ) )
				stripMarkup = lua_toboolean( L, 2 );
			lua_pushstring( L, obj->toString( stripMarkup ).toLatin1() );
		}else
		{
			lua_pushstring( L, lua_tostring( L, 1 ) );
		}
		return 1;
	}
	static int toQString( lua_State * L)
	{
		LuaBinding::RichText* obj = ValueBinding<LuaBinding::RichText>::check( L, 1 );
		bool stripMarkup = true;
		if( lua_isboolean( L, 2 ) )
			stripMarkup = lua_toboolean( L, 2 );
		*QtValue<QString>::create( L ) = obj->toString( stripMarkup );
		return 1;
	}
	static int toHtml( lua_State* L )
	{
		if( lua_isuserdata(L, 1 ) )
		{
			// Params: this, fragmentOnly, charset
			LuaBinding::RichText* obj = ValueBinding<LuaBinding::RichText>::check( L, 1 );
			QString* str = QtValue<QString>::create( L );
			QByteArray charset = "latin-1";
			if( lua_isstring( L, 3 ) )
				charset = lua_tostring( L, 3 );
			if( obj->isHtml() )
			{
				if( lua_toboolean( L, 2 ) )
				{
					QTextDocument doc;
					doc.setHtml( obj->getStr() );
					QTextDocumentFragment f( &doc );
					*str = f.toHtml( charset );
				}else
					*str = obj->getStr();
			}else if( obj->isBml() )
			{
				QTextDocument doc;
				Txt::TextInStream in;
				in.readFromTo( *obj, &doc );
				Txt::TextOutHtml out( &doc );
				*str = out.toHtml( lua_toboolean( L, 2 ), charset );
			}else
			{
				if( lua_toboolean( L, 2 ) )
					*str = obj->toString();
				else
					*str = QString( "<html><body>%1</body></html>" ).arg( obj->toString() );
			}
		}else
		{
			*QtValue<QString>::create( L ) = QString::fromLatin1( lua_tostring( L, 1 ) );
		}
		return 1;
	}
	static int setString(lua_State* L)
	{
		LuaBinding::RichText* obj = ValueBinding<LuaBinding::RichText>::check( L, 1 );
		obj->setString( QtValueBase::toString(L, 2 ) );
		return 1;
	}
	static int init(lua_State* L)
	{
		LuaBinding::RichText* obj = ValueBinding<LuaBinding::RichText>::check( L, 1 );
		obj->fromVariant( QtValueBase::toVariant( L, 2 ) );
		return 0;
	}
};

static const luaL_reg _RichText_reg[] =
{
	{ "setString", _RichText::setString },
	{ "toLuaString", _RichText::toLuaString },
	{ "toQString", _RichText::toQString },
	{ "toHtml", _RichText::toHtml },
	{ "init", _RichText::init },
	{ 0, 0 }
};

struct _ContentObject
{
	static int getCreatedOn(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		*QtValue<QDateTime>::create(L) = obj->getCreatedOn();
		return 1;
	}
	static int getModifiedOn(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		*QtValue<QDateTime>::create(L) = obj->getModifiedOn();
		return 1;
	}
	static int getInternalID(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		*QtValue<QString>::create(L) = obj->getIdent();
		return 1;
	}
	static int getCustomID(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		*QtValue<QString>::create(L) = obj->getAltIdent();
		return 1;
	}
	static int setCustomID(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		obj->setAltIdent( QtValueBase::toString( L, 2 ) );
		obj->setModifiedOn();
		return 1;
	}
	static int getText(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		const DataCell v = obj->getValue( ContentObject::AttrText );
		if( v.isBml() || v.isHtml() )
			ValueBinding<LuaBinding::RichText>::create(L)->assign( v );
		else if( v.isStr() )
			*QtValue<QString>::create(L) = v.getStr();
		else if( v.isCStr() )
			lua_pushstring( L, v.getArr() );
		else
			lua_pushnil(L);
		return 1;
	}
	static int setText(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		if( QtValueBase::isString( L, 2 ) )
			obj->setText( QtValueBase::toString( L, 2, false ) );
		else if( LuaBinding::RichText* rt = ValueBinding<LuaBinding::RichText>::cast( L, 2 ) )
			obj->setValue( ContentObject::AttrText, *rt );
		else
			luaL_argerror( L, 2, "expecting luastring, QString or LuaBinding::RichText" );
		obj->setModifiedOn();
		return 0;
	}
	static int commit(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		obj->commit();
		return 0;
	}
	static int rollback(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		obj->getTxn()->rollback();
		return 0;
	}
	static int getParent(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		LuaBinding::pushObject( L, obj->getParent() );
		return 1;
	}
	static int getOid(lua_State *L)
	{
		ContentObject* obj = CoBin<ContentObject>::check( L, 1 );
		lua_pushinteger( L, obj->getOid() );
		return 1;
	}
};

static const luaL_reg _ContentObject_reg[] =
{
	{ "getOid", _ContentObject::getOid },
	{ "setText", _ContentObject::setText },
	{ "getText", _ContentObject::getText },
	{ "setCustomID", _ContentObject::setCustomID },
	{ "getCustomID", _ContentObject::getCustomID },
	{ "getInternalID", _ContentObject::getInternalID },
	{ "getCreatedOn", _ContentObject::getCreatedOn },
	{ "getModifiedOn", _ContentObject::getModifiedOn },
	{ "commit", _ContentObject::commit },
	{ "rollback", _ContentObject::rollback },
	{ "getParent", _ContentObject::getParent },
	{ 0, 0 }
};

struct _TextDocument
{
	static int getBody(lua_State *L)
	{
		TextDocument* obj = CoBin<TextDocument>::check( L, 1 );
		const DataCell v = obj->getValue( TextDocument::AttrBody );
		if( v.isBml() || v.isHtml() )
			ValueBinding<LuaBinding::RichText>::create(L)->assign( v );
		else if( v.isStr() )
			*QtValue<QString>::create(L) = v.getStr();
		else if( v.isCStr() )
			lua_pushstring( L, v.getArr() );
		else
			lua_pushnil(L);
		return 1;
	}
	static int setBody(lua_State *L)
	{
		TextDocument* obj = CoBin<TextDocument>::check( L, 1 );
		if( QtValueBase::isString( L, 2 ) )
			obj->setBody( QtValueBase::toString( L, 2, false ) );
		else if( LuaBinding::RichText* rt = ValueBinding<LuaBinding::RichText>::cast( L, 2 ) )
			obj->setValue( TextDocument::AttrBody, *rt );
		else
			luaL_argerror( L, 2, "expecting luastring, QString or LuaBinding::RichText" );
		obj->setModifiedOn();
		return 0;
	}
};

static const luaL_reg _TextDocument_reg[] =
{
	{ "getBody", _TextDocument::getBody },
	{ "setBody", _TextDocument::setBody },
	{ 0, 0 }
};

struct _ScriptSource
{
	static int getSource(lua_State *L)
	{
		ScriptSource* obj = CoBin<ScriptSource>::check( L, 1 );
		const DataCell v = obj->getValue( ScriptSource::AttrSource );
		if( v.isStr() )
			*QtValue<QString>::create(L) = v.getStr();
		else if( v.isCStr() )
			lua_pushstring( L, v.getArr() );
		else
			lua_pushnil(L);
		return 1;
	}
	static int setSource(lua_State *L)
	{
		ScriptSource* obj = CoBin<ScriptSource>::check( L, 1 );
		if( QtValueBase::isString( L, 2 ) )
			obj->setSource( QtValueBase::toString( L, 2, false ) );
		else
			luaL_argerror( L, 2, "expecting luastring or QString" );
		obj->setModifiedOn();
		return 0;
	}
	static int getLang(lua_State *L)
	{
		ScriptSource* obj = CoBin<ScriptSource>::check( L, 1 );
		lua_pushstring( L, obj->getLang() );
		return 1;
	}
};

static const luaL_reg _ScriptSource_reg[] =
{
	{ "getSource", _ScriptSource::getSource },
	{ "setSource", _ScriptSource::setSource },
	{ "getLang", _ScriptSource::getLang },
	{ 0, 0 }
};


void LuaBinding::install(lua_State *L)
{
	ValueBinding<LuaBinding::RichText>::install( L, "LuaBinding::RichText", _RichText_reg );
	ValueBinding<LuaBinding::RichText>::addMetaMethod( L, "__tostring", _RichText::toLuaString );
	CoBin<ContentObject>::install( L, "ContentObject", _ContentObject_reg );
	CoBin<Folder,ContentObject>::install( L, "Folder" );
	CoBin<TextDocument,ContentObject>::install( L, "TextDocument", _TextDocument_reg );
	CoBin<ScriptSource,ContentObject>::install( L, "Script", _ScriptSource_reg );
}

void LuaBinding::pushValue(lua_State *L, const DataCell & v, Transaction* txn)
{
	switch( v.getType() )
	{
	case DataCell::TypeNull:
		lua_pushnil( L );
		break;
	case DataCell::TypeAtom:
		if( txn )
			lua_pushstring( L, getName( v.getAtom(), txn ).toLatin1() );
		else
			lua_pushnil( L );
		break;
	case DataCell::TypeOid:
		if( txn )
			pushObject( L, txn->getObject( v ) );
		else
			lua_pushnil( L );
		break;
	case DataCell::TypeBml:
	case DataCell::TypeHtml:
		ValueBinding<LuaBinding::RichText>::create( L )->assign( v );
		break;
	default:
		QtValueBase::pushVariant( L, v.toVariant() );
		break;
	}
}

DataCell LuaBinding::toValue(lua_State *L, int n)
{
	DataCell res;
	if( LuaBinding::RichText* obj = ValueBinding<LuaBinding::RichText>::cast( L, n ) )
		res = *obj;
	else if( ContentObject* obj = ValueBinding<ContentObject>::cast( L, n ) )
		res.setOid( obj->getOid() );
	else
		res.fromVariant( QtValueBase::toVariant( L, n ) );
	return res;
}

static QString _getName( quint32 atom, Transaction* txn )
{
	return txn->getAtomString( atom );
}

QString (*LuaBinding::getName)( quint32 atom, Transaction* txn ) = _getName;


static Atom _getAtom( const QByteArray& s, Transaction* txn )
{
	return txn->getAtom( s );
}

Atom (*LuaBinding::getAtom)( const QByteArray&, Transaction* txn ) = _getAtom;

static void _pushObject(lua_State * L, const Obj& o )
{
	if( o.isNull() )
		lua_pushnil(L);
	else
		*CoBin<ContentObject>::create( L, o );
}

void (*LuaBinding::pushObject)(lua_State * L, const Obj& ) = _pushObject;
