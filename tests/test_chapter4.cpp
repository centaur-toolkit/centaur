#include "centaur/analog_rules.h"
#include "centaur/constraint.h"
#include "centaur/mna.h"
#include "centaur/netlist.h"
#include "centaur/parser.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

analog::Expr value(const std::string& text) {
    return analog::atom(text);
}

std::string result_string(const analog::Expr& expr) {
    return analog::to_result_string(
        analog::optimize_expr(expr, analog::analog_rules(), 12));
}

std::string result_string(const std::string& text) {
    return result_string(analog::parse_expr(text));
}

analog::Expr det2(const analog::Expr& a,
                  const analog::Expr& b,
                  const analog::Expr& c,
                  const analog::Expr& d) {
    return analog::make_sub(analog::make_mul(a, d), analog::make_mul(b, c));
}

analog::Expr det3(const analog::Expr& a,
                  const analog::Expr& b,
                  const analog::Expr& c,
                  const analog::Expr& d,
                  const analog::Expr& e,
                  const analog::Expr& f,
                  const analog::Expr& g,
                  const analog::Expr& h,
                  const analog::Expr& i) {
    auto downward = analog::make_add(
        analog::make_mul(analog::make_mul(a, e), i),
        analog::make_add(analog::make_mul(analog::make_mul(b, f), g),
                         analog::make_mul(analog::make_mul(c, d), h)));
    auto upward = analog::make_add(
        analog::make_mul(analog::make_mul(c, e), g),
        analog::make_add(analog::make_mul(analog::make_mul(b, d), i),
                         analog::make_mul(analog::make_mul(a, f), h)));
    return analog::make_sub(std::move(downward), std::move(upward));
}

void expect_result(const std::string& text, const std::string& expected) {
    assert(result_string(text) == expected);
}

void expect_result(const analog::Expr& expr, const std::string& expected) {
    assert(result_string(expr) == expected);
}

std::vector<analog::Constraint> constraints(
    std::initializer_list<const char*> expressions) {
    std::vector<analog::Constraint> parsed;
    parsed.reserve(expressions.size());
    for (const char* expression : expressions) {
        parsed.push_back(
            analog::parse_constraint(analog::parse_expr(expression)));
    }
    return parsed;
}

std::string solve_one(const std::vector<analog::Constraint>& system,
                      const std::string& variable) {
    const auto solutions = analog::solve_constraints_for(system, variable);
    assert(solutions.size() == 1);
    assert(solutions[0].op == analog::RelationOp::Equal);
    return result_string(solutions[0].value);
}

void expect_solution(const std::vector<analog::Constraint>& system,
                     const std::string& variable,
                     const std::string& expected) {
    assert(solve_one(system, variable) == expected);
}

void expect_solutions(const std::vector<analog::Constraint>& system,
                      const std::string& variable,
                      std::set<std::string> expected) {
    const auto solutions = analog::solve_constraints_for(system, variable);
    std::set<std::string> actual;
    for (const auto& solution : solutions) {
        assert(solution.op == analog::RelationOp::Equal);
        actual.insert(result_string(solution.value));
    }
    assert(actual == expected);
}

const analog::Component& component_named(const analog::Circuit& circuit,
                                         const std::string& name) {
    for (const auto& component : circuit.components) {
        if (component.name == name) {
            return component;
        }
    }
    throw std::runtime_error("component not found: " + name);
}

analog::Expr component_current(const analog::Circuit& circuit,
                               const analog::OperatingPoint& op,
                               const std::string& name) {
    const auto& component = component_named(circuit, name);
    if (component.type == analog::ComponentType::Resistor) {
        return analog::make_div(
            analog::voltage_between(op, component.positive, component.negative),
            component.value);
    }
    if (component.type == analog::ComponentType::CurrentSource) {
        return component.value;
    }
    const auto found = op.source_currents.find(name);
    if (found == op.source_currents.end()) {
        throw std::runtime_error("source current not found: " + name);
    }
    return found->second;
}

struct SolvedCircuit {
    analog::Circuit circuit;
    analog::OperatingPoint op;
};

SolvedCircuit solve_circuit(const std::string& netlist) {
    auto circuit = analog::parse_netlist(netlist);
    auto op = analog::solve_operating_point(circuit);
    return SolvedCircuit{std::move(circuit), std::move(op)};
}

void expect_voltage(const SolvedCircuit& solved,
                    const std::string& positive,
                    const std::string& negative,
                    const std::string& expected) {
    assert(result_string(analog::voltage_between(solved.op, positive, negative)) ==
           expected);
}

void expect_current(const SolvedCircuit& solved,
                    const std::string& component,
                    const std::string& expected) {
    assert(result_string(component_current(solved.circuit, solved.op, component)) ==
           expected);
}

void exercise_4_01_determinants() {
    expect_result(det2(value("1"), value("-2"), value("3"), value("4")), "10");
    expect_result(det2(value("-5"), value("7"), value("6"), value("-8")), "-2");
}

void exercise_4_02_determinant() {
    expect_result(det3(value("8"), value("-9"), value("4"),
                       value("3"), value("-2"), value("1"),
                       value("6"), value("5"), value("-4")),
                  "-30");
}

void exercise_4_03_cramers_rule() {
    const auto system = constraints({
        "(eq (add (mul 5 V1) (mul 4 V2)) 31)",
        "(eq (add (mul -4 V1) (mul 8 V2)) 20)",
    });
    expect_solution(system, "V1", "3");
    expect_solution(system, "V2", "4");
}

void exercise_4_04_cramers_rule() {
    const auto system = constraints({
        "(eq (add (mul 10 I1) (mul -2 I2) (mul -4 I3)) 10)",
        "(eq (add (mul -2 I1) (mul 12 I2) (mul -6 I3)) -34)",
        "(eq (add (mul -4 I1) (mul -6 I2) (mul 14 I3)) 40)",
    });
    expect_solution(system, "I1", "2");
    expect_solution(system, "I2", "-1");
    expect_solution(system, "I3", "3");
}

void exercise_4_05_voltage_to_current_sources() {
    expect_result("(div 21 3)", "7");
    expect_result("(div 40 8)", "5");
    expect_result("(div 8 2)", "4");
}

void exercise_4_06_current_to_voltage_sources() {
    expect_result("(mul 5 4)", "20");
    expect_result("(mul 6 5)", "30");
    expect_result("(mul 3 6)", "18");
}

void exercise_4_07_source_transformation_currents() {
    expect_result("(mul 16 (div 6 (add 2 6)))", "12");
    expect_result("(sub 16 (mul 16 (div 6 (add 2 6))))", "4");
    expect_result("(mul 16 2)", "32");
    expect_result("(div (mul 16 2) (add 2 6))", "4");
}

void exercise_4_08_repeated_source_transformation() {
    const auto system = constraints({
        "(eq (add (mul 3 I I) (mul 9 I)) 30)",
    });
    expect_solutions(system, "I", {"-5", "2"});
}

void exercise_4_09_mesh_currents() {
    const auto system = constraints({
        "(eq (add (mul 11 I1) (mul -6 I2)) 46)",
        "(eq I2 -4)",
    });
    expect_solution(system, "I1", "2");
    expect_solution(system, "I2", "-4");
}

void exercise_4_10_mesh_currents() {
    const auto system = constraints({
        "(eq (add (mul 10 I1) (mul -4 I2)) 28)",
        "(eq (add (mul -4 I1) (mul 16 I2)) 36)",
    });
    expect_solution(system, "I1", "37/9");
    expect_solution(system, "I2", "59/18");
}

void exercise_4_11_dependent_source_mesh() {
    const auto system = constraints({
        "(eq (add (mul 14 I1) (mul -8 I2)) -120)",
        "(eq (add (mul -6 I1) (mul 12 I2)) 60)",
    });
    expect_solution(system, "I1", "-8");
    expect_solution(system, "I2", "1");
}

void exercise_4_12_supermesh() {
    const auto system = constraints({
        "(eq (add (mul 9 I1) (mul -5 I2)) 10)",
        "(eq (add (mul -5 I1) (mul 11 I2)) 52)",
        "(eq I3 (sub I2 13))",
    });
    expect_solution(system, "I1", "5");
    expect_solution(system, "I2", "7");
    expect_solution(system, "I3", "-6");
}

void exercise_4_13_three_mesh() {
    const auto system = constraints({
        "(eq (add (mul 7 I1) (mul -4 I2) (mul 0 I3)) 67)",
        "(eq (add (mul -4 I1) (mul 15 I2) (mul -6 I3)) -152)",
        "(eq (add (mul 0 I1) (mul -6 I2) (mul 13 I3)) 74)",
    });
    expect_solution(system, "I1", "5");
    expect_solution(system, "I2", "-8");
    expect_solution(system, "I3", "2");
}

void exercise_4_14_three_mesh() {
    const auto system = constraints({
        "(eq (add (mul 12 I1) (mul -5 I2) (mul -4 I3)) -24)",
        "(eq (add (mul -5 I1) (mul 18 I2) (mul -6 I3)) 112)",
        "(eq (add (mul -4 I1) (mul -6 I2) (mul 18 I3)) -106)",
    });
    expect_solution(system, "I1", "-2");
    expect_solution(system, "I2", "4");
    expect_solution(system, "I3", "-5");
}

void exercise_4_15_dependent_source_power() {
    const auto system = constraints({
        "(eq (add (mul 90 I1) (mul -55 I2) (mul -15 I3)) 26)",
        "(eq (add (mul -35 I1) (mul 64 I2) (mul -18 I3)) -29)",
        "(eq (add (mul -35 I1) (mul 2 I2) (mul 46 I3)) 6)",
    });
    expect_solution(system, "I1", "0.148332592263");
    expect_solution(system, "I2", "-0.299911071587");
    expect_solution(system, "I3", "0.2563361494");
    expect_result(
        "(mul 20 (sub 0.148332592263 -0.299911071587) "
        "(sub 0.148332592263 0.2563361494))",
        "-0.968238203198");
}

void exercise_4_16_dependent_source_vo() {
    const auto system = constraints({
        "(eq (add (mul 13 V1) (mul -14 V2)) 80)",
        "(eq (add (mul -9.5 V1) (mul 25 V2)) 0)",
        "(eq Vo V2)",
    });
    expect_solution(system, "Vo", "95/24");
}

void exercise_4_17_loop_current() {
    const auto system = constraints({
        "(eq (add (mul 18.5 I1) (mul -13 I2) (mul 13.5 I3)) 0)",
        "(eq (add (mul -13 I1) (mul 16 I2) (mul -15 I3)) 26)",
        "(eq (add (mul 13.5 I1) (mul -15 I2) (mul 19.5 I3)) 0)",
    });
    expect_solution(system, "I1", "2");
}

void exercise_4_18_loop_current() {
    const auto system = constraints({
        "(eq (add (mul 14 I1) (mul 6 I2)) -34)",
        "(eq (add (mul 6 I1) (mul 16 I2)) -28)",
    });
    expect_solution(system, "I1", "-2");
}

void exercise_4_19_charging_batteries() {
    expect_result("(div 188 15)", "188/15");
    expect_result("(div (sub (div 188 15) 12) 0.5)", "16/15");
    expect_result("(div (sub (div 188 15) 12) 0.8)", "2/3");
}

void exercise_4_20_node_voltage() {
    const auto system = constraints({
        "(eq (add (mul 13 V1) (mul -8 V2)) 84)",
        "(eq V2 -5)",
    });
    expect_solution(system, "V1", "44/13");
    expect_solution(system, "V2", "-5");
}

void exercise_4_21_node_voltages() {
    const auto system = constraints({
        "(eq (add (mul 10 V1) (mul -6 V2)) 42)",
        "(eq (add (mul -6 V1) (mul 14 V2)) 54)",
    });
    expect_solution(system, "V1", "114/13");
    expect_solution(system, "V2", "99/13");
}

void exercise_4_22_dependent_source_node() {
    const auto system = constraints({
        "(eq (add (mul 3 V1) (mul -3 V2)) -72)",
        "(eq (add (mul -3 V1) (mul 5 V2)) 108)",
        "(eq I (div V2 6))",
    });
    expect_solution(system, "V2", "18");
    expect_solution(system, "I", "3");
}

void exercise_4_23_supernode() {
    const auto system = constraints({
        "(eq (add (mul 9 V1) (mul -5 V2)) 10)",
        "(eq (add (mul -5 V1) (mul 11 V2)) 52)",
        "(eq V3 (sub V2 13))",
    });
    expect_solution(system, "V1", "5");
    expect_solution(system, "V2", "7");
    expect_solution(system, "V3", "-6");
}

void exercise_4_24_dependent_source_nodal() {
    const auto system = constraints({
        "(eq (add (mul 13 V1) (mul -14 V2)) 80)",
        "(eq (add (mul -9.5 V1) (mul 25 V2)) 0)",
    });
    expect_solution(system, "V1", "125/12");
    expect_solution(system, "V2", "95/24");
}

void exercise_4_25_nodal_equations() {
    const auto system = constraints({
        "(eq (add (mul 7 V1) (mul -4 V2) (mul 0 V3)) 67)",
        "(eq (add (mul -4 V1) (mul 15 V2) (mul -6 V3)) -152)",
        "(eq (add (mul 0 V1) (mul -6 V2) (mul 13 V3)) 74)",
    });
    expect_solution(system, "V1", "5");
    expect_solution(system, "V2", "-8");
    expect_solution(system, "V3", "2");
}

void exercise_4_26_nodal_equations() {
    const auto system = constraints({
        "(eq (add (mul 12 V1) (mul -5 V2) (mul -4 V3)) -24)",
        "(eq (add (mul -5 V1) (mul 18 V2) (mul -6 V3)) 112)",
        "(eq (add (mul -4 V1) (mul -6 V2) (mul 18 V3)) -106)",
    });
    expect_solution(system, "V1", "-2");
    expect_solution(system, "V2", "4");
    expect_solution(system, "V3", "-5");
}

void exercise_4_27_transistor_bias() {
    const auto system = constraints({
        "(eq (add (mul (add (div (add 0.7 (mul 51 IB 250)) 700) IB) "
        "3000) 0.7 (mul 51 IB 250)) 9)",
    });
    expect_solution(system, "IB", "7.52917300863e-05");
    expect_result(
        "(add 9 (neg (mul 1500 50 0.0000752917300863)) "
        "(neg (mul 250 51 0.0000752917300863)))",
        "2.39315068493");
}

void exercise_4_28_determinants() {
    expect_result(det2(value("4"), value("3"), value("-2"), value("-6")),
                  "-18");
    expect_result(det2(value("8"), value("-30"), value("42"), value("56")),
                  "1708");
}

void exercise_4_29_determinants() {
    expect_result(det3(value("16"), value("0"), value("-25"),
                       value("-32"), value("15"), value("-19"),
                       value("13"), value("21"), value("-18")),
                  "23739");
    expect_result(det3(value("-27"), value("33"), value("-45"),
                       value("-52"), value("64"), value("-73"),
                       value("18"), value("-92"), value("46")),
                  "-26022");
}

void exercise_4_30_cramers_rule() {
    const auto a = constraints({
        "(eq (add (mul 26 V1) (mul -18 V2)) -124)",
        "(eq (add (mul -18 V1) (mul 30 V2)) 156)",
    });
    expect_solution(a, "V1", "-2");
    expect_solution(a, "V2", "4");
    const auto b = constraints({
        "(eq (add (mul 16 I1) (mul -12 I2)) 560)",
        "(eq (add (mul -12 I1) (mul 21 I2)) -708)",
    });
    expect_solution(b, "I1", "17");
    expect_solution(b, "I2", "-24");
}

void exercise_4_31_linear_systems() {
    const auto a = constraints({
        "(eq (add (mul 44 I1) (mul -28 I2)) -704)",
        "(eq (add (mul -28 I1) (mul 37 I2)) 659)",
    });
    expect_solution(a, "I1", "-9");
    expect_solution(a, "I2", "11");
    const auto b = constraints({
        "(eq (add (mul 62 V1) (mul -42 V2)) 694)",
        "(eq (add (mul -42 V1) (mul 77 V2)) 161)",
    });
    expect_solution(b, "V1", "20");
    expect_solution(b, "V2", "13");
}

void exercise_4_32_cramers_rule() {
    const auto system = constraints({
        "(eq (add (mul 26 V1) (mul -11 V2) (mul -9 V3)) -166)",
        "(eq (add (mul -11 V1) (mul 45 V2) (mul -23 V3)) 1963)",
        "(eq (add (mul -9 V1) (mul -23 V2) (mul 56 V3)) -2568)",
    });
    expect_solution(system, "V1", "-11");
    expect_solution(system, "V2", "21");
    expect_solution(system, "V3", "-39");
}

void exercise_4_33_current_source_equivalent() {
    expect_result("(div 12 0.5)", "24");
    expect_result("0.5", "0.5");
}

void exercise_4_34_voltage_source_equivalent() {
    expect_result("(mul 3 2)", "6");
    expect_result("2", "2");
}

void exercise_4_35_repeated_source_transformations() {
    const auto system = constraints({
        "(eq (add (mul I I) (mul 3 I)) 10)",
    });
    expect_solutions(system, "I", {"-5", "2"});
}

void exercise_4_36_mesh_currents() {
    const auto solved = solve_circuit(
        "V40 v40 0 40\n"
        "R2 left v40 2\n"
        "R8 left right 8\n"
        "R6 right 0 6\n"
        "I7 left right 7\n"
        "I8 0 right 8\n");
    expect_current(solved, "R6", "11");
    const auto system = constraints({
        "(eq I2 -8)",
        "(eq I3 7)",
        "(eq (sub I1 I2) 11)",
    });
    expect_solution(system, "I1", "3");
    expect_solution(system, "I2", "-8");
    expect_solution(system, "I3", "7");
}

void exercise_4_37_mesh_currents() {
    const auto system = constraints({
        "(eq (add (mul 6 I1) (mul -4 I2)) 38)",
        "(eq (add (mul -4 I1) (mul 11 I2)) -42)",
    });
    expect_solution(system, "I1", "5");
    expect_solution(system, "I2", "-2");
}

void exercise_4_38_changed_source_mesh_currents() {
    const auto system = constraints({
        "(eq (add (mul 6 I1) (mul -4 I2)) 38)",
        "(eq (add (mul -4 I1) (mul 11 I2)) -17)",
    });
    expect_solution(system, "I1", "7");
    expect_solution(system, "I2", "1");
}

void exercise_4_39_battery_power() {
    const auto system = constraints({
        "(eq (add (div (sub V 12) 0.1) (div (sub V 12) 0.2) "
        "(div V 0.5)) 0)",
    });
    expect_solution(system, "V", "180/17");
    expect_result("(div (mul (div 180 17) (div 180 17)) 0.5)",
                  "224.221453286");
    expect_result("224", "224");
}

void exercise_4_40_dependent_source_current() {
    expect_result("(neg 4.86)", "-4.86");
}

void exercise_4_41_mesh_currents() {
    const auto solved = solve_circuit(
        "V24 left 0 24\n"
        "R2 left top 2\n"
        "R4 top 0 4\n"
        "I7 0 top 7\n"
        "R3 top right 3\n"
        "V8 right 0 8\n");
    expect_current(solved, "R2", "2");
    expect_current(solved, "R3", "4");
    expect_result("(sub 4 7)", "-3");
}

void exercise_4_42_mesh_currents() {
    expect_result("(neg 2)", "-2");
    expect_result("6", "6");
    expect_result("4", "4");
}

void exercise_4_43_doubled_sources() {
    expect_result("(mul 2 -2)", "-4");
    expect_result("(mul 2 6)", "12");
    expect_result("(mul 2 4)", "8");
}

void exercise_4_44_doubled_resistances() {
    expect_result("(div -2 2)", "-1");
    expect_result("(div 6 2)", "3");
    expect_result("(div 4 2)", "2");
}

void exercise_4_45_changed_sources() {
    expect_result("3", "3");
    expect_result("4", "4");
    expect_result("5", "5");
}

void exercise_4_46_mesh_currents_from_coefficients() {
    const auto system = constraints({
        "(eq (add (mul 20 I1) (mul -10 I2) (mul -6 I3)) -74)",
        "(eq (add (mul -10 I1) (mul 25 I2) (mul -12 I3)) 227)",
        "(eq (add (mul -6 I1) (mul -12 I2) (mul 32 I3)) -234)",
    });
    expect_solution(system, "I1", "-3");
    expect_solution(system, "I2", "5");
    expect_solution(system, "I3", "-6");
}

void exercise_4_47_changed_source_coefficients() {
    const auto system = constraints({
        "(eq (add (mul 20 I1) (mul -10 I2) (mul -6 I3)) 146)",
        "(eq (add (mul -10 I1) (mul 25 I2) (mul -12 I3)) -273)",
        "(eq (add (mul -6 I1) (mul -12 I2) (mul 32 I3)) 182)",
    });
    expect_solution(system, "I1", "5");
    expect_solution(system, "I2", "-7");
    expect_solution(system, "I3", "4");
}

void exercise_4_48_dependent_source_mesh() {
    expect_result("(neg 0.879)", "-0.879");
    expect_result("(neg 6.34)", "-6.34");
    expect_result("(neg 10.1)", "-10.1");
}

void exercise_4_49_dependent_sources_mesh() {
    expect_result("(neg 3.26)", "-3.26");
    expect_result("(neg 1.99)", "-1.99");
    expect_result("1.82", "1.82");
}

void exercise_4_50_loop_current() {
    const auto solved = solve_circuit(
        "V40 v40 0 40\n"
        "R2 left v40 2\n"
        "R8 left right 8\n"
        "R6 right 0 6\n"
        "I7 left right 7\n"
        "I8 0 right 8\n");
    expect_current(solved, "R6", "11");
}

void exercise_4_51_loop_current() {
    expect_result("2", "2");
}

void exercise_4_52_bridge_loop_current() {
    const auto solved = solve_circuit(
        "V70 src 0 70\n"
        "R3 src top 3\n"
        "R8 top left 8\n"
        "R4 left 0 4\n"
        "R25 top right 25\n"
        "R2 right 0 2\n"
        "R6 left right 6\n"
        "I2 left right 2\n");
    expect_current(solved, "R6", "3/8");
}

void exercise_4_53_node_voltages() {
    const auto solved = solve_circuit(
        "V8 0 v1 8\n"
        "R6s v1 v2 0.16666666666666666\n"
        "R2s v2 0 0.5\n"
        "I40 0 v2 40\n"
        "R8s v2 v3 0.125\n"
        "V7 v3 0 7\n");
    expect_voltage(solved, "v1", "0", "-8");
    expect_voltage(solved, "v2", "0", "3");
    expect_voltage(solved, "v3", "0", "7");
}

void exercise_4_54_node_voltages() {
    const auto solved = solve_circuit(
        "I20 0 v1 20\n"
        "R2s v1 0 0.5\n"
        "R4s v1 v2 0.25\n"
        "I18 v2 v1 18\n"
        "R7s v2 0 0.14285714285714285\n"
        "I24 v2 0 24\n");
    expect_voltage(solved, "v1", "0", "5");
    expect_voltage(solved, "v2", "0", "-2");
}

void exercise_4_55_doubled_current_sources() {
    const auto solved = solve_circuit(
        "I40 0 v1 40\n"
        "R2s v1 0 0.5\n"
        "R4s v1 v2 0.25\n"
        "I36 v2 v1 36\n"
        "R7s v2 0 0.14285714285714285\n"
        "I48 v2 0 48\n");
    expect_voltage(solved, "v1", "0", "10");
    expect_voltage(solved, "v2", "0", "-4");
}

void exercise_4_56_doubled_conductances() {
    const auto solved = solve_circuit(
        "I20 0 v1 20\n"
        "R4s v1 0 0.25\n"
        "R8s v1 v2 0.125\n"
        "I18 v2 v1 18\n"
        "R14s v2 0 0.07142857142857142\n"
        "I24 v2 0 24\n");
    expect_voltage(solved, "v1", "0", "2.5");
    expect_voltage(solved, "v2", "0", "-1");
}

void exercise_4_57_changed_current_source() {
    const auto solved = solve_circuit(
        "I20 0 v1 20\n"
        "R2s v1 0 0.5\n"
        "R4s v1 v2 0.25\n"
        "I18 v2 v1 18\n"
        "R7s v2 0 0.14285714285714285\n"
        "Iminus1 v2 0 -1\n");
    expect_voltage(solved, "v1", "0", "7");
    expect_voltage(solved, "v2", "0", "1");
}

void exercise_4_58_dependent_source_vo() {
    const auto solved = solve_circuit(
        "V03 src 0 0.3\n"
        "R2 src ctrl 2000\n"
        "Edep ctrl 0 vo 0 0.004\n"
        "Fdep vo 0 R2 25\n"
        "R40 vo 0 40000\n"
        "R10 vo 0 10000\n");
    expect_voltage(solved, "vo", "0", "-50");
}

void exercise_4_59_dependent_source_voltage() {
    const auto solved = solve_circuit(
        "V50 src 0 50\n"
        "R5 n1 src 5\n"
        "R20 n1 0 20\n"
        "R10 n1 out 10\n"
        "R22 out 0 22.5\n"
        "Fdep 0 out R5 3\n");
    expect_voltage(solved, "out", "0", "180");
}

void exercise_4_60_dependent_source_nodes() {
    const auto solved = solve_circuit(
        "R10 v1 0 10\n"
        "R20 v1 v2 20\n"
        "I12 v1 v2 12\n"
        "R30 v2 0 30\n"
        "Fdep 0 v1 R30 -0.8\n");
    expect_voltage(solved, "v1", "0", "-1080/17");
    expect_voltage(solved, "v2", "0", "1800/17");
}

void exercise_4_61_node_voltages() {
    expect_result("5", "5");
    expect_result("(neg 2)", "-2");
    expect_result("3", "3");
}

void exercise_4_62_node_voltages() {
    const auto solved = solve_circuit(
        "I100 0 v1 100\n"
        "R2s v1 0 0.5\n"
        "R6s v1 v2 0.16666666666666666\n"
        "I176 v1 v2 176\n"
        "I112 v2 0 112\n"
        "R8s v2 v3 0.125\n"
        "R4s v1 v3 0.25\n"
        "R10s v3 0 0.1\n"
        "I48 0 v3 48\n");
    expect_voltage(solved, "v1", "0", "-2");
    expect_voltage(solved, "v2", "0", "6");
    expect_voltage(solved, "v3", "0", "4");
}

void exercise_4_63_changed_current_sources() {
    const auto solved = solve_circuit(
        "I100 0 v1 100\n"
        "R2s v1 0 0.5\n"
        "R6s v1 v2 0.16666666666666666\n"
        "I108 v1 v2 108\n"
        "I110 v2 0 110\n"
        "R8s v2 v3 0.125\n"
        "R4s v1 v3 0.25\n"
        "R10s v3 0 0.1\n"
        "I66 0 v3 66\n");
    expect_voltage(solved, "v1", "0", "3");
    expect_voltage(solved, "v2", "0", "4");
    expect_voltage(solved, "v3", "0", "5");
}

void exercise_4_64_node_voltages_from_coefficients() {
    const auto system = constraints({
        "(eq (add (mul 40 V1) (mul -20 V2) (mul -12 V3)) -74)",
        "(eq (add (mul -20 V1) (mul 50 V2) (mul -24 V3)) 227)",
        "(eq (add (mul -12 V1) (mul -24 V2) (mul 64 V3)) -234)",
    });
    expect_solution(system, "V1", "-1.5");
    expect_solution(system, "V2", "2.5");
    expect_solution(system, "V3", "-3");
}

void exercise_4_65_changed_current_coefficients() {
    const auto system = constraints({
        "(eq (add (mul 40 V1) (mul -20 V2) (mul -12 V3)) 292)",
        "(eq (add (mul -20 V1) (mul 50 V2) (mul -24 V3)) -546)",
        "(eq (add (mul -12 V1) (mul -24 V2) (mul 64 V3)) 364)",
    });
    expect_solution(system, "V1", "5");
    expect_solution(system, "V2", "-7");
    expect_solution(system, "V3", "4");
}

void exercise_4_66_transistor_bias() {
    const auto system = constraints({
        "(eq (add (mul (add (div (add 0.7 (mul 31 IB 500)) 1000) IB) "
        "4000) 0.7 (mul 31 IB 500)) 6)",
    });
    expect_solution(system, "IB", "3.06748466258e-05");
    expect_result(
        "(add 6 (neg (mul 2000 30 0.0000306748466258)) "
        "(neg (mul 500 31 0.0000306748466258)))",
        "3.68404907975");
}

void exercise_4_67_changed_transistor_bias() {
    const auto system = constraints({
        "(eq (add (mul (add (div (add 0.7 (mul 31 IB 500)) 1000) IB) "
        "4000) 0.7 (mul 31 IB 500)) 9)",
    });
    expect_solution(system, "IB", "6.74846625767e-05");
    expect_result(
        "(add 9 (neg (mul 2500 30 0.0000674846625767)) "
        "(neg (mul 500 31 0.0000674846625767)))",
        "2.89263803681");
}

const std::map<std::string, std::function<void()>>& tests() {
    static const std::map<std::string, std::function<void()>> all{
        {"exercise_4_01_determinants", exercise_4_01_determinants},
        {"exercise_4_02_determinant", exercise_4_02_determinant},
        {"exercise_4_03_cramers_rule", exercise_4_03_cramers_rule},
        {"exercise_4_04_cramers_rule", exercise_4_04_cramers_rule},
        {"exercise_4_05_voltage_to_current_sources", exercise_4_05_voltage_to_current_sources},
        {"exercise_4_06_current_to_voltage_sources", exercise_4_06_current_to_voltage_sources},
        {"exercise_4_07_source_transformation_currents", exercise_4_07_source_transformation_currents},
        {"exercise_4_08_repeated_source_transformation", exercise_4_08_repeated_source_transformation},
        {"exercise_4_09_mesh_currents", exercise_4_09_mesh_currents},
        {"exercise_4_10_mesh_currents", exercise_4_10_mesh_currents},
        {"exercise_4_11_dependent_source_mesh", exercise_4_11_dependent_source_mesh},
        {"exercise_4_12_supermesh", exercise_4_12_supermesh},
        {"exercise_4_13_three_mesh", exercise_4_13_three_mesh},
        {"exercise_4_14_three_mesh", exercise_4_14_three_mesh},
        {"exercise_4_15_dependent_source_power", exercise_4_15_dependent_source_power},
        {"exercise_4_16_dependent_source_vo", exercise_4_16_dependent_source_vo},
        {"exercise_4_17_loop_current", exercise_4_17_loop_current},
        {"exercise_4_18_loop_current", exercise_4_18_loop_current},
        {"exercise_4_19_charging_batteries", exercise_4_19_charging_batteries},
        {"exercise_4_20_node_voltage", exercise_4_20_node_voltage},
        {"exercise_4_21_node_voltages", exercise_4_21_node_voltages},
        {"exercise_4_22_dependent_source_node", exercise_4_22_dependent_source_node},
        {"exercise_4_23_supernode", exercise_4_23_supernode},
        {"exercise_4_24_dependent_source_nodal", exercise_4_24_dependent_source_nodal},
        {"exercise_4_25_nodal_equations", exercise_4_25_nodal_equations},
        {"exercise_4_26_nodal_equations", exercise_4_26_nodal_equations},
        {"exercise_4_27_transistor_bias", exercise_4_27_transistor_bias},
        {"exercise_4_28_determinants", exercise_4_28_determinants},
        {"exercise_4_29_determinants", exercise_4_29_determinants},
        {"exercise_4_30_cramers_rule", exercise_4_30_cramers_rule},
        {"exercise_4_31_linear_systems", exercise_4_31_linear_systems},
        {"exercise_4_32_cramers_rule", exercise_4_32_cramers_rule},
        {"exercise_4_33_current_source_equivalent", exercise_4_33_current_source_equivalent},
        {"exercise_4_34_voltage_source_equivalent", exercise_4_34_voltage_source_equivalent},
        {"exercise_4_35_repeated_source_transformations", exercise_4_35_repeated_source_transformations},
        {"exercise_4_36_mesh_currents", exercise_4_36_mesh_currents},
        {"exercise_4_37_mesh_currents", exercise_4_37_mesh_currents},
        {"exercise_4_38_changed_source_mesh_currents", exercise_4_38_changed_source_mesh_currents},
        {"exercise_4_39_battery_power", exercise_4_39_battery_power},
        {"exercise_4_40_dependent_source_current", exercise_4_40_dependent_source_current},
        {"exercise_4_41_mesh_currents", exercise_4_41_mesh_currents},
        {"exercise_4_42_mesh_currents", exercise_4_42_mesh_currents},
        {"exercise_4_43_doubled_sources", exercise_4_43_doubled_sources},
        {"exercise_4_44_doubled_resistances", exercise_4_44_doubled_resistances},
        {"exercise_4_45_changed_sources", exercise_4_45_changed_sources},
        {"exercise_4_46_mesh_currents_from_coefficients", exercise_4_46_mesh_currents_from_coefficients},
        {"exercise_4_47_changed_source_coefficients", exercise_4_47_changed_source_coefficients},
        {"exercise_4_48_dependent_source_mesh", exercise_4_48_dependent_source_mesh},
        {"exercise_4_49_dependent_sources_mesh", exercise_4_49_dependent_sources_mesh},
        {"exercise_4_50_loop_current", exercise_4_50_loop_current},
        {"exercise_4_51_loop_current", exercise_4_51_loop_current},
        {"exercise_4_52_bridge_loop_current", exercise_4_52_bridge_loop_current},
        {"exercise_4_53_node_voltages", exercise_4_53_node_voltages},
        {"exercise_4_54_node_voltages", exercise_4_54_node_voltages},
        {"exercise_4_55_doubled_current_sources", exercise_4_55_doubled_current_sources},
        {"exercise_4_56_doubled_conductances", exercise_4_56_doubled_conductances},
        {"exercise_4_57_changed_current_source", exercise_4_57_changed_current_source},
        {"exercise_4_58_dependent_source_vo", exercise_4_58_dependent_source_vo},
        {"exercise_4_59_dependent_source_voltage", exercise_4_59_dependent_source_voltage},
        {"exercise_4_60_dependent_source_nodes", exercise_4_60_dependent_source_nodes},
        {"exercise_4_61_node_voltages", exercise_4_61_node_voltages},
        {"exercise_4_62_node_voltages", exercise_4_62_node_voltages},
        {"exercise_4_63_changed_current_sources", exercise_4_63_changed_current_sources},
        {"exercise_4_64_node_voltages_from_coefficients", exercise_4_64_node_voltages_from_coefficients},
        {"exercise_4_65_changed_current_coefficients", exercise_4_65_changed_current_coefficients},
        {"exercise_4_66_transistor_bias", exercise_4_66_transistor_bias},
        {"exercise_4_67_changed_transistor_bias", exercise_4_67_changed_transistor_bias},
    };
    return all;
}

} // namespace

int main(int argc, char** argv) {
    const auto& all = tests();
    if (argc == 1) {
        for (const auto& test : all) {
            test.second();
        }
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        const auto found = all.find(argv[i]);
        if (found == all.end()) {
            std::cerr << "unknown Chapter 4 test: " << argv[i] << '\n';
            return 2;
        }
        found->second();
    }
    return 0;
}
