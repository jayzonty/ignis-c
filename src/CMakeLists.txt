cmake_minimum_required(VERSION 3.10)

IF (WIN32)
    set(ENV{VULKAN_SDK} "C:/VulkanSDK/1.3.236.0")
ENDIF()

find_package(Vulkan REQUIRED)

set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
add_subdirectory(deps/glfw-3.3.8/)

set (SOURCE_FILES
    Ignis.cpp
)

add_library(Ignis ${SOURCE_FILES} "Logging/Logging.cpp")

target_include_directories(Ignis PUBLIC
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

target_link_libraries(Ignis PRIVATE ${LINK_LIBRARIES})
