cmake_minimum_required(VERSION 3.16)

# Supports both a standalone build and add_subdirectory() from the repo root.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(X19Thruster LANGUAGES CXX)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    include(CTest)
endif()

get_filename_component(X19_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

find_package(Eigen3 REQUIRED NO_MODULE)

add_library(thrust_mapping STATIC thrust.mapping.cpp thrust_mapping.hpp)
target_compile_features(thrust_mapping PUBLIC cxx_std_17)
target_include_directories(thrust_mapping PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(thrust_mapping PUBLIC Eigen3::Eigen)

add_library(thrust_control_core STATIC thrust.control.cpp thrust_control.hpp)
target_link_libraries(thrust_control_core PUBLIC thrust_mapping)

# This interactive executable has no ZMQ or Protobuf dependency.
add_executable(thrust_test "${X19_REPO_ROOT}/testing/cpp/thrust.test.cpp")
target_link_libraries(thrust_test PRIVATE thrust_mapping)

option(X19_BUILD_THRUST_CONTROLLER "Build the ZMQ thruster controller" ON)
if(X19_BUILD_THRUST_CONTROLLER)
    if(NOT TARGET messaging_lib)
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
            set(schema "${X19_REPO_ROOT}/proto/${name}.proto")
            set(source "${CMAKE_CURRENT_BINARY_DIR}/${name}.pb.cc")
            set(header "${CMAKE_CURRENT_BINARY_DIR}/${name}.pb.h")
            add_custom_command(
                OUTPUT "${source}" "${header}"
                COMMAND protobuf::protoc
                    "--proto_path=${X19_REPO_ROOT}/proto"
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
        target_include_directories(messaging_lib INTERFACE "${X19_REPO_ROOT}/src/cpp")
        target_link_libraries(messaging_lib INTERFACE cppzmq protocols_lib Threads::Threads)
        target_compile_features(messaging_lib INTERFACE cxx_std_17)
    endif()
    add_executable(thrust_control thrust.control.cpp)
    target_compile_definitions(thrust_control PRIVATE X19_THRUST_CONTROL_WITH_MESSAGING)
    target_link_libraries(thrust_control PRIVATE thrust_control_core messaging_lib)
endif()

if(BUILD_TESTING)
    add_executable(thrust_unit_test "${X19_REPO_ROOT}/testing/cpp/thrust.unit.test.cpp")
    target_link_libraries(thrust_unit_test PRIVATE thrust_control_core)
    add_test(NAME thrust_unit COMMAND thrust_unit_test)
    if(X19_BUILD_THRUST_CONTROLLER)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            execute_process(
                COMMAND "${Python3_EXECUTABLE}" -c "import zmq; import google.protobuf"
                RESULT_VARIABLE X19_PYTHON_MESSAGING_STATUS
                OUTPUT_QUIET ERROR_QUIET
            )
            if(X19_PYTHON_MESSAGING_STATUS EQUAL 0)
                add_test(NAME thrust_zmq_integration
                    COMMAND "${Python3_EXECUTABLE}" "${X19_REPO_ROOT}/testing/thrust.integration.test.py"
                        "$<TARGET_FILE:thrust_control>" "$<TARGET_FILE:protobuf::protoc>"
                        "${X19_REPO_ROOT}/proto")
                set_tests_properties(thrust_zmq_integration PROPERTIES TIMEOUT 25)
            else()
                message(STATUS "ZMQ integration test requires Python pyzmq and protobuf")
            endif()
        endif()
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    foreach(target IN ITEMS thrust_mapping thrust_control_core thrust_test)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
    if(TARGET thrust_control)
        target_compile_options(thrust_control PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()
