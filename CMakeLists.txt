cmake_minimum_required(VERSION 3.10)
project(timemachine VERSION 0.3.4 LANGUAGES C)

include(CheckCSourceCompiles)
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)

# Required dependencies — IMPORTED_TARGET creates PkgConfig::* targets that
# carry include dirs, compile flags, library names AND library search paths.
pkg_check_modules(GTK     REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(JACK    REQUIRED IMPORTED_TARGET jack>=0.80.0)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile>=1.0.0)

find_library(MATH_LIB m REQUIRED)
find_package(Threads REQUIRED)

# Optional: liblo (OSC support)
pkg_check_modules(LIBLO IMPORTED_TARGET liblo>=0.24)

# Optional: readline (enables interactive console mode)
find_library(CURSES_LIB curses)
find_library(READLINE_LIB readline)

# Check for extended sndfile format support.
# SF_FORMAT_W64 and SF_FORMAT_FLAC are enum values, not macros, so
# check_symbol_exists cannot detect them — use check_c_source_compiles instead.
set(CMAKE_REQUIRED_INCLUDES ${SNDFILE_INCLUDE_DIRS})
check_c_source_compiles("#include <sndfile.h>
int main(void) { int x = SF_FORMAT_W64;  return 0; }" HAVE_W64)
check_c_source_compiles("#include <sndfile.h>
int main(void) { int x = SF_FORMAT_FLAC; return 0; }" HAVE_FLAC)
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
    src/nsm.c
)

add_executable(timemachine ${SOURCES})

target_include_directories(timemachine PRIVATE src)

target_compile_options(timemachine PRIVATE -g -Wall)

target_link_libraries(timemachine PRIVATE
    PkgConfig::GTK
    PkgConfig::JACK
    PkgConfig::SNDFILE
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
    target_link_libraries(timemachine PRIVATE PkgConfig::LIBLO)
endif()

# readline + curses (interactive console mode)
if(READLINE_LIB)
    target_compile_definitions(timemachine PRIVATE HAVE_LIBREADLINE)
    target_link_libraries(timemachine PRIVATE ${READLINE_LIB})
    if(CURSES_LIB)
        target_link_libraries(timemachine PRIVATE ${CURSES_LIB})
    endif()
endif()

# i18n (gettext)
find_program(MSGFMT_EXECUTABLE msgfmt)
find_library(INTL_LIB intl)

if(MSGFMT_EXECUTABLE)
    target_compile_definitions(timemachine PRIVATE
        GETTEXT_PACKAGE="${PROJECT_NAME}"
        LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
    )
    if(INTL_LIB)
        target_link_libraries(timemachine PRIVATE ${INTL_LIB})
    endif()

    set(MO_FILES "")
    foreach(lang en fr)
        set(po_file ${CMAKE_CURRENT_SOURCE_DIR}/po/${lang}.po)
        set(mo_dir  ${CMAKE_CURRENT_BINARY_DIR}/po/${lang}/LC_MESSAGES)
        set(mo_file ${mo_dir}/${PROJECT_NAME}.mo)
        add_custom_command(
            OUTPUT ${mo_file}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${mo_dir}
            COMMAND ${MSGFMT_EXECUTABLE} -o ${mo_file} ${po_file}
            DEPENDS ${po_file}
            COMMENT "Compiling ${lang} translation"
        )
        list(APPEND MO_FILES ${mo_file})
        install(FILES ${mo_file}
            DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES
        )
    endforeach()
    add_custom_target(translations ALL DEPENDS ${MO_FILES})
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
