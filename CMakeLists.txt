cmake_minimum_required(VERSION 3.16)

project(centaur VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(centaur_core
    src/analog_rules.cpp
    src/egraph.cpp
    src/expr.cpp
    src/mna.cpp
    src/netlist.cpp
    src/parser.cpp
    src/rewrite.cpp
    src/topology.cpp
)

target_include_directories(centaur_core PUBLIC include)

add_executable(centaur src/main.cpp)
target_link_libraries(centaur PRIVATE centaur_core)

enable_testing()

add_executable(centaur_tests tests/test_optimizer.cpp)
target_link_libraries(centaur_tests PRIVATE centaur_core)
add_test(NAME centaur_tests COMMAND centaur_tests)

add_test(
    NAME exercise_3_20_queries
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_20.cir
            --current R10 --current R25 --voltage top n15
)
set_tests_properties(
    exercise_3_20_queries
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R10: 9[\r\n]+I R25: 1\\.5[\r\n]+V top n15: 67\\.5"
)

add_test(
    NAME exercise_3_21_voltage_division
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_21.cir
            --current R6 --voltage n5 n6
)
set_tests_properties(
    exercise_3_21_voltage_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R6: 3[\r\n]+V n5 n6: 18"
)

add_test(
    NAME exercise_3_22_voltage_division
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_22.cir
            --voltage r6bot v8right --voltage 0 v8left
)
set_tests_properties(
    exercise_3_22_voltage_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V r6bot v8right: 2\\.4[\r\n]+V 0 v8left: -3\\.3"
)

add_test(
    NAME exercise_3_23_open_circuit_voltage
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_23.cir
            --voltage a b --current R10
)
set_tests_properties(
    exercise_3_23_open_circuit_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V a b: 80[\r\n]+I R10: 0"
)

add_test(
    NAME exercise_3_24_dependent_source
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_24.cir
            --current R4 --power Edep
)
set_tests_properties(
    exercise_3_24_dependent_source
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R4: -2[\r\n]+P Edep: -72"
)

add_test(
    NAME exercise_3_25_seen_resistance
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_24.cir
            --seen-resistance V24
)
set_tests_properties(
    exercise_3_25_seen_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rseen V24: -12"
)

add_test(
    NAME exercise_3_26_dependent_sources
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_26.cir
            --voltage v1 0 --current R16 --current Fdep
)
set_tests_properties(
    exercise_3_26_dependent_sources
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V v1 0: -6[\r\n]+I R16: 3[\r\n]+I Fdep: 1\\.5"
)

add_test(
    NAME exercise_3_27_open_branch_voltage
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_27.cir
            --current R8 --voltage a 0
)
set_tests_properties(
    exercise_3_27_open_branch_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R8: 5[\r\n]+V a 0: 20"
)

add_test(
    NAME exercise_3_28_open_circuit_voltage
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_28.cir
            --voltage a 0 --current R9 --current R13
)
set_tests_properties(
    exercise_3_28_open_circuit_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V a 0: -35[\r\n]+I R9: 0[\r\n]+I R13: 0"
)

add_test(
    NAME exercise_3_28_protected_parallel_resistor
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_28.cir
            --current R18
)
set_tests_properties(
    exercise_3_28_protected_parallel_resistor
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R18: 0\\.833333333333"
)
