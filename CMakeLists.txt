cmake_minimum_required(VERSION 3.20)
project(DaisySphere)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add libDaisy first (as it's a dependency)
add_subdirectory(libDaisy)

# Add DaisySP as submodule
add_subdirectory(lib/DaisySP)

# Main executable
add_executable(daisysphere main.cpp)

# Link both libraries
target_link_libraries(daisysphere PRIVATE daisy daisysp)

# Include directories
target_include_directories(daisysphere PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/libDaisy/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/DaisySP/Source
)
