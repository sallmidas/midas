add_library(midas
    src/Engine.cpp
    src/Window.cpp
    src/Input.cpp
    src/Time.cpp
    src/Renderer.cpp
    src/Texture.cpp
)

add_library(midas::midas ALIAS midas)

target_include_directories(midas
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(midas PUBLIC cxx_std_20)

target_link_libraries(midas PUBLIC SDL3::SDL3)

if(MSVC)
    target_compile_options(midas PRIVATE /W4 /permissive-)
else()
    target_compile_options(midas PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
