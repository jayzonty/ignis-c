add_executable(pong pong.c)
target_link_libraries(pong ignis)

add_executable(breakout breakout.c)
target_link_libraries(breakout ignis)

set(SRC_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(DST_RESOURCES_DIR ${CMAKE_CURRENT_BINARY_DIR}/resources)

add_custom_target(CopyResources ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${SRC_RESOURCES_DIR}" "${DST_RESOURCES_DIR}"
	COMMENT "Copying resources directory..."
)

add_dependencies(pong CopyResources)
add_dependencies(breakout CopyResources)