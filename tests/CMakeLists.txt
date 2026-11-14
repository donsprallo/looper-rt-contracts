# Checks of the contract itself: that it compiles as C11 and as C++17 with
# every warning an error, that a model can be written against it, that its
# version and the project's major version agree, and that it defines nothing
# a linker would see.
enable_language(CXX)

function(looper_contract_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else ()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif ()
endfunction()

add_executable(test_contract_c test_contract.c)
set_target_properties(test_contract_c PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(test_contract_c PRIVATE looper::device)
target_compile_definitions(test_contract_c PRIVATE
    LOOPER_CONTRACTS_MAJOR=${PROJECT_VERSION_MAJOR})
looper_contract_warnings(test_contract_c)
add_test(NAME contract_c COMMAND test_contract_c)

add_executable(test_contract_cpp test_contract.cpp)
set_target_properties(test_contract_cpp PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(test_contract_cpp PRIVATE looper::device)
looper_contract_warnings(test_contract_cpp)
add_test(NAME contract_cpp COMMAND test_contract_cpp)

# A unit that includes the headers and nothing else must define no symbol.
add_library(contract_only_header OBJECT only_header.c)
target_link_libraries(contract_only_header PRIVATE looper::device)
looper_contract_warnings(contract_only_header)
if (CMAKE_NM AND NOT MSVC)
    add_test(NAME contract_defines_nothing
        COMMAND ${CMAKE_COMMAND}
            -DNM=${CMAKE_NM}
            -DOBJECTS=$<TARGET_OBJECTS:contract_only_header>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/defines_nothing.cmake)
endif ()
