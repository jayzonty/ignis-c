cmake_minimum_required(VERSION 3.10)

IF (WIN32)
    set(ENV{VULKAN_SDK} "C:/VulkanSDK/1.4.350.0")
ENDIF()

find_package(Vulkan REQUIRED)

set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(deps/glfw-3.3.8/)

set (SOURCE_FILES
    dynamic_array.c

    ignis_bezier.c
    ignis_graphics.c
    ignis_math.c
    ignis_vulkan.c
    ignis.c
)

add_library(ignis ${SOURCE_FILES})

target_include_directories(ignis PUBLIC
    ${Vulkan_INCLUDE_DIR}
    deps/glfw-3.3.8/include/
    ${PROJECT_SOURCE_DIR}/include/
)

IF (WIN32)
    set(LINK_LIBRARIES
        ${Vulkan_LIBRARY}
        glfw
        ${CMAKE_DL_LIBS} 
        winmm
    )
ELSE ()
    set(LINK_LIBRARIES
        ${Vulkan_LIBRARY}
        glfw
        ${CMAKE_DL_LIBS} 
        m
    )
ENDIF()

target_link_libraries(ignis PRIVATE ${LINK_LIBRARIES})
