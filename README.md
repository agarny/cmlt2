# CellML Text 2.0

A human-friendly, text-based format for [CellML 2.0](https://www.cellml.org/) mathematical models, with a C++ library for bidirectional conversion.

CellML Text 2.0 lets you author biological models using **natural mathematical notation** — no XML, no CellML knowledge required.

## What it looks like

```
hodgkin_huxley_squid_axon_model_1952 {
  environment {
    time: leakage_current.time
  }

  membrane {
    V: mV = 0
    E_R: mV = 0
    Cm: uF/cm^2 = 1
    time: environment.time
    i_Na: sodium_channel.i_Na
    i_K: potassium_channel.i_K
    i_L: leakage_current.i_L
    i_Stim: uA/cm^2

    i_Stim = {
      -20.0  if time >= 10.0 and time <= 10.5
      0.0  otherwise
    }
    V' = -(-i_Stim + i_Na + i_K + i_L) / Cm
  }

  leakage_current {
    i_L: uA/cm^2
    g_L: mS/cm^2 = 0.3
    E_L: mV
    time: environment.time
    V: membrane.V
    E_R: membrane.E_R

    E_L = E_R - 10.613
    i_L = g_L * (V - E_L)
  }

  sodium_channel {
    i_Na: uA/cm^2
    g_Na: mS/cm^2 = 120
    E_Na: mV
    time: environment.time
    V: membrane.V
    E_R: membrane.E_R
    m: sodium_channel_m_gate.m
    h: sodium_channel_h_gate.h

    E_Na = E_R - 115.0
    i_Na = g_Na * m^3.0 * h * (V - E_Na)

    sodium_channel_m_gate {
      m: dimensionless = 0.05
      alpha_m: 1/ms
      beta_m: 1/ms
      V: membrane.V
      time: sodium_channel.time

      alpha_m = 0.1 * (V + 25.0) / (exp((V + 25.0) / 10.0) - 1.0)
      beta_m = 4.0 * exp(V / 18.0)
      m' = alpha_m * (1.0 - m) - beta_m * m
    }

    sodium_channel_h_gate {
      h: dimensionless = 0.6
      alpha_h: 1/ms
      beta_h: 1/ms
      V: membrane.V
      time: sodium_channel.time

      alpha_h = 0.07 * exp(V / 20.0)
      beta_h = 1.0 / (exp((V + 30.0) / 10.0) + 1.0)
      h' = alpha_h * (1.0 - h) - beta_h * h
    }
  }

  potassium_channel {
    i_K: uA/cm^2
    g_K: mS/cm^2 = 36
    E_K: mV
    time: environment.time
    V: membrane.V
    E_R: membrane.E_R
    n: potassium_channel_n_gate.n

    E_K = E_R + 12.0
    i_K = g_K * n^4.0 * (V - E_K)

    potassium_channel_n_gate {
      n: dimensionless = 0.325
      alpha_n: 1/ms
      beta_n: 1/ms
      V: membrane.V
      time: potassium_channel.time

      alpha_n = 0.01 * (V + 10.0) / (exp((V + 10.0) / 10.0) - 1.0)
      beta_n = 0.125 * exp(V / 80.0)
      n' = alpha_n * (1.0 - n) - beta_n * n
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
- **Piecewise** — use `if`/`otherwise` for conditional expressions
- **Resets** — define reset behaviour with `reset var at order ... if cond { ... }`
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
| Piecewise | `x = { expr if cond \| expr otherwise }` |
| Reset | `reset V at order 1 if V > thresh { V = V_reset }` |
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
