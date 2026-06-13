#include "centaur/egraph.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace analog {

int EGraph::add(const Expr& expr) {
    std::vector<int> children;
    children.reserve(expr.args.size());
    for (const auto& arg : expr.args) {
        children.push_back(add(arg));
    }
    return add_enode(expr.op, std::move(children));
}

int EGraph::add_enode(const std::string& op, std::vector<int> children) {
    ENode node{op, std::move(children)};
    node = canonicalize(std::move(node));
    const std::string key = key_for(node);

    auto found = hashcons_.find(key);
    if (found != hashcons_.end()) {
        return find(found->second);
    }

    const int id = static_cast<int>(classes_.size());
    EClass klass;
    klass.parent = id;
    klass.nodes.push_back(std::move(node));
    classes_.push_back(std::move(klass));
    hashcons_[key] = id;
    return id;
}

int EGraph::find(int id) {
    if (id < 0 || id >= static_cast<int>(classes_.size())) {
        throw std::out_of_range("invalid e-class id");
    }
    int parent = classes_[id].parent;
    if (parent != id) {
        classes_[id].parent = find(parent);
    }
    return classes_[id].parent;
}

bool EGraph::unite(int lhs, int rhs) {
    int left = find(lhs);
    int right = find(rhs);
    if (left == right) {
        return false;
    }

    if (classes_[left].rank < classes_[right].rank) {
        std::swap(left, right);
    }
    classes_[right].parent = left;
    if (classes_[left].rank == classes_[right].rank) {
        ++classes_[left].rank;
    }
    return true;
}

void EGraph::rebuild() {
    bool changed = false;
    do {
        changed = false;
        for (int i = 0; i < static_cast<int>(classes_.size()); ++i) {
            find(i);
        }

        std::vector<std::pair<int, ENode>> items;
        for (int i = 0; i < static_cast<int>(classes_.size()); ++i) {
            const int root = find(i);
            for (auto node : classes_[i].nodes) {
                items.emplace_back(root, canonicalize(std::move(node)));
            }
            classes_[i].nodes.clear();
        }

        hashcons_.clear();
        std::unordered_set<std::string> seen_in_class;

        for (auto& item : items) {
            int root = find(item.first);
            ENode node = canonicalize(std::move(item.second));
            const std::string key = key_for(node);

            auto found = hashcons_.find(key);
            if (found == hashcons_.end()) {
                hashcons_[key] = root;
                classes_[root].nodes.push_back(std::move(node));
                continue;
            }

            const int other = find(found->second);
            root = find(root);
            if (other != root) {
                changed = unite(other, root) || changed;
            }
        }
    } while (changed);

    for (int i = 0; i < static_cast<int>(classes_.size()); ++i) {
        find(i);
    }
}

std::vector<int> EGraph::representatives() {
    rebuild();
    std::vector<int> reps;
    for (int i = 0; i < static_cast<int>(classes_.size()); ++i) {
        if (find(i) == i && !classes_[i].nodes.empty()) {
            reps.push_back(i);
        }
    }
    return reps;
}

const std::vector<ENode>& EGraph::nodes(int class_id) {
    const int root = find(class_id);
    return classes_[root].nodes;
}

std::size_t EGraph::class_count() {
    return representatives().size();
}

ENode EGraph::canonicalize(ENode node) {
    for (int& child : node.children) {
        child = find(child);
    }
    if (is_commutative(node.op)) {
        std::sort(node.children.begin(), node.children.end());
    }
    return node;
}

std::string EGraph::key_for(const ENode& node) const {
    std::ostringstream out;
    out << node.op;
    for (int child : node.children) {
        out << '\x1f' << child;
    }
    return out.str();
}

} // namespace analog
