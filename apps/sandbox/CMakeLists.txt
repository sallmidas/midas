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

add_custom_command(TARGET midas_sandbox POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:midas_sandbox>/assets"
    COMMENT "Copy sandbox assets next to midas_sandbox"
)

# Shared SDL3 next to the binary / Homebrew prefixes when running from the build tree.
if(APPLE)
    set_target_properties(midas_sandbox PROPERTIES
        BUILD_RPATH "@executable_path;/opt/homebrew/lib;/usr/local/lib"
    )
elseif(UNIX)
    set_target_properties(midas_sandbox PROPERTIES
        BUILD_RPATH "$ORIGIN"
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
