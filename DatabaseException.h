#ifndef __Udb_DatabaseException__
#define __Udb_DatabaseException__

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

#include <exception>
#include <QString>

namespace Udb
{
	class DatabaseException : public std::exception
	{
	public:
		enum Code {
			OpenDbFile,
			StartTrans,
			CommitTrans,
			AccessMeta,
			CreateBtCursor,
			CreateTable,
			RemoveTable,
			ClearTable,
			DatabaseMeta,
			AccessDatabase,
			DatabaseFormat,
			DirectoryFormat,
			DuplicateAtom,
			RecordLocked,
			RecordDeleted,
			WrongContext,
			ReservedName,
			AccessRecord,
			AccessCursor,
			IndexExists,
			NotOpen,
			OidOutOfRange
			// Strigliste anpassen
		};
		static const char* s_code[];
		DatabaseException( Code c, const QString& msg = "" ):d_err(c),d_msg(msg){}
		virtual ~DatabaseException() throw() {}
		Code getCode() const { return d_err; }
		QString getCodeString() const;
		const QString& getMsg() const { return d_msg; }
	private:
		Code d_err;
		QString d_msg;
	};

}

#endif
