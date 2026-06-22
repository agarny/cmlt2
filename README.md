# CellML Text 2.0

A human-friendly, text-based format for [CellML 2.0](https://www.cellml.org/) mathematical models, with a C++ library for bidirectional conversion.

CellML Text 2.0 lets you author biological models using **natural mathematical notation** — no XML, no CellML knowledge required.

## What it looks like

```
NobleModel1962 {
  membrane {
    V: mV = -87.0
    t: ms
    Cm: uF/cm^2 = 12.0
    I_Na: sodium_channel.I_Na
    I_K: uA/cm^2

    Cm * d(V)/d(t) = -(I_Na + I_K)

    sodium_channel {
      V: membrane.V
      g_Na: mS/cm^2 = 400.0
      E_Na: mV = 40.0
      m: dimensionless
      h: dimensionless
      I_Na: uA/cm^2

      I_Na = g_Na * m^3 * h * (V - E_Na)
    }
  }
}
```

## Key features

- **Natural math** — write `d(V)/d(t) = -(I_Na + I_K) / Cm` instead of XML/MathML
- **SI units with prefixes** — `mV`, `mS/cm^2`, `uA/cm^2`, `mmol/L` — just type them
- **No CellML knowledge needed** — the syntax reads like a maths textbook
- **Nested components** — hierarchy is expressed directly via nesting
- **Dot-notation connections** — connect variables across components with `comp.var`
- **Piecewise functions** — use `when`/`otherwise` for conditional expressions
- **Resets** — define reset behaviour with `reset var at order ... when cond { ... }`
- **Imports** — reuse components and units from other files
- **Custom units** — define your own units from SI building blocks
- **Full round-trip** — convert CellML XML ↔ CellML Text 2.0 losslessly
- **Built on libCellML** — leverages the official CellML library

## Building

### Prerequisites

- C++17 compiler
- CMake 3.14+
- Git (for FetchContent)

### Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Run tests

```bash
cd build
ctest --output-on-failure
```

## Library API

```cpp
#include "api.h"

// CellML Text 2.0 → libCellML Model
std::vector<cellmltext::Error> errors;
auto model = cellmltext::textToModel(sourceText, &errors);

// libCellML Model → CellML Text 2.0
std::string text = cellmltext::modelToText(model, &errors);

// Direct conversion: CellML Text 2.0 → CellML XML
std::string xml = cellmltext::textToCellML(sourceText, &errors);

// Direct conversion: CellML XML → CellML Text 2.0
std::string text = cellmltext::cellMLToText(xmlString, &errors);
```

## Syntax overview

See [SYNTAX.md](SYNTAX.md) for the complete specification. Highlights:

| Feature | Syntax |
|---|---|
| Model | `Name { ... }` |
| Component | `name { ... }` (nested inside parent) |
| Variable | `V: mV = -75.0` |
| Connection | `V: parent.V` |
| Equation | `I = g * (V - E)` |
| Derivative | `d(V)/d(t) = -I / Cm` |
| Piecewise | `x = { expr when cond \| expr otherwise }` |
| Reset | `reset V at order 1 when V > thresh { V = V_reset }` |
| Custom unit | `unit beats_per_min = 1/60 * Hz` |
| Import | `import "file.cellml" { component name }` |
| Units | `mV`, `mS/cm^2`, `uA/cm^2`, `mmol/L`, `1/ms` |
| Functions | `exp()`, `ln()`, `sin()`, `sqrt()`, `abs()`, ... |
| Constants | `pi`, `e`, `inf`, `nan`, `true`, `false` |
| Logical ops | `and`, `or`, `not` |

## Project structure

```
cmlt2/
├── CMakeLists.txt            Top-level build
├── SYNTAX.md                 Complete syntax specification
├── README.md                 This file
├── src/
│   ├── api.h / api.cpp       Public API
│   ├── token.h               Token types
│   ├── ast.h                 Expression AST nodes
│   ├── lexer.h / .cpp        Tokenizer
│   ├── parser.h / .cpp       CellML Text → libCellML Model
│   ├── serializer.h / .cpp   libCellML Model → CellML Text
│   ├── units.h / .cpp        SI unit parsing and generation
│   └── mathml.h / .cpp       MathML ↔ expression AST
├── tests/
│   ├── tests.cpp             Test suite
│   ├── CMakeLists.txt        Test build configuration
│   └── models/               Reference CellML XML models
│       ├── hodgkin_huxley_squid_axon_model_1952.cellml
│       ├── noble_model_1962.cellml
│       ├── garny_kohl_hunter_boyett_noble_rabbit_san_model_2003.cellml
│       └── fabbri_fantini_wilders_severi_human_san_model_2017.cellml
└── examples/
    ├── hodgkin_huxley.cml     Full Hodgkin-Huxley model
    ├── feature_showcase.cml   Demonstrates every syntax feature
    ├── custom_units.cml       Custom units and mathematical functions
    └── reset.cml              Integrate-and-fire neuron with resets
```

## Examples

- **[examples/hodgkin_huxley.cml](examples/hodgkin_huxley.cml)** — Full Hodgkin-Huxley squid giant axon model
- **[examples/feature_showcase.cml](examples/feature_showcase.cml)** — Demonstrates every CellML Text 2.0 feature
- **[examples/custom_units.cml](examples/custom_units.cml)** — Custom unit definitions and pharmacokinetic modelling
- **[examples/reset.cml](examples/reset.cml)** — Integrate-and-fire neuron with reset behaviour

## License

Apache 2.0 (same as libCellML).
