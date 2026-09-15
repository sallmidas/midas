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

set(MIDAS_ROOM_BMP "${CMAKE_CURRENT_SOURCE_DIR}/assets/room_atlas.bmp")
if(NOT EXISTS "${MIDAS_ROOM_BMP}")
    message(FATAL_ERROR "Midas room is missing ${MIDAS_ROOM_BMP}")
endif()

# File-level dependency so editing the BMP recopies it even if the binary
# did not relink. POST_BUILD below still copies next to the real executable
# (multi-config / TARGET_FILE_DIR safety). Same pattern as the sandbox BMP.
set(MIDAS_ROOM_BMP_DST "${CMAKE_BINARY_DIR}/bin/assets/room_atlas.bmp")
add_custom_command(
    OUTPUT "${MIDAS_ROOM_BMP_DST}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/bin/assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${MIDAS_ROOM_BMP}"
        "${MIDAS_ROOM_BMP_DST}"
    DEPENDS "${MIDAS_ROOM_BMP}"
    COMMENT "Copy room_atlas.bmp into the build bin/assets directory"
    VERBATIM
)
add_custom_target(midas_room_assets ALL DEPENDS "${MIDAS_ROOM_BMP_DST}")
add_dependencies(midas_room midas_room_assets)

add_custom_command(TARGET midas_room POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:midas_room>/assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${MIDAS_ROOM_BMP}"
        "$<TARGET_FILE_DIR:midas_room>/assets/room_atlas.bmp"
    COMMENT "Copy room_atlas.bmp next to midas_room"
    VERBATIM
)

include(GNUInstallDirs)
install(TARGETS midas_room RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${MIDAS_ROOM_BMP}" DESTINATION "${CMAKE_INSTALL_BINDIR}/assets")

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
