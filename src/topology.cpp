#include "centaur/topology.h"

#include <algorithm>
#include <optional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace analog {
namespace {

struct NodePair {
    std::string first;
    std::string second;

    bool operator<(const NodePair& other) const {
        return std::tie(first, second) < std::tie(other.first, other.second);
    }
};

NodePair unordered_pair_key(const std::string& lhs, const std::string& rhs) {
    if (lhs <= rhs) {
        return NodePair{lhs, rhs};
    }
    return NodePair{rhs, lhs};
}

bool is_ground_node(const std::string& node) {
    return node == "0" || node == "gnd" || node == "GND";
}

bool is_current_source(const Component& component) {
    return component.type == ComponentType::CurrentSource ||
           component.type == ComponentType::VoltageControlledCurrentSource ||
           component.type == ComponentType::CurrentControlledCurrentSource;
}

bool is_voltage_source(const Component& component) {
    return component.type == ComponentType::VoltageSource ||
           component.type == ComponentType::VoltageControlledVoltageSource;
}

Expr parallel_value(const std::vector<const Component*>& resistors) {
    std::vector<Expr> values;
    values.reserve(resistors.size());
    for (const auto* resistor : resistors) {
        values.push_back(resistor->value);
    }
    return call("par", std::move(values));
}

Expr series_value(const Component& first, const Component& second) {
    return make_add(first.value, second.value);
}

std::string other_terminal(const Component& component, const std::string& node) {
    return component.positive == node ? component.negative : component.positive;
}

std::map<std::string, std::vector<std::size_t>> incident_components(
    const Circuit& circuit) {
    std::map<std::string, std::vector<std::size_t>> incident;
    for (std::size_t i = 0; i < circuit.components.size(); ++i) {
        const auto& component = circuit.components[i];
        incident[component.positive].push_back(i);
        if (component.negative != component.positive) {
            incident[component.negative].push_back(i);
        }
    }
    return incident;
}

std::set<std::string> protected_node_set(const Circuit& circuit,
                                         const std::vector<std::string>& nodes) {
    std::set<std::string> protected_nodes(nodes.begin(), nodes.end());
    protected_nodes.insert("0");
    protected_nodes.insert("gnd");
    protected_nodes.insert("GND");
    for (const auto& component : circuit.components) {
        if (component.type == ComponentType::VoltageControlledVoltageSource ||
            component.type == ComponentType::VoltageControlledCurrentSource) {
            protected_nodes.insert(component.control_positive);
            protected_nodes.insert(component.control_negative);
        }
    }
    return protected_nodes;
}

std::set<std::string> protected_component_set(
    const Circuit& circuit,
    const std::vector<std::string>& component_names) {
    std::set<std::string> protected_components(component_names.begin(),
                                               component_names.end());
    for (const auto& component : circuit.components) {
        if (component.type == ComponentType::CurrentControlledCurrentSource &&
            !component.control_component.empty()) {
            protected_components.insert(component.control_component);
        }
    }
    return protected_components;
}

bool is_protected(const std::set<std::string>& protected_nodes,
                  const std::string& node) {
    return is_ground_node(node) || protected_nodes.find(node) != protected_nodes.end();
}

bool is_protected_component(const std::set<std::string>& protected_components,
                            const Component& component) {
    return protected_components.find(component.name) != protected_components.end();
}

void add_counts(TopologyRewriteResult& total, const TopologyRewriteResult& delta) {
    total.merged_parallel_resistor_groups += delta.merged_parallel_resistor_groups;
    total.merged_series_resistor_groups += delta.merged_series_resistor_groups;
    total.removed_short_resistors += delta.removed_short_resistors;
    total.removed_zero_voltage_sources += delta.removed_zero_voltage_sources;
    total.removed_dangling_resistors += delta.removed_dangling_resistors;
    total.removed_current_source_series_resistors +=
        delta.removed_current_source_series_resistors;
    total.removed_voltage_source_parallel_resistors +=
        delta.removed_voltage_source_parallel_resistors;
    total.removed_components += delta.removed_components;
}

TopologyRewriteResult remove_shorted_resistors_once(
    const Circuit& circuit,
    const std::set<std::string>& protected_components) {
    TopologyRewriteResult result;
    for (const auto& component : circuit.components) {
        if (component.type == ComponentType::Resistor &&
            !is_protected_component(protected_components, component) &&
            component.positive == component.negative) {
            ++result.removed_short_resistors;
            ++result.removed_components;
            continue;
        }
        result.circuit.components.push_back(component);
    }
    return result;
}

void replace_node(Component& component,
                  const std::string& old_node,
                  const std::string& new_node) {
    if (component.positive == old_node) {
        component.positive = new_node;
    }
    if (component.negative == old_node) {
        component.negative = new_node;
    }
    if (component.control_positive == old_node) {
        component.control_positive = new_node;
    }
    if (component.control_negative == old_node) {
        component.control_negative = new_node;
    }
}

std::pair<std::string, std::string> zero_source_merge_nodes(
    const Component& source,
    const std::set<std::string>& protected_nodes) {
    const bool positive_protected = is_protected(protected_nodes, source.positive);
    const bool negative_protected = is_protected(protected_nodes, source.negative);

    if (positive_protected && !negative_protected) {
        return {source.positive, source.negative};
    }
    if (negative_protected && !positive_protected) {
        return {source.negative, source.positive};
    }
    if (source.positive <= source.negative) {
        return {source.positive, source.negative};
    }
    return {source.negative, source.positive};
}

TopologyRewriteResult remove_one_zero_voltage_source_once(
    const Circuit& circuit,
    const std::set<std::string>& protected_nodes,
    const std::set<std::string>& protected_components) {
    TopologyRewriteResult result;

    for (std::size_t i = 0; i < circuit.components.size(); ++i) {
        const auto& component = circuit.components[i];
        if (component.type != ComponentType::VoltageSource || !is_zero(component.value) ||
            is_protected_component(protected_components, component)) {
            continue;
        }
        if (is_protected(protected_nodes, component.positive) &&
            is_protected(protected_nodes, component.negative) &&
            component.positive != component.negative) {
            continue;
        }

        const auto [keep_node, replace_node_name] =
            zero_source_merge_nodes(component, protected_nodes);

        for (std::size_t j = 0; j < circuit.components.size(); ++j) {
            if (j == i) {
                ++result.removed_zero_voltage_sources;
                ++result.removed_components;
                continue;
            }
            Component kept = circuit.components[j];
            replace_node(kept, replace_node_name, keep_node);
            result.circuit.components.push_back(std::move(kept));
        }
        return result;
    }

    result.circuit = circuit;
    return result;
}

TopologyRewriteResult remove_dangling_resistors_once(
    const Circuit& circuit,
    const std::set<std::string>& protected_nodes,
    const std::set<std::string>& protected_components) {
    TopologyRewriteResult result;
    const auto incident = incident_components(circuit);
    std::set<std::size_t> removed;

    for (std::size_t i = 0; i < circuit.components.size(); ++i) {
        const auto& component = circuit.components[i];
        if (component.type != ComponentType::Resistor) {
            continue;
        }
        if (is_protected_component(protected_components, component)) {
            continue;
        }

        auto positive = incident.find(component.positive);
        auto negative = incident.find(component.negative);
        const bool positive_leaf =
            positive != incident.end() && positive->second.size() == 1 &&
            !is_protected(protected_nodes, component.positive);
        const bool negative_leaf =
            negative != incident.end() && negative->second.size() == 1 &&
            !is_protected(protected_nodes, component.negative);

        if (positive_leaf || negative_leaf) {
            removed.insert(i);
        }
    }

    for (std::size_t i = 0; i < circuit.components.size(); ++i) {
        if (removed.find(i) != removed.end()) {
            ++result.removed_dangling_resistors;
            ++result.removed_components;
            continue;
        }
        result.circuit.components.push_back(circuit.components[i]);
    }

    return result;
}

TopologyRewriteResult merge_parallel_resistors_once(const Circuit& circuit,
                                                    int first_replacement_id,
                                                    const std::set<std::string>&
                                                        protected_components) {
    std::map<NodePair, std::vector<std::size_t>> resistor_groups;
    for (std::size_t i = 0; i < circuit.components.size(); ++i) {
        const auto& component = circuit.components[i];
        if (component.type == ComponentType::Resistor &&
            !is_protected_component(protected_components, component)) {
            resistor_groups[unordered_pair_key(component.positive, component.negative)]
                .push_back(i);
        }
    }

    std::map<std::size_t, Component> replacements;
    std::set<std::size_t> removed;
    int replacement_id = first_replacement_id;

    for (const auto& [nodes, indices] : resistor_groups) {
        if (indices.size() < 2) {
            continue;
        }

        std::vector<const Component*> group;
        group.reserve(indices.size());
        for (std::size_t index : indices) {
            group.push_back(&circuit.components[index]);
        }

        Component merged = *group.front();
        merged.name = "Rpar" + std::to_string(replacement_id++);
        merged.value = parallel_value(group);
        replacements[indices.front()] = merged;

        for (std::size_t i = 1; i < indices.size(); ++i) {
            removed.insert(indices[i]);
        }
    }

    TopologyRewriteResult result;
    result.merged_parallel_resistor_groups = static_cast<int>(replacements.size());
    result.removed_components = static_cast<int>(removed.size());

    for (std::size_t i = 0; i < circuit.components.size(); ++i) {
        auto replacement = replacements.find(i);
        if (replacement != replacements.end()) {
            result.circuit.components.push_back(replacement->second);
            continue;
        }
        if (removed.find(i) == removed.end()) {
            result.circuit.components.push_back(circuit.components[i]);
        }
    }

    return result;
}

std::optional<std::string> find_series_node(
    const Circuit& circuit,
    const std::set<std::string>& protected_nodes,
    const std::set<std::string>& protected_components) {
    const auto incident = incident_components(circuit);
    for (const auto& [node, indices] : incident) {
        if (is_protected(protected_nodes, node) || indices.size() != 2) {
            continue;
        }
        const auto& first = circuit.components[indices[0]];
        const auto& second = circuit.components[indices[1]];
        if (first.type == ComponentType::Resistor &&
            second.type == ComponentType::Resistor &&
            !is_protected_component(protected_components, first) &&
            !is_protected_component(protected_components, second)) {
            return node;
        }
    }
    return std::nullopt;
}

TopologyRewriteResult merge_one_series_resistor_pair(
    const Circuit& circuit,
    const std::set<std::string>& protected_nodes,
    const std::set<std::string>& protected_components,
    int replacement_id = 1) {
    TopologyRewriteResult result;
    const auto maybe_node = find_series_node(circuit, protected_nodes,
                                             protected_components);
    if (!maybe_node.has_value()) {
        result.circuit = circuit;
        return result;
    }

    const std::string node = *maybe_node;
    const auto incident = incident_components(circuit);
    const auto& indices = incident.at(node);
    const std::size_t first_index = indices[0];
    const std::size_t second_index = indices[1];
    const auto& first = circuit.components[first_index];
    const auto& second = circuit.components[second_index];

    Component merged = first;
    merged.name = "Rser" + std::to_string(replacement_id);
    merged.positive = other_terminal(first, node);
    merged.negative = other_terminal(second, node);
    merged.value = series_value(first, second);

    for (std::size_t i = 0; i < circuit.components.size(); ++i) {
        if (i == first_index) {
            result.circuit.components.push_back(merged);
        } else if (i == second_index) {
            ++result.removed_components;
        } else {
            result.circuit.components.push_back(circuit.components[i]);
        }
    }
    result.merged_series_resistor_groups = 1;
    return result;
}

TopologyRewriteResult remove_one_resistor_in_series_with_current_source(
    const Circuit& circuit,
    const std::set<std::string>& protected_nodes,
    const std::set<std::string>& protected_components) {
    TopologyRewriteResult result;
    const auto incident = incident_components(circuit);

    for (const auto& [node, indices] : incident) {
        if (is_protected(protected_nodes, node) || indices.size() != 2) {
            continue;
        }

        const auto& first = circuit.components[indices[0]];
        const auto& second = circuit.components[indices[1]];
        const bool first_resistor = first.type == ComponentType::Resistor;
        const bool second_resistor = second.type == ComponentType::Resistor;
        const bool first_current_source = is_current_source(first);
        const bool second_current_source = is_current_source(second);
        if (!((first_resistor && second_current_source) ||
              (second_resistor && first_current_source))) {
            continue;
        }

        const std::size_t resistor_index = first_resistor ? indices[0] : indices[1];
        const std::size_t source_index = first_resistor ? indices[1] : indices[0];
        const auto& resistor = circuit.components[resistor_index];
        const auto& source = circuit.components[source_index];
        if (is_protected_component(protected_components, resistor) ||
            is_protected_component(protected_components, source)) {
            continue;
        }

        Component moved_source = source;
        const std::string external_node = other_terminal(resistor, node);
        if (moved_source.positive == node) {
            moved_source.positive = external_node;
        } else {
            moved_source.negative = external_node;
        }

        for (std::size_t i = 0; i < circuit.components.size(); ++i) {
            if (i == source_index) {
                result.circuit.components.push_back(moved_source);
            } else if (i == resistor_index) {
                ++result.removed_components;
            } else {
                result.circuit.components.push_back(circuit.components[i]);
            }
        }
        result.removed_current_source_series_resistors = 1;
        return result;
    }

    result.circuit = circuit;
    return result;
}

TopologyRewriteResult remove_resistors_parallel_with_voltage_sources_once(
    const Circuit& circuit,
    const std::set<std::string>& protected_components) {
    std::set<NodePair> voltage_source_pairs;
    std::set<NodePair> blocked_pairs;
    for (const auto& component : circuit.components) {
        if (!is_voltage_source(component)) {
            continue;
        }

        const auto pair = unordered_pair_key(component.positive, component.negative);
        if (is_protected_component(protected_components, component)) {
            blocked_pairs.insert(pair);
        } else {
            voltage_source_pairs.insert(pair);
        }
    }

    TopologyRewriteResult result;
    for (const auto& component : circuit.components) {
        const auto pair = unordered_pair_key(component.positive, component.negative);
        if (component.type == ComponentType::Resistor &&
            !is_protected_component(protected_components, component) &&
            voltage_source_pairs.find(pair) != voltage_source_pairs.end() &&
            blocked_pairs.find(pair) == blocked_pairs.end()) {
            ++result.removed_voltage_source_parallel_resistors;
            ++result.removed_components;
            continue;
        }
        result.circuit.components.push_back(component);
    }
    return result;
}

} // namespace

TopologyRewriteResult rewrite_parallel_resistors(const Circuit& circuit) {
    return merge_parallel_resistors_once(circuit, 1, {});
}

TopologyRewriteResult rewrite_topology(
    const Circuit& circuit,
    const std::vector<std::string>& protected_nodes,
    const std::vector<std::string>& protected_components) {
    TopologyRewriteResult total;
    total.circuit = circuit;
    const auto protected_node_names = protected_node_set(circuit, protected_nodes);
    const auto protected_component_names =
        protected_component_set(circuit, protected_components);

    for (int iteration = 0; iteration < 64; ++iteration) {
        bool changed = false;

        auto shorted =
            remove_shorted_resistors_once(total.circuit, protected_component_names);
        if (shorted.removed_components > 0) {
            changed = true;
            total.circuit = std::move(shorted.circuit);
            add_counts(total, shorted);
        }

        auto zero_voltage = remove_one_zero_voltage_source_once(
            total.circuit,
            protected_node_names,
            protected_component_names);
        if (zero_voltage.removed_components > 0) {
            changed = true;
            total.circuit = std::move(zero_voltage.circuit);
            add_counts(total, zero_voltage);
        }

        auto voltage_parallel = remove_resistors_parallel_with_voltage_sources_once(
            total.circuit,
            protected_component_names);
        if (voltage_parallel.removed_components > 0) {
            changed = true;
            total.circuit = std::move(voltage_parallel.circuit);
            add_counts(total, voltage_parallel);
        }

        auto current_series = remove_one_resistor_in_series_with_current_source(
            total.circuit,
            protected_node_names,
            protected_component_names);
        if (current_series.removed_components > 0) {
            changed = true;
            total.circuit = std::move(current_series.circuit);
            add_counts(total, current_series);
        }

        auto dangling = remove_dangling_resistors_once(total.circuit,
                                                       protected_node_names,
                                                       protected_component_names);
        if (dangling.removed_components > 0) {
            changed = true;
            total.circuit = std::move(dangling.circuit);
            add_counts(total, dangling);
        }

        auto series = merge_one_series_resistor_pair(
            total.circuit,
            protected_node_names,
            protected_component_names,
            total.merged_series_resistor_groups + 1);
        if (series.merged_series_resistor_groups > 0) {
            changed = true;
            total.circuit = std::move(series.circuit);
            add_counts(total, series);
        }

        auto parallel = merge_parallel_resistors_once(
            total.circuit,
            total.merged_parallel_resistor_groups + 1,
            protected_component_names);
        if (parallel.merged_parallel_resistor_groups > 0) {
            changed = true;
            total.circuit = std::move(parallel.circuit);
            add_counts(total, parallel);
        }

        if (!changed) {
            total.iterations = iteration + 1;
            break;
        }
    }

    if (total.iterations == 0) {
        total.iterations = 64;
    }

    return total;
}

} // namespace analog
