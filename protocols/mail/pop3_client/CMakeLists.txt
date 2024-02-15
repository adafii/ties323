cmake_minimum_required(VERSION 3.18)
project("pop3 client" CXX)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(OpenGL 2.0 REQUIRED)

set(imgui_dir imgui)
set(imgui_backends_dir ${imgui_dir}/backends)

set(imgui_h_files
        ${imgui_dir}/imgui.h
        ${imgui_dir}/imconfig.h
        ${imgui_dir}/imgui_internal.h
        ${imgui_dir}/imstb_rectpack.h
        ${imgui_dir}/imstb_textedit.h
        ${imgui_dir}/imstb_truetype.h
)

set(imgui_cxx_files
        ${imgui_dir}/imgui.cpp
        ${imgui_dir}/imgui_draw.cpp
        ${imgui_dir}/imgui_widgets.cpp
        ${imgui_dir}/imgui_demo.cpp
        ${imgui_dir}/imgui_tables.cpp
)

set(imgui_backend_files
        ${imgui_backends_dir}/imgui_impl_glfw.h
        ${imgui_backends_dir}/imgui_impl_glfw.cpp
        ${imgui_backends_dir}/imgui_impl_opengl2.h
        ${imgui_backends_dir}/imgui_impl_opengl2.cpp
)

file(GLOB imgui_fonts ${imgui_dir}/misc/fonts/*.ttf)

set(imgui_files
        ${imgui_h_files}
        ${imgui_cxx_files}
        ${imgui_backend_files}
        ${imgui_fonts}
)

add_library(imgui SHARED ${imgui_files})
target_include_directories(imgui PUBLIC ${imgui_dir} ${imgui_backends_dir})
target_link_libraries(imgui INTERFACE OpenGL::GL)

add_library(asio INTERFACE /usr/include/asio.hpp)
target_include_directories(asio SYSTEM INTERFACE /usr/include/asio)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_link_libraries(asio INTERFACE Threads::Threads OpenSSL::SSL)

set(main_target pop3c)
set(source_dir src)
set(src ${source_dir}/main.cpp
        ${source_dir}/client.cpp
        ${source_dir}/gui.cpp)

add_executable(${main_target} ${src})
target_include_directories(${main_target} PRIVATE ${source_dir})
target_link_libraries(${main_target} PRIVATE asio glfw imgui)

target_compile_features(${main_target} PRIVATE cxx_std_20)
target_compile_options(${main_target} PRIVATE $<$<CONFIG:Debug>:-Wall -Wextra -Wpedantic -fsanitize=address -fsanitize=undefined>)
target_compile_options(${main_target} PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:-Wall -Wextra -Wpedantic -flto>)
target_link_options(${main_target} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=undefined>)
target_link_options(${main_target} PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:-flto>)
