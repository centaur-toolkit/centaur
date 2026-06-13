#include "centaur/solve.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace analog {
namespace {

bool contains_variable(const Expr& expr, const std::string& variable) {
    if (expr.is_atom()) {
        return expr.op == variable;
    }
    for (const auto& arg : expr.args) {
        if (contains_variable(arg, variable)) {
            return true;
        }
    }
    return false;
}

std::optional<std::size_t> single_variable_arg(const std::vector<Expr>& args,
                                               const std::string& variable) {
    std::optional<std::size_t> found;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!contains_variable(args[i], variable)) {
            continue;
        }
        if (found.has_value()) {
            return std::nullopt;
        }
        found = i;
    }
    return found;
}

Expr sum_except(const std::vector<Expr>& args, std::size_t skip) {
    Expr result = atom("0");
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != skip) {
            result = make_add(std::move(result), args[i]);
        }
    }
    return result;
}

Expr product_except(const std::vector<Expr>& args, std::size_t skip) {
    Expr result = atom("1");
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != skip) {
            result = make_mul(std::move(result), args[i]);
        }
    }
    return result;
}

Expr parallel_target_for_arg(const std::vector<Expr>& args,
                             std::size_t variable_arg,
                             const Expr& target) {
    Expr other_conductance = atom("0");
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != variable_arg) {
            other_conductance =
                make_add(std::move(other_conductance), make_inv(args[i]));
        }
    }
    return make_inv(make_sub(make_inv(target), std::move(other_conductance)));
}

std::optional<Expr> isolate(Expr lhs, Expr rhs, const std::string& variable) {
    lhs = simplify_expr(lhs);
    rhs = simplify_expr(rhs);

    if (lhs.is_atom() && lhs.op == variable) {
        return rhs;
    }

    const bool lhs_contains = contains_variable(lhs, variable);
    const bool rhs_contains = contains_variable(rhs, variable);
    if (!lhs_contains && rhs_contains) {
        return isolate(std::move(rhs), std::move(lhs), variable);
    }
    if (!lhs_contains || lhs.is_atom()) {
        return std::nullopt;
    }

    if (lhs.op == "neg" && lhs.args.size() == 1) {
        return isolate(lhs.args[0], make_neg(std::move(rhs)), variable);
    }
    if (lhs.op == "inv" && lhs.args.size() == 1) {
        return isolate(lhs.args[0], make_inv(std::move(rhs)), variable);
    }
    if (lhs.op == "add") {
        const auto variable_arg = single_variable_arg(lhs.args, variable);
        if (!variable_arg.has_value()) {
            return std::nullopt;
        }
        return isolate(lhs.args[*variable_arg],
                       make_sub(std::move(rhs), sum_except(lhs.args, *variable_arg)),
                       variable);
    }
    if (lhs.op == "mul") {
        const auto variable_arg = single_variable_arg(lhs.args, variable);
        if (!variable_arg.has_value()) {
            return std::nullopt;
        }
        return isolate(lhs.args[*variable_arg],
                       make_div(std::move(rhs),
                                product_except(lhs.args, *variable_arg)),
                       variable);
    }
    if (lhs.op == "sub" && lhs.args.size() == 2) {
        const bool left_contains = contains_variable(lhs.args[0], variable);
        const bool right_contains = contains_variable(lhs.args[1], variable);
        if (left_contains == right_contains) {
            return std::nullopt;
        }
        if (left_contains) {
            return isolate(lhs.args[0], make_add(std::move(rhs), lhs.args[1]),
                           variable);
        }
        return isolate(lhs.args[1], make_sub(lhs.args[0], std::move(rhs)),
                       variable);
    }
    if (lhs.op == "div" && lhs.args.size() == 2) {
        const bool numerator_contains = contains_variable(lhs.args[0], variable);
        const bool denominator_contains = contains_variable(lhs.args[1], variable);
        if (numerator_contains == denominator_contains) {
            return std::nullopt;
        }
        if (numerator_contains) {
            return isolate(lhs.args[0], make_mul(std::move(rhs), lhs.args[1]),
                           variable);
        }
        return isolate(lhs.args[1], make_div(lhs.args[0], std::move(rhs)),
                       variable);
    }
    if (lhs.op == "par") {
        const auto variable_arg = single_variable_arg(lhs.args, variable);
        if (!variable_arg.has_value()) {
            return std::nullopt;
        }
        return isolate(lhs.args[*variable_arg],
                       parallel_target_for_arg(lhs.args, *variable_arg, rhs),
                       variable);
    }

    return std::nullopt;
}

} // namespace

std::optional<Expr> solve_for(const Expr& lhs,
                              const Expr& rhs,
                              const std::string& variable) {
    return isolate(lhs, rhs, variable);
}

} // namespace analog
