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

if(NOT default_output MATCHES "000001:.*nop")
	message(FATAL_ERROR "Default output omitted the zero-word NOP:\n${default_output}")
endif()

execute_process(
	COMMAND "${DISASSEMBLER}" -in "${input_file}" -pc 1 -skipnops
	RESULT_VARIABLE skipped_result
	OUTPUT_VARIABLE skipped_output
	ERROR_VARIABLE skipped_error
)

if(NOT skipped_result EQUAL 0)
	message(FATAL_ERROR
		"-skipnops disassembly failed (${skipped_result})\n${skipped_output}${skipped_error}")
endif()

if(skipped_output MATCHES "000001:.*nop")
	message(FATAL_ERROR "-skipnops retained the zero-word NOP:\n${skipped_output}")
endif()

if(NOT skipped_output MATCHES "000002:")
	message(FATAL_ERROR
		"-skipnops removed or failed to advance to the following word:\n${skipped_output}")
endif()
