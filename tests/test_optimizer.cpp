#include "centaur/analog_rules.h"
#include "centaur/mna.h"
#include "centaur/netlist.h"
#include "centaur/parser.h"
#include "centaur/rewrite.h"
#include "centaur/solve.h"
#include "centaur/topology.h"

#include <cassert>
#include <utility>
#include <string>
#include <unordered_map>

namespace {

std::string optimize_to_string(const std::string& input) {
    const auto expr = analog::parse_expr(input);
    return analog::to_string(analog::optimize_expr(expr, analog::analog_rules(), 12));
}

std::string optimize_to_string(const analog::Expr& expr) {
    return analog::to_string(analog::optimize_expr(expr, analog::analog_rules(), 12));
}

analog::Expr substitute_atoms(
    const analog::Expr& expr,
    const std::unordered_map<std::string, std::string>& substitutions) {
    if (expr.is_atom()) {
        const auto found = substitutions.find(expr.op);
        if (found != substitutions.end()) {
            return analog::atom(found->second);
        }
        return expr;
    }

    std::vector<analog::Expr> args;
    args.reserve(expr.args.size());
    for (const auto& arg : expr.args) {
        args.push_back(substitute_atoms(arg, substitutions));
    }
    return analog::call(expr.op, std::move(args));
}

bool either(const std::string& value, const std::string& lhs, const std::string& rhs) {
    return value == lhs || value == rhs;
}

std::pair<std::string, std::string> thevenin_to_strings(const analog::Circuit& circuit,
                                                        const std::string& positive,
                                                        const std::string& negative) {
    const auto th = analog::solve_thevenin(circuit, positive, negative);
    return {
        analog::to_string(analog::optimize_expr(th.vth, analog::analog_rules(), 12)),
        analog::to_string(analog::optimize_expr(th.rth, analog::analog_rules(), 12)),
    };
}

std::pair<std::string, std::string> thevenin_to_strings(const std::string& netlist,
                                                        const std::string& positive,
                                                        const std::string& negative) {
    return thevenin_to_strings(analog::parse_netlist(netlist), positive, negative);
}

} // namespace

int main() {
    assert(optimize_to_string("(mul Vin (div R2 (add R1 R2)))") ==
           "(vdiv Vin R1 R2)");
    assert(optimize_to_string("(div 90 10)") == "9");
    assert(optimize_to_string("(add (mul 1.5 25) 30)") == "67.5");
    assert(optimize_to_string("(div (mul 5 20) (add 5 20))") == "4");
    assert(optimize_to_string("(par 12 24 32)") == "6.4");
    assert(optimize_to_string("(div (mul (par 12 24) 32) (add (par 12 24) 32))") ==
           "6.4");
    assert(optimize_to_string(
               "(par (div (mul 120 120) 60) "
               "(div (mul 120 120) 100) "
               "(div (mul 120 120) 200))") == "40");
    assert(optimize_to_string("(div (mul 120 120) (add 60 100 200))") == "40");
    assert(optimize_to_string("(par (add 4 (par 24 12)) 6)") == "4");
    assert(optimize_to_string("(add 16 14 (par 5 (add 9 3 (par 24 (add 4 8)))))") ==
           "34");
    assert(optimize_to_string("(par (add 40 90) (add 60 10))") == "45.5");
    assert(optimize_to_string("(add (par 40 60) (par 90 10))") == "33");
    assert(optimize_to_string("(mul 90 (par 5 6 12 20))") == "180");
    assert(optimize_to_string("(div (mul 90 (par 5 6 12 20)) 5)") == "36");
    assert(optimize_to_string("(div (mul 90 (par 5 6 12 20)) 6)") == "30");
    assert(optimize_to_string("(div (mul 90 (par 5 6 12 20)) 12)") == "15");
    assert(optimize_to_string("(div (mul 90 (par 5 6 12 20)) 20)") == "9");
    assert(optimize_to_string("(div (add 190 (neg 50) 60) (add 6 12 24 8))") == "4");
    assert(optimize_to_string(
               "(mul 6 (div (add 190 (neg 50) 60) (add 6 12 24 8)))") == "24");
    assert(optimize_to_string(
               "(neg (mul 12 (div (add 190 (neg 50) 60) (add 6 12 24 8))))") ==
           "-48");
    assert(optimize_to_string(
               "(mul 24 (div (add 190 (neg 50) 60) (add 6 12 24 8)))") == "96");
    assert(optimize_to_string(
               "(neg (mul 8 (div (add 190 (neg 50) 60) (add 6 12 24 8))))") ==
           "-32");
    assert(optimize_to_string(
               "(neg (mul (div 12 (add 6 12 24 8)) (add 190 (neg 50) 60)))") ==
           "-48");
    assert(optimize_to_string(
               "(mul (div 24 (add 6 12 24 8)) (add 190 (neg 50) 60))") == "96");
    assert(optimize_to_string("(mul (div 12 (add 12 24)) 90)") == "30");
    assert(optimize_to_string("(mul 30 24)") == "720");
    assert(optimize_to_string("(div 9 (add (inv 5) (inv 10) (div 3 5)))") ==
           "10");
    assert(optimize_to_string("(add (mul 9 12) 10)") == "118");
    assert(optimize_to_string("(mul (div 20 (add 20 5)) 40)") == "32");
    assert(optimize_to_string("(add (neg 900) (mul 32 5))") == "-740");
    assert(optimize_to_string("(add (inv 1) (inv 0.5) (inv 0.25) (inv 0.125))") ==
           "15");
    assert(optimize_to_string("(inv (add (inv 1) (inv 0.5) (inv 0.25) (inv 0.125)))") ==
           "0.0666666666667");

    const std::string parallel =
        optimize_to_string("(div 1 (add (inv R1) (inv R2)))");
    assert(either(parallel, "(par R1 R2)", "(par R2 R1)"));
    assert(analog::to_string(analog::simplify_expr(
               analog::parse_expr("(par R1 (par R2 R3))"))) ==
           "(par R1 R2 R3)");
    assert(analog::to_string(analog::simplify_expr(
               analog::parse_expr(
                   "(par 200 200 200 200 200 200 200 200 200 200 "
                   "200 200 200 200 200 200 200 200 200 200 "
                   "200 200 200 200 200 200 200 200 200 200 "
                   "200 200 200 200 200 200 200 200 200 200 "
                   "200 200 200 200 200 200 200 200 200 200)"))) ==
           "4");
    assert(optimize_to_string(
               "(inv (sub (inv 12000) (add (inv 10000) (inv 20000))))") ==
           "-15000");

    const auto exercise_3_35_solution = analog::solve_for(
        analog::parse_expr("(par 10000 20000 R)"),
        analog::parse_expr("12000"),
        "R");
    assert(exercise_3_35_solution.has_value());
    assert(analog::to_string(analog::optimize_expr(
               *exercise_3_35_solution, analog::analog_rules(), 12)) ==
           "-15000");
    assert(optimize_to_string("(par 10000 20000 -15000)") == "12000");

    const auto exercise_3_36_solution = analog::solve_for(
        analog::parse_expr("(add (inv 1) (inv 2) (inv R))"),
        analog::parse_expr("1.75"),
        "R");
    assert(exercise_3_36_solution.has_value());
    assert(analog::to_string(analog::optimize_expr(
               *exercise_3_36_solution, analog::analog_rules(), 12)) == "4");
    assert(optimize_to_string("(add (inv 1) (inv 2) (inv 4))") == "1.75");

    const auto parallel_topology = analog::rewrite_parallel_resistors(
        analog::parse_netlist(
            "R1 out 0 R1\n"
            "R2 0 out R2\n"
            "R3 out next R3\n"));
    assert(parallel_topology.merged_parallel_resistor_groups == 1);
    assert(parallel_topology.removed_components == 1);
    assert(parallel_topology.circuit.components.size() == 2);
    assert(parallel_topology.circuit.components[0].name == "Rpar1");
    assert(parallel_topology.circuit.components[0].positive == "out");
    assert(parallel_topology.circuit.components[0].negative == "0");
    assert(analog::to_string(parallel_topology.circuit.components[0].value) ==
           "(par R1 R2)");
    assert(analog::parse_netlist(analog::circuit_to_netlist(parallel_topology.circuit))
               .components.size() == 2);

    const auto series_topology = analog::rewrite_topology(
        analog::parse_netlist(
            "R1 a mid R1\n"
            "R2 mid b R2\n"),
        {"a", "b"});
    assert(series_topology.merged_series_resistor_groups == 1);
    assert(series_topology.removed_components == 1);
    assert(series_topology.circuit.components.size() == 1);
    assert(series_topology.circuit.components[0].positive == "a");
    assert(series_topology.circuit.components[0].negative == "b");
    assert(analog::to_string(series_topology.circuit.components[0].value) ==
           "(add R1 R2)");
    assert(series_topology.trace.size() == 1);
    assert(series_topology.trace[0].find("series-resistors:") != std::string::npos);

    const auto shorted_topology = analog::rewrite_topology(
        analog::parse_netlist(
            "Rkeep a b Rkeep\n"
            "Rshort a a Rshort\n"),
        {"a", "b"});
    assert(shorted_topology.removed_short_resistors == 1);
    assert(shorted_topology.removed_components == 1);
    assert(shorted_topology.circuit.components.size() == 1);
    assert(shorted_topology.circuit.components[0].name == "Rkeep");
    assert(shorted_topology.trace.size() == 1);
    assert(shorted_topology.trace[0].find("shorted-resistor:") !=
           std::string::npos);

    const auto zero_voltage_topology = analog::rewrite_topology(
        analog::parse_netlist(
            "Vshort a mid 0\n"
            "Rload mid 0 Rload\n"),
        {"a"});
    assert(zero_voltage_topology.removed_zero_voltage_sources == 1);
    assert(zero_voltage_topology.removed_components == 1);
    assert(zero_voltage_topology.circuit.components.size() == 1);
    assert(zero_voltage_topology.circuit.components[0].positive == "a");

    const auto protected_zero_voltage_topology = analog::rewrite_topology(
        analog::parse_netlist(
            "Vshort a mid 0\n"
            "Rload mid 0 Rload\n"),
        {"a"},
        {"Vshort"});
    assert(protected_zero_voltage_topology.removed_zero_voltage_sources == 0);
    assert(protected_zero_voltage_topology.circuit.components.size() == 2);

    const auto dangling_topology = analog::rewrite_topology(
        analog::parse_netlist(
            "V1 in 0 Vin\n"
            "Rmain in out Rmain\n"
            "Rdead in dead Rdead\n"),
        {"out"});
    assert(dangling_topology.removed_dangling_resistors == 1);
    assert(dangling_topology.removed_components == 1);
    assert(dangling_topology.circuit.components.size() == 2);

    const auto combined_topology = analog::rewrite_topology(
        analog::parse_netlist(
            "V1 in 0 Vin\n"
            "R1 in mid R1\n"
            "R2 mid out R2\n"
            "R3 in out R3\n"
            "Rshort out out Rshort\n"
            "Rdead out dead Rdead\n"),
        {"out"});
    assert(combined_topology.removed_short_resistors == 1);
    assert(combined_topology.removed_dangling_resistors == 1);
    assert(combined_topology.merged_series_resistor_groups == 1);
    assert(combined_topology.merged_parallel_resistor_groups == 1);
    assert(combined_topology.circuit.components.size() == 2);
    assert(analog::to_string(combined_topology.circuit.components[1].value) ==
           "(par R3 (add R1 R2))");
    assert(combined_topology.trace.size() == 4);

    const auto current_source_series = analog::rewrite_topology(
        analog::parse_netlist(
            "Rseries out mid Rs\n"
            "I1 mid 0 Iin\n"
            "Rload out 0 Rload\n"),
        {"out"});
    assert(current_source_series.removed_current_source_series_resistors == 1);
    assert(current_source_series.removed_components == 1);
    assert(current_source_series.circuit.components.size() == 2);
    assert(current_source_series.circuit.components[0].name == "I1");
    assert(current_source_series.circuit.components[0].positive == "out");
    assert(current_source_series.circuit.components[0].negative == "0");
    assert(current_source_series.trace.size() == 1);
    assert(current_source_series.trace[0].find("current-source-series-resistor:") !=
           std::string::npos);

    const auto parallel_current_sources = analog::rewrite_topology(
        analog::parse_netlist(
            "I1 0 out I1\n"
            "I2 out 0 I2\n"
            "I3 0 out I3\n"
            "Rload out 0 Rload\n"),
        {"out"});
    assert(parallel_current_sources.merged_parallel_current_source_groups == 1);
    assert(parallel_current_sources.removed_components == 2);
    assert(parallel_current_sources.circuit.components.size() == 2);
    assert(parallel_current_sources.circuit.components[0].name == "Ipar1");
    assert(parallel_current_sources.circuit.components[0].positive == "0");
    assert(parallel_current_sources.circuit.components[0].negative == "out");
    assert(optimize_to_string(substitute_atoms(
               parallel_current_sources.circuit.components[0].value,
               {{"I1", "190"}, {"I2", "50"}, {"I3", "60"}})) == "200");
    assert(parallel_current_sources.trace.size() == 1);
    assert(parallel_current_sources.trace[0].find("parallel-current-sources:") !=
           std::string::npos);

    const auto self_controlled_current = analog::rewrite_topology(
        analog::parse_netlist(
            "R5 out 0 R\n"
            "F1 out 0 R5 k\n"),
        {"out"});
    assert(self_controlled_current.folded_self_controlled_current_source_groups == 1);
    assert(self_controlled_current.removed_components == 1);
    assert(self_controlled_current.circuit.components.size() == 1);
    assert(self_controlled_current.circuit.components[0].name == "R5");
    assert(analog::to_string(self_controlled_current.circuit.components[0].value) ==
           "(div R (add 1 k))");
    assert(self_controlled_current.trace.size() == 1);
    assert(self_controlled_current.trace[0].find("self-controlled-current-source:") !=
           std::string::npos);

    const auto reversed_self_controlled_current = analog::rewrite_topology(
        analog::parse_netlist(
            "R5 out 0 R\n"
            "F1 0 out R5 k\n"),
        {"out"});
    assert(reversed_self_controlled_current
               .folded_self_controlled_current_source_groups == 1);
    assert(analog::to_string(
               reversed_self_controlled_current.circuit.components[0].value) ==
           "(div R (add 1 (neg k)))");

    const auto multi_self_controlled_current = analog::rewrite_topology(
        analog::parse_netlist(
            "R5 out 0 R\n"
            "F1 out 0 R5 k1\n"
            "F2 0 out R5 k2\n"),
        {"out"});
    assert(multi_self_controlled_current
               .folded_self_controlled_current_source_groups == 1);
    assert(multi_self_controlled_current.removed_components == 2);
    assert(optimize_to_string(substitute_atoms(
               multi_self_controlled_current.circuit.components[0].value,
               {{"R", "10"}, {"k1", "3"}, {"k2", "0.5"}})) ==
           "2.85714285714");

    const auto protected_self_controlled_current = analog::rewrite_topology(
        analog::parse_netlist(
            "R5 out 0 R\n"
            "F1 out 0 R5 k\n"),
        {"out"},
        {"R5"});
    assert(protected_self_controlled_current
               .folded_self_controlled_current_source_groups == 0);
    assert(protected_self_controlled_current.circuit.components.size() == 2);

    const auto external_controlled_current = analog::rewrite_topology(
        analog::parse_netlist(
            "R5 out 0 R\n"
            "F1 out 0 R5 k\n"
            "F2 other 0 R5 h\n"
            "Rother other 0 Rout\n"),
        {"out", "other"});
    assert(external_controlled_current
               .folded_self_controlled_current_source_groups == 0);
    assert(external_controlled_current.circuit.components.size() == 4);

    const auto voltage_source_parallel = analog::rewrite_topology(
        analog::parse_netlist(
            "V1 a b Vin\n"
            "Rleak a b Rleak\n"
            "Rload a 0 Rload\n"),
        {"a", "b"});
    assert(voltage_source_parallel.removed_voltage_source_parallel_resistors == 1);
    assert(voltage_source_parallel.removed_components == 1);
    assert(voltage_source_parallel.circuit.components.size() == 2);

    const auto protected_parallel = analog::rewrite_topology(
        analog::parse_netlist(
            "V1 a b Vin\n"
            "Rleak a b Rleak\n"),
        {"a", "b"},
        {"Rleak"});
    assert(protected_parallel.removed_voltage_source_parallel_resistors == 0);
    assert(protected_parallel.circuit.components.size() == 2);

    const auto rewritten_parallel_divider = analog::rewrite_topology(
        analog::parse_netlist(
            "V1 in 0 Vin\n"
            "Rtop in out Rtop\n"
            "Rb1 out 0 Rb1\n"
            "Rb2 out 0 Rb2\n"),
        {"out"});
    assert(rewritten_parallel_divider.merged_parallel_resistor_groups == 1);
    const auto parallel_divider =
        thevenin_to_strings(rewritten_parallel_divider.circuit, "out", "0");
    assert(parallel_divider.first == "(vdiv Vin Rtop (par Rb1 Rb2))");
    assert(parallel_divider.second == "(par Rb1 Rb2 Rtop)");

    const auto divider = thevenin_to_strings(
        "V1 in 0 Vin\n"
        "R1 in out R1\n"
        "R2 out 0 R2\n",
        "out", "0");
    assert(divider.first == "(vdiv Vin R1 R2)");
    assert(divider.second == "(par R1 R2)");

    const auto source_resistance = thevenin_to_strings(
        "V1 src 0 Vin\n"
        "Rs src out Rs\n",
        "out", "0");
    assert(source_resistance.first == "Vin");
    assert(source_resistance.second == "Rs");

    const auto series_chain = thevenin_to_strings(
        "V1 in 0 Vin\n"
        "R1 in mid R1\n"
        "R2 mid out R2\n",
        "out", "0");
    assert(series_chain.first == "Vin");
    assert(series_chain.second == "(add R1 R2)");

    const auto isolated_divider = thevenin_to_strings(
        "V1 in 0 Vin\n"
        "R1 in tap R1\n"
        "R2 tap 0 R2\n"
        "R3 tap out R3\n",
        "out", "0");
    assert(isolated_divider.first == "(vdiv Vin R1 R2)");
    assert(isolated_divider.second == "(add R3 (par R1 R2))");

    const auto current_source_parallel = thevenin_to_strings(
        "I1 0 out Iin\n"
        "R1 out 0 R1\n"
        "R2 out 0 R2\n",
        "out", "0");
    assert(current_source_parallel.first == "(mul Iin (par R1 R2))");
    assert(current_source_parallel.second == "(par R1 R2)");

    const auto exercise_3_20 = analog::parse_netlist(
        "V90 top 0 90\n"
        "R10 top 0 10\n"
        "R25 top n25 25\n"
        "V30 n25 n15 30\n"
        "R15 n15 0 15\n");
    const auto exercise_voltages = analog::solve_node_voltages(exercise_3_20);
    assert(analog::to_string(analog::optimize_expr(
               exercise_voltages.at("top"), analog::analog_rules(), 12)) == "90");
    assert(analog::to_string(analog::optimize_expr(
               exercise_voltages.at("n25"), analog::analog_rules(), 12)) == "52.5");
    assert(analog::to_string(analog::optimize_expr(
               exercise_voltages.at("n15"), analog::analog_rules(), 12)) == "22.5");

    const auto exercise_3_26 = analog::parse_netlist(
        "V24 src 0 24\n"
        "R16 src dep_top 16\n"
        "Eleft dep_top 0 v1 0 4\n"
        "Fdep v1 0 R16 0.5\n"
        "R4 v1 0 4\n");
    assert(analog::circuit_to_netlist(exercise_3_26).find("Fdep v1 0 R16 0.5") !=
           std::string::npos);
    const auto exercise_3_26_voltages = analog::solve_node_voltages(exercise_3_26);
    assert(analog::to_string(analog::optimize_expr(
               exercise_3_26_voltages.at("v1"), analog::analog_rules(), 12)) == "-6");

    return 0;
}
