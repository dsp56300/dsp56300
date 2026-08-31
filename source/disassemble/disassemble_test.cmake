if(NOT DEFINED DISASSEMBLER OR NOT DEFINED TEST_WORK_DIR)
	message(FATAL_ERROR "DISASSEMBLER and TEST_WORK_DIR are required")
endif()

file(MAKE_DIRECTORY "${TEST_WORK_DIR}")
set(input_file "${TEST_WORK_DIR}/zero_words.txt")
file(WRITE "${input_file}" "000000 000004\n")

execute_process(
	COMMAND "${DISASSEMBLER}" -in "${input_file}" -pc 1
	RESULT_VARIABLE default_result
	OUTPUT_VARIABLE default_output
	ERROR_VARIABLE default_error
)

if(NOT default_result EQUAL 0)
	message(FATAL_ERROR
		"Default disassembly failed (${default_result})\n${default_output}${default_error}")
endif()

if(default_output MATCHES "000001:.*nop")
	message(FATAL_ERROR "Default output retained the zero-word NOP:\n${default_output}")
endif()

if(NOT default_output MATCHES "000002:")
	message(FATAL_ERROR
		"Default output removed or failed to advance to the following word:\n${default_output}")
endif()

execute_process(
	COMMAND "${DISASSEMBLER}" -in "${input_file}" -pc 1 -nops
	RESULT_VARIABLE nops_result
	OUTPUT_VARIABLE nops_output
	ERROR_VARIABLE nops_error
)

if(NOT nops_result EQUAL 0)
	message(FATAL_ERROR
		"-nops disassembly failed (${nops_result})\n${nops_output}${nops_error}")
endif()

if(NOT nops_output MATCHES "000001:.*nop")
	message(FATAL_ERROR "-nops omitted the zero-word NOP:\n${nops_output}")
endif()

if(NOT nops_output MATCHES "000002:")
	message(FATAL_ERROR
		"-nops removed or failed to advance to the following word:\n${nops_output}")
endif()
