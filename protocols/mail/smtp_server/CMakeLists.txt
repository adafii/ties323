cmake_minimum_required(VERSION 3.18)
project("SMTP server" CXX)

find_package(Threads REQUIRED)

set(main_target app)
set(source_dir src)
set(src ${source_dir}/main.cpp
        ${source_dir}/smtp.cpp
        ${source_dir}/pop3.cpp)

add_library(asio INTERFACE /usr/include/asio.hpp)
target_include_directories(asio SYSTEM INTERFACE /usr/include/asio)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_link_libraries(asio INTERFACE Threads::Threads)

add_executable(${main_target} ${src})
target_include_directories(${main_target} PRIVATE ${source_dir})
target_link_libraries(${main_target} PRIVATE asio)

target_compile_features(${main_target} PRIVATE cxx_std_20)
target_compile_options(${main_target} PRIVATE $<$<CONFIG:Debug>:-Wall -Wextra -Wpedantic -fsanitize=address -fsanitize=undefined>)
target_compile_options(${main_target} PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:-Wall -Wextra -Wpedantic -flto>)
target_link_options(${main_target} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=undefined>)
target_link_options(${main_target} PRIVATE $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:-flto>)
