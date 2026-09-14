add_executable(midas_room
    main.cpp
)

target_compile_features(midas_room PRIVATE cxx_std_20)
target_link_libraries(midas_room PRIVATE midas::midas)

if(MSVC)
    target_compile_options(midas_room PRIVATE /W4 /permissive-)
else()
    target_compile_options(midas_room PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

set_target_properties(midas_room PROPERTIES
    OUTPUT_NAME midas_room
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(GNUInstallDirs)
install(TARGETS midas_room RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(APPLE)
    set(_midas_rpath "@executable_path;/opt/homebrew/lib;/usr/local/lib")
    if(MIDAS_SDL3_HOMEBREW_LIBDIR)
        set(_midas_rpath "@executable_path;${MIDAS_SDL3_HOMEBREW_LIBDIR};/opt/homebrew/lib;/usr/local/lib")
    endif()
    set_target_properties(midas_room PROPERTIES
        BUILD_RPATH "${_midas_rpath}"
        INSTALL_RPATH "${_midas_rpath}"
    )
elseif(UNIX)
    set(_midas_rpath "$ORIGIN")
    if(MIDAS_SDL3_HOMEBREW_LIBDIR)
        set(_midas_rpath "$ORIGIN;${MIDAS_SDL3_HOMEBREW_LIBDIR}")
    endif()
    set_target_properties(midas_room PROPERTIES
        BUILD_RPATH "${_midas_rpath}"
        INSTALL_RPATH "${_midas_rpath}"
    )
endif()

if(BUILD_TESTING)
    add_test(
        NAME room_smoke
        COMMAND "$<TARGET_FILE:midas_room>" --smoke
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:midas_room>"
    )
    set_tests_properties(room_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    )
endif()
