cmake_minimum_required(VERSION 3.16)
project(X19Core LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

find_package(cppzmq CONFIG REQUIRED)
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()
find_package(Threads REQUIRED)

# Generate bindings in the build directory with the installed protoc.
# Add new schema names here; do not compile checked-in generated bindings.
set(protocol_names telemetry thruster)
set(protocol_sources)
foreach(name IN LISTS protocol_names)
    set(schema "${CMAKE_CURRENT_SOURCE_DIR}/proto/${name}.proto")
    set(source "${CMAKE_CURRENT_BINARY_DIR}/${name}.pb.cc")
    set(header "${CMAKE_CURRENT_BINARY_DIR}/${name}.pb.h")
    add_custom_command(
        OUTPUT "${source}" "${header}"
        COMMAND protobuf::protoc
            "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto"
            "--cpp_out=${CMAKE_CURRENT_BINARY_DIR}"
            "${schema}"
        DEPENDS "${schema}" protobuf::protoc
        VERBATIM
    )
    list(APPEND protocol_sources "${source}" "${header}")
endforeach()
add_library(protocols_lib STATIC ${protocol_sources})
target_include_directories(protocols_lib PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(protocols_lib PUBLIC protobuf::libprotobuf)
target_compile_features(protocols_lib PUBLIC cxx_std_17)

# The messaging wrappers are header-only: no recursive source globbing.
add_library(messaging_lib INTERFACE)
target_include_directories(messaging_lib INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/cpp")
target_link_libraries(messaging_lib INTERFACE cppzmq protocols_lib Threads::Threads)
target_compile_features(messaging_lib INTERFACE cxx_std_17)

# These are manually paired network examples, not terminating CTest tests.
option(X19_BUILD_MESSAGING_EXAMPLES "Build hello_pub and hello_sub" ON)
if(X19_BUILD_MESSAGING_EXAMPLES)
    foreach(example IN ITEMS hello_pub hello_sub)
        add_executable(${example} "testing/cpp/${example}.cpp")
        target_link_libraries(${example} PRIVATE messaging_lib)
    endforeach()
endif()
