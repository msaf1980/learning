if(NOT TARGET benchmarks)
  add_custom_target(benchmarks)
endif()

function(add_bench targetname commandname)
	add_custom_target(${targetname}-run COMMAND ${commandname}
		COMMENT "Running benchmark ${targetname}" )
	add_dependencies(${targetname}-run ${targetname})
	add_dependencies(benchmarks ${targetname}-run)
endfunction()
