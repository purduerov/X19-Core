cmake_minimum_required(VERSION 3.16)

project(X19Thruster LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Eigen3 REQUIRED NO_MODULE)

# Mapping library.
# Add new mapping implementation files here.
add_library(thrust_mapping STATIC
    thrust.mapping.cpp
)

target_compile_features(thrust_mapping PUBLIC cxx_std_17)

set_target_properties(thrust_mapping PROPERTIES
    CXX_EXTENSIONS OFF
)

target_include_directories(thrust_mapping PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(thrust_mapping PUBLIC
    Eigen3::Eigen
)

# Interactive mapping test.
# Each program containing main() gets its own executable target.
add_executable(thrust_test
    thrust.test.cpp
)

target_link_libraries(thrust_test PRIVATE
    thrust_mapping
)

# Enable useful compiler warnings for these targets.
if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    target_compile_options(thrust_mapping PRIVATE
        -Wall -Wextra -Wpedantic
    )

    target_compile_options(thrust_test PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()