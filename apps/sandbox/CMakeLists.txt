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

# Homebrew's libSDL3.dylib when running from the build tree (Apple Silicon / Intel).
if(APPLE)
    set_target_properties(midas_sandbox PROPERTIES
        BUILD_RPATH "@executable_path;/opt/homebrew/lib;/usr/local/lib"
    )
endif()
