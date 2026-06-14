#pragma once

#include "centaur/expr.h"
#include "centaur/solve.h"

#include <optional>
#include <string>

namespace analog {

std::optional<RelationOp> parse_constraint_op(const std::string& op);
std::string constraint_op_text(RelationOp op);
Constraint parse_constraint(const Expr& expr);
std::string to_string(const Constraint& constraint);

class ConstraintSubstitution {
  public:
    explicit ConstraintSubstitution(Constraint constraint);

    ConstraintSubstitution& substitute(std::string atom_name, Expr replacement);
    Constraint build() const;

  private:
    Constraint constraint_;
};

} // namespace analog
