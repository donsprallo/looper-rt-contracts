# Fails when an object compiled from the header alone defines an external
# symbol. The contract is types only; a function or an object here would
# tie every component to whoever provides it.
execute_process(
    COMMAND ${NM} -g --defined-only ${OBJECTS}
    OUTPUT_VARIABLE defined
    RESULT_VARIABLE status
)
if (NOT status EQUAL 0)
    message(FATAL_ERROR "${NM} could not read ${OBJECTS}")
endif ()
string(STRIP "${defined}" defined)
if (NOT defined STREQUAL "")
    message(FATAL_ERROR "the contract defines symbols:\n${defined}")
endif ()
