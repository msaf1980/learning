add_custom_target(
	cppcheck
	COMMAND cppcheck
	--enable=all
	-i CMakeFiles
	--template="{file}:{line}: [{severity}][{id}] {message}"
	--project=${CMAKE_BINARY_DIR}/compile_commands.json
	#--verbose
	--quiet
	${CMAKE_SOURCE_DIR}
)
