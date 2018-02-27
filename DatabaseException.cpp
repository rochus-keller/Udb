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
#include "DatabaseException.h"
using namespace Udb;

const char* DatabaseException::s_code[] =
{
	"OpenDbFile",
	"StartTrans",
	"CommitTrans",
	"AccessMeta",
	"CreateBtCursor",
	"CreateTable",
	"RemoveTable",
	"ClearTable",
	"DatabaseMeta",
	"AccessDatabase",
	"DatabaseFormat",
	"DirectoryFormat",
	"DuplicateAtom",
	"RecordLocked",
	"RecordDeleted",
	"WrongContext",
	"ReservedName",
	"AccessRecord",
	"AccessCursor",
	"IndexExists",
	"OidOutOfRange"
};

QString DatabaseException::getCodeString() const
{
	return s_code[d_err];
}
