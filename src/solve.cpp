#include "centaur/solve.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
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

std::optional<double> evaluate_numeric(const Expr& expr,
                                       const std::string& variable,
                                       double variable_value) {
    if (expr.is_atom()) {
        if (expr.op == variable) {
            return variable_value;
        }
        double value = 0.0;
        if (parse_number(expr.op, value)) {
            return value;
        }
        return std::nullopt;
    }

    if (expr.op == "neg" && expr.args.size() == 1) {
        auto value = evaluate_numeric(expr.args[0], variable, variable_value);
        if (!value.has_value()) {
            return std::nullopt;
        }
        return -*value;
    }
    if (expr.op == "inv" && expr.args.size() == 1) {
        auto value = evaluate_numeric(expr.args[0], variable, variable_value);
        if (!value.has_value() || std::abs(*value) < 1e-12) {
            return std::nullopt;
        }
        return 1.0 / *value;
    }
    if (expr.op == "add") {
        double sum = 0.0;
        for (const auto& arg : expr.args) {
            auto value = evaluate_numeric(arg, variable, variable_value);
            if (!value.has_value()) {
                return std::nullopt;
            }
            sum += *value;
        }
        return sum;
    }
    if (expr.op == "mul") {
        double product = 1.0;
        for (const auto& arg : expr.args) {
            auto value = evaluate_numeric(arg, variable, variable_value);
            if (!value.has_value()) {
                return std::nullopt;
            }
            product *= *value;
        }
        return product;
    }
    if (expr.op == "sub" && expr.args.size() == 2) {
        auto lhs = evaluate_numeric(expr.args[0], variable, variable_value);
        auto rhs = evaluate_numeric(expr.args[1], variable, variable_value);
        if (!lhs.has_value() || !rhs.has_value()) {
            return std::nullopt;
        }
        return *lhs - *rhs;
    }
    if (expr.op == "div" && expr.args.size() == 2) {
        auto lhs = evaluate_numeric(expr.args[0], variable, variable_value);
        auto rhs = evaluate_numeric(expr.args[1], variable, variable_value);
        if (!lhs.has_value() || !rhs.has_value() || std::abs(*rhs) < 1e-12) {
            return std::nullopt;
        }
        return *lhs / *rhs;
    }
    if (expr.op == "par") {
        double conductance = 0.0;
        for (const auto& arg : expr.args) {
            auto value = evaluate_numeric(arg, variable, variable_value);
            if (!value.has_value() || std::abs(*value) < 1e-12) {
                return std::nullopt;
            }
            conductance += 1.0 / *value;
        }
        if (std::abs(conductance) < 1e-12) {
            return std::nullopt;
        }
        return 1.0 / conductance;
    }
    if (expr.op == "vdiv" && expr.args.size() == 3) {
        auto input = evaluate_numeric(expr.args[0], variable, variable_value);
        auto top = evaluate_numeric(expr.args[1], variable, variable_value);
        auto bottom = evaluate_numeric(expr.args[2], variable, variable_value);
        if (!input.has_value() || !top.has_value() || !bottom.has_value()) {
            return std::nullopt;
        }
        const double denominator = *top + *bottom;
        if (std::abs(denominator) < 1e-12) {
            return std::nullopt;
        }
        return *input * *bottom / denominator;
    }

    return std::nullopt;
}

bool constraint_holds(RelationOp op, double lhs, double rhs);

struct Polynomial {
    std::vector<double> coefficients;
};

struct RationalPolynomial {
    Polynomial numerator;
    Polynomial denominator;
};

void normalize(Polynomial& polynomial) {
    double scale = 1.0;
    for (const double coefficient_value : polynomial.coefficients) {
        scale = std::max(scale, std::abs(coefficient_value));
    }
    const double tolerance = 1e-10 * scale;
    while (!polynomial.coefficients.empty() &&
           std::abs(polynomial.coefficients.back()) < tolerance) {
        polynomial.coefficients.pop_back();
    }
}

Polynomial constant_poly(double value) {
    Polynomial result{{value}};
    normalize(result);
    return result;
}

Polynomial variable_poly() {
    return Polynomial{{0.0, 1.0}};
}

int degree(const Polynomial& polynomial) {
    return polynomial.coefficients.empty()
               ? -1
               : static_cast<int>(polynomial.coefficients.size()) - 1;
}

double coefficient(const Polynomial& polynomial, std::size_t index) {
    return index < polynomial.coefficients.size() ? polynomial.coefficients[index]
                                                  : 0.0;
}

Polynomial add_poly(const Polynomial& lhs, const Polynomial& rhs) {
    const std::size_t size =
        std::max(lhs.coefficients.size(), rhs.coefficients.size());
    Polynomial result;
    result.coefficients.resize(size, 0.0);
    for (std::size_t i = 0; i < size; ++i) {
        result.coefficients[i] = coefficient(lhs, i) + coefficient(rhs, i);
    }
    normalize(result);
    return result;
}

Polynomial neg_poly(const Polynomial& value) {
    Polynomial result = value;
    for (double& coefficient_value : result.coefficients) {
        coefficient_value = -coefficient_value;
    }
    normalize(result);
    return result;
}

Polynomial sub_poly(const Polynomial& lhs, const Polynomial& rhs) {
    return add_poly(lhs, neg_poly(rhs));
}

Polynomial mul_poly(const Polynomial& lhs, const Polynomial& rhs) {
    if (lhs.coefficients.empty() || rhs.coefficients.empty()) {
        return Polynomial{};
    }

    Polynomial result;
    result.coefficients.resize(lhs.coefficients.size() + rhs.coefficients.size() - 1,
                               0.0);
    for (std::size_t i = 0; i < lhs.coefficients.size(); ++i) {
        for (std::size_t j = 0; j < rhs.coefficients.size(); ++j) {
            result.coefficients[i + j] += lhs.coefficients[i] * rhs.coefficients[j];
        }
    }
    normalize(result);
    return result;
}

double evaluate_poly(const Polynomial& polynomial, double value) {
    double result = 0.0;
    for (auto iter = polynomial.coefficients.rbegin();
         iter != polynomial.coefficients.rend();
         ++iter) {
        result = result * value + *iter;
    }
    return result;
}

Polynomial scale_poly(Polynomial polynomial, double factor) {
    for (double& coefficient_value : polynomial.coefficients) {
        coefficient_value *= factor;
    }
    normalize(polynomial);
    return polynomial;
}

Polynomial monic_poly(Polynomial polynomial) {
    normalize(polynomial);
    if (polynomial.coefficients.empty()) {
        return polynomial;
    }
    const double leading = polynomial.coefficients.back();
    if (std::abs(leading) < 1e-12) {
        return Polynomial{};
    }
    return scale_poly(std::move(polynomial), 1.0 / leading);
}

std::pair<Polynomial, Polynomial> div_mod_poly(Polynomial dividend,
                                               const Polynomial& divisor) {
    normalize(dividend);
    if (divisor.coefficients.empty()) {
        return {Polynomial{}, dividend};
    }

    const int divisor_degree = degree(divisor);
    const double divisor_leading = coefficient(divisor, divisor_degree);
    if (std::abs(divisor_leading) < 1e-12) {
        return {Polynomial{}, dividend};
    }

    Polynomial quotient;
    if (degree(dividend) >= divisor_degree) {
        quotient.coefficients.resize(
            static_cast<std::size_t>(degree(dividend) - divisor_degree + 1),
            0.0);
    }

    while (!dividend.coefficients.empty() &&
           degree(dividend) >= divisor_degree) {
        const int shift = degree(dividend) - divisor_degree;
        const double factor =
            coefficient(dividend, degree(dividend)) / divisor_leading;
        quotient.coefficients[static_cast<std::size_t>(shift)] += factor;
        for (int i = 0; i <= divisor_degree; ++i) {
            dividend.coefficients[static_cast<std::size_t>(i + shift)] -=
                factor * coefficient(divisor, static_cast<std::size_t>(i));
        }
        normalize(dividend);
    }

    normalize(quotient);
    return {quotient, dividend};
}

Polynomial gcd_poly(Polynomial lhs, Polynomial rhs) {
    normalize(lhs);
    normalize(rhs);
    while (!rhs.coefficients.empty()) {
        auto division = div_mod_poly(lhs, rhs);
        lhs = rhs;
        rhs = std::move(division.second);
    }
    return monic_poly(std::move(lhs));
}

RationalPolynomial reduce_rational(RationalPolynomial value) {
    normalize(value.numerator);
    normalize(value.denominator);
    if (value.numerator.coefficients.empty()) {
        return RationalPolynomial{Polynomial{}, constant_poly(1.0)};
    }
    if (value.denominator.coefficients.empty()) {
        return value;
    }

    const auto common = gcd_poly(value.numerator, value.denominator);
    if (degree(common) > 0) {
        value.numerator = div_mod_poly(value.numerator, common).first;
        value.denominator = div_mod_poly(value.denominator, common).first;
    }

    const double denominator_leading =
        coefficient(value.denominator,
                    static_cast<std::size_t>(degree(value.denominator)));
    if (std::abs(denominator_leading) >= 1e-12 &&
        std::abs(denominator_leading - 1.0) >= 1e-12) {
        value.numerator =
            scale_poly(std::move(value.numerator), 1.0 / denominator_leading);
        value.denominator =
            scale_poly(std::move(value.denominator), 1.0 / denominator_leading);
    }

    return value;
}

RationalPolynomial rational_constant(double value) {
    return RationalPolynomial{constant_poly(value), constant_poly(1.0)};
}

RationalPolynomial rational_variable() {
    return RationalPolynomial{variable_poly(), constant_poly(1.0)};
}

RationalPolynomial add_rational(const RationalPolynomial& lhs,
                                const RationalPolynomial& rhs) {
    return reduce_rational(RationalPolynomial{
        add_poly(mul_poly(lhs.numerator, rhs.denominator),
                 mul_poly(rhs.numerator, lhs.denominator)),
        mul_poly(lhs.denominator, rhs.denominator)});
}

RationalPolynomial neg_rational(const RationalPolynomial& value) {
    return reduce_rational(
        RationalPolynomial{neg_poly(value.numerator), value.denominator});
}

RationalPolynomial sub_rational(const RationalPolynomial& lhs,
                                const RationalPolynomial& rhs) {
    return add_rational(lhs, neg_rational(rhs));
}

RationalPolynomial mul_rational(const RationalPolynomial& lhs,
                                const RationalPolynomial& rhs) {
    return reduce_rational(
        RationalPolynomial{mul_poly(lhs.numerator, rhs.numerator),
                           mul_poly(lhs.denominator, rhs.denominator)});
}

std::optional<RationalPolynomial> div_rational(const RationalPolynomial& lhs,
                                               const RationalPolynomial& rhs) {
    if (rhs.numerator.coefficients.empty()) {
        return std::nullopt;
    }
    return reduce_rational(
        RationalPolynomial{mul_poly(lhs.numerator, rhs.denominator),
                           mul_poly(lhs.denominator, rhs.numerator)});
}

std::optional<RationalPolynomial> rational_from_expr(
    const Expr& expr,
    const std::string& variable) {
    if (expr.is_atom()) {
        if (expr.op == variable) {
            return rational_variable();
        }
        double value = 0.0;
        if (parse_number(expr.op, value)) {
            return rational_constant(value);
        }
        return std::nullopt;
    }

    if (expr.op == "neg" && expr.args.size() == 1) {
        auto value = rational_from_expr(expr.args[0], variable);
        if (!value.has_value()) {
            return std::nullopt;
        }
        return neg_rational(*value);
    }
    if (expr.op == "inv" && expr.args.size() == 1) {
        auto value = rational_from_expr(expr.args[0], variable);
        if (!value.has_value()) {
            return std::nullopt;
        }
        return div_rational(rational_constant(1.0), *value);
    }
    if (expr.op == "add") {
        RationalPolynomial result = rational_constant(0.0);
        for (const auto& arg : expr.args) {
            auto value = rational_from_expr(arg, variable);
            if (!value.has_value()) {
                return std::nullopt;
            }
            result = add_rational(result, *value);
        }
        return result;
    }
    if (expr.op == "sub" && expr.args.size() == 2) {
        auto lhs = rational_from_expr(expr.args[0], variable);
        auto rhs = rational_from_expr(expr.args[1], variable);
        if (!lhs.has_value() || !rhs.has_value()) {
            return std::nullopt;
        }
        return sub_rational(*lhs, *rhs);
    }
    if (expr.op == "mul") {
        RationalPolynomial result = rational_constant(1.0);
        for (const auto& arg : expr.args) {
            auto value = rational_from_expr(arg, variable);
            if (!value.has_value()) {
                return std::nullopt;
            }
            result = mul_rational(result, *value);
        }
        return result;
    }
    if (expr.op == "div" && expr.args.size() == 2) {
        auto lhs = rational_from_expr(expr.args[0], variable);
        auto rhs = rational_from_expr(expr.args[1], variable);
        if (!lhs.has_value() || !rhs.has_value()) {
            return std::nullopt;
        }
        return div_rational(*lhs, *rhs);
    }
    if (expr.op == "par") {
        RationalPolynomial conductance = rational_constant(0.0);
        for (const auto& arg : expr.args) {
            auto value = rational_from_expr(arg, variable);
            if (!value.has_value()) {
                return std::nullopt;
            }
            auto inverse = div_rational(rational_constant(1.0), *value);
            if (!inverse.has_value()) {
                return std::nullopt;
            }
            conductance = add_rational(conductance, *inverse);
        }
        return div_rational(rational_constant(1.0), conductance);
    }
    if (expr.op == "vdiv" && expr.args.size() == 3) {
        auto input = rational_from_expr(expr.args[0], variable);
        auto top = rational_from_expr(expr.args[1], variable);
        auto bottom = rational_from_expr(expr.args[2], variable);
        if (!input.has_value() || !top.has_value() || !bottom.has_value()) {
            return std::nullopt;
        }
        const auto numerator = mul_rational(*input, *bottom);
        const auto denominator = add_rational(*top, *bottom);
        return div_rational(numerator, denominator);
    }

    return std::nullopt;
}

std::vector<ConstraintSolution> solve_numeric_equality_roots(
    const Constraint& constraint,
    const std::string& variable) {
    if (constraint.op != RelationOp::Equal) {
        return {};
    }

    const auto lhs = rational_from_expr(constraint.lhs, variable);
    const auto rhs = rational_from_expr(constraint.rhs, variable);
    if (!lhs.has_value() || !rhs.has_value()) {
        return {};
    }

    Polynomial equation =
        sub_poly(mul_poly(lhs->numerator, rhs->denominator),
                 mul_poly(rhs->numerator, lhs->denominator));
    normalize(equation);

    std::vector<double> roots;
    const int polynomial_degree = degree(equation);
    if (polynomial_degree == 1) {
        const double a = coefficient(equation, 1);
        if (std::abs(a) >= 1e-12) {
            roots.push_back(-coefficient(equation, 0) / a);
        }
    } else if (polynomial_degree == 2) {
        const double a = coefficient(equation, 2);
        const double b = coefficient(equation, 1);
        const double c = coefficient(equation, 0);
        const double discriminant = b * b - 4.0 * a * c;
        const double tolerance = 1e-9 * std::max({1.0, std::abs(b * b),
                                                  std::abs(4.0 * a * c)});
        if (std::abs(a) < 1e-12) {
            if (std::abs(b) >= 1e-12) {
                roots.push_back(-c / b);
            }
        } else if (discriminant >= -tolerance) {
            const double root_discriminant =
                std::sqrt(std::max(0.0, discriminant));
            roots.push_back((-b - root_discriminant) / (2.0 * a));
            roots.push_back((-b + root_discriminant) / (2.0 * a));
        }
    } else {
        return {};
    }

    std::sort(roots.begin(), roots.end());
    std::vector<ConstraintSolution> solutions;
    for (double root : roots) {
        const double lhs_denominator = evaluate_poly(lhs->denominator, root);
        const double rhs_denominator = evaluate_poly(rhs->denominator, root);
        if (std::abs(lhs_denominator) < 1e-8 ||
            std::abs(rhs_denominator) < 1e-8) {
            continue;
        }
        const auto lhs_value = evaluate_numeric(constraint.lhs, variable, root);
        const auto rhs_value = evaluate_numeric(constraint.rhs, variable, root);
        if (!lhs_value.has_value() || !rhs_value.has_value() ||
            !constraint_holds(RelationOp::Equal, *lhs_value, *rhs_value)) {
            continue;
        }
        if (!solutions.empty()) {
            double previous = 0.0;
            if (parse_number(solutions.back().value.op, previous) &&
                std::abs(previous - root) < 1e-8) {
                continue;
            }
        }
        solutions.push_back(ConstraintSolution{RelationOp::Equal, numeric_atom(root)});
    }
    return solutions;
}

bool constraint_holds(RelationOp op, double lhs, double rhs) {
    const double tolerance = 1e-9 * std::max({1.0, std::abs(lhs), std::abs(rhs)});
    if (op == RelationOp::Less) {
        return lhs < rhs - tolerance;
    }
    if (op == RelationOp::LessEqual) {
        return lhs <= rhs + tolerance;
    }
    if (op == RelationOp::Greater) {
        return lhs > rhs + tolerance;
    }
    if (op == RelationOp::GreaterEqual) {
        return lhs + tolerance >= rhs;
    }
    return std::abs(lhs - rhs) <= tolerance;
}

bool constraint_satisfied_at(const Constraint& constraint,
                             const std::string& variable,
                             double variable_value) {
    const auto lhs = evaluate_numeric(constraint.lhs, variable, variable_value);
    const auto rhs = evaluate_numeric(constraint.rhs, variable, variable_value);
    if (!lhs.has_value() || !rhs.has_value()) {
        return false;
    }
    return constraint_holds(constraint.op, *lhs, *rhs);
}

struct LinearExpression {
    double constant = 0.0;
    std::map<std::string, double> coefficients;
};

bool has_variable_terms(const LinearExpression& expression) {
    for (const auto& [_, coefficient_value] : expression.coefficients) {
        if (std::abs(coefficient_value) >= 1e-12) {
            return true;
        }
    }
    return false;
}

void add_coefficient(LinearExpression& expression,
                     const std::string& variable,
                     double value) {
    expression.coefficients[variable] += value;
    if (std::abs(expression.coefficients[variable]) < 1e-12) {
        expression.coefficients.erase(variable);
    }
}

LinearExpression scale_linear(LinearExpression expression, double factor) {
    expression.constant *= factor;
    for (auto& [_, coefficient_value] : expression.coefficients) {
        coefficient_value *= factor;
    }
    for (auto iter = expression.coefficients.begin();
         iter != expression.coefficients.end();) {
        if (std::abs(iter->second) < 1e-12) {
            iter = expression.coefficients.erase(iter);
        } else {
            ++iter;
        }
    }
    return expression;
}

LinearExpression add_linear(LinearExpression lhs, const LinearExpression& rhs) {
    lhs.constant += rhs.constant;
    for (const auto& [variable, coefficient_value] : rhs.coefficients) {
        add_coefficient(lhs, variable, coefficient_value);
    }
    return lhs;
}

std::optional<LinearExpression> mul_linear(const LinearExpression& lhs,
                                           const LinearExpression& rhs) {
    const bool lhs_has_variables = has_variable_terms(lhs);
    const bool rhs_has_variables = has_variable_terms(rhs);
    if (lhs_has_variables && rhs_has_variables) {
        return std::nullopt;
    }
    if (lhs_has_variables) {
        return scale_linear(lhs, rhs.constant);
    }
    return scale_linear(rhs, lhs.constant);
}

std::optional<LinearExpression> linear_from_expr(const Expr& expr) {
    if (expr.is_atom()) {
        double value = 0.0;
        if (parse_number(expr.op, value)) {
            return LinearExpression{value, {}};
        }
        return LinearExpression{0.0, {{expr.op, 1.0}}};
    }

    if (expr.op == "neg" && expr.args.size() == 1) {
        auto value = linear_from_expr(expr.args[0]);
        if (!value.has_value()) {
            return std::nullopt;
        }
        return scale_linear(*value, -1.0);
    }
    if (expr.op == "inv" && expr.args.size() == 1) {
        auto value = linear_from_expr(expr.args[0]);
        if (!value.has_value() || has_variable_terms(*value) ||
            std::abs(value->constant) < 1e-12) {
            return std::nullopt;
        }
        return LinearExpression{1.0 / value->constant, {}};
    }
    if (expr.op == "add") {
        LinearExpression result;
        for (const auto& arg : expr.args) {
            auto value = linear_from_expr(arg);
            if (!value.has_value()) {
                return std::nullopt;
            }
            result = add_linear(std::move(result), *value);
        }
        return result;
    }
    if (expr.op == "sub" && expr.args.size() == 2) {
        auto lhs = linear_from_expr(expr.args[0]);
        auto rhs = linear_from_expr(expr.args[1]);
        if (!lhs.has_value() || !rhs.has_value()) {
            return std::nullopt;
        }
        return add_linear(*lhs, scale_linear(*rhs, -1.0));
    }
    if (expr.op == "mul") {
        LinearExpression result{1.0, {}};
        for (const auto& arg : expr.args) {
            auto value = linear_from_expr(arg);
            if (!value.has_value()) {
                return std::nullopt;
            }
            auto product = mul_linear(result, *value);
            if (!product.has_value()) {
                return std::nullopt;
            }
            result = std::move(*product);
        }
        return result;
    }
    if (expr.op == "div" && expr.args.size() == 2) {
        auto numerator = linear_from_expr(expr.args[0]);
        auto denominator = linear_from_expr(expr.args[1]);
        if (!numerator.has_value() || !denominator.has_value() ||
            has_variable_terms(*denominator) ||
            std::abs(denominator->constant) < 1e-12) {
            return std::nullopt;
        }
        return scale_linear(*numerator, 1.0 / denominator->constant);
    }
    if (expr.op == "par") {
        double conductance = 0.0;
        for (const auto& arg : expr.args) {
            auto value = linear_from_expr(arg);
            if (!value.has_value() || has_variable_terms(*value) ||
                std::abs(value->constant) < 1e-12) {
                return std::nullopt;
            }
            conductance += 1.0 / value->constant;
        }
        if (std::abs(conductance) < 1e-12) {
            return std::nullopt;
        }
        return LinearExpression{1.0 / conductance, {}};
    }
    if (expr.op == "vdiv" && expr.args.size() == 3) {
        auto input = linear_from_expr(expr.args[0]);
        auto top = linear_from_expr(expr.args[1]);
        auto bottom = linear_from_expr(expr.args[2]);
        if (!input.has_value() || !top.has_value() || !bottom.has_value() ||
            has_variable_terms(*top) || has_variable_terms(*bottom)) {
            return std::nullopt;
        }
        const double denominator = top->constant + bottom->constant;
        if (std::abs(denominator) < 1e-12) {
            return std::nullopt;
        }
        return scale_linear(*input, bottom->constant / denominator);
    }

    return std::nullopt;
}

std::optional<ConstraintSolution> solve_linear_equalities_for(
    const std::vector<Constraint>& constraints,
    const std::string& variable) {
    std::vector<LinearExpression> equations;
    equations.reserve(constraints.size());
    std::vector<std::string> variables;
    for (const auto& constraint : constraints) {
        if (constraint.op != RelationOp::Equal) {
            return std::nullopt;
        }
        auto lhs = linear_from_expr(constraint.lhs);
        auto rhs = linear_from_expr(constraint.rhs);
        if (!lhs.has_value() || !rhs.has_value()) {
            return std::nullopt;
        }
        auto equation = add_linear(*lhs, scale_linear(*rhs, -1.0));
        for (const auto& [name, _] : equation.coefficients) {
            if (std::find(variables.begin(), variables.end(), name) ==
                variables.end()) {
                variables.push_back(name);
            }
        }
        equations.push_back(std::move(equation));
    }

    if (variables.empty() ||
        std::find(variables.begin(), variables.end(), variable) == variables.end()) {
        return std::nullopt;
    }
    std::sort(variables.begin(), variables.end());

    const std::size_t column_count = variables.size();
    std::vector<std::vector<double>> matrix;
    matrix.reserve(equations.size());
    for (const auto& equation : equations) {
        std::vector<double> row(column_count + 1, 0.0);
        for (std::size_t i = 0; i < column_count; ++i) {
            const auto found = equation.coefficients.find(variables[i]);
            if (found != equation.coefficients.end()) {
                row[i] = found->second;
            }
        }
        row[column_count] = -equation.constant;
        matrix.push_back(std::move(row));
    }

    std::size_t rank = 0;
    std::vector<std::size_t> pivot_columns;
    for (std::size_t column = 0; column < column_count && rank < matrix.size();
         ++column) {
        std::size_t pivot = rank;
        for (std::size_t row = rank + 1; row < matrix.size(); ++row) {
            if (std::abs(matrix[row][column]) >
                std::abs(matrix[pivot][column])) {
                pivot = row;
            }
        }
        if (std::abs(matrix[pivot][column]) < 1e-12) {
            continue;
        }
        std::swap(matrix[rank], matrix[pivot]);

        const double pivot_value = matrix[rank][column];
        for (std::size_t col = column; col <= column_count; ++col) {
            matrix[rank][col] /= pivot_value;
        }
        for (std::size_t row = 0; row < matrix.size(); ++row) {
            if (row == rank) {
                continue;
            }
            const double factor = matrix[row][column];
            if (std::abs(factor) < 1e-12) {
                continue;
            }
            for (std::size_t col = column; col <= column_count; ++col) {
                matrix[row][col] -= factor * matrix[rank][col];
            }
        }

        pivot_columns.push_back(column);
        ++rank;
    }

    for (const auto& row : matrix) {
        bool all_zero = true;
        for (std::size_t column = 0; column < column_count; ++column) {
            if (std::abs(row[column]) >= 1e-10) {
                all_zero = false;
                break;
            }
        }
        if (all_zero && std::abs(row[column_count]) >= 1e-10) {
            return std::nullopt;
        }
    }
    if (rank < column_count) {
        return std::nullopt;
    }

    const auto variable_column =
        static_cast<std::size_t>(std::find(variables.begin(), variables.end(),
                                           variable) -
                                 variables.begin());
    for (std::size_t row = 0; row < pivot_columns.size(); ++row) {
        if (pivot_columns[row] == variable_column) {
            return ConstraintSolution{RelationOp::Equal,
                                      numeric_atom(matrix[row][column_count])};
        }
    }
    return std::nullopt;
}

std::optional<double> solution_numeric_value(const ConstraintSolution& solution,
                                             const std::string& variable) {
    return evaluate_numeric(solution.value, variable, 0.0);
}

bool is_lower_bound(RelationOp op) {
    return op == RelationOp::Greater || op == RelationOp::GreaterEqual;
}

bool is_upper_bound(RelationOp op) {
    return op == RelationOp::Less || op == RelationOp::LessEqual;
}

bool is_strict_bound(RelationOp op) {
    return op == RelationOp::Less || op == RelationOp::Greater;
}

struct NumericBound {
    bool has_value = false;
    double value = 0.0;
    bool strict = false;
};

void merge_lower_bound(NumericBound& bound, double value, bool strict) {
    const double tolerance =
        1e-9 * std::max({1.0, std::abs(bound.value), std::abs(value)});
    if (!bound.has_value || value > bound.value + tolerance) {
        bound.has_value = true;
        bound.value = value;
        bound.strict = strict;
    } else if (std::abs(value - bound.value) <= tolerance) {
        bound.strict = bound.strict || strict;
    }
}

void merge_upper_bound(NumericBound& bound, double value, bool strict) {
    const double tolerance =
        1e-9 * std::max({1.0, std::abs(bound.value), std::abs(value)});
    if (!bound.has_value || value < bound.value - tolerance) {
        bound.has_value = true;
        bound.value = value;
        bound.strict = strict;
    } else if (std::abs(value - bound.value) <= tolerance) {
        bound.strict = bound.strict || strict;
    }
}

bool bounds_conflict(const NumericBound& lower, const NumericBound& upper) {
    if (!lower.has_value || !upper.has_value) {
        return false;
    }
    const double tolerance =
        1e-9 * std::max({1.0, std::abs(lower.value), std::abs(upper.value)});
    if (lower.value > upper.value + tolerance) {
        return true;
    }
    return std::abs(lower.value - upper.value) <= tolerance &&
           (lower.strict || upper.strict);
}

RelationOp bound_op(bool lower_values_hold, bool strict) {
    if (lower_values_hold) {
        return strict ? RelationOp::Less : RelationOp::LessEqual;
    }
    return strict ? RelationOp::Greater : RelationOp::GreaterEqual;
}

std::optional<RelationOp> infer_bound_op(const Constraint& constraint,
                                         const std::string& variable,
                                         const Expr& boundary) {
    if (contains_variable(boundary, variable)) {
        return std::nullopt;
    }

    const auto maybe_boundary = evaluate_numeric(boundary, variable, 0.0);
    if (!maybe_boundary.has_value()) {
        return std::nullopt;
    }
    const double value = *maybe_boundary;

    for (double scale : {1e-3, 1e-2, 1e-1, 1.0}) {
        const double delta = std::max(1e-3, std::abs(value) * scale);
        const double lower_value = value - delta;
        const double upper_value = value + delta;
        const auto lower_lhs =
            evaluate_numeric(constraint.lhs, variable, lower_value);
        const auto lower_rhs =
            evaluate_numeric(constraint.rhs, variable, lower_value);
        const auto upper_lhs =
            evaluate_numeric(constraint.lhs, variable, upper_value);
        const auto upper_rhs =
            evaluate_numeric(constraint.rhs, variable, upper_value);
        if (!lower_lhs.has_value() || !lower_rhs.has_value() ||
            !upper_lhs.has_value() || !upper_rhs.has_value()) {
            continue;
        }

        const bool lower_holds =
            constraint_holds(constraint.op, *lower_lhs, *lower_rhs);
        const bool upper_holds =
            constraint_holds(constraint.op, *upper_lhs, *upper_rhs);
        if (lower_holds != upper_holds) {
            const bool strict = constraint.op == RelationOp::Less ||
                                constraint.op == RelationOp::Greater;
            return bound_op(lower_holds, strict);
        }
    }

    return std::nullopt;
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

std::vector<ConstraintSolution> solve_constraint_for(const Constraint& constraint,
                                                     const std::string& variable) {
    auto boundary = isolate(constraint.lhs, constraint.rhs, variable);
    if (!boundary.has_value()) {
        return solve_numeric_equality_roots(constraint, variable);
    }
    boundary = simplify_expr(*boundary);

    if (constraint.op == RelationOp::Equal) {
        return {ConstraintSolution{RelationOp::Equal, *boundary}};
    }

    const auto bound_op = infer_bound_op(constraint, variable, *boundary);
    if (!bound_op.has_value()) {
        return {};
    }
    return {ConstraintSolution{*bound_op, *boundary}};
}

std::vector<ConstraintSolution> solve_constraints_for(
    const std::vector<Constraint>& constraints,
    const std::string& variable) {
    if (constraints.empty()) {
        return {};
    }
    if (constraints.size() == 1) {
        return solve_constraint_for(constraints.front(), variable);
    }
    if (const auto linear_solution =
            solve_linear_equalities_for(constraints, variable)) {
        return {*linear_solution};
    }

    std::vector<Constraint> active_constraints;
    active_constraints.reserve(constraints.size());
    for (const auto& constraint : constraints) {
        const bool constant_constraint =
            !contains_variable(constraint.lhs, variable) &&
            !contains_variable(constraint.rhs, variable);
        if (!constant_constraint) {
            active_constraints.push_back(constraint);
            continue;
        }
        if (!constraint_satisfied_at(constraint, variable, 0.0)) {
            return {};
        }
    }
    if (active_constraints.empty()) {
        return {};
    }
    if (active_constraints.size() == 1) {
        return solve_constraint_for(active_constraints.front(), variable);
    }

    std::vector<ConstraintSolution> equality_candidates;
    std::vector<std::vector<ConstraintSolution>> solved_constraints;
    solved_constraints.reserve(active_constraints.size());
    for (const auto& constraint : active_constraints) {
        auto solutions = solve_constraint_for(constraint, variable);
        if (solutions.empty()) {
            return {};
        }
        if (equality_candidates.empty()) {
            for (const auto& solution : solutions) {
                if (solution.op == RelationOp::Equal) {
                    equality_candidates.push_back(solution);
                }
            }
        }
        solved_constraints.push_back(std::move(solutions));
    }

    if (!equality_candidates.empty()) {
        std::vector<ConstraintSolution> solutions;
        for (const auto& candidate : equality_candidates) {
            const auto value = solution_numeric_value(candidate, variable);
            if (!value.has_value()) {
                continue;
            }
            bool satisfies_all_constraints = true;
            for (const auto& constraint : active_constraints) {
                if (!constraint_satisfied_at(constraint, variable, *value)) {
                    satisfies_all_constraints = false;
                    break;
                }
            }
            if (!satisfies_all_constraints) {
                continue;
            }
            bool duplicate = false;
            for (const auto& solution : solutions) {
                const auto existing = solution_numeric_value(solution, variable);
                if (existing.has_value() && std::abs(*existing - *value) < 1e-8) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                solutions.push_back(
                    ConstraintSolution{RelationOp::Equal, numeric_atom(*value)});
            }
        }
        return solutions;
    }

    NumericBound lower;
    NumericBound upper;
    std::vector<ConstraintSolution> symbolic_bounds;
    for (const auto& solutions : solved_constraints) {
        for (const auto& solution : solutions) {
            const auto value = solution_numeric_value(solution, variable);
            if (!value.has_value()) {
                symbolic_bounds.push_back(solution);
                continue;
            }
            if (is_lower_bound(solution.op)) {
                merge_lower_bound(lower, *value, is_strict_bound(solution.op));
            } else if (is_upper_bound(solution.op)) {
                merge_upper_bound(upper, *value, is_strict_bound(solution.op));
            }
        }
    }

    if (bounds_conflict(lower, upper)) {
        return {};
    }

    std::vector<ConstraintSolution> solutions = std::move(symbolic_bounds);
    if (lower.has_value && upper.has_value) {
        const double tolerance =
            1e-9 * std::max({1.0, std::abs(lower.value), std::abs(upper.value)});
        if (std::abs(lower.value - upper.value) <= tolerance) {
            solutions.push_back(
                ConstraintSolution{RelationOp::Equal, numeric_atom(lower.value)});
            return solutions;
        }
    }
    if (lower.has_value) {
        solutions.push_back(
            ConstraintSolution{lower.strict ? RelationOp::Greater
                                            : RelationOp::GreaterEqual,
                               numeric_atom(lower.value)});
    }
    if (upper.has_value) {
        solutions.push_back(
            ConstraintSolution{upper.strict ? RelationOp::Less : RelationOp::LessEqual,
                               numeric_atom(upper.value)});
    }
    return solutions;
}

} // namespace analog
