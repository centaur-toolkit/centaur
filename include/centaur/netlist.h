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

enum class ObservableKind {
    Current,
    Voltage,
    Power,
    Rth,
};

struct ObservableRequest {
    ObservableKind kind;
    std::string first;
    std::string second;
};

struct Circuit {
    std::vector<Component> components;
    std::vector<Expr> constraints;
};

Circuit parse_netlist(const std::string& text);
Circuit parse_netlist_file(const std::string& path);
std::string circuit_to_netlist(const Circuit& circuit);

} // namespace analog
