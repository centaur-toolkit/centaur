#include "centaur/expr.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <tuple>
#include <utility>

namespace analog {
namespace {

bool is_unary(const Expr& expr, const std::string& op) {
    return !expr.is_atom() && expr.op == op && expr.args.size() == 1;
}

bool is_binary(const Expr& expr, const std::string& op) {
    return !expr.is_atom() && expr.op == op && expr.args.size() == 2;
}

bool is_negation_of(const Expr& expr, const Expr& value) {
    return is_unary(expr, "neg") && expr.args[0] == value;
}

bool is_inverse_of(const Expr& expr, const Expr& value) {
    return is_unary(expr, "inv") && expr.args[0] == value;
}

bool parse_number(const std::string& text, double& value) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    value = std::strtod(text.c_str(), &end);
    return errno == 0 && end == text.c_str() + text.size();
}

bool numeric_value(const Expr& expr, double& value) {
    return expr.is_atom() && parse_number(expr.op, value);
}

std::string format_number(double value) {
    if (std::abs(value) < 1e-12) {
        value = 0.0;
    }
    const double rounded = std::round(value);
    if (std::abs(value - rounded) < 1e-8) {
        value = rounded;
    }
    std::ostringstream out;
    out << std::setprecision(12) << value;
    return out.str();
}

Expr numeric_atom(double value) {
    return atom(format_number(value));
}

void collect_add_terms(Expr expr, std::vector<Expr>& terms) {
    if (!expr.is_atom() && expr.op == "add") {
        for (auto& arg : expr.args) {
            collect_add_terms(std::move(arg), terms);
        }
        return;
    }
    terms.push_back(std::move(expr));
}

Expr apply_sign(bool negative, Expr value) {
    return negative ? make_neg(std::move(value)) : std::move(value);
}

bool display_less(const std::string& lhs, const std::string& rhs) {
    const bool lhs_compound = !lhs.empty() && lhs.front() == '(';
    const bool rhs_compound = !rhs.empty() && rhs.front() == '(';
    return std::tuple(lhs_compound, lhs) < std::tuple(rhs_compound, rhs);
}

} // namespace

Expr::Expr(std::string atom_value) : op(std::move(atom_value)) {}

Expr::Expr(std::string op_value, std::vector<Expr> children)
    : op(std::move(op_value)), args(std::move(children)) {}

bool Expr::is_atom() const {
    return args.empty();
}

bool operator==(const Expr& lhs, const Expr& rhs) {
    return lhs.op == rhs.op && lhs.args == rhs.args;
}

bool operator!=(const Expr& lhs, const Expr& rhs) {
    return !(lhs == rhs);
}

Expr atom(std::string value) {
    return Expr(std::move(value));
}

Expr call(std::string op, std::vector<Expr> args) {
    return Expr(std::move(op), std::move(args));
}

bool is_atom_value(const Expr& expr, const std::string& value) {
    return expr.is_atom() && expr.op == value;
}

bool is_zero(const Expr& expr) {
    double value = 0.0;
    if (numeric_value(expr, value)) {
        return std::abs(value) < 1e-12;
    }
    return is_atom_value(expr, "0");
}

bool is_one(const Expr& expr) {
    double value = 0.0;
    if (numeric_value(expr, value)) {
        return std::abs(value - 1.0) < 1e-12;
    }
    return is_atom_value(expr, "1");
}

bool is_commutative(const std::string& op) {
    return op == "add" || op == "mul" || op == "par" || op == "ser";
}

std::string to_string(const Expr& expr) {
    if (expr.is_atom()) {
        return expr.op;
    }

    std::vector<std::string> rendered_args;
    rendered_args.reserve(expr.args.size());
    for (const auto& arg : expr.args) {
        rendered_args.push_back(to_string(arg));
    }
    if (is_commutative(expr.op)) {
        std::sort(rendered_args.begin(), rendered_args.end(), display_less);
    }

    std::ostringstream out;
    out << '(' << expr.op;
    for (const auto& arg : rendered_args) {
        out << ' ' << arg;
    }
    out << ')';
    return out.str();
}

Expr make_add(Expr lhs, Expr rhs) {
    std::vector<Expr> terms;
    collect_add_terms(std::move(lhs), terms);
    collect_add_terms(std::move(rhs), terms);

    std::vector<Expr> kept;
    double numeric_sum = 0.0;
    for (auto& term : terms) {
        if (is_zero(term)) {
            continue;
        }

        double value = 0.0;
        if (numeric_value(term, value)) {
            numeric_sum += value;
            continue;
        }
        if (is_unary(term, "neg") && numeric_value(term.args[0], value)) {
            numeric_sum -= value;
            continue;
        }

        bool cancelled = false;
        for (auto iter = kept.begin(); iter != kept.end(); ++iter) {
            if (is_negation_of(term, *iter) || is_negation_of(*iter, term)) {
                kept.erase(iter);
                cancelled = true;
                break;
            }
        }
        if (!cancelled) {
            kept.push_back(std::move(term));
        }
    }
    if (std::abs(numeric_sum) >= 1e-12) {
        kept.push_back(numeric_atom(numeric_sum));
    }

    if (kept.empty()) {
        return atom("0");
    }
    if (kept.size() == 1) {
        return std::move(kept.front());
    }

    bool all_negative = true;
    std::vector<Expr> positive_terms;
    positive_terms.reserve(kept.size());
    for (const auto& term : kept) {
        if (!is_unary(term, "neg")) {
            all_negative = false;
            break;
        }
        positive_terms.push_back(term.args[0]);
    }
    if (all_negative) {
        Expr positive_sum = std::move(positive_terms[0]);
        for (std::size_t i = 1; i < positive_terms.size(); ++i) {
            positive_sum = make_add(std::move(positive_sum), std::move(positive_terms[i]));
        }
        return make_neg(std::move(positive_sum));
    }

    Expr result = call("add", {std::move(kept[0]), std::move(kept[1])});
    for (std::size_t i = 2; i < kept.size(); ++i) {
        result = call("add", {std::move(result), std::move(kept[i])});
    }
    return result;
}

Expr make_sub(Expr lhs, Expr rhs) {
    if (is_zero(rhs)) {
        return lhs;
    }
    if (is_zero(lhs)) {
        return make_neg(std::move(rhs));
    }
    if (lhs == rhs) {
        return atom("0");
    }
    return make_add(std::move(lhs), make_neg(std::move(rhs)));
}

Expr make_mul(Expr lhs, Expr rhs) {
    bool negative = false;
    if (is_unary(lhs, "neg")) {
        negative = !negative;
        Expr inner = lhs.args[0];
        lhs = std::move(inner);
    }
    if (is_unary(rhs, "neg")) {
        negative = !negative;
        Expr inner = rhs.args[0];
        rhs = std::move(inner);
    }

    if (is_zero(lhs) || is_zero(rhs)) {
        return atom("0");
    }

    double left_value = 0.0;
    double right_value = 0.0;
    if (numeric_value(lhs, left_value) && numeric_value(rhs, right_value)) {
        return apply_sign(negative, numeric_atom(left_value * right_value));
    }
    if (numeric_value(lhs, left_value) && std::abs(left_value + 1.0) < 1e-12) {
        return apply_sign(!negative, std::move(rhs));
    }
    if (numeric_value(rhs, right_value) && std::abs(right_value + 1.0) < 1e-12) {
        return apply_sign(!negative, std::move(lhs));
    }

    if (is_one(lhs)) {
        return apply_sign(negative, std::move(rhs));
    }
    if (is_one(rhs)) {
        return apply_sign(negative, std::move(lhs));
    }
    if (is_inverse_of(lhs, rhs) || is_inverse_of(rhs, lhs)) {
        return apply_sign(negative, atom("1"));
    }
    if (is_binary(lhs, "div") && lhs.args[1] == rhs) {
        return apply_sign(negative, lhs.args[0]);
    }
    if (is_binary(rhs, "div") && rhs.args[1] == lhs) {
        return apply_sign(negative, rhs.args[0]);
    }
    return apply_sign(negative, call("mul", {std::move(lhs), std::move(rhs)}));
}

Expr make_div(Expr lhs, Expr rhs) {
    bool negative = false;
    if (is_unary(lhs, "neg")) {
        negative = !negative;
        Expr inner = lhs.args[0];
        lhs = std::move(inner);
    }
    if (is_unary(rhs, "neg")) {
        negative = !negative;
        Expr inner = rhs.args[0];
        rhs = std::move(inner);
    }

    if (is_zero(lhs)) {
        return atom("0");
    }

    double left_value = 0.0;
    double right_value = 0.0;
    if (numeric_value(lhs, left_value) && numeric_value(rhs, right_value) &&
        std::abs(right_value) >= 1e-12) {
        return apply_sign(negative, numeric_atom(left_value / right_value));
    }
    if (numeric_value(lhs, left_value) && std::abs(left_value - 1.0) < 1e-12) {
        return apply_sign(negative, make_inv(std::move(rhs)));
    }
    if (numeric_value(lhs, left_value) && std::abs(left_value + 1.0) < 1e-12) {
        return apply_sign(!negative, make_inv(std::move(rhs)));
    }

    if (is_one(rhs)) {
        return apply_sign(negative, std::move(lhs));
    }
    if (lhs == rhs) {
        return apply_sign(negative, atom("1"));
    }
    if (is_unary(rhs, "inv")) {
        return apply_sign(negative, make_mul(std::move(lhs), rhs.args[0]));
    }
    if (is_binary(lhs, "mul")) {
        if (lhs.args[0] == rhs) {
            return apply_sign(negative, lhs.args[1]);
        }
        if (lhs.args[1] == rhs) {
            return apply_sign(negative, lhs.args[0]);
        }
    }
    return apply_sign(negative, call("div", {std::move(lhs), std::move(rhs)}));
}

Expr make_neg(Expr value) {
    if (is_zero(value)) {
        return atom("0");
    }
    double numeric = 0.0;
    if (numeric_value(value, numeric)) {
        return numeric_atom(-numeric);
    }
    if (!value.is_atom() && value.op == "neg" && value.args.size() == 1) {
        return value.args[0];
    }
    return call("neg", {std::move(value)});
}

Expr make_inv(Expr value) {
    if (is_one(value)) {
        return atom("1");
    }
    double numeric = 0.0;
    if (numeric_value(value, numeric) && std::abs(numeric) >= 1e-12) {
        return numeric_atom(1.0 / numeric);
    }
    if (!value.is_atom() && value.op == "neg" && value.args.size() == 1) {
        return make_neg(make_inv(value.args[0]));
    }
    if (!value.is_atom() && value.op == "inv" && value.args.size() == 1) {
        return value.args[0];
    }
    return call("inv", {std::move(value)});
}

Expr simplify_expr(const Expr& expr) {
    if (expr.is_atom()) {
        return expr;
    }

    std::vector<Expr> args;
    args.reserve(expr.args.size());
    for (const auto& arg : expr.args) {
        args.push_back(simplify_expr(arg));
    }

    if (expr.op == "add" && args.size() == 2) {
        return make_add(std::move(args[0]), std::move(args[1]));
    }
    if (expr.op == "mul" && args.size() == 2) {
        return make_mul(std::move(args[0]), std::move(args[1]));
    }
    if (expr.op == "div" && args.size() == 2) {
        return make_div(std::move(args[0]), std::move(args[1]));
    }
    if (expr.op == "neg" && args.size() == 1) {
        return make_neg(std::move(args[0]));
    }
    if (expr.op == "inv" && args.size() == 1) {
        return make_inv(std::move(args[0]));
    }

    return call(expr.op, std::move(args));
}

} // namespace analog
