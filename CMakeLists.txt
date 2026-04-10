cmake_minimum_required(VERSION 3.28)

project(particle VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
    src/main.cpp
    src/platform/macos_app.cpp
    src/render/metal_renderer.cpp
    src/sim/particle_simulation.cpp
    PRIVATE FILE_SET CXX_MODULES FILES
    src/platform/macos_app.cppm
    src/render/metal_renderer.cppm
    src/sim/particle_simulation.cppm
)

find_package(TBB REQUIRED)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic

    # for #embed
    -Wno-c23-extensions
)

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/metal-cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/metal-cpp-extensions"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    TBB::tbb
    "-framework AppKit"
    "-framework Foundation"
    "-framework Metal"
    "-framework MetalKit"
    "-framework QuartzCore"
)
