#include "centaur/parser.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace analog {
namespace {

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;

    auto flush = [&]() {
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
    };

    for (char ch : input) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            flush();
        } else if (ch == '(' || ch == ')') {
            flush();
            tokens.emplace_back(1, ch);
        } else {
            current.push_back(ch);
        }
    }
    flush();
    return tokens;
}

Expr parse_at(const std::vector<std::string>& tokens, std::size_t& pos) {
    if (pos >= tokens.size()) {
        throw std::runtime_error("unexpected end of expression");
    }

    const std::string& token = tokens[pos++];
    if (token == "(") {
        if (pos >= tokens.size()) {
            throw std::runtime_error("missing operator after '('");
        }
        std::string op = tokens[pos++];
        if (op == "(" || op == ")") {
            throw std::runtime_error("invalid operator in expression");
        }

        std::vector<Expr> args;
        while (pos < tokens.size() && tokens[pos] != ")") {
            args.push_back(parse_at(tokens, pos));
        }
        if (pos >= tokens.size() || tokens[pos] != ")") {
            throw std::runtime_error("missing ')' in expression");
        }
        ++pos;
        return call(std::move(op), std::move(args));
    }

    if (token == ")") {
        throw std::runtime_error("unexpected ')' in expression");
    }

    return atom(token);
}

} // namespace

Expr parse_expr(const std::string& input) {
    auto tokens = tokenize(input);
    if (tokens.empty()) {
        throw std::runtime_error("empty expression");
    }

    std::size_t pos = 0;
    Expr expr = parse_at(tokens, pos);
    if (pos != tokens.size()) {
        throw std::runtime_error("trailing tokens in expression");
    }
    return expr;
}

} // namespace analog
