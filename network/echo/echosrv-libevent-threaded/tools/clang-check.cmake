add_custom_target(
	clang-tidy
	COMMAND /usr/bin/clang-tidy
	-p ${CMAKE_BINARY_DIR}
	#-checks='*,-clang-analyzer-alpha.*'
	${SOURCES}
)
