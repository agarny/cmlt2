# CellML Text 2.0

A human-friendly, text-based format for [CellML 2.0](https://www.cellml.org/) mathematical models, with a C++ library for bidirectional conversion.

CellML Text 2.0 lets you author biological models using **natural mathematical notation** — no XML, no CellML knowledge required.

## What it looks like

```
model NobleModel1962

component membrane {
    V: mV = -87.0
    t: ms
    Cm: uF/cm^2 = 12.0
    I_Na: uA/cm^2
    I_K: uA/cm^2

    Cm * d(V)/d(t) = -(I_Na + I_K)
}

component sodium_channel {
    V: mV
    g_Na: mS/cm^2 = 400.0
    E_Na: mV = 40.0
    m: dimensionless
    h: dimensionless
    I_Na: uA/cm^2

    I_Na = g_Na * m^3 * h * (V - E_Na)
}

map membrane.V <-> sodium_channel.V
map membrane.I_Na <-> sodium_channel.I_Na
```

## Key features

- **Natural math** — write `d(V)/d(t) = -(I_Na + I_K) / Cm` instead of XML/MathML
- **SI units with prefixes** — `mV`, `mS/cm^2`, `uA/cm^2`, `mmol/L` — just type them
- **No CellML knowledge needed** — the syntax reads like a maths textbook
- **Piecewise functions** — use `when`/`otherwise` for conditional expressions
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
| Model | `model MyModel` |
| Component | `component name { ... }` |
| Variable | `V: mV = -75.0` |
| Equation | `I = g * (V - E)` |
| Derivative | `d(V)/d(t) = -I / Cm` |
| Piecewise | `x = { expr when cond \| expr otherwise }` |
| Connection | `map comp1.var <-> comp2.var` |
| Hierarchy | `group parent contains { child1, child2 }` |
| Import | `import "file.cellml" { component name }` |
| Units | `mV`, `mS/cm^2`, `uA/cm^2`, `mmol/L`, `1/ms` |
| Functions | `exp()`, `ln()`, `sin()`, `sqrt()`, `abs()`, ... |
| Constants | `pi`, `e`, `inf`, `nan`, `true`, `false` |

## Project structure

```
cmlt2/
├── CMakeLists.txt          Top-level build
├── SYNTAX.md               Complete syntax specification
├── README.md               This file
├── src/
│   ├── api.h / api.cpp     Public API
│   ├── token.h             Token types
│   ├── ast.h               Expression AST nodes
│   ├── lexer.h / .cpp      Tokenizer
│   ├── parser.h / .cpp     CellML Text → libCellML Model
│   ├── serializer.h / .cpp libCellML Model → CellML Text
│   ├── units.h / .cpp      SI unit parsing and generation
│   └── mathml.h / .cpp     MathML ↔ expression AST
├── tests/
│   └── tests.cpp           Test suite
└── examples/
    └── hodgkin_huxley.cml  Full Hodgkin-Huxley model
```

## Example: Hodgkin-Huxley model

See [examples/hodgkin_huxley.cml](examples/hodgkin_huxley.cml) for a complete implementation of the classic squid giant axon model in CellML Text 2.0.

## License

Apache 2.0 (same as libCellML).
