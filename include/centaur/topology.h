#pragma once

#include "centaur/netlist.h"

#include <string>
#include <vector>

namespace analog {

struct TopologyRewriteResult {
    Circuit circuit;
    int merged_parallel_resistor_groups = 0;
    int merged_series_resistor_groups = 0;
    int removed_short_resistors = 0;
    int removed_dangling_resistors = 0;
    int removed_current_source_series_resistors = 0;
    int removed_voltage_source_parallel_resistors = 0;
    int removed_components = 0;
    int iterations = 0;
};

TopologyRewriteResult rewrite_parallel_resistors(const Circuit& circuit);
TopologyRewriteResult rewrite_topology(
    const Circuit& circuit,
    const std::vector<std::string>& protected_nodes = {},
    const std::vector<std::string>& protected_components = {});

} // namespace analog
