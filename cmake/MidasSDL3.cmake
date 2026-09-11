# Prefer an installed SDL3 (Homebrew on macOS / Linuxbrew, or a system package
# such as Debian/Ubuntu `libsdl3-dev`). If none is found, fetch SDL3 3.4.16 via
# CMake FetchContent.

set(MIDAS_SDL3_FETCH_VERSION "3.4.16")

if(APPLE)
    find_program(MIDAS_BREW brew)
    if(MIDAS_BREW)
        execute_process(
            COMMAND "${MIDAS_BREW}" --prefix sdl3
            OUTPUT_VARIABLE MIDAS_BREW_SDL3_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE MIDAS_BREW_SDL3_RESULT
        )
        if(MIDAS_BREW_SDL3_RESULT EQUAL 0 AND EXISTS "${MIDAS_BREW_SDL3_PREFIX}")
            list(PREPEND CMAKE_PREFIX_PATH "${MIDAS_BREW_SDL3_PREFIX}")
            message(STATUS "Midas: Homebrew sdl3 prefix is ${MIDAS_BREW_SDL3_PREFIX}")
        endif()
    endif()

    # Apple Silicon default prefix, then Intel Homebrew.
    if(EXISTS "/opt/homebrew")
        list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    endif()
    if(EXISTS "/usr/local")
        list(PREPEND CMAKE_PREFIX_PATH "/usr/local")
    endif()
endif()

find_package(SDL3 3 CONFIG QUIET)

if(SDL3_FOUND)
    message(STATUS "Midas: using installed SDL3 ${SDL3_VERSION}")
else()
    message(STATUS "Midas: SDL3 not found; fetching ${MIDAS_SDL3_FETCH_VERSION} via FetchContent")

    include(FetchContent)

    set(SDL_SHARED OFF CACHE BOOL "Build SDL3 as a shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL3 as a static library" FORCE)
    set(SDL_TEST OFF CACHE BOOL "Build SDL3 test library" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3_test library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Build SDL3 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL3 examples" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "Install SDL3" FORCE)

    # Linux CI / headless hosts often lack X11/Wayland dev packages.
    # Dummy video is enough for configure/build and SDL_VIDEODRIVER=dummy smokes.
    if(UNIX AND NOT APPLE)
        set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "Allow SDL3 without a display server" FORCE)
    endif()

    FetchContent_Declare(
        SDL3
        URL "https://github.com/libsdl-org/SDL/releases/download/release-${MIDAS_SDL3_FETCH_VERSION}/SDL3-${MIDAS_SDL3_FETCH_VERSION}.tar.gz"
        URL_HASH SHA512=74a5ee1e5bba138a8daa710e200ac3585c60f696311c46252d86bc685c55e8271cd0d68439f8e13ca515b702d46ef58914d0f7b2352019870a5763ed79739e9a
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

if(NOT TARGET SDL3::SDL3)
    if(TARGET SDL3::SDL3-static)
        add_library(SDL3::SDL3 ALIAS SDL3::SDL3-static)
    else()
        message(FATAL_ERROR "Midas: SDL3 was resolved but no SDL3::SDL3 target exists")
    endif()
endif()
