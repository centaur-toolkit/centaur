#include "centaur/analog_rules.h"

#include "centaur/parser.h"

#include <utility>

namespace analog {
namespace {

Rewrite rw(std::string name, const std::string& lhs, const std::string& rhs) {
    return Rewrite{std::move(name), parse_expr(lhs), parse_expr(rhs)};
}

} // namespace

std::vector<Rewrite> analog_rules() {
    return {
        rw("add-zero-r", "(add ?x 0)", "?x"),
        rw("add-zero-l", "(add 0 ?x)", "?x"),
        rw("mul-one-r", "(mul ?x 1)", "?x"),
        rw("mul-one-l", "(mul 1 ?x)", "?x"),
        rw("mul-zero-r", "(mul ?x 0)", "0"),
        rw("mul-zero-l", "(mul 0 ?x)", "0"),
        rw("div-one", "(div ?x 1)", "?x"),
        rw("div-self", "(div ?x ?x)", "1"),
        rw("div-zero", "(div 0 ?x)", "0"),
        rw("inv-one", "(inv 1)", "1"),
        rw("inv-inv", "(inv (inv ?x))", "?x"),
        rw("inv-neg", "(inv (neg ?x))", "(neg (inv ?x))"),
        rw("inv-one-over", "(inv (div 1 ?x))", "?x"),
        rw("neg-inv-one-over", "(neg (inv (div 1 ?x)))", "(neg ?x)"),
        rw("neg-neg", "(neg (neg ?x))", "?x"),
        rw("add-neg", "(add ?x (neg ?x))", "0"),
        rw("mul-inv-self", "(mul ?x (inv ?x))", "1"),
        rw("div-as-mul-inv", "(div ?x ?y)", "(mul ?x (inv ?y))"),
        rw("mul-inv-as-div", "(mul ?x (inv ?y))", "(div ?x ?y)"),
        rw("cancel-mul-div-l", "(div (mul ?a ?b) ?a)", "?b"),
        rw("cancel-mul-div-r", "(div (mul ?a ?b) ?b)", "?a"),

        rw("parallel-product", "(div (mul ?a ?b) (add ?a ?b))", "(par ?a ?b)"),
        rw("parallel-product-expand", "(par ?a ?b)", "(div (mul ?a ?b) (add ?a ?b))"),
        rw("parallel-conductance", "(inv (add (inv ?a) (inv ?b)))", "(par ?a ?b)"),
        rw("parallel-conductance-div", "(div 1 (add (inv ?a) (inv ?b)))", "(par ?a ?b)"),
        rw("parallel-conductance-expand", "(par ?a ?b)", "(inv (add (inv ?a) (inv ?b)))"),

        rw("series-expand", "(ser ?a ?b)", "(add ?a ?b)"),

        rw("capacitor-impedance", "(inv (mul s ?c))", "(zc ?c)"),
        rw("capacitor-impedance-expand", "(zc ?c)", "(inv (mul s ?c))"),
        rw("inductor-impedance", "(mul s ?l)", "(zl ?l)"),
        rw("inductor-impedance-expand", "(zl ?l)", "(mul s ?l)"),

        rw("voltage-divider", "(mul ?vin (div ?zbot (add ?ztop ?zbot)))",
           "(vdiv ?vin ?ztop ?zbot)"),
        rw("voltage-divider-scaled", "(mul ?zbot (div ?vin (add ?ztop ?zbot)))",
           "(vdiv ?vin ?ztop ?zbot)"),
        rw("voltage-divider-div", "(div (mul ?vin ?zbot) (add ?ztop ?zbot))",
           "(vdiv ?vin ?ztop ?zbot)"),
        rw("voltage-divider-expand", "(vdiv ?vin ?ztop ?zbot)",
           "(mul ?vin (div ?zbot (add ?ztop ?zbot)))"),

        rw("conductance-divider", "(div (mul ?vin (inv ?r1)) (add (inv ?r1) (inv ?r2)))",
           "(vdiv ?vin ?r1 ?r2)"),
        rw("conductance-divider-mul", "(mul ?vin (div (inv ?r1) (add (inv ?r1) (inv ?r2))))",
           "(vdiv ?vin ?r1 ?r2)"),
        rw("divider-remainder-par", "(neg (add (neg ?b) (vdiv ?b ?a ?b)))",
           "(par ?a ?b)"),
        rw("divider-remainder-par-alt", "(add ?b (neg (vdiv ?b ?a ?b)))",
           "(par ?a ?b)"),
        rw("divider-remainder-par-with-series",
           "(neg (add (neg (add ?b ?tail)) (vdiv ?b ?a ?b)))",
           "(add ?tail (par ?a ?b))"),
        rw("divider-remainder-par-with-series-alt",
           "(add (add ?b ?tail) (neg (vdiv ?b ?a ?b)))",
           "(add ?tail (par ?a ?b))"),

        rw("rc-lowpass", "(vdiv ?vin ?r (zc ?c))", "(rc_lowpass ?r ?c ?vin)"),
        rw("rc-lowpass-expand", "(rc_lowpass ?r ?c ?vin)", "(vdiv ?vin ?r (zc ?c))"),
        rw("rc-lowpass-canonical", "(div ?vin (add 1 (mul s (mul ?r ?c))))",
           "(rc_lowpass ?r ?c ?vin)"),

        rw("common-source-gain", "(neg (mul ?gm (par ?rd ?ro)))",
           "(gain_common_source ?gm ?rd ?ro)"),
        rw("common-source-gain-expand", "(gain_common_source ?gm ?rd ?ro)",
           "(neg (mul ?gm (par ?rd ?ro)))"),
    };
}

} // namespace analog
