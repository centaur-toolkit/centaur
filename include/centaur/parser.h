#pragma once

#include "centaur/expr.h"

#include <string>

namespace analog {

Expr parse_expr(const std::string& input);

} // namespace analog
