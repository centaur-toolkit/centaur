#pragma once

#include "centaur/expr.h"

#include <string>
#include <vector>

namespace analog {

enum class ComponentType {
    Resistor,
    CurrentSource,
    VoltageSource,
    VoltageControlledVoltageSource,
    VoltageControlledCurrentSource,
    CurrentControlledCurrentSource,
};

struct Component {
    ComponentType type;
    std::string name;
    std::string positive;
    std::string negative;
    std::string control_positive;
    std::string control_negative;
    std::string control_component;
    Expr value;
};

struct Circuit {
    std::vector<Component> components;
};

Circuit parse_netlist(const std::string& text);
Circuit parse_netlist_file(const std::string& path);
std::string circuit_to_netlist(const Circuit& circuit);

} // namespace analog
