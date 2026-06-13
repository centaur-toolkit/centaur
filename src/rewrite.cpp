#include "centaur/rewrite.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace analog {
namespace {

using Substitution = std::unordered_map<std::string, int>;

bool is_variable(const Expr& expr) {
    return expr.is_atom() && !expr.op.empty() && expr.op.front() == '?';
}

void match_pattern(EGraph& graph,
                   const Expr& pattern,
                   int class_id,
                   const Substitution& substitution,
                   std::vector<Substitution>& out);

void match_children(EGraph& graph,
                    const std::vector<Expr>& patterns,
                    const std::vector<int>& children,
                    std::size_t index,
                    const Substitution& substitution,
                    std::vector<Substitution>& out) {
    if (index == patterns.size()) {
        out.push_back(substitution);
        return;
    }

    std::vector<Substitution> partial;
    match_pattern(graph, patterns[index], children[index], substitution, partial);
    for (const auto& subst : partial) {
        match_children(graph, patterns, children, index + 1, subst, out);
    }
}

void match_pattern(EGraph& graph,
                   const Expr& pattern,
                   int class_id,
                   const Substitution& substitution,
                   std::vector<Substitution>& out) {
    const int root = graph.find(class_id);

    if (is_variable(pattern)) {
        auto found = substitution.find(pattern.op);
        if (found == substitution.end()) {
            Substitution next = substitution;
            next.emplace(pattern.op, root);
            out.push_back(std::move(next));
        } else if (graph.find(found->second) == root) {
            out.push_back(substitution);
        }
        return;
    }

    for (const auto& node : graph.nodes(root)) {
        if (node.op != pattern.op || node.children.size() != pattern.args.size()) {
            continue;
        }

        match_children(graph, pattern.args, node.children, 0, substitution, out);

        if (is_commutative(node.op) && node.children.size() == 2) {
            std::vector<int> reversed = {node.children[1], node.children[0]};
            match_children(graph, pattern.args, reversed, 0, substitution, out);
        }
    }
}

int instantiate(EGraph& graph, const Expr& pattern, const Substitution& substitution) {
    if (is_variable(pattern)) {
        auto found = substitution.find(pattern.op);
        if (found == substitution.end()) {
            throw std::runtime_error("rewrite rhs references an unbound variable: " + pattern.op);
        }
        return graph.find(found->second);
    }

    std::vector<int> children;
    children.reserve(pattern.args.size());
    for (const auto& arg : pattern.args) {
        children.push_back(instantiate(graph, arg, substitution));
    }
    return graph.add_enode(pattern.op, std::move(children));
}

double atom_cost(const std::string& op) {
    if (op == "0" || op == "1") {
        return 0.05;
    }
    return 1.0;
}

double op_cost(const std::string& op, std::size_t arity) {
    if (op == "par" || op == "ser" || op == "zc" || op == "zl") {
        return 0.35 + 0.15 * static_cast<double>(arity);
    }
    if (op == "vdiv" || op == "rc_lowpass" || op == "gain_common_source") {
        return 0.55 + 0.15 * static_cast<double>(arity);
    }
    if (op == "neg" || op == "inv") {
        return 0.75;
    }
    if (op == "div") {
        return 1.20;
    }
    if (op == "add" || op == "mul") {
        return 1.00;
    }
    return 1.25 + 0.20 * static_cast<double>(arity);
}

struct Best {
    Expr expr;
    double cost = std::numeric_limits<double>::infinity();
    bool set = false;
};

bool better(const Expr& expr, double cost, const Best& current) {
    if (!current.set) {
        return true;
    }
    if (cost + 1e-9 < current.cost) {
        return true;
    }
    return std::abs(cost - current.cost) <= 1e-9 &&
           to_string(expr).size() < to_string(current.expr).size();
}

} // namespace

SaturationResult saturate(EGraph& graph,
                          const std::vector<Rewrite>& rules,
                          int max_iterations) {
    SaturationResult result;

    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        graph.rebuild();
        bool changed = false;

        const auto reps = graph.representatives();
        for (const auto& rule : rules) {
            for (int rep : reps) {
                std::vector<Substitution> matches;
                match_pattern(graph, rule.lhs, rep, {}, matches);

                for (const auto& subst : matches) {
                    const int rhs = instantiate(graph, rule.rhs, subst);
                    if (graph.unite(rep, rhs)) {
                        changed = true;
                        ++result.applied;
                        ++result.rule_counts[rule.name];
                    }
                }
            }
        }

        result.iterations = iteration + 1;
        graph.rebuild();

        if (!changed) {
            break;
        }
    }

    return result;
}

Extraction extract_best(EGraph& graph, int root_class, int rounds) {
    graph.rebuild();
    const int root = graph.find(root_class);

    std::vector<Best> best(1024);

    auto ensure_size = [&](int id) {
        if (id >= static_cast<int>(best.size())) {
            best.resize(static_cast<std::size_t>(id + 1));
        }
    };

    for (int round = 0; round < rounds; ++round) {
        bool changed = false;
        const auto reps = graph.representatives();
        for (int rep : reps) {
            ensure_size(rep);
            for (const auto& node : graph.nodes(rep)) {
                Expr candidate;
                double candidate_cost = op_cost(node.op, node.children.size());

                if (node.children.empty()) {
                    candidate = atom(node.op);
                    candidate_cost = atom_cost(node.op);
                } else {
                    std::vector<Expr> args;
                    args.reserve(node.children.size());
                    bool ready = true;
                    for (int child : node.children) {
                        const int child_root = graph.find(child);
                        ensure_size(child_root);
                        if (!best[child_root].set) {
                            ready = false;
                            break;
                        }
                        args.push_back(best[child_root].expr);
                        candidate_cost += best[child_root].cost;
                    }
                    if (!ready) {
                        continue;
                    }
                    candidate = call(node.op, std::move(args));
                }

                if (better(candidate, candidate_cost, best[rep])) {
                    best[rep] = Best{std::move(candidate), candidate_cost, true};
                    changed = true;
                }
            }
        }

        if (!changed) {
            break;
        }
    }

    ensure_size(root);
    if (!best[root].set) {
        throw std::runtime_error("could not extract an acyclic expression from the e-graph");
    }

    return Extraction{best[root].expr, best[root].cost};
}

Expr optimize_expr(const Expr& expr,
                   const std::vector<Rewrite>& rules,
                   int max_iterations) {
    EGraph graph;
    const int root = graph.add(simplify_expr(expr));
    saturate(graph, rules, max_iterations);
    return simplify_expr(extract_best(graph, root).expr);
}

} // namespace analog
