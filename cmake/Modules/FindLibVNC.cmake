# ich geh einfach davon aus das die Bibliothek wie in der Anleitung beschrieben, neben diesem gebaut wird und
# dann faken wir einfach das Suchen nach dem Modul

set(LibVNC_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../libvncserver" "${CMAKE_CURRENT_BINARY_DIR}/../libvncserver")
set(LibVNC_DEFINITIONS LIBVNCSERVER_WITH_TLS=1 LIBVNCSERVER_HAVE_LIBZ=1 LIBVNCSERVER_HAVE_LIBPNG=1 LIBVNCSERVER_HAVE_LIBJPEG=1)
find_library(LibVNC_LIBRARIES NAMES libvncserver PATHS ${CMAKE_CURRENT_BINARY_DIR}/../libvncserver)
