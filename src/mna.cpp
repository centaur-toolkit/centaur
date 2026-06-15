#include "centaur/mna.h"

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace analog {
namespace {

using Matrix = std::vector<std::vector<Expr>>;

bool is_ground(const std::string& node) {
    return node == "0" || node == "gnd" || node == "GND";
}

std::vector<std::string> collect_nodes(const Circuit& circuit) {
    std::vector<std::string> nodes;
    auto add = [&](const std::string& node) {
        if (is_ground(node)) {
            return;
        }
        if (std::find(nodes.begin(), nodes.end(), node) == nodes.end()) {
            nodes.push_back(node);
        }
    };

    for (const auto& component : circuit.components) {
        add(component.positive);
        add(component.negative);
        if (component.type == ComponentType::VoltageControlledVoltageSource ||
            component.type == ComponentType::VoltageControlledCurrentSource) {
            add(component.control_positive);
            add(component.control_negative);
        }
    }
    std::sort(nodes.begin(), nodes.end());
    return nodes;
}

int node_index(const std::map<std::string, int>& nodes, const std::string& node) {
    if (is_ground(node)) {
        return -1;
    }
    auto found = nodes.find(node);
    if (found == nodes.end()) {
        throw std::runtime_error("unknown node: " + node);
    }
    return found->second;
}

void add_to(Expr& slot, Expr value) {
    slot = make_add(std::move(slot), std::move(value));
}

void sub_from(Expr& slot, Expr value) {
    slot = make_sub(std::move(slot), std::move(value));
}

void stamp_conductance(Matrix& a, int p, int n, const Expr& conductance) {
    if (p >= 0) {
        add_to(a[p][p], conductance);
    }
    if (n >= 0) {
        add_to(a[n][n], conductance);
    }
    if (p >= 0 && n >= 0) {
        sub_from(a[p][n], conductance);
        sub_from(a[n][p], conductance);
    }
}

void stamp_current_source(std::vector<Expr>& z, int p, int n, const Expr& current) {
    if (p >= 0) {
        sub_from(z[p], current);
    }
    if (n >= 0) {
        add_to(z[n], current);
    }
}

void stamp_vccs(Matrix& a, int p, int n, int cp, int cn, const Expr& gain) {
    if (p >= 0 && cp >= 0) {
        add_to(a[p][cp], gain);
    }
    if (p >= 0 && cn >= 0) {
        sub_from(a[p][cn], gain);
    }
    if (n >= 0 && cp >= 0) {
        sub_from(a[n][cp], gain);
    }
    if (n >= 0 && cn >= 0) {
        add_to(a[n][cn], gain);
    }
}

void stamp_current_controlled_source(Matrix& a, int p, int n, int control_branch,
                                     const Expr& gain) {
    if (p >= 0) {
        add_to(a[p][control_branch], gain);
    }
    if (n >= 0) {
        sub_from(a[n][control_branch], gain);
    }
}

const Component& find_component(const Circuit& circuit, const std::string& name) {
    for (const auto& component : circuit.components) {
        if (component.name == name) {
            return component;
        }
    }
    throw std::runtime_error("control component not found: " + name);
}

std::vector<Expr> solve_linear(Matrix a, std::vector<Expr> z) {
    const int n = static_cast<int>(a.size());
    if (n == 0) {
        return {};
    }

    for (int col = 0; col < n; ++col) {
        int pivot = -1;
        for (int row = col; row < n; ++row) {
            if (!is_zero(a[row][col])) {
                pivot = row;
                break;
            }
        }
        if (pivot < 0) {
            throw std::runtime_error("singular MNA matrix; check grounding and source topology");
        }
        if (pivot != col) {
            std::swap(a[pivot], a[col]);
            std::swap(z[pivot], z[col]);
        }

        const Expr pivot_value = a[col][col];
        for (int j = col; j < n; ++j) {
            a[col][j] = make_div(std::move(a[col][j]), pivot_value);
        }
        z[col] = make_div(std::move(z[col]), pivot_value);

        for (int row = 0; row < n; ++row) {
            if (row == col || is_zero(a[row][col])) {
                continue;
            }
            const Expr factor = a[row][col];
            for (int j = col; j < n; ++j) {
                a[row][j] = make_sub(std::move(a[row][j]),
                                     make_mul(factor, a[col][j]));
            }
            z[row] = make_sub(std::move(z[row]), make_mul(factor, z[col]));
        }
    }

    return z;
}

struct MnaSolution {
    std::map<std::string, Expr> voltages;
    std::map<std::string, Expr> source_currents;
    std::optional<Expr> test_voltage_current;
};

MnaSolution solve_mna(const Circuit& circuit,
                      bool zero_independent_sources,
                      const std::string* test_positive,
                      const std::string* test_negative,
                      const std::string* test_voltage_positive,
                      const std::string* test_voltage_negative) {
    const auto node_names = collect_nodes(circuit);
    std::map<std::string, int> node_ids;
    for (int i = 0; i < static_cast<int>(node_names.size()); ++i) {
        node_ids[node_names[i]] = i;
    }

    std::vector<const Component*> voltage_sources;
    for (const auto& component : circuit.components) {
        if (component.type == ComponentType::VoltageSource ||
            component.type == ComponentType::VoltageControlledVoltageSource) {
            voltage_sources.push_back(&component);
        }
    }

    const int node_count = static_cast<int>(node_names.size());
    const int voltage_source_count = static_cast<int>(voltage_sources.size());
    const int test_voltage_count =
        (test_voltage_positive != nullptr && test_voltage_negative != nullptr) ? 1 : 0;
    const int dimension = node_count + voltage_source_count + test_voltage_count;

    std::map<std::string, int> voltage_source_branches;
    for (int i = 0; i < voltage_source_count; ++i) {
        voltage_source_branches[voltage_sources[static_cast<std::size_t>(i)]->name] =
            node_count + i;
    }

    Matrix a(static_cast<std::size_t>(dimension),
             std::vector<Expr>(static_cast<std::size_t>(dimension), atom("0")));
    std::vector<Expr> z(static_cast<std::size_t>(dimension), atom("0"));

    auto stamp_cccs = [&](const Component& source, int p, int n) {
        const Component& control = find_component(circuit, source.control_component);
        if (control.type == ComponentType::Resistor) {
            const int cp = node_index(node_ids, control.positive);
            const int cn = node_index(node_ids, control.negative);
            stamp_vccs(a, p, n, cp, cn, make_div(source.value, control.value));
            return;
        }
        if (control.type == ComponentType::CurrentSource) {
            if (!zero_independent_sources) {
                stamp_current_source(z, p, n, make_mul(source.value, control.value));
            }
            return;
        }
        if (control.type == ComponentType::VoltageControlledCurrentSource) {
            const int cp = node_index(node_ids, control.control_positive);
            const int cn = node_index(node_ids, control.control_negative);
            stamp_vccs(a, p, n, cp, cn, make_mul(source.value, control.value));
            return;
        }
        if (control.type == ComponentType::VoltageSource ||
            control.type == ComponentType::VoltageControlledVoltageSource) {
            auto branch = voltage_source_branches.find(control.name);
            if (branch == voltage_source_branches.end()) {
                throw std::runtime_error("control source branch not found: " +
                                         control.name);
            }
            stamp_current_controlled_source(a, p, n, branch->second, source.value);
            return;
        }

        throw std::runtime_error(
            "current-controlled current source cannot be controlled by another "
            "current-controlled current source yet: " +
            source.name);
    };

    for (const auto& component : circuit.components) {
        const int p = node_index(node_ids, component.positive);
        const int n = node_index(node_ids, component.negative);

        if (component.type == ComponentType::Resistor) {
            stamp_conductance(a, p, n, make_inv(component.value));
        } else if (component.type == ComponentType::CurrentSource) {
            if (!zero_independent_sources) {
                stamp_current_source(z, p, n, component.value);
            }
        } else if (component.type == ComponentType::VoltageControlledCurrentSource) {
            const int cp = node_index(node_ids, component.control_positive);
            const int cn = node_index(node_ids, component.control_negative);
            stamp_vccs(a, p, n, cp, cn, component.value);
        } else if (component.type == ComponentType::CurrentControlledCurrentSource) {
            stamp_cccs(component, p, n);
        }
    }

    auto stamp_voltage_source = [&](int p, int n, int branch, const Expr& value) {
        if (p >= 0) {
            a[p][branch] = make_add(std::move(a[p][branch]), atom("1"));
            a[branch][p] = make_add(std::move(a[branch][p]), atom("1"));
        }
        if (n >= 0) {
            a[n][branch] = make_sub(std::move(a[n][branch]), atom("1"));
            a[branch][n] = make_sub(std::move(a[branch][n]), atom("1"));
        }
        z[branch] = value;
    };

    auto stamp_dependent_voltage_source = [&](int p,
                                              int n,
                                              int cp,
                                              int cn,
                                              int branch,
                                              const Expr& gain) {
        stamp_voltage_source(p, n, branch, atom("0"));
        if (cp >= 0) {
            a[branch][cp] = make_sub(std::move(a[branch][cp]), gain);
        }
        if (cn >= 0) {
            a[branch][cn] = make_add(std::move(a[branch][cn]), gain);
        }
    };

    for (int i = 0; i < voltage_source_count; ++i) {
        const Component& source = *voltage_sources[static_cast<std::size_t>(i)];
        const int p = node_index(node_ids, source.positive);
        const int n = node_index(node_ids, source.negative);
        const int branch = node_count + i;
        if (source.type == ComponentType::VoltageSource) {
            stamp_voltage_source(p, n, branch,
                                 zero_independent_sources ? atom("0") : source.value);
        } else {
            const int cp = node_index(node_ids, source.control_positive);
            const int cn = node_index(node_ids, source.control_negative);
            stamp_dependent_voltage_source(p, n, cp, cn, branch, source.value);
        }
    }

    if (test_positive != nullptr && test_negative != nullptr) {
        const int p = node_index(node_ids, *test_positive);
        const int n = node_index(node_ids, *test_negative);
        // A 1 A test source is injected into the positive terminal and removed
        // from the negative terminal, so V(pos)-V(neg) is the equivalent R.
        if (p >= 0) {
            add_to(z[p], atom("1"));
        }
        if (n >= 0) {
            sub_from(z[n], atom("1"));
        }
    }

    std::optional<int> test_branch;
    if (test_voltage_positive != nullptr && test_voltage_negative != nullptr) {
        const int p = node_index(node_ids, *test_voltage_positive);
        const int n = node_index(node_ids, *test_voltage_negative);
        test_branch = node_count + voltage_source_count;
        stamp_voltage_source(p, n, *test_branch, atom("1"));
    }

    const auto solution = solve_linear(std::move(a), std::move(z));

    MnaSolution result;
    result.voltages["0"] = atom("0");
    result.voltages["gnd"] = atom("0");
    result.voltages["GND"] = atom("0");
    for (int i = 0; i < node_count; ++i) {
        result.voltages[node_names[static_cast<std::size_t>(i)]] =
            solution[static_cast<std::size_t>(i)];
    }
    if (test_branch.has_value()) {
        result.test_voltage_current = solution[static_cast<std::size_t>(*test_branch)];
    }
    for (int i = 0; i < voltage_source_count; ++i) {
        const Component& source = *voltage_sources[static_cast<std::size_t>(i)];
        const int branch = node_count + i;
        result.source_currents[source.name] = solution[static_cast<std::size_t>(branch)];
    }
    return result;
}

Expr node_voltage(const std::map<std::string, Expr>& voltages,
                  const std::string& node) {
    if (is_ground(node)) {
        return atom("0");
    }
    auto found = voltages.find(node);
    if (found == voltages.end()) {
        throw std::runtime_error("node not present in solution: " + node);
    }
    return found->second;
}

Expr voltage_between(const std::map<std::string, Expr>& voltages,
                     const std::string& positive,
                     const std::string& negative) {
    return make_sub(node_voltage(voltages, positive),
                    node_voltage(voltages, negative));
}

} // namespace

OperatingPoint solve_operating_point(const Circuit& circuit) {
    const auto solution = solve_mna(circuit, false, nullptr, nullptr, nullptr, nullptr);
    return OperatingPoint{solution.voltages, solution.source_currents};
}

std::map<std::string, Expr> solve_node_voltages(const Circuit& circuit) {
    return solve_operating_point(circuit).voltages;
}

Expr node_voltage(const OperatingPoint& operating_point, const std::string& node) {
    return node_voltage(operating_point.voltages, node);
}

Expr voltage_between(const OperatingPoint& operating_point,
                     const std::string& positive,
                     const std::string& negative) {
    return voltage_between(operating_point.voltages, positive, negative);
}

TheveninResult solve_thevenin(const Circuit& circuit,
                              const std::string& positive,
                              const std::string& negative) {
    const auto open_circuit = solve_mna(circuit, false, nullptr, nullptr, nullptr, nullptr);
    const Expr vth = voltage_between(open_circuit.voltages, positive, negative);

    const auto test_solution =
        solve_mna(circuit, true, nullptr, nullptr, &positive, &negative);
    if (!test_solution.test_voltage_current.has_value()) {
        throw std::runtime_error("internal error: missing test-source current");
    }
    const Expr rth = make_neg(make_inv(*test_solution.test_voltage_current));

    return TheveninResult{vth, rth};
}

} // namespace analog
