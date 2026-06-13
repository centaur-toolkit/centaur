#include "centaur/analog_rules.h"
#include "centaur/mna.h"
#include "centaur/netlist.h"
#include "centaur/parser.h"
#include "centaur/rewrite.h"
#include "centaur/solve.h"
#include "centaur/topology.h"

#include <exception>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

void usage(const char* program) {
    std::cerr
        << "Usage:\n"
        << "  " << program << " [--iters N] '<expr>'\n"
        << "  " << program << " [--iters N] --solve-for <var> '(eq lhs-expr rhs-expr)'\n"
        << "  " << program << " [--iters N] --solve <netlist.cir> "
        << "[--current name] [--voltage node+ node-] [--power name] "
        << "[--seen-resistance Vname]\n"
        << "  " << program << " --rewrite-topology <netlist.cir> [protected-node ...]\n"
        << "  " << program << " [--iters N] --thevenin <netlist.cir> <node+> <node->\n"
        << "  " << program
        << " [--iters N] --solve-rth-for <netlist.cir> <node+> <node-> <var> "
        << "'(eq Rth target-rth)'\n\n"
        << "Examples:\n"
        << "  " << program << " '(mul Vin (div R2 (add R1 R2)))'\n"
        << "  " << program << " --solve-for R '(eq (par 10000 20000 R) 12000)'\n"
        << "  " << program << " --thevenin examples/voltage_divider.cir out 0\n";
}

void print_topology_summary(const analog::TopologyRewriteResult& topology,
                            const std::string& prefix = "") {
    std::cerr << prefix << "topology iterations: " << topology.iterations << '\n';
    std::cerr << prefix << "topology parallel resistor groups merged: "
              << topology.merged_parallel_resistor_groups << '\n';
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
    std::cerr << prefix << "topology components removed: "
              << topology.removed_components << '\n';
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

std::pair<analog::Expr, analog::Expr> parse_equation(const std::string& query) {
    const auto expr = analog::parse_expr(query);
    if (expr.is_atom() || expr.op != "eq" || expr.args.size() != 2) {
        throw std::runtime_error("equation query must be (eq lhs rhs)");
    }

    return {expr.args[0], expr.args[1]};
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
            std::cout << "V " << node << " 0: " << analog::to_string(best) << '\n';
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
                      << analog::to_string(best) << '\n';
        }
    }

    for (const auto& query : queries.ordered) {
        if (query.kind == SolveQueries::Query::Kind::Voltage) {
            const auto voltage = voltage_between(voltages, query.first, query.second);
            const auto best = optimize(voltage, rules, iterations, explain,
                                       "V(" + query.first + "," + query.second + ")");
            std::cout << "V " << query.first << ' ' << query.second << ": "
                      << analog::to_string(best) << '\n';
            continue;
        }

        const auto& component = find_component(circuit, query.first);
        if (query.kind == SolveQueries::Query::Kind::Power) {
            const auto power = component_power(component, operating_point, circuit);
            const auto best = optimize(power, rules, iterations, explain,
                                       "P(" + component.name + ")");
            std::cout << "P " << component.name << ": " << analog::to_string(best)
                      << '\n';
        } else if (query.kind == SolveQueries::Query::Kind::SeenResistance) {
            const auto resistance =
                source_seen_resistance(component, operating_point, circuit);
            const auto best = optimize(resistance, rules, iterations, explain,
                                       "Rseen(" + component.name + ")");
            std::cout << "Rseen " << component.name << ": "
                      << analog::to_string(best) << '\n';
        } else {
            const auto current = component_current(component, operating_point, circuit);
            const auto best = optimize(current, rules, iterations, explain,
                                       "I(" + component.name + ")");
            std::cout << "I " << component.name << ": " << analog::to_string(best)
                      << '\n';
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

            const auto [lhs, rhs] = parse_equation(filtered[2]);
            const auto solution = analog::solve_for(lhs, rhs, filtered[1]);
            if (!solution.has_value()) {
                throw std::runtime_error("could not isolate variable: " + filtered[1]);
            }
            if (explain) {
                std::cerr << "raw " << filtered[1] << ": "
                          << analog::to_string(*solution) << '\n';
            }
            const auto best = optimize(*solution, rules, iterations, explain,
                                       "solve(" + filtered[1] + ")");
            std::cout << filtered[1] << ": " << analog::to_string(best) << '\n';
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

            const auto [query_lhs, query_rhs] = parse_equation(filtered[5]);
            analog::Expr target;
            if (analog::is_atom_value(query_lhs, "Rth")) {
                target = query_rhs;
            } else if (analog::is_atom_value(query_rhs, "Rth")) {
                target = query_lhs;
            } else {
                throw std::runtime_error(
                    "--solve-rth-for equation must compare Rth with a target");
            }
            const auto solution = analog::solve_for(thevenin.rth, target, filtered[4]);
            if (!solution.has_value()) {
                throw std::runtime_error("could not isolate variable: " + filtered[4]);
            }
            if (explain) {
                std::cerr << "raw " << filtered[4] << ": "
                          << analog::to_string(*solution) << '\n';
            }
            const auto best = optimize(*solution, rules, iterations, explain,
                                       "solve(" + filtered[4] + ")");
            std::cout << filtered[4] << ": " << analog::to_string(best) << '\n';
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
                      << analog::to_string(vth) << '\n';
            std::cout << "Rth " << filtered[2] << " " << filtered[3] << ": "
                      << analog::to_string(rth) << '\n';
            return 0;
        }

        const std::string input = join(filtered, 0);
        const auto expr = analog::parse_expr(input);
        const auto best = optimize(expr, rules, iterations, explain, "expr");
        std::cout << analog::to_string(best) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
