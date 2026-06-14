#include "centaur/analog_rules.h"
#include "centaur/constraint.h"
#include "centaur/mna.h"
#include "centaur/netlist.h"
#include "centaur/parser.h"
#include "centaur/rewrite.h"
#include "centaur/solve.h"
#include "centaur/topology.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void usage(const char* program) {
    std::cerr
        << "Usage:\n"
        << "  " << program << " [--iters N] '<expr>'\n"
        << "  " << program << " [--iters N] --solve-for <var> "
        << "'(eq|lt|le|gt|ge lhs-expr rhs-expr)'\n"
        << "  " << program << " [--iters N] --solve <netlist.cir> "
        << "[--current name] [--voltage node+ node-] [--power name] "
        << "[--seen-resistance Vname]\n"
        << "  " << program
        << " [--iters N] --solve-constraint <netlist.cir> "
        << "<var> ['(eq|lt|le|gt|ge lhs rhs)' ...]\n"
        << "  " << program << " --rewrite-topology <netlist.cir> [protected-node ...]\n"
        << "  " << program << " [--iters N] --thevenin <netlist.cir> <node+> <node->\n"
        << "  " << program
        << " [--iters N] --solve-rth-for <netlist.cir> <node+> <node-> <var> "
        << "'(eq|lt|le|gt|ge Rth target-rth)'\n\n"
        << "Examples:\n"
        << "  " << program << " '(mul Vin (div R2 (add R1 R2)))'\n"
        << "  " << program << " --solve-for R '(eq (par 10000 20000 R) 12000)'\n"
        << "  " << program
        << " --solve-constraint file.cir R '(le (current Rlimit) 2)'\n"
        << "  " << program << " --thevenin examples/voltage_divider.cir out 0\n";
}

void print_topology_summary(const analog::TopologyRewriteResult& topology,
                            const std::string& prefix = "") {
    std::cerr << prefix << "topology iterations: " << topology.iterations << '\n';
    std::cerr << prefix << "topology parallel resistor groups merged: "
              << topology.merged_parallel_resistor_groups << '\n';
    std::cerr << prefix << "topology parallel current source groups merged: "
              << topology.merged_parallel_current_source_groups << '\n';
    std::cerr << prefix << "topology series resistor groups merged: "
              << topology.merged_series_resistor_groups << '\n';
    std::cerr << prefix << "topology shorted resistors removed: "
              << topology.removed_short_resistors << '\n';
    std::cerr << prefix << "topology zero-voltage sources removed: "
              << topology.removed_zero_voltage_sources << '\n';
    std::cerr << prefix << "topology dangling resistors removed: "
              << topology.removed_dangling_resistors << '\n';
    std::cerr << prefix << "topology current-source series resistors removed: "
              << topology.removed_current_source_series_resistors << '\n';
    std::cerr << prefix << "topology voltage-source parallel resistors removed: "
              << topology.removed_voltage_source_parallel_resistors << '\n';
    std::cerr << prefix << "topology self-controlled current source groups folded: "
              << topology.folded_self_controlled_current_source_groups << '\n';
    std::cerr << prefix << "topology components removed: "
              << topology.removed_components << '\n';
    if (!topology.trace.empty()) {
        std::cerr << prefix << "topology rewrite trace:\n";
        for (std::size_t i = 0; i < topology.trace.size(); ++i) {
            std::cerr << prefix << "  " << (i + 1) << ". " << topology.trace[i]
                      << '\n';
        }
    }
}

std::string join(const std::vector<std::string>& values, std::size_t start) {
    std::ostringstream out;
    for (std::size_t i = start; i < values.size(); ++i) {
        if (i != start) {
            out << ' ';
        }
        out << values[i];
    }
    return out.str();
}

bool contains_atom(const analog::Expr& expr, const std::string& name) {
    if (expr.is_atom()) {
        return expr.op == name;
    }
    for (const auto& arg : expr.args) {
        if (contains_atom(arg, name)) {
            return true;
        }
    }
    return false;
}

void print_constraint_solution(const std::string& variable,
                               const analog::ConstraintSolution& solution,
                               const analog::Expr& value) {
    if (solution.op == analog::RelationOp::Equal) {
        std::cout << variable << ": " << analog::to_result_string(value) << '\n';
        return;
    }
    std::cout << variable << ' ' << analog::constraint_op_text(solution.op) << ' '
              << analog::to_result_string(value) << '\n';
}

void print_constraint_solutions(
    const std::string& variable,
    const std::vector<std::pair<analog::ConstraintSolution, analog::Expr>>&
        solutions) {
    const bool all_equal =
        std::all_of(solutions.begin(), solutions.end(), [](const auto& solution) {
            return solution.first.op == analog::RelationOp::Equal;
        });
    if (all_equal && solutions.size() > 1) {
        std::cout << variable << ": ";
        for (std::size_t i = 0; i < solutions.size(); ++i) {
            if (i != 0) {
                std::cout << " or ";
            }
            std::cout << analog::to_result_string(solutions[i].second);
        }
        std::cout << '\n';
        return;
    }

    for (const auto& [solution, value] : solutions) {
        print_constraint_solution(variable, solution, value);
    }
}

analog::Expr optimize(const analog::Expr& expr,
                      const std::vector<analog::Rewrite>& rules,
                      int iterations,
                      bool explain,
                      const std::string& label) {
    analog::EGraph graph;
    const int root = graph.add(analog::simplify_expr(expr));
    const auto result = analog::saturate(graph, rules, iterations);
    const auto extracted = analog::extract_best(graph, root);

    if (explain) {
        std::cerr << label << ": saturated " << result.applied << " rewrites over "
                  << result.iterations << " iterations; " << graph.class_count()
                  << " e-classes\n";
        for (const auto& [name, count] : result.rule_counts) {
            std::cerr << "  " << name << ": " << count << '\n';
        }
    }

    return analog::simplify_expr(extracted.expr);
}

void solve_and_print_constraint(const analog::Constraint& constraint,
                                const std::string& variable,
                                const std::vector<analog::Rewrite>& rules,
                                int iterations,
                                bool explain) {
    const std::vector<analog::Constraint> constraints{constraint};
    const auto solutions = analog::solve_constraints_for(constraints, variable);
    if (solutions.empty()) {
        throw std::runtime_error("could not solve constraint for variable: " +
                                 variable);
    }
    std::vector<std::pair<analog::ConstraintSolution, analog::Expr>> optimized;
    optimized.reserve(solutions.size());
    for (const auto& solution : solutions) {
        if (explain) {
            std::cerr << "raw " << variable << ' '
                      << analog::constraint_op_text(solution.op) << ' '
                      << analog::to_string(solution.value) << '\n';
        }
        optimized.push_back(
            {solution,
             optimize(solution.value, rules, iterations, explain,
                      "solve(" + variable + ")")});
    }
    print_constraint_solutions(variable, optimized);
}

void solve_and_print_constraints(const std::vector<analog::Constraint>& constraints,
                                 const std::string& variable,
                                 const std::vector<analog::Rewrite>& rules,
                                 int iterations,
                                 bool explain) {
    const auto solutions = analog::solve_constraints_for(constraints, variable);
    if (solutions.empty()) {
        throw std::runtime_error("could not solve constraints for variable: " +
                                 variable);
    }
    std::vector<std::pair<analog::ConstraintSolution, analog::Expr>> optimized;
    optimized.reserve(solutions.size());
    for (const auto& solution : solutions) {
        if (explain) {
            std::cerr << "raw " << variable << ' '
                      << analog::constraint_op_text(solution.op) << ' '
                      << analog::to_string(solution.value) << '\n';
        }
        optimized.push_back(
            {solution,
             optimize(solution.value, rules, iterations, explain,
                      "solve(" + variable + ")")});
    }
    print_constraint_solutions(variable, optimized);
}

analog::Expr node_voltage(const std::map<std::string, analog::Expr>& voltages,
                          const std::string& node) {
    if (node == "0" || node == "gnd" || node == "GND") {
        return analog::atom("0");
    }
    auto found = voltages.find(node);
    if (found == voltages.end()) {
        throw std::runtime_error("node not present in operating-point solution: " +
                                 node);
    }
    return found->second;
}

analog::Expr voltage_between(const std::map<std::string, analog::Expr>& voltages,
                             const std::string& positive,
                             const std::string& negative) {
    return analog::make_sub(node_voltage(voltages, positive),
                            node_voltage(voltages, negative));
}

const analog::Component& find_component(const analog::Circuit& circuit,
                                        const std::string& name) {
    for (const auto& component : circuit.components) {
        if (component.name == name) {
            return component;
        }
    }
    throw std::runtime_error("component not found: " + name);
}

analog::Expr resistor_current(
    const analog::Component& component,
    const std::map<std::string, analog::Expr>& voltages) {
    if (component.type != analog::ComponentType::Resistor) {
        throw std::runtime_error("current query currently supports resistors only: " +
                                 component.name);
    }
    const auto voltage = voltage_between(voltages, component.positive,
                                         component.negative);
    return analog::make_div(voltage, component.value);
}

analog::Expr source_voltage(const analog::Component& component,
                            const std::map<std::string, analog::Expr>& voltages) {
    if (component.type == analog::ComponentType::VoltageSource) {
        return component.value;
    }
    if (component.type == analog::ComponentType::VoltageControlledVoltageSource) {
        return analog::make_mul(
            component.value,
            voltage_between(voltages, component.control_positive,
                            component.control_negative));
    }
    return voltage_between(voltages, component.positive, component.negative);
}

analog::Expr component_current(
    const analog::Component& component,
    const analog::OperatingPoint& operating_point,
    const analog::Circuit& circuit) {
    if (component.type == analog::ComponentType::Resistor) {
        return resistor_current(component, operating_point.voltages);
    }
    if (component.type == analog::ComponentType::CurrentSource) {
        return component.value;
    }
    if (component.type == analog::ComponentType::VoltageControlledCurrentSource) {
        return analog::make_mul(
            component.value,
            voltage_between(operating_point.voltages, component.control_positive,
                            component.control_negative));
    }
    if (component.type == analog::ComponentType::CurrentControlledCurrentSource) {
        const auto& control = find_component(circuit, component.control_component);
        if (control.type == analog::ComponentType::CurrentControlledCurrentSource) {
            throw std::runtime_error(
                "current-controlled current source cannot be controlled by another "
                "current-controlled current source yet: " +
                component.name);
        }
        return analog::make_mul(
            component.value,
            component_current(control, operating_point, circuit));
    }

    auto found = operating_point.source_currents.find(component.name);
    if (found == operating_point.source_currents.end()) {
        throw std::runtime_error("source current not present in solution: " +
                                 component.name);
    }
    return found->second;
}

analog::Expr component_power(
    const analog::Component& component,
    const analog::OperatingPoint& operating_point,
    const analog::Circuit& circuit) {
    return analog::make_mul(source_voltage(component, operating_point.voltages),
                            component_current(component, operating_point, circuit));
}

analog::Expr source_seen_resistance(
    const analog::Component& component,
    const analog::OperatingPoint& operating_point,
    const analog::Circuit& circuit) {
    if (component.type != analog::ComponentType::VoltageSource) {
        throw std::runtime_error("--seen-resistance requires an independent voltage source: " +
                                 component.name);
    }

    // MNA source current is referenced from the positive node through the source.
    // Textbook input resistance uses current flowing out of the positive terminal
    // into the external circuit, which is the opposite direction.
    return analog::make_div(component.value,
                            analog::make_neg(component_current(component,
                                                               operating_point,
                                                               circuit)));
}

struct SolveQueries {
    struct Query {
        enum class Kind {
            Current,
            Voltage,
            Power,
            SeenResistance,
        };

        Kind kind;
        std::string first;
        std::string second;
    };

    std::vector<Query> ordered;
};

SolveQueries parse_solve_queries(const std::vector<std::string>& args,
                                 std::size_t start) {
    SolveQueries queries;
    for (std::size_t i = start; i < args.size(); ++i) {
        if (args[i] == "--current") {
            if (i + 1 >= args.size()) {
                throw std::runtime_error("--current requires a component name");
            }
            queries.ordered.push_back(
                SolveQueries::Query{SolveQueries::Query::Kind::Current,
                                    args[++i],
                                    ""});
        } else if (args[i] == "--voltage") {
            if (i + 2 >= args.size()) {
                throw std::runtime_error("--voltage requires two node names");
            }
            queries.ordered.push_back(
                SolveQueries::Query{SolveQueries::Query::Kind::Voltage,
                                    args[i + 1],
                                    args[i + 2]});
            i += 2;
        } else if (args[i] == "--power") {
            if (i + 1 >= args.size()) {
                throw std::runtime_error("--power requires a component name");
            }
            queries.ordered.push_back(
                SolveQueries::Query{SolveQueries::Query::Kind::Power,
                                    args[++i],
                                    ""});
        } else if (args[i] == "--seen-resistance") {
            if (i + 1 >= args.size()) {
                throw std::runtime_error("--seen-resistance requires a source name");
            }
            queries.ordered.push_back(
                SolveQueries::Query{SolveQueries::Query::Kind::SeenResistance,
                                    args[++i],
                                    ""});
        } else {
            throw std::runtime_error("unknown --solve option: " + args[i]);
        }
    }
    return queries;
}

SolveQueries protection_queries_for_observables(
    const std::vector<analog::ObservableRequest>& observables) {
    SolveQueries queries;
    for (const auto& observable : observables) {
        if (observable.kind == analog::ObservableKind::Current) {
            queries.ordered.push_back(SolveQueries::Query{
                SolveQueries::Query::Kind::Current,
                observable.first,
                ""});
        } else if (observable.kind == analog::ObservableKind::Voltage) {
            queries.ordered.push_back(SolveQueries::Query{
                SolveQueries::Query::Kind::Voltage,
                observable.first,
                observable.second});
        } else if (observable.kind == analog::ObservableKind::Power) {
            queries.ordered.push_back(SolveQueries::Query{
                SolveQueries::Query::Kind::Power,
                observable.first,
                ""});
        }
    }
    return queries;
}

std::optional<analog::ObservableKind> inline_observable_kind(
    const std::string& op) {
    if (op == "current" || op == "i") {
        return analog::ObservableKind::Current;
    }
    if (op == "voltage" || op == "v") {
        return analog::ObservableKind::Voltage;
    }
    if (op == "power" || op == "p") {
        return analog::ObservableKind::Power;
    }
    if (op == "rth") {
        return analog::ObservableKind::Rth;
    }
    return std::nullopt;
}

std::string observable_atom_arg(const analog::Expr& expr,
                                const std::string& observable) {
    if (!expr.is_atom()) {
        throw std::runtime_error(observable +
                                 " observable arguments must be names");
    }
    return expr.op;
}

std::optional<analog::ObservableRequest> inline_observable(
    const analog::Expr& expr) {
    if (expr.is_atom()) {
        return std::nullopt;
    }

    const auto kind = inline_observable_kind(expr.op);
    if (!kind.has_value()) {
        return std::nullopt;
    }

    if (*kind == analog::ObservableKind::Current ||
        *kind == analog::ObservableKind::Power) {
        if (expr.args.size() != 1) {
            throw std::runtime_error(expr.op +
                                     " observable requires one component name");
        }
        return analog::ObservableRequest{
            *kind,
            observable_atom_arg(expr.args[0], expr.op),
            ""};
    }

    if (expr.args.size() != 2) {
        throw std::runtime_error(expr.op + " observable requires two node names");
    }
    return analog::ObservableRequest{
        *kind,
        observable_atom_arg(expr.args[0], expr.op),
        observable_atom_arg(expr.args[1], expr.op)};
}

void collect_inline_observables(
    const analog::Expr& expr,
    std::vector<analog::ObservableRequest>& observables) {
    if (const auto observable = inline_observable(expr)) {
        observables.push_back(*observable);
        return;
    }
    for (const auto& arg : expr.args) {
        collect_inline_observables(arg, observables);
    }
}

std::vector<analog::ObservableRequest> inline_observables(
    const analog::Constraint& constraint) {
    std::vector<analog::ObservableRequest> observables;
    collect_inline_observables(constraint.lhs, observables);
    collect_inline_observables(constraint.rhs, observables);
    return observables;
}

std::vector<analog::ObservableRequest> inline_observables(
    const std::vector<analog::Constraint>& constraints) {
    std::vector<analog::ObservableRequest> observables;
    for (const auto& constraint : constraints) {
        collect_inline_observables(constraint.lhs, observables);
        collect_inline_observables(constraint.rhs, observables);
    }
    return observables;
}

analog::Expr evaluate_observable(
    const analog::ObservableRequest& observable,
    const analog::Circuit& circuit,
    const analog::Circuit& topology_circuit,
    const analog::OperatingPoint& operating_point,
    const std::vector<analog::Rewrite>& rules,
    int iterations,
    bool explain) {
    analog::Expr value;
    std::string label;
    if (observable.kind == analog::ObservableKind::Current) {
        const auto& component = find_component(topology_circuit, observable.first);
        value = component_current(component, operating_point, topology_circuit);
        label = "I(" + observable.first + ")";
    } else if (observable.kind == analog::ObservableKind::Voltage) {
        value = voltage_between(operating_point.voltages,
                                observable.first,
                                observable.second);
        label = "V(" + observable.first + "," + observable.second + ")";
    } else if (observable.kind == analog::ObservableKind::Power) {
        const auto& component = find_component(topology_circuit, observable.first);
        value = component_power(component, operating_point, topology_circuit);
        label = "P(" + observable.first + ")";
    } else {
        const auto rth_topology =
            analog::rewrite_topology(circuit, {observable.first, observable.second});
        if (explain) {
            print_topology_summary(rth_topology, "rth ");
        }
        value = analog::solve_thevenin(rth_topology.circuit,
                                       observable.first,
                                       observable.second)
                    .rth;
        label = "Rth(" + observable.first + "," + observable.second + ")";
    }
    return optimize(value, rules, iterations, explain, label);
}

analog::Expr lower_inline_observables(
    const analog::Expr& expr,
    const analog::Circuit& circuit,
    const analog::Circuit& topology_circuit,
    const analog::OperatingPoint& operating_point,
    const std::vector<analog::Rewrite>& rules,
    int iterations,
    bool explain) {
    if (const auto observable = inline_observable(expr)) {
        return evaluate_observable(*observable,
                                   circuit,
                                   topology_circuit,
                                   operating_point,
                                   rules,
                                   iterations,
                                   explain);
    }
    if (expr.is_atom()) {
        return expr;
    }

    std::vector<analog::Expr> args;
    args.reserve(expr.args.size());
    for (const auto& arg : expr.args) {
        args.push_back(lower_inline_observables(arg,
                                                circuit,
                                                topology_circuit,
                                                operating_point,
                                                rules,
                                                iterations,
                                                explain));
    }
    return analog::call(expr.op, std::move(args));
}

analog::Constraint lower_inline_observables(
    const analog::Constraint& constraint,
    const analog::Circuit& circuit,
    const analog::Circuit& topology_circuit,
    const analog::OperatingPoint& operating_point,
    const std::vector<analog::Rewrite>& rules,
    int iterations,
    bool explain) {
    return analog::Constraint{
        constraint.op,
        lower_inline_observables(constraint.lhs,
                                 circuit,
                                 topology_circuit,
                                 operating_point,
                                 rules,
                                 iterations,
                                 explain),
        lower_inline_observables(constraint.rhs,
                                 circuit,
                                 topology_circuit,
                                 operating_point,
                                 rules,
                                 iterations,
                                 explain)};
}

std::vector<analog::Constraint> lower_inline_observables(
    const std::vector<analog::Constraint>& constraints,
    const analog::Circuit& circuit,
    const analog::Circuit& topology_circuit,
    const analog::OperatingPoint& operating_point,
    const std::vector<analog::Rewrite>& rules,
    int iterations,
    bool explain) {
    std::vector<analog::Constraint> lowered;
    lowered.reserve(constraints.size());
    for (const auto& constraint : constraints) {
        lowered.push_back(lower_inline_observables(constraint,
                                                   circuit,
                                                   topology_circuit,
                                                   operating_point,
                                                   rules,
                                                   iterations,
                                                   explain));
    }
    return lowered;
}

std::vector<std::string> protected_nodes_for_queries(const SolveQueries& queries) {
    std::vector<std::string> nodes;
    for (const auto& query : queries.ordered) {
        if (query.kind == SolveQueries::Query::Kind::Voltage) {
            nodes.push_back(query.first);
            nodes.push_back(query.second);
        }
    }
    return nodes;
}

std::vector<std::string> protected_components_for_queries(
    const SolveQueries& queries) {
    std::vector<std::string> components;
    for (const auto& query : queries.ordered) {
        if (query.kind != SolveQueries::Query::Kind::Voltage) {
            components.push_back(query.first);
        }
    }
    return components;
}

void print_operating_point(const analog::Circuit& circuit,
                           const std::vector<analog::Rewrite>& rules,
                           int iterations,
                           bool explain,
                           const SolveQueries& queries) {
    const auto operating_point = analog::solve_operating_point(circuit);
    const auto& voltages = operating_point.voltages;
    const bool print_all = queries.ordered.empty();

    if (print_all) {
        for (const auto& [node, voltage] : voltages) {
            if (node == "0" || node == "gnd" || node == "GND") {
                continue;
            }
            const auto best = optimize(voltage, rules, iterations, explain,
                                       "V(" + node + ")");
            std::cout << "V " << node << " 0: "
                      << analog::to_result_string(best) << '\n';
        }
    }

    if (print_all) {
        for (const auto& component : circuit.components) {
            if (component.type != analog::ComponentType::Resistor) {
                continue;
            }
            const auto current = resistor_current(component, voltages);
            const auto best = optimize(current, rules, iterations, explain,
                                       "I(" + component.name + ")");
            std::cout << "I " << component.name << ' ' << component.positive
                      << "->" << component.negative << ": "
                      << analog::to_result_string(best) << '\n';
        }
    }

    for (const auto& query : queries.ordered) {
        if (query.kind == SolveQueries::Query::Kind::Voltage) {
            const auto voltage = voltage_between(voltages, query.first, query.second);
            const auto best = optimize(voltage, rules, iterations, explain,
                                       "V(" + query.first + "," + query.second + ")");
            std::cout << "V " << query.first << ' ' << query.second << ": "
                      << analog::to_result_string(best) << '\n';
            continue;
        }

        const auto& component = find_component(circuit, query.first);
        if (query.kind == SolveQueries::Query::Kind::Power) {
            const auto power = component_power(component, operating_point, circuit);
            const auto best = optimize(power, rules, iterations, explain,
                                       "P(" + component.name + ")");
            std::cout << "P " << component.name << ": "
                      << analog::to_result_string(best) << '\n';
        } else if (query.kind == SolveQueries::Query::Kind::SeenResistance) {
            const auto resistance =
                source_seen_resistance(component, operating_point, circuit);
            const auto best = optimize(resistance, rules, iterations, explain,
                                       "Rseen(" + component.name + ")");
            std::cout << "Rseen " << component.name << ": "
                      << analog::to_result_string(best) << '\n';
        } else {
            const auto current = component_current(component, operating_point, circuit);
            const auto best = optimize(current, rules, iterations, explain,
                                       "I(" + component.name + ")");
            std::cout << "I " << component.name << ": "
                      << analog::to_result_string(best) << '\n';
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        int iterations = 12;
        bool explain = false;
        std::vector<std::string> filtered;

        for (std::size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--iters") {
                if (i + 1 >= args.size()) {
                    throw std::runtime_error("--iters requires a value");
                }
                iterations = std::stoi(args[++i]);
            } else if (args[i] == "--explain") {
                explain = true;
            } else {
                filtered.push_back(args[i]);
            }
        }

        if (filtered.empty()) {
            usage(argv[0]);
            return 2;
        }

        const auto rules = analog::analog_rules();

        if (filtered[0] == "--solve-for") {
            if (filtered.size() != 3) {
                usage(argv[0]);
                return 2;
            }

            const auto constraint =
                analog::parse_constraint(analog::parse_expr(filtered[2]));
            solve_and_print_constraint(constraint, filtered[1], rules, iterations,
                                       explain);
            return 0;
        }

        if (filtered[0] == "--solve") {
            if (filtered.size() < 2) {
                usage(argv[0]);
                return 2;
            }

            const auto circuit = analog::parse_netlist_file(filtered[1]);
            const auto queries = parse_solve_queries(filtered, 2);
            analog::Circuit analysis_circuit = circuit;
            if (!queries.ordered.empty()) {
                const auto topology = analog::rewrite_topology(
                    circuit,
                    protected_nodes_for_queries(queries),
                    protected_components_for_queries(queries));
                analysis_circuit = topology.circuit;
                if (explain) {
                    print_topology_summary(topology);
                }
            }
            print_operating_point(analysis_circuit, rules, iterations, explain, queries);
            return 0;
        }

        if (filtered[0] == "--solve-constraint") {
            if (filtered.size() < 3) {
                usage(argv[0]);
                return 2;
            }

            const auto circuit = analog::parse_netlist_file(filtered[1]);
            const std::string variable = filtered[2];
            std::vector<analog::Constraint> constraints;
            constraints.reserve(circuit.constraints.size() + filtered.size() - 3);
            for (const auto& constraint_expr : circuit.constraints) {
                constraints.push_back(analog::parse_constraint(constraint_expr));
            }
            for (std::size_t i = 3; i < filtered.size(); ++i) {
                constraints.push_back(
                    analog::parse_constraint(analog::parse_expr(filtered[i])));
            }
            if (constraints.empty()) {
                throw std::runtime_error(
                    "--solve-constraint requires at least one .constraint directive "
                    "or command-line constraint");
            }

            const auto observables = inline_observables(constraints);

            const auto queries = protection_queries_for_observables(observables);
            analog::Circuit topology_circuit = circuit;
            if (!queries.ordered.empty()) {
                const auto topology = analog::rewrite_topology(
                    circuit,
                    protected_nodes_for_queries(queries),
                    protected_components_for_queries(queries));
                topology_circuit = topology.circuit;
                if (explain) {
                    print_topology_summary(topology);
                }
            }

            const bool needs_operating_point = !queries.ordered.empty();
            analog::OperatingPoint operating_point;
            if (needs_operating_point) {
                operating_point = analog::solve_operating_point(topology_circuit);
            }

            auto lowered = lower_inline_observables(constraints,
                                                    circuit,
                                                    topology_circuit,
                                                    operating_point,
                                                    rules,
                                                    iterations,
                                                    explain);
            if (explain) {
                for (std::size_t i = 0; i < lowered.size(); ++i) {
                    std::cerr << "lowered constraint " << (i + 1) << ": "
                              << analog::to_string(lowered[i]) << '\n';
                }
            }
            solve_and_print_constraints(lowered, variable, rules, iterations,
                                        explain);
            return 0;
        }

        if (filtered[0] == "--rewrite-topology") {
            if (filtered.size() < 2) {
                usage(argv[0]);
                return 2;
            }

            const auto circuit = analog::parse_netlist_file(filtered[1]);
            std::vector<std::string> protected_nodes;
            for (std::size_t i = 2; i < filtered.size(); ++i) {
                protected_nodes.push_back(filtered[i]);
            }
            const auto result = analog::rewrite_topology(circuit, protected_nodes);
            if (explain) {
                print_topology_summary(result);
            }
            std::cout << analog::circuit_to_netlist(result.circuit);
            return 0;
        }

        if (filtered[0] == "--solve-rth-for") {
            if (filtered.size() != 6) {
                usage(argv[0]);
                return 2;
            }

            const auto circuit = analog::parse_netlist_file(filtered[1]);
            const auto topology =
                analog::rewrite_topology(circuit, {filtered[2], filtered[3]});
            if (explain) {
                print_topology_summary(topology);
            }

            const auto thevenin =
                analog::solve_thevenin(topology.circuit, filtered[2], filtered[3]);
            if (explain) {
                std::cerr << "raw Rth: " << analog::to_string(thevenin.rth) << '\n';
            }

            const auto constraint =
                analog::parse_constraint(analog::parse_expr(filtered[5]));
            if (!contains_atom(constraint.lhs, "Rth") &&
                !contains_atom(constraint.rhs, "Rth")) {
                throw std::runtime_error(
                    "--solve-rth-for constraint must reference Rth");
            }
            const auto lowered =
                analog::ConstraintSubstitution(constraint)
                    .substitute("Rth", thevenin.rth)
                    .build();
            if (explain) {
                std::cerr << "lowered constraint: " << analog::to_string(lowered)
                          << '\n';
            }
            solve_and_print_constraint(lowered, filtered[4], rules, iterations,
                                       explain);
            return 0;
        }

        if (filtered[0] == "--thevenin") {
            if (filtered.size() != 4) {
                usage(argv[0]);
                return 2;
            }

            const auto circuit = analog::parse_netlist_file(filtered[1]);
            const auto topology =
                analog::rewrite_topology(circuit, {filtered[2], filtered[3]});
            if (explain) {
                print_topology_summary(topology);
            }

            const auto result =
                analog::solve_thevenin(topology.circuit, filtered[2], filtered[3]);
            if (explain) {
                std::cerr << "raw Vth: " << analog::to_string(result.vth) << '\n';
                std::cerr << "raw Rth: " << analog::to_string(result.rth) << '\n';
            }
            const auto vth = optimize(result.vth, rules, iterations, explain, "Vth");
            const auto rth = optimize(result.rth, rules, iterations, explain, "Rth");

            std::cout << "Vth " << filtered[2] << " " << filtered[3] << ": "
                      << analog::to_result_string(vth) << '\n';
            std::cout << "Rth " << filtered[2] << " " << filtered[3] << ": "
                      << analog::to_result_string(rth) << '\n';
            return 0;
        }

        const std::string input = join(filtered, 0);
        const auto expr = analog::parse_expr(input);
        const auto best = optimize(expr, rules, iterations, explain, "expr");
        std::cout << analog::to_result_string(best) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
