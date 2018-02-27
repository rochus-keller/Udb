#ifndef UDBLUABINDING_H
#define UDBLUABINDING_H

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

#include <Udb/Obj.h>
#include <Script2/ValueBinding.h>

namespace Udb
{
	class LuaBinding
	{
	public:
		class RichText : public Stream::DataCell
		{
		public:
			// Vertritt HTML und BML
			RichText() {}
		};

		static QString (*getName)( quint32 atom, Udb::Transaction* txn );
		static Atom (*getAtom)( const QByteArray&, Udb::Transaction* txn );
		static void (*pushObject)(lua_State * L, const Obj& );
		static void install(lua_State * L);
		static void pushValue(lua_State * L, const Stream::DataCell&, Udb::Transaction* = 0 );
		static Stream::DataCell toValue( lua_State * L, int n );
	private:
		LuaBinding() {}
	};

	template<class T, class SuperClass = T>
	class CoBin : public Lua::ValueBinding<T,SuperClass>
	{
	public:
		typedef Lua::ValueBinding<T,SuperClass> Super;
		static void install( lua_State *L, const char* publicName, const luaL_reg* ms = 0 )
		{
			Super::install( L, publicName, ms, false );

			const int stackTest = lua_gettop(L);
			Super::pushMetaTable( L );
			const int metaTable = lua_gettop( L );

			lua_pushliteral(L, "CoBin" );
			lua_rawseti(L, metaTable, Lua::ValueBindingBase::BindingName );

			lua_pushliteral(L, "__index" );
			lua_pushcfunction(L, index2 );
			lua_rawset(L, metaTable);

			lua_pushliteral(L, "__newindex" );
			lua_pushcfunction(L, newindex2 );
			lua_rawset(L, metaTable);

			lua_pop(L, 1);  // drop meta
			if( stackTest != lua_gettop(L) )
				throw Lua::ValueBindingBase::Exception("stack missmatch");
		}
		static T* cast(lua_State *L, int narg = 1, bool recursive = true, bool checkAlive = false )
		{
			T* p = Super::cast( L, narg, recursive );
			if( p != 0 && p->isNull(true,true) )
				luaL_error( L, "dereferencing null object!" );
			else
				return p;
		}
		static T* check(lua_State *L, int narg = 1, bool recursive = true )
		{
			T* p = Super::check( L, narg, recursive );
			Q_ASSERT( p != 0 );
			if( p->isNull(true,true) )
				luaL_error( L, "dereferencing null object!" );
			return p;
		}
		static int index2(lua_State *L)
		{
			T* obj = check( L, 1 );
			const QByteArray fieldName = luaL_checkstring( L, 2 );
			if( fieldName == "class" || fieldName == "type" || fieldName == "data" )
				return Super::fetch( L, false, false );
			Super::fetch( L, true, false ); // Suche immer zuerst nach Methoden mit gegebenem Namen
			if( !lua_isnil( L, -1 ) )
				return 1;
			lua_pop( L, 1 );
			const quint32 atom = LuaBinding::getAtom( fieldName, obj->getTxn() );
			if( atom )
				LuaBinding::pushValue( L, obj->getValue( atom ) );
			else
				lua_pushnil(L);
			return 1;
		}
		static int newindex2(lua_State *L)
		{
			T* obj = check( L, 1 );
			const QByteArray fieldName = luaL_checkstring( L, 2 );
			if( fieldName == "class" || fieldName == "type" || fieldName == "data" )
				luaL_argerror( L, 2, "cannot write to read-only attributes");
			Super::fetch( L, true, false ); // Suche immer zuerst nach Methoden mit gegebenem Namen
			if( !lua_isnil( L, -1 ) )
				luaL_error( L, "cannot write to '%s'", fieldName.data() );
			const quint32 atom = LuaBinding::getAtom( fieldName, obj->getTxn() );
			if( atom == 0 )
				luaL_error( L, "cannot write to '%s'", fieldName.data() );
			return 0;
		}
	};
}

#endif // UDBLUABINDING_H
