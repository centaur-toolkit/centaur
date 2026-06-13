#pragma once

#include "centaur/expr.h"

#include <optional>
#include <string>

namespace analog {

std::optional<Expr> solve_for(const Expr& lhs,
                              const Expr& rhs,
                              const std::string& variable);

} // namespace analog
