#pragma once

#include "centaur/egraph.h"
#include "centaur/expr.h"

#include <map>
#include <string>
#include <vector>

namespace analog {

struct Rewrite {
    std::string name;
    Expr lhs;
    Expr rhs;
};

struct SaturationResult {
    int iterations = 0;
    int applied = 0;
    std::map<std::string, int> rule_counts;
};

struct Extraction {
    Expr expr;
    double cost = 0.0;
};

SaturationResult saturate(EGraph& graph,
                          const std::vector<Rewrite>& rules,
                          int max_iterations);

Extraction extract_best(EGraph& graph, int root_class, int rounds = 48);

Expr optimize_expr(const Expr& expr,
                   const std::vector<Rewrite>& rules,
                   int max_iterations = 12);

} // namespace analog
