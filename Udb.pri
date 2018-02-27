SOURCES += \
    ../Udb/BtreeCursor.cpp \
    ../Udb/BtreeMeta.cpp \
    ../Udb/BtreeStore.cpp \
    ../Udb/Database.cpp \
    ../Udb/DatabaseException.cpp \
    ../Udb/Extent.cpp \
    ../Udb/Idx.cpp \
    ../Udb/Mit.cpp \
    ../Udb/Obj.cpp \
    ../Udb/Qit.cpp \
    ../Udb/Record.cpp \
    ../Udb/Transaction.cpp \
    ../Udb/ContentObject.cpp \
    ../Udb/Global.cpp \
	../Udb/InvQueueMdl.cpp

HEADERS += \
    ../Udb/BtreeCursor.h \
    ../Udb/BtreeMeta.h \
    ../Udb/BtreeStore.h \
    ../Udb/Database.h \
    ../Udb/DatabaseException.h \
    ../Udb/Extent.h \
    ../Udb/Idx.h \
    ../Udb/IndexMeta.h \
    ../Udb/Mit.h \
    ../Udb/Obj.h \
    ../Udb/Private.h \
    ../Udb/Qit.h \
    ../Udb/Record.h \
    ../Udb/Transaction.h \
    ../Udb/UpdateInfo.h \
    ../Udb/ContentObject.h \
    ../Udb/Global.h \
	../Udb/InvQueueMdl.h


HasLua {
SOURCES += \
	../Udb/UdbLuaBinding.cpp
HEADERS += \
	../Udb/LuaBinding.h
}
