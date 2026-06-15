#pragma once

#include "centaur/netlist.h"

#include <map>
#include <string>

namespace analog {

struct TheveninResult {
    Expr vth;
    Expr rth;
};

struct OperatingPoint {
    std::map<std::string, Expr> voltages;
    std::map<std::string, Expr> source_currents;
};

OperatingPoint solve_operating_point(const Circuit& circuit);
std::map<std::string, Expr> solve_node_voltages(const Circuit& circuit);
Expr node_voltage(const OperatingPoint& operating_point, const std::string& node);
Expr voltage_between(const OperatingPoint& operating_point,
                     const std::string& positive,
                     const std::string& negative);
TheveninResult solve_thevenin(const Circuit& circuit,
                              const std::string& positive,
                              const std::string& negative);

} // namespace analog
