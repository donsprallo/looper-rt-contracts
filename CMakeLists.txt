# The contracts between the looper runtime and the models it runs -- chip
# models and time models: headers only.
#
#   include(FetchContent)
#   FetchContent_Declare(looper_rt_contracts
#       GIT_REPOSITORY https://github.com/donsprallo/looper-rt-contracts.git
#       GIT_TAG        v0.5.0)
#   FetchContent_MakeAvailable(looper_rt_contracts)
#
#   target_link_libraries(my_chip PUBLIC looper::device)
#
# While the major version is 0 the contracts may still change with any minor
# version. From 1.0 on the major version is LOOPER_CONTRACT_ABI; a check in
# tests/ holds the two together.
cmake_minimum_required(VERSION 3.20)
project(looper_rt_contracts VERSION 0.5.0 LANGUAGES C)

# Guarded, because the runtime and a component may both bring the contract
# into one build; the first one defines the target, the second uses it.
if (NOT TARGET looperDevice)
    add_library(looperDevice INTERFACE)
    target_include_directories(looperDevice INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_library(looper::device ALIAS looperDevice)
endif ()

# The checks in tests/, on when this is the project being built and off when
# another project brings the contract in.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(looper_rt_contracts_is_top_level ON)
else ()
    set(looper_rt_contracts_is_top_level OFF)
endif ()
option(LOOPER_RT_CONTRACTS_TESTS "Build the checks in tests/" ${looper_rt_contracts_is_top_level})
if (LOOPER_RT_CONTRACTS_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
