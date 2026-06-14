#pragma once

#include <string>
#include <vector>

namespace analog {

struct Expr {
    std::string op;
    std::vector<Expr> args;

    Expr() = default;
    explicit Expr(std::string atom);
    Expr(std::string op, std::vector<Expr> args);

    bool is_atom() const;
};

bool operator==(const Expr& lhs, const Expr& rhs);
bool operator!=(const Expr& lhs, const Expr& rhs);

Expr atom(std::string value);
Expr call(std::string op, std::vector<Expr> args);

bool is_atom_value(const Expr& expr, const std::string& value);
bool is_zero(const Expr& expr);
bool is_one(const Expr& expr);
bool is_commutative(const std::string& op);

std::string to_string(const Expr& expr);
std::string to_result_string(const Expr& expr);

Expr make_add(Expr lhs, Expr rhs);
Expr make_sub(Expr lhs, Expr rhs);
Expr make_mul(Expr lhs, Expr rhs);
Expr make_div(Expr lhs, Expr rhs);
Expr make_neg(Expr value);
Expr make_inv(Expr value);
Expr make_par(std::vector<Expr> values);
Expr simplify_expr(const Expr& expr);

} // namespace analog
