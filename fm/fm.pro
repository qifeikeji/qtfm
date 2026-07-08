include($${top_srcdir}/share/qtfm.pri)

QT+= widgets concurrent svg
CONFIG += c++11

TARGET = $${QTFM_TARGET}
TARGET_NAME = $${QTFM_TARGET_NAME}
VERSION = $${QTFM_MAJOR}.$${QTFM_MINOR}.$${QTFM_PATCH}
TEMPLATE = app

INCLUDEPATH += src $${top_srcdir}/libfm $${top_srcdir}/libfm/qtcopydialog

DEFINES += APP=\"\\\"$${TARGET}\\\"\"
DEFINES += APP_NAME=\"\\\"$${TARGET_NAME}\\\"\"
DEFINES += APP_VERSION=\"\\\"$${VERSION}\\\"\"

HEADERS += \
    src/mainwindow.h \
    src/tabbar.h \
    src/settingsdialog.h \
    src/openwithsettingswidget.h \
    src/sidebaritemdelegate.h

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/bookmarks.cpp \
    src/tabbar.cpp \
    src/settingsdialog.cpp \
    src/openwithsettingswidget.cpp \
    src/actiondefs.cpp \
    src/actiontriggers.cpp \
    src/sidebaritemdelegate.cpp

RESOURCES += $${top_srcdir}/share/$${TARGET}.qrc \
              $${top_srcdir}/share/mimes.qrc

macx {
    LIBS += -L$${top_builddir}/libfm -lQtFM
    DEFINES += NO_DBUS NO_UDISKS
    RESOURCES += bundle/adwaita.qrc
    mimes_bundle.files = $$files($${top_srcdir}/share/icons/mimes/*.*)
    mimes_bundle.path = Contents/Resources/mimes
    QMAKE_BUNDLE_DATA += mimes_bundle
    ICON = $${top_srcdir}/share/images/QtFM.icns
    QMAKE_INFO_PLIST = $${top_srcdir}/share/Info.plist
}

unix:!macx {
    DESTDIR = $${top_builddir}/bin
    OBJECTS_DIR = $${DESTDIR}/.obj_fm
    MOC_DIR = $${DESTDIR}/.moc_fm
    RCC_DIR = $${DESTDIR}/.qrc_fm
    LIBS += -L$${top_builddir}/lib$${LIBSUFFIX} -lQtFM

    target.path = $${PREFIX}/bin
    desktop.files += $${TARGET}.desktop
    desktop.path += $${PREFIX}/share/applications
    man.files += qtfm.1
    man.path += $${MANDIR}/man1
    INSTALLS += target desktop man

    hicolor.files = $${top_srcdir}/share/hicolor
    hicolor.path = $${PREFIX}/share/icons
    INSTALLS += hicolor

    qtfm_mimes.files = $${top_srcdir}/share/icons/mimes
    qtfm_mimes.path = $${PREFIX}/share/qtfm
    INSTALLS += qtfm_mimes

    CONFIG(no_dbus) {
        DEFINES += NO_DBUS
        DEFINES += NO_UDISKS
    }
    !CONFIG(no_dbus) : QT += dbus
    !CONFIG(staticlib): QMAKE_RPATHDIR += $ORIGIN/../lib$${LIBSUFFIX}
}

CONFIG(with_magick): include($${top_srcdir}/share/imagemagick.pri)
# Video/audio thumbnails now shell out to the `ffmpeg` binary at runtime
# (see Common::videoFirstFrameImage), so libav*/CONFIG(with_ffmpeg) is no
# longer needed here.
