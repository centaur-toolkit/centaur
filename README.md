# Centaur

[![CI](https://github.com/centaur-toolkit/centaur/actions/workflows/ci.yml/badge.svg)](https://github.com/centaur-toolkit/centaur/actions/workflows/ci.yml)

Centaur is a C++ prototype for symbolic analog-circuit analysis and topology
rewriting. The goal is Herbie-like exploration for circuit structure: keep the
input close to textbook circuits, infer useful equivalent forms, and produce
readable expressions such as `vdiv`, `par`, and simplified Thevenin results.

Centaur currently focuses on linear DC/resistive circuits with independent and
linear dependent sources. It combines three pieces:

- a SPICE-like netlist parser
- symbolic modified nodal analysis (MNA)
- equality-saturation and topology rewrite passes

## Status

Implemented today:

- symbolic expressions with local simplification
- egg-style e-graph rewrites for common analog forms
- Thevenin equivalent calculation
- operating-point node voltages, branch currents, powers, and source seen
  resistance
- one-variable symbolic equation isolation for backward design queries
- topology rewrites for series/parallel resistors and ideal-source patterns
- independent voltage/current sources
- voltage-controlled voltage/current sources
- current-controlled current sources using component current references
- regression examples from Schaum Basic Circuit Analysis exercises 3.20-3.48

This is still a prototype. The current solver is linear and DC-oriented; AC
impedance forms are present in the expression rewrite layer, but not yet a full
frequency-domain circuit frontend.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The main executable is:

```sh
./build/centaur
```

## Quick Examples

Optimize a symbolic expression:

```sh
./build/centaur '(mul Vin (div R2 (add R1 R2)))'
```

Output:

```text
(vdiv Vin R1 R2)
```

Compute a Thevenin equivalent:

```sh
./build/centaur --thevenin examples/voltage_divider.cir out 0
```

Output:

```text
Vth out 0: (vdiv Vin R1 R2)
Rth out 0: (par R1 R2)
```

Solve selected textbook quantities:

```sh
./build/centaur --solve examples/exercise_3_28.cir \
  --voltage a 0 \
  --current R9 \
  --current R13
```

Output:

```text
V a 0: -35
I R9: 0
I R13: 0
```

Use `--explain` to see topology trace lines and expression rewrite activity:

```sh
./build/centaur --explain --solve examples/exercise_3_28.cir --voltage a 0
```

Infer an unknown resistance from a target equivalent resistance:

```sh
./build/centaur --solve-rth-for examples/exercise_3_35.cir a 0 R \
  '(eq Rth 12000)'
```

Output:

```text
R: -15000
```

## Netlist Format

The parser accepts a compact SPICE-like format:

```text
Rname node+ node- value
Vname node+ node- value
Iname node+ node- value
Ename node+ node- ctrl+ ctrl- gain
Gname node+ node- ctrl+ ctrl- gain
Fname node+ node- control-component gain
```

Component meanings:

- `R`: resistor
- `V`: independent voltage source
- `I`: independent current source
- `E`: voltage-controlled voltage source
- `G`: voltage-controlled current source
- `F`: current-controlled current source

For `F`, the control is another component name. For example:

```text
Fdep v1 0 R16 0.5
```

means:

```text
I(Fdep from v1 to 0) = 0.5 * I(R16)
```

Values may be numeric atoms such as `10`, or symbolic atoms such as `R1`,
`Vin`, and `gm`.

Ground may be written as `0`, `gnd`, or `GND`.

## Commands

Solve an operating point:

```sh
./build/centaur --solve file.cir
```

With no explicit queries, Centaur prints node voltages relative to ground and
resistor currents. With explicit queries, it prints only the requested values:

```sh
./build/centaur --solve file.cir \
  --voltage node+ node- \
  --current component \
  --power component \
  --seen-resistance Vsource
```

Compute a Thevenin equivalent:

```sh
./build/centaur --thevenin file.cir node+ node-
```

Solve a one-variable symbolic equation:

```sh
./build/centaur --solve-for R '(eq (par 10000 20000 R) 12000)'
```

Infer a value from a target Thevenin resistance:

```sh
./build/centaur --solve-rth-for file.cir node+ node- R '(eq Rth 12000)'
```

The `Rth` atom may appear anywhere in the equation expression:

```sh
./build/centaur --solve-rth-for file.cir node+ node- R '(eq (inv Rth) 1.75)'
```

Run only the topology prepass:

```sh
./build/centaur --rewrite-topology file.cir protected-node ...
```

Protected nodes are observed terminals; the topology pass avoids removing or
contracting them.

## Topology Rewrites

Topology rewrites operate on the circuit graph before symbolic MNA. They are
symbolic: resistor values such as `R1` and `R2` are preserved and combined into
expressions such as `(par R1 R2)` and `(add R1 R2)`.

Current rules:

- parallel resistors with the same two terminals become one resistor with
  `(par ...)`
- series resistors through an unprotected degree-2 node become one resistor
  with `(add ...)`
- shorted resistors are removed
- unprotected independent `0 V` sources are removed by merging their two nodes
- dangling resistor branches ending at unprotected nodes are removed
- a resistor in series with an ideal current source is removed and the source is
  reconnected across the branch
- a resistor in parallel with an ideal voltage source is removed

The solve path is query-aware. Explicit `--voltage` queries protect their nodes;
explicit `--current`, `--power`, and `--seen-resistance` queries protect their
components. This lets Centaur simplify the circuit for a `Vab` query while still
keeping a resistor if the user asks for that resistor current.

Example:

```sh
./build/centaur --explain --rewrite-topology examples/exercise_3_28.cir a 0
```

The topology summary reports two current-source series resistor removals and
one voltage-source parallel resistor removal. For ladder-style reductions,
`--explain` also prints each topology rewrite step:

```sh
./build/centaur --explain --rewrite-topology examples/exercise_3_41.cir a 0
```

Example trace line:

```text
series-resistors: R4 inner tail 4 + R8 tail right 8 through tail -> Rser2 inner right 12
```

## Expression Language

Expressions use s-expressions:

```text
(add a b)
(sub a b)
(mul a b)
(div a b)
(neg a)
(inv a)
(par R1 R2 ...)
(vdiv Vin Rtop Rbot)
(zc C1)
(zl L1)
```

The rewrite rules include algebraic identities, parallel impedance forms,
voltage-divider recognition, capacitor/inductor impedance forms, RC low-pass
forms, and a common-source gain idiom.

Equation queries are ordinary expressions:

```text
(eq (par 10000 20000 R) 12000)
(eq Rth 12000)
(eq (inv Rth) 1.75)
```

## Textbook Examples

The `examples/` directory includes checked examples from Schaum Basic Circuit
Analysis:

```sh
./build/centaur --solve examples/exercise_3_20.cir \
  --current R10 --current R25 --voltage top n15

./build/centaur --solve examples/exercise_3_26.cir \
  --voltage v1 0 --current R16 --current Fdep

./build/centaur --solve examples/exercise_3_28.cir \
  --voltage a 0 --current R9 --current R13

./build/centaur --solve-rth-for examples/exercise_3_35.cir \
  a 0 R '(eq Rth 12000)'

./build/centaur --solve-rth-for examples/exercise_3_36.cir \
  a 0 R '(eq (inv Rth) 1.75)'
```

These are also covered by CTest.

## Project Direction

Near-term work:

- add more source-control forms, including current-controlled voltage sources
- generalize `(eq ...)` constraints to multiple variables with a solver backend
  and verified bindings
- improve symbolic linear solving to avoid floating-point artifacts
- add typed symbols and dimensions
- extend the frontend toward impedance-domain `Z` circuits
- integrate `c-spice` as an optional parser/simulation frontend
- grow the textbook regression suite into a rule-discovery harness
