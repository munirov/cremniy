QT += widgets

DEFINES += "QHEXVIEW_ENABLE_DIALOGS=1"

HEADERS += $$PWD/include/QHexView/model/commands/hexviewcommand.h \
           $$PWD/include/QHexView/model/commands/insertcommand.h \
           $$PWD/include/QHexView/model/commands/removecommand.h \
           $$PWD/include/QHexView/model/commands/replacecommand.h \
           $$PWD/include/QHexView/model/buffer/qdevicebuffer.h \
           $$PWD/include/QHexView/model/buffer/qhexbuffer.h \
           $$PWD/include/QHexView/model/buffer/qmemorybuffer.h \
           $$PWD/include/QHexView/model/buffer/qmemoryrefbuffer.h \
           $$PWD/include/QHexView/model/buffer/qmappedfilebuffer.h \
           $$PWD/include/QHexView/model/qhexdelegate.h \
           $$PWD/include/QHexView/model/qhexchanges.h \
           $$PWD/include/QHexView/model/qhexutils.h \
           $$PWD/include/QHexView/model/qhexcursor.h \
           $$PWD/include/QHexView/model/qhexmetadata.h \
           $$PWD/include/QHexView/model/qhexoptions.h \
           $$PWD/include/QHexView/model/qhexdocument.h \
           $$PWD/include/QHexView/dialogs/hexfinddialog.h \
           $$PWD/include/QHexView/qhexview.h

SOURCES += $$PWD/src/model/commands/hexviewcommand.cpp \
           $$PWD/src/model/commands/insertcommand.cpp \
           $$PWD/src/model/commands/removecommand.cpp \
           $$PWD/src/model/commands/replacecommand.cpp \
           $$PWD/src/model/buffer/qdevicebuffer.cpp \
           $$PWD/src/model/buffer/qhexbuffer.cpp \
           $$PWD/src/model/buffer/qmemorybuffer.cpp \
           $$PWD/src/model/buffer/qmemoryrefbuffer.cpp \
           $$PWD/src/model/buffer/qmappedfilebuffer.cpp \
           $$PWD/src/model/qhexdelegate.cpp \
           $$PWD/src/model/qhexutils.cpp \
           $$PWD/src/model/qhexcursor.cpp \
           $$PWD/src/model/qhexmetadata.cpp \
           $$PWD/src/model/qhexdocument.cpp \
           $$PWD/src/dialogs/hexfinddialog.cpp \
           $$PWD/src/qhexview.cpp

INCLUDEPATH += $$PWD/include
