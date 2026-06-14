#include "centaur/constraint.h"

#include <stdexcept>
#include <utility>
#include <vector>

namespace analog {
namespace {

Expr substitute_atom(const Expr& expr,
                     const std::string& name,
                     const Expr& replacement) {
    if (expr.is_atom()) {
        return expr.op == name ? replacement : expr;
    }

    std::vector<Expr> args;
    args.reserve(expr.args.size());
    for (const auto& arg : expr.args) {
        args.push_back(substitute_atom(arg, name, replacement));
    }
    return call(expr.op, std::move(args));
}

} // namespace

std::optional<RelationOp> parse_constraint_op(const std::string& op) {
    if (op == "eq" || op == "=") {
        return RelationOp::Equal;
    }
    if (op == "lt" || op == "<") {
        return RelationOp::Less;
    }
    if (op == "le" || op == "lte" || op == "<=") {
        return RelationOp::LessEqual;
    }
    if (op == "gt" || op == ">") {
        return RelationOp::Greater;
    }
    if (op == "ge" || op == "gte" || op == ">=") {
        return RelationOp::GreaterEqual;
    }
    return std::nullopt;
}

std::string constraint_op_text(RelationOp op) {
    if (op == RelationOp::Less) {
        return "<";
    }
    if (op == RelationOp::LessEqual) {
        return "<=";
    }
    if (op == RelationOp::Greater) {
        return ">";
    }
    if (op == RelationOp::GreaterEqual) {
        return ">=";
    }
    return "=";
}

Constraint parse_constraint(const Expr& expr) {
    if (expr.is_atom() || expr.args.size() != 2) {
        throw std::runtime_error("constraint must be (eq|lt|le|gt|ge lhs rhs)");
    }

    const auto op = parse_constraint_op(expr.op);
    if (!op.has_value()) {
        throw std::runtime_error("constraint must be (eq|lt|le|gt|ge lhs rhs)");
    }

    return Constraint{*op, expr.args[0], expr.args[1]};
}

std::string to_string(const Constraint& constraint) {
    return "(" + constraint_op_text(constraint.op) + " " +
           to_string(constraint.lhs) + " " + to_string(constraint.rhs) + ")";
}

ConstraintSubstitution::ConstraintSubstitution(Constraint constraint)
    : constraint_(std::move(constraint)) {}

ConstraintSubstitution& ConstraintSubstitution::substitute(std::string atom_name,
                                                           Expr replacement) {
    constraint_.lhs = substitute_atom(constraint_.lhs, atom_name, replacement);
    constraint_.rhs = substitute_atom(constraint_.rhs, atom_name, replacement);
    return *this;
}

Constraint ConstraintSubstitution::build() const {
    return constraint_;
}

} // namespace analog
