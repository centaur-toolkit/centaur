#include "centaur/netlist.h"

#include "centaur/parser.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace analog {
namespace {

std::string trim(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

bool is_comment_or_empty(const std::string& line) {
    const std::string stripped = trim(line);
    return stripped.empty() || stripped.front() == '*' || stripped.front() == '#' ||
           stripped.front() == ';';
}

ComponentType type_from_name(const std::string& name) {
    if (name.empty()) {
        throw std::runtime_error("component without a name");
    }

    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())))) {
    case 'R':
        return ComponentType::Resistor;
    case 'I':
        return ComponentType::CurrentSource;
    case 'V':
        return ComponentType::VoltageSource;
    case 'E':
        return ComponentType::VoltageControlledVoltageSource;
    case 'G':
        return ComponentType::VoltageControlledCurrentSource;
    case 'F':
        return ComponentType::CurrentControlledCurrentSource;
    default:
        throw std::runtime_error("unsupported component type for '" + name +
                                 "'; supported prefixes are R, I, V, E, G, and F");
    }
}

} // namespace

Circuit parse_netlist(const std::string& text) {
    Circuit circuit;
    std::istringstream input(text);
    std::string line;
    int line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        if (is_comment_or_empty(line)) {
            continue;
        }

        std::istringstream fields(line);
        std::string name;
        std::string positive;
        std::string negative;
        std::string control_positive;
        std::string control_negative;
        std::string control_component;
        if (!(fields >> name >> positive >> negative)) {
            throw std::runtime_error("line " + std::to_string(line_number) +
                                     ": expected '<name> <node+> <node-> <value>'");
        }

        const ComponentType type = type_from_name(name);
        if ((type == ComponentType::VoltageControlledVoltageSource ||
             type == ComponentType::VoltageControlledCurrentSource) &&
            !(fields >> control_positive >> control_negative)) {
            throw std::runtime_error("line " + std::to_string(line_number) +
                                     ": expected dependent source as "
                                     "'<name> <node+> <node-> <ctrl+> <ctrl-> <gain>'");
        }
        if (type == ComponentType::CurrentControlledCurrentSource &&
            !(fields >> control_component)) {
            throw std::runtime_error("line " + std::to_string(line_number) +
                                     ": expected current-controlled source as "
                                     "'<name> <node+> <node-> <control-component> <gain>'");
        }

        std::string value_text;
        std::getline(fields, value_text);
        value_text = trim(value_text);
        if (value_text.empty()) {
            throw std::runtime_error("line " + std::to_string(line_number) +
                                     ": missing component value");
        }

        circuit.components.push_back(Component{
            type,
            name,
            positive,
            negative,
            control_positive,
            control_negative,
            control_component,
            parse_expr(value_text),
        });
    }

    return circuit;
}

Circuit parse_netlist_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("could not open netlist file: " + path);
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parse_netlist(buffer.str());
}

std::string circuit_to_netlist(const Circuit& circuit) {
    std::ostringstream out;
    for (const auto& component : circuit.components) {
        out << component.name << ' ' << component.positive << ' ' << component.negative
            << ' ';
        if (component.type == ComponentType::VoltageControlledVoltageSource ||
            component.type == ComponentType::VoltageControlledCurrentSource) {
            out << component.control_positive << ' ' << component.control_negative
                << ' ';
        } else if (component.type == ComponentType::CurrentControlledCurrentSource) {
            out << component.control_component << ' ';
        }
        out << to_string(component.value) << '\n';
    }
    return out.str();
}

} // namespace analog
