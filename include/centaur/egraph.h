#pragma once

#include "centaur/expr.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace analog {

struct ENode {
    std::string op;
    std::vector<int> children;
};

struct EClass {
    int parent = 0;
    int rank = 0;
    std::vector<ENode> nodes;
};

class EGraph {
public:
    int add(const Expr& expr);
    int add_enode(const std::string& op, std::vector<int> children);

    int find(int id);
    bool unite(int lhs, int rhs);
    void rebuild();

    std::vector<int> representatives();
    const std::vector<ENode>& nodes(int class_id);
    std::size_t class_count();

private:
    std::vector<EClass> classes_;
    std::unordered_map<std::string, int> hashcons_;

    ENode canonicalize(ENode node);
    std::string key_for(const ENode& node) const;
};

} // namespace analog
