cmake_minimum_required(VERSION 3.10)
project(timemachine VERSION 0.3.4 LANGUAGES C)

include(CheckSymbolExists)
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)

# Required dependencies
pkg_check_modules(GTK    REQUIRED gtk4)
pkg_check_modules(JACK   REQUIRED jack>=0.80.0)
pkg_check_modules(SNDFILE REQUIRED sndfile>=1.0.0)

find_library(MATH_LIB m REQUIRED)
find_package(Threads REQUIRED)

# Optional: liblo (OSC support)
pkg_check_modules(LIBLO liblo>=0.24)

# Optional: readline (enables interactive console mode)
find_library(CURSES_LIB curses)
find_library(READLINE_LIB readline)

# Check for extended sndfile format support
set(CMAKE_REQUIRED_INCLUDES ${SNDFILE_INCLUDE_DIRS})
check_symbol_exists(SF_FORMAT_W64  "sndfile.h" HAVE_W64)
check_symbol_exists(SF_FORMAT_FLAC "sndfile.h" HAVE_FLAC)
unset(CMAKE_REQUIRED_INCLUDES)

if(NOT HAVE_W64)
    message(WARNING "libsndfile does not support WAV64 — files > 2 GB will not be available")
endif()
if(NOT HAVE_FLAC)
    message(WARNING "libsndfile does not support FLAC")
endif()

set(SOURCES
    src/main.c
    src/support.c
    src/interface.c
    src/callbacks.c
    src/gtkmeter.c
    src/gtkmeterscale.c
    src/meters.c
    src/threads.c
)

add_executable(timemachine ${SOURCES})

target_include_directories(timemachine PRIVATE
    src
    ${GTK_INCLUDE_DIRS}
    ${JACK_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
)

target_compile_options(timemachine PRIVATE
    -g -Wall
    ${GTK_CFLAGS_OTHER}
    ${JACK_CFLAGS_OTHER}
    ${SNDFILE_CFLAGS_OTHER}
)

target_link_libraries(timemachine PRIVATE
    ${GTK_LIBRARIES}
    ${JACK_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    ${MATH_LIB}
    Threads::Threads
)

target_compile_definitions(timemachine PRIVATE
    PACKAGE="timemachine"
    PACKAGE_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}"
)

if(HAVE_W64)
    target_compile_definitions(timemachine PRIVATE HAVE_W64)
endif()

if(HAVE_FLAC)
    target_compile_definitions(timemachine PRIVATE HAVE_FLAC)
endif()

# liblo (OSC)
if(LIBLO_FOUND)
    target_compile_definitions(timemachine PRIVATE HAVE_LIBLO)
    target_include_directories(timemachine PRIVATE ${LIBLO_INCLUDE_DIRS})
    target_compile_options(timemachine PRIVATE ${LIBLO_CFLAGS_OTHER})
    target_link_libraries(timemachine PRIVATE ${LIBLO_LIBRARIES})
endif()

# readline + curses (interactive console mode)
if(READLINE_LIB)
    target_compile_definitions(timemachine PRIVATE HAVE_LIBREADLINE)
    target_link_libraries(timemachine PRIVATE ${READLINE_LIB})
    if(CURSES_LIB)
        target_link_libraries(timemachine PRIVATE ${CURSES_LIB})
    endif()
endif()

# Install
install(TARGETS timemachine DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES
    pixmaps/on.png
    pixmaps/off.png
    pixmaps/busy.png
    pixmaps/on-icon.png
    pixmaps/off-icon.png
    pixmaps/timemachine-icon.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/timemachine/pixmaps
)
