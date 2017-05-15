cmake_minimum_required (VERSION 2.6)
project (konferenzen.eu-vncserver)

# Setup
#set( CMAKE_VERBOSE_MAKEFILE on )
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++0x -mwindows -static-libgcc -static-libstdc++")
SET(CMAKE_EXE_LINKER_FLAGS "-static")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
add_executable(konferenzen.eu-vncserver WIN32 "main.cpp" "PresentationServer.cpp" "ScreenCapture.cpp" "resources.rc")
set_target_properties(konferenzen.eu-vncserver PROPERTIES LINK_SEARCH_END_STATIC 1)
# Erstellen der Ressourcenfiles aktivieren
set(CMAKE_RC_COMPILER_INIT windres)
ENABLE_LANGUAGE(RC)
SET(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff <DEFINES> -i <SOURCE> -o <OBJECT> ${CMAKE_RC_FLAGS}")

# Bibliotheken sammeln:
## Windows System Libraries
target_link_libraries(konferenzen.eu-vncserver PRIVATE gdi32 gdiplus shlwapi)

## LibVNC
find_package(LibVNC REQUIRED)
target_include_directories(konferenzen.eu-vncserver PRIVATE ${LibVNC_INCLUDE_DIRS})
target_link_libraries(konferenzen.eu-vncserver PRIVATE ${LibVNC_LIBRARIES})
target_compile_definitions(konferenzen.eu-vncserver PRIVATE ${LibVNC_DEFINITIONS})

## zlib
find_package(ZLIB)
target_include_directories(konferenzen.eu-vncserver PRIVATE ${ZLIB_INCLUDE_DIR})
target_link_libraries(konferenzen.eu-vncserver PRIVATE ${ZLIB_LIBRARIES})

## jpeg
find_package(JPEG)
target_include_directories(konferenzen.eu-vncserver PRIVATE ${JPEG_INCLUDE_DIR})
target_link_libraries(konferenzen.eu-vncserver PRIVATE ${JPEG_LIBRARIES})

## png
find_package(PNG)
target_include_directories(konferenzen.eu-vncserver PRIVATE ${PNG_INCLUDE_DIR})
target_link_libraries(konferenzen.eu-vncserver PRIVATE ${PNG_LIBRARIES})

## Intl
find_package(Intl)
if (Intl_FOUND)
  target_link_libraries(konferenzen.eu-vncserver PRIVATE ${Intl_LIBRARIES})
endif()

## Idn
find_library(IDN_LIBRARY idn)
if (IDN_FOUND)
  target_link_libraries(konferenzen.eu-vncserver PRIVATE ${IDN_LIBRARY})
endif()

## gmp, nettle, gnutls
find_package(GnuTLS)
target_include_directories(konferenzen.eu-vncserver PRIVATE ${GNUTLS_INCLUDE_DIR})
target_link_libraries(konferenzen.eu-vncserver PRIVATE ${GNUTLS_LIBRARIES})

find_library(LIBHOGWEED_LIBRARY libhogweed)
target_link_libraries(konferenzen.eu-vncserver PRIVATE ${LIBHOGWEED_LIBRARY})

find_library(NETTLE_LIBRARY nettle)
target_link_libraries(konferenzen.eu-vncserver PRIVATE ${NETTLE_LIBRARY})

find_library(GMP_LIBRARY gmp)
target_link_libraries(konferenzen.eu-vncserver PRIVATE ${GMP_LIBRARY})

## Iconv
find_library(ICONV_LIBRARY NAMES libiconv.a iconv.a )
if (ICONV_FOUND)
  target_link_libraries(konferenzen.eu-vncserver PRIVATE ${ICONV_LIBRARY})
endif()

## ws2 crypt32
target_link_libraries(konferenzen.eu-vncserver PRIVATE crypt32 ws2_32)
