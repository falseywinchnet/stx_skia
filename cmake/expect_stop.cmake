# Runs a test program in one of its Release-check modes and passes only if a Release check
# stopped it. SkASSERT_RELEASE ends the process with a trap or abort, which execute_process
# reports as a description instead of an exit status; a normal exit, with any status, fails.
#
# Usage: cmake -DPROGRAM=<path> -DMODE=<--argument> -P expect_stop.cmake
execute_process(COMMAND "${PROGRAM}" "${MODE}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(result MATCHES "^-?[0-9]+$")
    message(FATAL_ERROR "${PROGRAM} ${MODE} exited with status ${result}; expected a Release-check stop")
endif()
message(STATUS "${PROGRAM} ${MODE} stopped: ${result}")
