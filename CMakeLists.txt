cmake_minimum_required(VERSION 3.16)

project(centaur VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(centaur_core
    src/analog_rules.cpp
    src/constraint.cpp
    src/egraph.cpp
    src/expr.cpp
    src/mna.cpp
    src/netlist.cpp
    src/parser.cpp
    src/rewrite.cpp
    src/solve.cpp
    src/topology.cpp
)

target_include_directories(centaur_core PUBLIC include)

add_executable(centaur src/main.cpp)
target_link_libraries(centaur PRIVATE centaur_core)

enable_testing()

add_executable(centaur_tests tests/test_optimizer.cpp)
target_link_libraries(centaur_tests PRIVATE centaur_core)
add_test(NAME centaur_tests COMMAND centaur_tests)

add_executable(centaur_chapter4_tests tests/test_chapter4.cpp)
target_link_libraries(centaur_chapter4_tests PRIVATE centaur_core)

function(add_chapter4_test name)
    add_test(NAME ${name} COMMAND centaur_chapter4_tests ${name})
endfunction()

set(CHAPTER4_TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_chapter4.cpp)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS ${CHAPTER4_TEST_SOURCE}
)
file(
    STRINGS ${CHAPTER4_TEST_SOURCE}
    CHAPTER4_TEST_LINES
    REGEX "^[ \t]*\\{\"exercise_4_[^\"]+\""
)
if(NOT CHAPTER4_TEST_LINES)
    message(FATAL_ERROR "No Chapter 4 tests found in ${CHAPTER4_TEST_SOURCE}")
endif()
foreach(line IN LISTS CHAPTER4_TEST_LINES)
    string(REGEX REPLACE "^[ \t]*\\{\"([^\"]+)\".*" "\\1" chapter4_test "${line}")
    add_chapter4_test(${chapter4_test})
endforeach()

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
        "I R18: 5/6"
)

add_test(
    NAME exercise_3_29_kcl_currents
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_29.cir
            --current V_I1 --current V_I2 --current V_I3
)
set_tests_properties(
    exercise_3_29_kcl_currents
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I V_I1: 9[\r\n]+I V_I2: 18[\r\n]+I V_I3: 1"
)

add_test(
    NAME exercise_3_30_kcl_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_30.cir
            --current V_I
)
set_tests_properties(
    exercise_3_30_kcl_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I V_I: 38"
)

add_test(
    NAME exercise_3_31_short_circuit_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_31.cir
            --current R20 --current R25 --current V_I3
)
set_tests_properties(
    exercise_3_31_short_circuit_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R20: 5[\r\n]+I R25: -8[\r\n]+I V_I3: -3"
)

add_test(
    NAME exercise_3_32_dependent_current_source
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_32.cir
            --current R20 --current V_I2 --voltage right 0
)
set_tests_properties(
    exercise_3_32_dependent_current_source
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R20: 0\\.0018[\r\n]+I V_I2: -0\\.0002[\r\n]+V right 0: 10"
)

add_test(
    NAME exercise_3_33_total_conductance
    COMMAND centaur
            "(add (inv 1) (inv 0.5) (inv 0.25) (inv 0.125))"
)
set_tests_properties(
    exercise_3_33_total_conductance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "15"
)

add_test(
    NAME exercise_3_33_total_resistance
    COMMAND centaur
            "(inv (add (inv 1) (inv 0.5) (inv 0.25) (inv 0.125)))"
)
set_tests_properties(
    exercise_3_33_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "1/15"
)

add_test(
    NAME exercise_3_33_parallel_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_33.cir
            a b
)
set_tests_properties(
    exercise_3_33_parallel_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar1 a b \\(par 0\\.125 0\\.25 0\\.5 1\\)"
)

add_test(
    NAME exercise_3_34_parallel_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_34.cir
            a 0
)
set_tests_properties(
    exercise_3_34_parallel_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar1 a 0 \\(par 200 200 200"
)

add_test(
    NAME exercise_3_34_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_34.cir
            a 0
)
set_tests_properties(
    exercise_3_34_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 4"
)

add_test(
    NAME exercise_3_35_unknown_resistance_expr
    COMMAND centaur --solve-for
            R
            "(eq (par 10000 20000 R) 12000)"
)
set_tests_properties(
    exercise_3_35_unknown_resistance_expr
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: -15000"
)

add_test(
    NAME exercise_3_35_unknown_resistance_netlist
    COMMAND centaur --solve-rth-for
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_35.cir
            a 0 R "(eq Rth 12000)"
)
set_tests_properties(
    exercise_3_35_unknown_resistance_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: -15000"
)

add_test(
    NAME exercise_3_36_unknown_resistance_expr
    COMMAND centaur --solve-for
            R
            "(eq (add (inv 1) (inv 2) (inv R)) 1.75)"
)
set_tests_properties(
    exercise_3_36_unknown_resistance_expr
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 4"
)

add_test(
    NAME exercise_3_36_unknown_resistance_netlist
    COMMAND centaur --solve-rth-for
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_36.cir
            a 0 R "(eq (inv Rth) 1.75)"
)
set_tests_properties(
    exercise_3_36_unknown_resistance_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 4"
)

add_test(
    NAME exercise_3_37_parallel_product_over_sum
    COMMAND centaur
            "(div (mul 5 20) (add 5 20))"
)
set_tests_properties(
    exercise_3_37_parallel_product_over_sum
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4"
)

add_test(
    NAME exercise_3_37_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_37.cir
            a 0
)
set_tests_properties(
    exercise_3_37_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 4"
)

add_test(
    NAME exercise_3_38_parallel_pairwise
    COMMAND centaur
            "(div (mul (par 12 24) 32) (add (par 12 24) 32))"
)
set_tests_properties(
    exercise_3_38_parallel_pairwise
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "6\\.4"
)

add_test(
    NAME exercise_3_38_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_38.cir
            a 0
)
set_tests_properties(
    exercise_3_38_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 6\\.4"
)

add_test(
    NAME exercise_3_39_total_resistance_from_power
    COMMAND centaur
            "(par (div (mul 120 120) 60) (div (mul 120 120) 100) (div (mul 120 120) 200))"
)
set_tests_properties(
    exercise_3_39_total_resistance_from_power
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "40"
)

add_test(
    NAME exercise_3_39_total_power_check
    COMMAND centaur
            "(div (mul 120 120) (add 60 100 200))"
)
set_tests_properties(
    exercise_3_39_total_power_check
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "40"
)

add_test(
    NAME exercise_3_39_total_resistance_netlist
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_39.cir
            a 0
)
set_tests_properties(
    exercise_3_39_total_resistance_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 40"
)

add_test(
    NAME exercise_3_40_nested_parallel_expression
    COMMAND centaur
            "(par (add 4 (par 24 12)) 6)"
)
set_tests_properties(
    exercise_3_40_nested_parallel_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4"
)

add_test(
    NAME exercise_3_40_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_40.cir
            a 0
)
set_tests_properties(
    exercise_3_40_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 4"
)

add_test(
    NAME exercise_3_41_ladder_expression
    COMMAND centaur
            "(add 16 14 (par 5 (add 9 3 (par 24 (add 4 8)))))"
)
set_tests_properties(
    exercise_3_41_ladder_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "34"
)

add_test(
    NAME exercise_3_41_ladder_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_41.cir
            a 0
)
set_tests_properties(
    exercise_3_41_ladder_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rser5 a 0 \\(add 14 16 \\(par 5 \\(add 3 9 \\(par 24 \\(add 4 8\\)\\)\\)\\)\\)"
)

add_test(
    NAME exercise_3_41_ladder_topology_trace
    COMMAND centaur --explain --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_41.cir
            a 0
)
set_tests_properties(
    exercise_3_41_ladder_topology_trace
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "topology rewrite trace:[\r\n]+  1\\. series-resistors:"
)

add_test(
    NAME exercise_3_41_ladder_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_41.cir
            a 0
)
set_tests_properties(
    exercise_3_41_ladder_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 34"
)

add_test(
    NAME exercise_3_42a_open_expression
    COMMAND centaur
            "(par (add 40 90) (add 60 10))"
)
set_tests_properties(
    exercise_3_42a_open_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "45\\.5"
)

add_test(
    NAME exercise_3_42a_open_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_42a.cir
            in 0
)
set_tests_properties(
    exercise_3_42a_open_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar1 in 0 \\(par \\(add 10 60\\) \\(add 40 90\\)\\)"
)

add_test(
    NAME exercise_3_42a_open_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_42a.cir
            in 0
)
set_tests_properties(
    exercise_3_42a_open_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth in 0: 0[\r\n]+Rth in 0: 45\\.5"
)

add_test(
    NAME exercise_3_42b_short_expression
    COMMAND centaur
            "(add (par 40 60) (par 90 10))"
)
set_tests_properties(
    exercise_3_42b_short_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "33"
)

add_test(
    NAME exercise_3_42b_short_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_42b.cir
            in 0
)
set_tests_properties(
    exercise_3_42b_short_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rser1 in 0 \\(add \\(par 10 90\\) \\(par 40 60\\)\\)"
)

add_test(
    NAME exercise_3_42b_short_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_42b.cir
            in 0
)
set_tests_properties(
    exercise_3_42b_short_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth in 0: 0[\r\n]+Rth in 0: 33"
)

add_test(
    NAME exercise_3_43_parallel_voltage
    COMMAND centaur
            "(mul 90 (par 5 6 12 20))"
)
set_tests_properties(
    exercise_3_43_parallel_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "180"
)

add_test(
    NAME exercise_3_43_parallel_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_43.cir
            top 0
)
set_tests_properties(
    exercise_3_43_parallel_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar1 top 0 \\(par 12 20 5 6\\)"
)

add_test(
    NAME exercise_3_43_thevenin
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_43.cir
            top 0
)
set_tests_properties(
    exercise_3_43_thevenin
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth top 0: 180[\r\n]+Rth top 0: 2"
)

add_test(
    NAME exercise_3_43_branch_currents
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_43.cir
            --voltage top 0
            --current R5
            --current R6
            --current R12
            --current R20
)
set_tests_properties(
    exercise_3_43_branch_currents
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: 180[\r\n]+I R5: 36[\r\n]+I R6: 30[\r\n]+I R12: 15[\r\n]+I R20: 9"
)

add_test(
    NAME exercise_3_44_conductance_voltage
    COMMAND centaur
            "(div (add 190 (neg 50) 60) (add 6 12 24 8))"
)
set_tests_properties(
    exercise_3_44_conductance_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4"
)

add_test(
    NAME exercise_3_44_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_44.cir
            top 0
)
set_tests_properties(
    exercise_3_44_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Ipar1 0 top 200[\r\n]+Rpar1 top 0 \\(par \\(inv 12\\) \\(inv 24\\) \\(inv 6\\) \\(inv 8\\)\\)"
)

add_test(
    NAME exercise_3_44_thevenin
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_44.cir
            top 0
)
set_tests_properties(
    exercise_3_44_thevenin
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth top 0: 4[\r\n]+Rth top 0: 0\\.02"
)

add_test(
    NAME exercise_3_44_branch_currents
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_44.cir
            --voltage top 0
            --current R6
            --current R12
            --current R24
            --current R8
)
set_tests_properties(
    exercise_3_44_branch_currents
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: 4[\r\n]+I R6: 24[\r\n]+I R12: -48[\r\n]+I R24: 96[\r\n]+I R8: -32"
)

add_test(
    NAME exercise_3_45_current_division_i2
    COMMAND centaur
            "(neg (mul (div 12 (add 6 12 24 8)) (add 190 (neg 50) 60)))"
)
set_tests_properties(
    exercise_3_45_current_division_i2
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "-48"
)

add_test(
    NAME exercise_3_45_current_division_i3
    COMMAND centaur
            "(mul (div 24 (add 6 12 24 8)) (add 190 (neg 50) 60))"
)
set_tests_properties(
    exercise_3_45_current_division_i3
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "96"
)

add_test(
    NAME exercise_3_45_netlist_currents
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_45.cir
            --current R12
            --current R24
)
set_tests_properties(
    exercise_3_45_netlist_currents
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R12: -48[\r\n]+I R24: 96"
)

add_test(
    NAME exercise_3_46_current_division
    COMMAND centaur
            "(mul (div 12 (add 12 24)) 90)"
)
set_tests_properties(
    exercise_3_46_current_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "30"
)

add_test(
    NAME exercise_3_46_netlist_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_46.cir
            --voltage top 0
            --current R12
            --current R24
)
set_tests_properties(
    exercise_3_46_netlist_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: 720[\r\n]+I R12: 60[\r\n]+I R24: 30"
)

add_test(
    NAME exercise_3_47_kcl_v1
    COMMAND centaur
            "(div 9 (add (inv 5) (inv 10) (div 3 5)))"
)
set_tests_properties(
    exercise_3_47_kcl_v1
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "10"
)

add_test(
    NAME exercise_3_47_kvl_v2
    COMMAND centaur
            "(add (mul 9 12) 10)"
)
set_tests_properties(
    exercise_3_47_kvl_v2
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "118"
)

add_test(
    NAME exercise_3_47_netlist_voltages
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_47.cir
            --voltage v1 0
            --voltage v2 0
            --current R5
            --current Fdep
)
set_tests_properties(
    exercise_3_47_netlist_voltages
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V v1 0: 10[\r\n]+V v2 0: 118[\r\n]+I R5: 2[\r\n]+I Fdep: 6"
)

add_test(
    NAME exercise_3_47_topology_fold
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_47.cir
            v1 0
)
set_tests_properties(
    exercise_3_47_topology_fold
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I9 0 v1 9[\r\n]+Rpar1 v1 0 \\(par 1\\.25 10\\)"
)

add_test(
    NAME exercise_3_47_topology_fold_trace
    COMMAND centaur --explain --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_47.cir
            v1 0
)
set_tests_properties(
    exercise_3_47_topology_fold_trace
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "self-controlled-current-source: R5 v1 0 5 with Fdep v1 0 R5 3 -> R5 v1 0 1\\.25"
)

add_test(
    NAME exercise_3_48_current_division
    COMMAND centaur
            "(mul (div 20 (add 20 5)) 40)"
)
set_tests_properties(
    exercise_3_48_current_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "32"
)

add_test(
    NAME exercise_3_48_source_voltage
    COMMAND centaur
            "(add (neg 900) (mul 32 5))"
)
set_tests_properties(
    exercise_3_48_source_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "-740"
)

add_test(
    NAME exercise_3_48_netlist
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_48.cir
            --voltage mid 0
            --current R5
)
set_tests_properties(
    exercise_3_48_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V mid 0: -740[\r\n]+I R5: 32"
)

add_test(
    NAME exercise_3_49_v2_division
    COMMAND centaur
            "(mul 80 (div (par 36 (add 54 18)) (add 16 (par 36 (add 54 18)))))"
)
set_tests_properties(
    exercise_3_49_v2_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "48"
)

add_test(
    NAME exercise_3_49_v1_division
    COMMAND centaur
            "(mul (div 18 (add 54 18)) (mul 80 (div (par 36 (add 54 18)) (add 16 (par 36 (add 54 18))))))"
)
set_tests_properties(
    exercise_3_49_v1_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "12"
)

add_test(
    NAME exercise_3_49_netlist
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_49.cir
            --voltage v2 0
            --voltage v1 0
            --current R18
)
set_tests_properties(
    exercise_3_49_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V v2 0: 48[\r\n]+V v1 0: 12[\r\n]+I R18: 2/3"
)

add_test(
    NAME exercise_3_50_i2_division
    COMMAND centaur
            "(mul (div 8 (add 8 (add 6 (par 20 5)))) 36)"
)
set_tests_properties(
    exercise_3_50_i2_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "16"
)

add_test(
    NAME exercise_3_50_i1_division
    COMMAND centaur
            "(mul (div 20 (add 20 5)) (mul (div 8 (add 8 (add 6 (par 20 5)))) 36))"
)
set_tests_properties(
    exercise_3_50_i1_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "12\\.8"
)

add_test(
    NAME exercise_3_50_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_50.cir
            left 0
)
set_tests_properties(
    exercise_3_50_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I36 0 left 36[\r\n]+Rpar2 left 0 \\(par 8 \\(add 6 \\(par 20 5\\)\\)\\)"
)

add_test(
    NAME exercise_3_50_netlist
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_50.cir
            --current R6
            --current R5
)
set_tests_properties(
    exercise_3_50_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R6: 16[\r\n]+I R5: 12\\.8"
)

add_test(
    NAME exercise_3_52_v1_expression
    COMMAND centaur
            "(sub (add 10 13 18) 15)"
)
set_tests_properties(
    exercise_3_52_v1_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "26"
)

add_test(
    NAME exercise_3_52_v2_expression
    COMMAND centaur
            "(sub 0 (sub (add 10 13 18) 20))"
)
set_tests_properties(
    exercise_3_52_v2_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "-21"
)

add_test(
    NAME exercise_3_52_v3_expression
    COMMAND centaur
            "(sub (add 10 13) (sub (add 10 13 18) 20))"
)
set_tests_properties(
    exercise_3_52_v3_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "2"
)

add_test(
    NAME exercise_3_52_netlist
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_52.cir
            --voltage right 0
            --voltage 0 center
            --voltage n13 center
)
set_tests_properties(
    exercise_3_52_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V right 0: 26[\r\n]+V 0 center: -21[\r\n]+V n13 center: 2"
)

add_test(
    NAME exercise_3_53_unknown_resistance_expr
    COMMAND centaur --solve-for
            R
            "(eq (add 100 150 200 R) 500)"
)
set_tests_properties(
    exercise_3_53_unknown_resistance_expr
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 50"
)

add_test(
    NAME exercise_3_53_series_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_53.cir
            a 0
)
set_tests_properties(
    exercise_3_53_series_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rser3 a 0 \\(add 100 150 200 R\\)"
)

add_test(
    NAME exercise_3_53_unknown_resistance_netlist
    COMMAND centaur --solve-rth-for
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_53.cir
            a 0 R "(eq Rth 500)"
)
set_tests_properties(
    exercise_3_53_unknown_resistance_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 50"
)

add_test(
    NAME exercise_3_54_total_conductance
    COMMAND centaur
            "(inv (add (inv 2) (inv 4) (inv 8) (inv 10)))"
)
set_tests_properties(
    exercise_3_54_total_conductance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "40/39"
)

add_test(
    NAME exercise_3_54_series_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_54.cir
            a 0
)
set_tests_properties(
    exercise_3_54_series_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rser3 a 0 \\(add \\(inv 10\\) \\(inv 2\\) \\(inv 4\\) \\(inv 8\\)\\)"
)

add_test(
    NAME exercise_3_54_total_resistance_netlist
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_54.cir
            a 0
)
set_tests_properties(
    exercise_3_54_total_resistance_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 0\\.975"
)

add_test(
    NAME exercise_3_55_unknown_series_resistor
    COMMAND centaur --solve-for
            R
            "(eq (mul 277 (div 240 (add R 240))) 120)"
)
set_tests_properties(
    exercise_3_55_unknown_series_resistor
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 314"
)

add_test(
    NAME exercise_3_55_resistor_power
    COMMAND centaur
            "(mul (sub 277 120) (div 60 120))"
)
set_tests_properties(
    exercise_3_55_resistor_power
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "78\\.5"
)

add_test(
    NAME exercise_3_55_netlist
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_55.cir
            --voltage bulb 0
            --current Rseries
            --power Rseries
            --power Rbulb
)
set_tests_properties(
    exercise_3_55_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V bulb 0: 120[\r\n]+I Rseries: 0\\.5[\r\n]+P Rseries: 78\\.5[\r\n]+P Rbulb: 60"
)

add_test(
    NAME exercise_3_56_source_voltage
    COMMAND centaur
            "(mul 7 (add 4 5 6))"
)
set_tests_properties(
    exercise_3_56_source_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "105"
)

add_test(
    NAME exercise_3_56_unknown_source_voltage
    COMMAND centaur --solve-for
            V
            "(eq (div V (add 4 5 6)) 7)"
)
set_tests_properties(
    exercise_3_56_unknown_source_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V: 105"
)

add_test(
    NAME exercise_3_56_netlist
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_56.cir
            --voltage src 0
            --current R4
            --current R5
            --current R6
)
set_tests_properties(
    exercise_3_56_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V src 0: 105[\r\n]+I R4: 7[\r\n]+I R5: 7[\r\n]+I R6: 7"
)

add_test(
    NAME exercise_3_57_minimum_series_resistor
    COMMAND centaur --solve-for
            R
            "(le (div (sub 15 12) (add R 0.3)) 2)"
)
set_tests_properties(
    exercise_3_57_minimum_series_resistor
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R >= 1\\.2"
)

add_test(
    NAME exercise_3_57_current_constraint_netlist
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_57.cir
            R
)
set_tests_properties(
    exercise_3_57_current_constraint_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R >= 1\\.2"
)

add_test(
    NAME exercise_3_57_symbolic_netlist
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_57.cir
            --current Rlimit
)
set_tests_properties(
    exercise_3_57_symbolic_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I Rlimit: \\(div 3 \\(add 0\\.3 R\\)\\)"
)

add_test(
    NAME exercise_3_57_strict_current_constraint
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_57.cir
            R
            "(lt (current Rlimit) 2)"
)
set_tests_properties(
    exercise_3_57_strict_current_constraint
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R > 1\\.2"
)

add_test(
    NAME exercise_3_58_unknown_resistance_expr
    COMMAND centaur --solve-for
            R
            "(eq (mul R (div 240 (add R 100)) (div 240 (add R 100))) 80)"
)
set_tests_properties(
    exercise_3_58_unknown_resistance_expr
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 20 or 500"
)

add_test(
    NAME exercise_3_58_unknown_resistance_netlist
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_58.cir
            R
)
set_tests_properties(
    exercise_3_58_unknown_resistance_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 20 or 500"
)

add_test(
    NAME exercise_3_58_filtered_resistance_netlist
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_58.cir
            R
            "(ge R 100)"
)
set_tests_properties(
    exercise_3_58_filtered_resistance_netlist
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 500"
)

add_test(
    NAME exercise_3_59_resistor_powers
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_59.cir
            --power R2
            --power R4
            --power R6
)
set_tests_properties(
    exercise_3_59_resistor_powers
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "P R2: 2/9[\r\n]+P R4: 4/9[\r\n]+P R6: 2/3"
)

add_test(
    NAME exercise_3_60_vab
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_60.cir
            --voltage a 0
)
set_tests_properties(
    exercise_3_60_vab
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V a 0: 20"
)

add_test(
    NAME exercise_3_61_v4
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_60.cir
            --voltage bottom_left bottom_mid
)
set_tests_properties(
    exercise_3_61_v4
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V bottom_left bottom_mid: -8"
)

add_test(
    NAME exercise_3_62_voltage_division
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_62.cir
            --voltage n5 n6
)
set_tests_properties(
    exercise_3_62_voltage_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V n5 n6: 20"
)

add_test(
    NAME exercise_3_63_dependent_source_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_63.cir
            --current R2
)
set_tests_properties(
    exercise_3_63_dependent_source_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I R2: 3"
)

add_test(
    NAME exercise_3_63_topology_preserves_control_node
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_63.cir
)
set_tests_properties(
    exercise_3_63_topology_preserves_control_node
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R2 src v1 2[\r\n]+R6 v1 dep_top 6[\r\n]+Edep 0 dep_top v1 0 2"
)

add_test(
    NAME exercise_3_64_open_circuit_voltage
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_64.cir
            --voltage out_plus out_minus
)
set_tests_properties(
    exercise_3_64_open_circuit_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V out_plus out_minus: -45"
)

add_test(
    NAME exercise_3_65_kcl_i1
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_65.cir
            I1
)
set_tests_properties(
    exercise_3_65_kcl_i1
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I1: 2"
)

add_test(
    NAME exercise_3_65_kcl_i2
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_65.cir
            I2
)
set_tests_properties(
    exercise_3_65_kcl_i2
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I2: -6"
)

add_test(
    NAME exercise_3_65_kcl_i3
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_65.cir
            I3
)
set_tests_properties(
    exercise_3_65_kcl_i3
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I3: -5"
)

add_test(
    NAME exercise_3_65_kcl_i4
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_65.cir
            I4
)
set_tests_properties(
    exercise_3_65_kcl_i4
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I4: 3"
)

add_test(
    NAME exercise_3_66_short_circuit_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_66.cir
            --current V_I
)
set_tests_properties(
    exercise_3_66_short_circuit_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "I V_I: 3"
)

add_test(
    NAME exercise_3_67_dependent_source_voltage
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_67.cir
            --voltage out_top 0
)
set_tests_properties(
    exercise_3_67_dependent_source_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V out_top 0: 96"
)

add_test(
    NAME exercise_3_68_one_resistor
    COMMAND centaur "4"
)
set_tests_properties(
    exercise_3_68_one_resistor
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4"
)

add_test(
    NAME exercise_3_68_two_parallel
    COMMAND centaur "(par 4 4)"
)
set_tests_properties(
    exercise_3_68_two_parallel
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "2"
)

add_test(
    NAME exercise_3_68_three_parallel
    COMMAND centaur "(par 4 4 4)"
)
set_tests_properties(
    exercise_3_68_three_parallel
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4/3"
)

add_test(
    NAME exercise_3_68_series_pair_parallel_one
    COMMAND centaur "(par (add 4 4) 4)"
)
set_tests_properties(
    exercise_3_68_series_pair_parallel_one
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "8/3"
)

add_test(
    NAME exercise_3_68_parallel_pair_series_one
    COMMAND centaur "(add (par 4 4) 4)"
)
set_tests_properties(
    exercise_3_68_parallel_pair_series_one
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "6"
)

add_test(
    NAME exercise_3_68_two_series
    COMMAND centaur "(add 4 4)"
)
set_tests_properties(
    exercise_3_68_two_series
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "8"
)

add_test(
    NAME exercise_3_68_three_series
    COMMAND centaur "(add 4 4 4)"
)
set_tests_properties(
    exercise_3_68_three_series
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "12"
)

add_test(
    NAME exercise_3_69_unknown_parallel_resistor
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_69.cir
            R
)
set_tests_properties(
    exercise_3_69_unknown_parallel_resistor
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 300"
)

add_test(
    NAME exercise_3_70_parallel_expression
    COMMAND centaur "(par 2 4 6 8)"
)
set_tests_properties(
    exercise_3_70_parallel_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "0\\.96"
)

add_test(
    NAME exercise_3_70_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_70.cir
            a 0
)
set_tests_properties(
    exercise_3_70_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 0\\.96"
)

add_test(
    NAME exercise_3_71_unknown_parallel_resistor
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_71.cir
            R
)
set_tests_properties(
    exercise_3_71_unknown_parallel_resistor
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 1\\.25"
)

add_test(
    NAME exercise_3_72_unknown_parallel_resistor
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_72.cir
            R
)
set_tests_properties(
    exercise_3_72_unknown_parallel_resistor
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 120/7"
)

add_test(
    NAME exercise_3_73_nested_parallel_expression
    COMMAND centaur "(par (add (par 24 48) 24) 10)"
)
set_tests_properties(
    exercise_3_73_nested_parallel_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "8"
)

add_test(
    NAME exercise_3_74_nested_parallel_expression
    COMMAND centaur "(par (add (par 6 12) (par 10 40)) (add 6 2))"
)
set_tests_properties(
    exercise_3_74_nested_parallel_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4\\.8"
)

add_test(
    NAME exercise_3_75_ladder_expression
    COMMAND centaur
            "(add 15 6 (par 10 (add 6 2 (par 8 (add 3 4 5)))))"
)
set_tests_properties(
    exercise_3_75_ladder_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "1517/57"
)

add_test(
    NAME exercise_3_75_ladder_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_75.cir
            a 0
)
set_tests_properties(
    exercise_3_75_ladder_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rser6 a 0 \\(add 15 6 \\(par 10 \\(add 2 6 \\(par 8 \\(add 3 4 5\\)\\)\\)\\)\\)"
)

add_test(
    NAME exercise_3_75_ladder_topology_trace
    COMMAND centaur --explain --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_75.cir
            a 0
)
set_tests_properties(
    exercise_3_75_ladder_topology_trace
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "topology parallel resistor groups merged: 2[\r\n]+topology parallel current source groups merged: 0[\r\n]+topology series resistor groups merged: 6"
)

add_test(
    NAME exercise_3_75_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_75.cir
            a 0
)
set_tests_properties(
    exercise_3_75_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 1517/57"
)

add_test(
    NAME exercise_3_76_ladder_expression
    COMMAND centaur
            "(add 30 12 (par 20 (add 12 4 (par 16 (add 6 8 10)))))"
)
set_tests_properties(
    exercise_3_76_ladder_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "3034/57"
)

add_test(
    NAME exercise_3_76_ladder_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_76.cir
            a 0
)
set_tests_properties(
    exercise_3_76_ladder_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rser6 a 0 \\(add 12 30 \\(par 20 \\(add 12 4 \\(par 16 \\(add 10 6 8\\)\\)\\)\\)\\)"
)

add_test(
    NAME exercise_3_76_ladder_topology_trace
    COMMAND centaur --explain --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_76.cir
            a 0
)
set_tests_properties(
    exercise_3_76_ladder_topology_trace
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "topology parallel resistor groups merged: 2[\r\n]+topology parallel current source groups merged: 0[\r\n]+topology series resistor groups merged: 6"
)

add_test(
    NAME exercise_3_76_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_76.cir
            a 0
)
set_tests_properties(
    exercise_3_76_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth a 0: 0[\r\n]+Rth a 0: 3034/57"
)

add_test(
    NAME exercise_3_77a_open_expression
    COMMAND centaur
            "(add 7 8 (par 10 (add 3 5) (add 8 4)))"
)
set_tests_properties(
    exercise_3_77a_open_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "675/37"
)

add_test(
    NAME exercise_3_77a_open_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_77a.cir
            in 0
)
set_tests_properties(
    exercise_3_77a_open_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rser4 in 0 \\(add 7 8 \\(par \\(add 4 8\\) \\(par 10 \\(add 3 5\\)\\)\\)\\)"
)

add_test(
    NAME exercise_3_77a_open_topology_trace
    COMMAND centaur --explain --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_77a.cir
            in 0
)
set_tests_properties(
    exercise_3_77a_open_topology_trace
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "topology parallel resistor groups merged: 2[\r\n]+topology parallel current source groups merged: 0[\r\n]+topology series resistor groups merged: 4"
)

add_test(
    NAME exercise_3_77a_open_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_77a.cir
            in 0
)
set_tests_properties(
    exercise_3_77a_open_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth in 0: 0[\r\n]+Rth in 0: 675/37"
)

add_test(
    NAME exercise_3_77b_short_expression
    COMMAND centaur
            "(add 7 8 (par 10 (add (par 3 8) (par 5 4))))"
)
set_tests_properties(
    exercise_3_77b_short_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "18\\.0575035063"
)

add_test(
    NAME exercise_3_77b_short_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_77b.cir
            in 0
)
set_tests_properties(
    exercise_3_77b_short_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rser3 in 0 \\(add 7 8 \\(par 10 \\(add \\(par 3 8\\) \\(par 4 5\\)\\)\\)\\)"
)

add_test(
    NAME exercise_3_77b_short_topology_trace
    COMMAND centaur --explain --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_77b.cir
            in 0
)
set_tests_properties(
    exercise_3_77b_short_topology_trace
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "topology parallel resistor groups merged: 3[\r\n]+topology parallel current source groups merged: 0[\r\n]+topology series resistor groups merged: 3"
)

add_test(
    NAME exercise_3_77b_short_total_resistance
    COMMAND centaur --thevenin
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_77b.cir
            in 0
)
set_tests_properties(
    exercise_3_77b_short_total_resistance
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vth in 0: 0[\r\n]+Rth in 0: 18\\.0575035063"
)

add_test(
    NAME exercise_3_78_current_division_r4
    COMMAND centaur
            "(mul 15 (div (inv 4) (add (inv 4) (inv 6) (inv 8) (inv 12))))"
)
set_tests_properties(
    exercise_3_78_current_division_r4
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "6"
)

add_test(
    NAME exercise_3_78_current_division_r6
    COMMAND centaur
            "(mul 15 (div (inv 6) (add (inv 4) (inv 6) (inv 8) (inv 12))))"
)
set_tests_properties(
    exercise_3_78_current_division_r6
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4"
)

add_test(
    NAME exercise_3_78_current_division_r8
    COMMAND centaur
            "(mul 15 (div (inv 8) (add (inv 4) (inv 6) (inv 8) (inv 12))))"
)
set_tests_properties(
    exercise_3_78_current_division_r8
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "3"
)

add_test(
    NAME exercise_3_78_current_division_r12
    COMMAND centaur
            "(mul 15 (div (inv 12) (add (inv 4) (inv 6) (inv 8) (inv 12))))"
)
set_tests_properties(
    exercise_3_78_current_division_r12
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "2"
)

add_test(
    NAME exercise_3_78_netlist_currents
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_78.cir
            --voltage top 0
            --current R4
            --current R6
            --current R8
            --current R12
)
set_tests_properties(
    exercise_3_78_netlist_currents
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: 24[\r\n]+I R4: 6[\r\n]+I R6: 4[\r\n]+I R8: 3[\r\n]+I R12: 2"
)

add_test(
    NAME exercise_3_79_current_division_r8
    COMMAND centaur
            "(mul 15 (div (inv 8) (add (inv 8) (inv 12) (inv 16) (inv 24))))"
)
set_tests_properties(
    exercise_3_79_current_division_r8
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "6"
)

add_test(
    NAME exercise_3_79_current_division_r12
    COMMAND centaur
            "(mul 15 (div (inv 12) (add (inv 8) (inv 12) (inv 16) (inv 24))))"
)
set_tests_properties(
    exercise_3_79_current_division_r12
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4"
)

add_test(
    NAME exercise_3_79_current_division_r16
    COMMAND centaur
            "(mul 15 (div (inv 16) (add (inv 8) (inv 12) (inv 16) (inv 24))))"
)
set_tests_properties(
    exercise_3_79_current_division_r16
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "3"
)

add_test(
    NAME exercise_3_79_current_division_r24
    COMMAND centaur
            "(mul 15 (div (inv 24) (add (inv 8) (inv 12) (inv 16) (inv 24))))"
)
set_tests_properties(
    exercise_3_79_current_division_r24
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "2"
)

add_test(
    NAME exercise_3_79_netlist_currents
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_79.cir
            --voltage top 0
            --current R8
            --current R12
            --current R16
            --current R24
)
set_tests_properties(
    exercise_3_79_netlist_currents
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: 48[\r\n]+I R8: 6[\r\n]+I R12: 4[\r\n]+I R16: 3[\r\n]+I R24: 2"
)

add_test(
    NAME exercise_3_80_top_rail_voltage
    COMMAND centaur
            "(div (sub (add 5 8) 43) (add (inv 24) (inv 20) (inv (add 24 (par 8 24)))))"
)
set_tests_properties(
    exercise_3_80_top_rail_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "-240"
)

add_test(
    NAME exercise_3_80_unknown_currents
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_80.cir
            --voltage top 0
            --voltage mid 0
            --current R24_i1
            --current R24_i2
            --current R8_i3
            --current R24_i4
            --current R20_i5
)
set_tests_properties(
    exercise_3_80_unknown_currents
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: -240[\r\n]+V mid 0: -48[\r\n]+I R24_i1: -10[\r\n]+I R24_i2: -8[\r\n]+I R8_i3: 6[\r\n]+I R24_i4: -2[\r\n]+I R20_i5: 12"
)

add_test(
    NAME exercise_3_81_load_voltage_constraint
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_81.cir
            Vload
)
set_tests_properties(
    exercise_3_81_load_voltage_constraint
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vload: 8"
)

add_test(
    NAME exercise_3_81_unknown_r1
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_81.cir
            R1
)
set_tests_properties(
    exercise_3_81_unknown_r1
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R1: 20"
)

add_test(
    NAME exercise_3_81_unknown_r2
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_81.cir
            R2
)
set_tests_properties(
    exercise_3_81_unknown_r2
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R2: 5"
)

add_test(
    NAME exercise_3_82_current_division_r1
    COMMAND centaur
            "(mul (div 12 (add 6 12)) (div 12 (add 2 (par 6 12))))"
)
set_tests_properties(
    exercise_3_82_current_division_r1
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4/3"
)

add_test(
    NAME exercise_3_82_netlist_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_82.cir
            --voltage top 0
            --current Rseries
            --current R1
            --current R2
)
set_tests_properties(
    exercise_3_82_netlist_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: 8[\r\n]+I Rseries: 2[\r\n]+I R1: 4/3[\r\n]+I R2: 2/3"
)

add_test(
    NAME exercise_3_83_current_division_r10
    COMMAND centaur
            "(mul 60 (div 40 (add 40 (add 12 (par 40 10)))) (div 40 (add 40 10)))"
)
set_tests_properties(
    exercise_3_83_current_division_r10
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "32"
)

add_test(
    NAME exercise_3_83_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_83.cir
            top 0
)
set_tests_properties(
    exercise_3_83_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar2 top 0 \\(par 40 \\(add 12 \\(par 10 40\\)\\)\\)"
)

add_test(
    NAME exercise_3_83_netlist_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_83.cir
            --voltage top 0
            --voltage inner 0
            --current R10
)
set_tests_properties(
    exercise_3_83_netlist_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: 800[\r\n]+V inner 0: 320[\r\n]+I R10: 32"
)

add_test(
    NAME exercise_3_84_load_equivalent
    COMMAND centaur "(par 30 20)"
)
set_tests_properties(
    exercise_3_84_load_equivalent
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "12"
)

add_test(
    NAME exercise_3_84_unknown_resistor
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_84.cir
            R
)
set_tests_properties(
    exercise_3_84_unknown_resistor
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "R: 30"
)

add_test(
    NAME exercise_3_84_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_84.cir
            out 0
)
set_tests_properties(
    exercise_3_84_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar1 out 0 \\(par 20 R\\)"
)

add_test(
    NAME exercise_3_85_current_division
    COMMAND centaur
            "(div (mul 240 (div (par 48 60 40) (add 8 (par 48 60 40)))) 40)"
)
set_tests_properties(
    exercise_3_85_current_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4"
)

add_test(
    NAME exercise_3_85_netlist_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_85.cir
            --voltage mid 0
            --current R40
)
set_tests_properties(
    exercise_3_85_netlist_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V mid 0: 160[\r\n]+I R40: 4"
)

add_test(
    NAME exercise_3_86_supply_voltage_expression
    COMMAND centaur
            "(add (add 120 (mul 40 (add (div 120 240) (div 120 60)))) (mul 10 (add (div (add 120 (mul 40 (add (div 120 240) (div 120 60)))) 55) (add (div 120 240) (div 120 60)))))"
)
set_tests_properties(
    exercise_3_86_supply_voltage_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "285"
)

add_test(
    NAME exercise_3_86_supply_voltage_constraint
    COMMAND centaur --solve-constraint
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_86.cir
            Vs
)
set_tests_properties(
    exercise_3_86_supply_voltage_constraint
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Vs: 285"
)

add_test(
    NAME exercise_3_86_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_86.cir
            bulb 0
)
set_tests_properties(
    exercise_3_86_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar1 bulb 0 \\(par 240 60\\)"
)

add_test(
    NAME exercise_3_87_current_expression
    COMMAND centaur
            "(div (div 16 (add (inv 14) (mul 2 (inv 70)) (inv 70))) 70)"
)
set_tests_properties(
    exercise_3_87_current_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "2"
)

add_test(
    NAME exercise_3_87_dependent_power_expression
    COMMAND centaur
            "(mul (div 16 (add (inv 14) (mul 2 (inv 70)) (inv 70))) (mul 2 (div (div 16 (add (inv 14) (mul 2 (inv 70)) (inv 70))) 70)))"
)
set_tests_properties(
    exercise_3_87_dependent_power_expression
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "560"
)

add_test(
    NAME exercise_3_87_netlist_current_power
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_87.cir
            --voltage top 0
            --current R70
            --current Fdep
            --power Fdep
)
set_tests_properties(
    exercise_3_87_netlist_current_power
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V top 0: 140[\r\n]+I R70: 2[\r\n]+I Fdep: 4[\r\n]+P Fdep: 560"
)

add_test(
    NAME exercise_3_88_voltage_division
    COMMAND centaur
            "(mul 80 (div (par 40 (add 15 45)) (add 16 (par 40 (add 15 45)))) (div 45 (add 15 45)))"
)
set_tests_properties(
    exercise_3_88_voltage_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "36"
)

add_test(
    NAME exercise_3_88_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_88.cir
            mid 0
)
set_tests_properties(
    exercise_3_88_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar1 mid 0 \\(par 40 \\(add 15 45\\)\\)"
)

add_test(
    NAME exercise_3_88_netlist_voltage
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_88.cir
            --voltage mid 0
            --voltage out 0
            --current R45
)
set_tests_properties(
    exercise_3_88_netlist_voltage
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V mid 0: 48[\r\n]+V out 0: 36[\r\n]+I R45: 0.8"
)

add_test(
    NAME exercise_3_89a_current_division
    COMMAND centaur
            "(mul 28 (div 8 (add 8 6 (par 20 0))) (div 20 (add 20 0)))"
)
set_tests_properties(
    exercise_3_89a_current_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "16"
)

add_test(
    NAME exercise_3_89b_current_division
    COMMAND centaur
            "(mul 28 (div 8 (add 8 6 (par 20 5))) (div 20 (add 20 5)))"
)
set_tests_properties(
    exercise_3_89b_current_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "448/45"
)

add_test(
    NAME exercise_3_89c_current_division
    COMMAND centaur
            "(mul 28 (div 8 (add 8 6 (par 20 20))) (div 20 (add 20 20)))"
)
set_tests_properties(
    exercise_3_89c_current_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "14/3"
)

add_test(
    NAME exercise_3_89b_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_89b.cir
            left 0
)
set_tests_properties(
    exercise_3_89b_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar2 left 0 \\(par 8 \\(add 6 \\(par 20 5\\)\\)\\)"
)

add_test(
    NAME exercise_3_89a_netlist_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_89a.cir
            --voltage left 0
            --voltage load 0
            --current Vload
)
set_tests_properties(
    exercise_3_89a_netlist_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V left 0: 96[\r\n]+V load 0: 0[\r\n]+I Vload: 16"
)

add_test(
    NAME exercise_3_89b_netlist_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_89b.cir
            --voltage left 0
            --voltage load 0
            --current Rload
)
set_tests_properties(
    exercise_3_89b_netlist_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V left 0: 1120/9[\r\n]+V load 0: 448/9[\r\n]+I Rload: 448/45"
)

add_test(
    NAME exercise_3_89c_netlist_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_89c.cir
            --voltage left 0
            --voltage load 0
            --current Rload
)
set_tests_properties(
    exercise_3_89c_netlist_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V left 0: 448/3[\r\n]+V load 0: 280/3[\r\n]+I Rload: 14/3"
)

add_test(
    NAME exercise_3_90_repeated_current_division
    COMMAND centaur
            "(mul (div 225 (add 3 (par (par 100 25) (add 10 (par 60 30))))) (div (par 100 25) (add (par 100 25) (add 10 (par 60 30)))) (div 60 (add 60 30)))"
)
set_tests_properties(
    exercise_3_90_repeated_current_division
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "4"
)

add_test(
    NAME exercise_3_90_topology
    COMMAND centaur --rewrite-topology
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_90.cir
            left 0
)
set_tests_properties(
    exercise_3_90_topology
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "Rpar3 left 0 \\(par \\(add 10 \\(par 30 60\\)\\) \\(par 100 25\\)\\)"
)

add_test(
    NAME exercise_3_90_netlist_current
    COMMAND centaur --solve
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/exercise_3_90.cir
            --voltage left 0
            --voltage mid 0
            --current R30
            --current R60
)
set_tests_properties(
    exercise_3_90_netlist_current
    PROPERTIES
        PASS_REGULAR_EXPRESSION
        "V left 0: 180[\r\n]+V mid 0: 120[\r\n]+I R30: 4[\r\n]+I R60: 2"
)
