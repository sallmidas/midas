add_executable(midas_sandbox
    main.cpp
)

target_compile_features(midas_sandbox PRIVATE cxx_std_20)
target_link_libraries(midas_sandbox PRIVATE midas::midas)

if(MSVC)
    target_compile_options(midas_sandbox PRIVATE /W4 /permissive-)
else()
    target_compile_options(midas_sandbox PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

set_target_properties(midas_sandbox PROPERTIES
    OUTPUT_NAME midas_sandbox
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set(MIDAS_SANDBOX_BMP "${CMAKE_CURRENT_SOURCE_DIR}/assets/midas_sprite.bmp")
if(NOT EXISTS "${MIDAS_SANDBOX_BMP}")
    message(FATAL_ERROR "Midas sandbox is missing ${MIDAS_SANDBOX_BMP}")
endif()

# File-level dependency so editing the BMP recopies it even if the binary
# did not relink. POST_BUILD below still copies next to the real executable
# (multi-config / TARGET_FILE_DIR safety).
set(MIDAS_SANDBOX_BMP_DST "${CMAKE_BINARY_DIR}/bin/assets/midas_sprite.bmp")
add_custom_command(
    OUTPUT "${MIDAS_SANDBOX_BMP_DST}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/bin/assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${MIDAS_SANDBOX_BMP}"
        "${MIDAS_SANDBOX_BMP_DST}"
    DEPENDS "${MIDAS_SANDBOX_BMP}"
    COMMENT "Copy midas_sprite.bmp into the build bin/assets directory"
    VERBATIM
)
add_custom_target(midas_sandbox_assets ALL DEPENDS "${MIDAS_SANDBOX_BMP_DST}")
add_dependencies(midas_sandbox midas_sandbox_assets)

add_custom_command(TARGET midas_sandbox POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:midas_sandbox>/assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${MIDAS_SANDBOX_BMP}"
        "$<TARGET_FILE_DIR:midas_sandbox>/assets/midas_sprite.bmp"
    COMMENT "Copy midas_sprite.bmp next to midas_sandbox"
    VERBATIM
)

include(GNUInstallDirs)
install(TARGETS midas_sandbox RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${MIDAS_SANDBOX_BMP}" DESTINATION "${CMAKE_INSTALL_BINDIR}/assets")

# Shared SDL3 next to the binary / Homebrew prefixes when running from the build tree.
if(APPLE)
    set_target_properties(midas_sandbox PROPERTIES
        BUILD_RPATH "@executable_path;/opt/homebrew/lib;/usr/local/lib"
        INSTALL_RPATH "@executable_path;/opt/homebrew/lib;/usr/local/lib"
    )
elseif(UNIX)
    set_target_properties(midas_sandbox PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

if(BUILD_TESTING)
    add_test(
        NAME sandbox_smoke
        COMMAND "$<TARGET_FILE:midas_sandbox>" --smoke
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:midas_sandbox>"
    )
    set_tests_properties(sandbox_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    )
endif()
