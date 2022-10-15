cmake_minimum_required(VERSION 3.10)

find_package(Freetype REQUIRED)
find_package(Vulkan REQUIRED)

set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(deps/glfw-3.3.8/)

set (SOURCE_FILES
    dynamic_array.c

    ignis.c
    ignis_graphics.c
    ignis_math.c
    ignis_vulkan.c
)

add_library(ignis ${SOURCE_FILES})

target_include_directories(ignis PUBLIC
    ${FREETYPE_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIR}
    deps/glfw-3.3.8/include/
    ${PROJECT_SOURCE_DIR}/include/
)

target_link_libraries(ignis PRIVATE
    ${Vulkan_LIBRARY}
    freetype
    glfw
    ${CMAKE_DL_LIBS} 
    m
)
