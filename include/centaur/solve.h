#pragma once

#include "centaur/expr.h"

#include <string>
#include <vector>

namespace analog {

enum class RelationOp {
    Equal,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
};

struct Constraint {
    RelationOp op;
    Expr lhs;
    Expr rhs;
};

struct ConstraintSolution {
    RelationOp op;
    Expr value;
};

std::vector<ConstraintSolution> solve_constraint_for(
    const Constraint& constraint,
    const std::string& variable);

std::vector<ConstraintSolution> solve_constraints_for(
    const std::vector<Constraint>& constraints,
    const std::string& variable);

} // namespace analog
