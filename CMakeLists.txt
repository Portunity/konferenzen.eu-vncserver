cmake_minimum_required (VERSION 2.6)
project (presentation_client)

# Magie um Ressourcen in die fertige EXE zu packen
set(RES_FILES "resources.rc")
set(CMAKE_RC_COMPILER_INIT windres)
ENABLE_LANGUAGE(RC)
SET(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff <DEFINES> -i <SOURCE> -o <OBJECT> ${CMAKE_RC_FLAGS}")

include_directories(${LIBVNCSERVER_INCLUDE_DIR} ${CMAKE_BINARY_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -mwindows -static-libgcc -static-libstdc++")

add_executable(presentation_client WIN32 main.cpp PresentationServer.cpp ScreenCapture.cpp ${RES_FILES})
find_library (SHLWAPI_LIBRARY libshlwapi.a)
find_library (WS2_32_LIBRARY libws2_32.a)
find_library (GDIPLUS_LIBRARY libgdiplus.a)
set(WIN_LIBS ${SHLWAPI_LIBRARY} ${WS2_32_LIBRARY} ${GDIPLUS_LIBRARY})

#find_library (GNUTLS_LIBRARY libgnutls.a)
#find_library (GNUTLSXX_LIBRARY libgnutlsxx.a)
#if (GNUTLS_LIBRARY AND GNUTLSXX_LIBRARY)
#	add_definitions(-DHAS_GNUTLS)
#endif()

target_link_libraries (presentation_client ${LIBVNCSERVER_LIBRARY} ${GNUTLSXX_LIBRARY} ${GNUTLS_LIBRARY} ${ADDITIONAL_LIBS} ${ZLIB_LIBRARY} ${JPEG_LIBRARY} ${PNG_LIBRARY} ${WIN_LIBS})
#find_library(GNUTLS_LIBRARY NAMES libgnutls.a PATHS ${CMAKE_SOURCE_DIR}/../gnutls_bin/lib)
#find_library(GNUTLSXX_LIBRARY NAMES libgnutlsxx.a PATHS ${CMAKE_SOURCE_DIR}/../gnutls_bin/lib)
#message(${GNUTLS_LIBRARY})

