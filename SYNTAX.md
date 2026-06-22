# CellML Text 2.0 — Syntax Specification

CellML Text 2.0 is a human-friendly, text-based representation of CellML 2.0
models. It is designed to be intuitive for scientists and engineers with a
mathematical background, requiring **no knowledge of CellML or XML**.

## Quick Example

```
HodgkinHuxley1952 {
  membrane {
    V: mV = -75.0
    t: ms
    Cm: uF/cm^2 = 1.0
    I_Na: sodium_channel.I_Na
    I_K: uA/cm^2

    Cm * V' = -(I_Na + I_K)

    sodium_channel {
      V: membrane.V
      g_Na: mS/cm^2 = 120.0
      E_Na: mV = 50.0
      I_Na: uA/cm^2

      I_Na = g_Na * (V - E_Na)
    }
  }
}
```

---

## 1. Model

Every file wraps the entire model in a named block:

```
<name> {
    ...
}
```

The name is a plain identifier (letters, digits, underscores).

---

## 2. Components

Components are named blocks nested inside the model (or inside other
components for hierarchy):

```
<name> {
    <variable declarations>
    <equations>
    <nested components>
}
```

Nesting defines the component hierarchy (encapsulation) directly:

```
membrane {
    sodium_channel {
        ...
    }
    potassium_channel {
        ...
    }
}
```

---

## 3. Variable Declarations

### Definition (with units)

```
<name>: <unit> [= <initial_value>]
```

Examples:
```
V: mV = -75.0          // millivolts, starts at -75
t: ms                   // milliseconds
Cm: uF/cm^2 = 1.0      // microfarads per square centimetre
ratio: 1                // dimensionless
```

### Connection (referencing another component's variable)

```
<name>: <component_name>.<variable_name>
```

This declares a variable that is connected to (equivalent to) the named
variable in another component. Units are inherited automatically from the
source variable.

Examples:
```
V: membrane.V           // connected to membrane's V variable
I_Na: sodium_channel.I_Na  // connected to sodium_channel's I_Na
```

---

## 4. Units

### Built-in SI Units

| Symbol | Unit        | Symbol | Unit        |
|--------|-------------|--------|-------------|
| `m`    | metre       | `V`    | volt        |
| `g`    | gram        | `F`    | farad       |
| `s`    | second      | `ohm`  | ohm         |
| `A`    | ampere      | `S`    | siemens     |
| `K`    | kelvin      | `Wb`   | weber       |
| `mol`  | mole        | `T`    | tesla       |
| `cd`   | candela     | `H`    | henry       |
| `Hz`   | hertz       | `lm`   | lumen       |
| `N`    | newton      | `lx`   | lux         |
| `Pa`   | pascal      | `Bq`   | becquerel   |
| `J`    | joule       | `Gy`   | gray        |
| `W`    | watt        | `Sv`   | sievert     |
| `C`    | coulomb     | `kat`  | katal       |
| `L`    | litre       | `rad`  | radian      |
| `kg`   | kilogram    | `sr`   | steradian   |

### SI Prefixes

Prefixes can be prepended to unit symbols:

| Symbol | Name  | Factor | Symbol | Name  | Factor |
|--------|-------|--------|--------|-------|--------|
| `Y`    | yotta | 10^24  | `d`    | deci  | 10^-1  |
| `Z`    | zetta | 10^21  | `c`    | centi | 10^-2  |
| `E`    | exa   | 10^18  | `m`    | milli | 10^-3  |
| `P`    | peta  | 10^15  | `u`    | micro | 10^-6  |
| `T`    | tera  | 10^12  | `n`    | nano  | 10^-9  |
| `G`    | giga  | 10^9   | `p`    | pico  | 10^-12 |
| `M`    | mega  | 10^6   | `f`    | femto | 10^-15 |
| `k`    | kilo  | 10^3   | `a`    | atto  | 10^-18 |
| `h`    | hecto | 10^2   | `z`    | zepto | 10^-21 |
| `da`   | deca  | 10^1   | `y`    | yocto | 10^-24 |

### Compound Units

Units can be combined with `*`, `/`, and `^`:

```
mS/cm^2         // millisiemens per square centimetre
uA/cm^2         // microamperes per square centimetre
kg*m/s^2        // kilogram metre per second squared
mmol/L          // millimoles per litre
1/ms            // per millisecond
```

### Dimensionless

Use `1` or `dimensionless` for unitless quantities.

### Custom Units (rarely needed)

```
unit beats_per_minute = 1/60 * Hz
```

---

## 5. Equations

### Algebraic Equations

```
I_Na = g_Na * m^3 * h * (V - E_Na)
```

### Differential Equations

```
Cm * V' = -(I_Na + I_K + I_L) / Cm
m' = alpha_m * (1 - m) - beta_m * m
```

The prime (`'`) notation represents the derivative with respect to the
independent variable (typically time). `V'` is shorthand for `d(V)/d(t)`.

For backward compatibility, the full `d(x)/d(y)` syntax is also accepted:

```
d(V)/d(t) = -(I_Na + I_K + I_L) / Cm
```

### Piecewise (Conditional) Expressions

```
alpha_m = {
    0.1 * (V + 25) / (exp((V + 25) / 10) - 1)  when V != -25
    1.0                                           otherwise
}
```

Multiple conditions:
```
x = {
    expr1  when condition1
    expr2  when condition2
    expr3  otherwise
}
```

---

## 6. Mathematical Functions and Operators

### Arithmetic
`+`, `-`, `*`, `/`, `^` (power), unary `-`

### Comparison
`==`, `!=`, `<`, `>`, `<=`, `>=`

### Logical
`and`, `or`, `not`

### Functions
| Function      | Description                |
|---------------|----------------------------|
| `exp(x)`      | Exponential                |
| `ln(x)`       | Natural logarithm          |
| `log10(x)`    | Base-10 logarithm          |
| `log2(x)`     | Base-2 logarithm           |
| `sqrt(x)`     | Square root                |
| `abs(x)`      | Absolute value             |
| `floor(x)`    | Floor                      |
| `ceil(x)`     | Ceiling                    |
| `min(x, y)`   | Minimum                    |
| `max(x, y)`   | Maximum                    |
| `rem(x, y)`   | Remainder                  |
| `sin(x)`      | Sine                       |
| `cos(x)`      | Cosine                     |
| `tan(x)`      | Tangent                    |
| `asin(x)`     | Inverse sine               |
| `acos(x)`     | Inverse cosine             |
| `atan(x)`     | Inverse tangent            |
| `sinh(x)`     | Hyperbolic sine            |
| `cosh(x)`     | Hyperbolic cosine          |
| `tanh(x)`     | Hyperbolic tangent         |
| `sec(x)`      | Secant                     |
| `csc(x)`      | Cosecant                   |
| `cot(x)`      | Cotangent                  |
| `sech(x)`     | Hyperbolic secant          |
| `csch(x)`     | Hyperbolic cosecant        |
| `coth(x)`     | Hyperbolic cotangent       |

### Constants
`pi`, `e`, `inf`, `nan`, `true`, `false`

`e` is always recognised as Euler's number and cannot be used as a variable name.
Use `exp(1)` where Euler's number is needed in expressions.

---

## 7. Imports

Reuse components and units from other files:

```
import "path/to/model.cellml" {
    component sodium_channel
    component potassium_channel as K_channel
    unit custom_unit
}
```

---

## 8. Resets

Define reset behaviour inside components:

```
stimulus {
    V: mV
    V_threshold: mV = 0
    V_reset: mV = -75

    reset V when V > V_threshold {
        V = V_reset
    }
}
```

The `at order <n>` clause is optional; it defaults to order 1 when omitted:

```
reset V at order 2 when V > V_threshold {
    V = V_reset
}
```

---

## 9. Comments

Single-line comments with `//`:

```
// This is a comment
V: mV = -75.0  // membrane potential
```

Block comments with `/* ... */`:

```
/*
 * Hodgkin-Huxley 1952 — Squid Giant Axon Model
 * Ref: Hodgkin & Huxley (1952) J Physiol 117, 500–544
 */
V: mV = -75.0  /* inline comment */
```

Block comments can span multiple lines and are nestable.

---

## 10. Grammar Summary (Informal)

```
file          = model_block
model_block   = IDENTIFIER "{" { top_level_item } "}"
top_level_item= block | import_stmt | unit_def
block         = IDENTIFIER "{" { block_item } "}"
block_item    = var_decl | equation | reset_stmt | block
var_decl      = IDENTIFIER ":" ( unit_expr [ "=" expression ] | connection )
connection    = IDENTIFIER "." IDENTIFIER
equation      = expression "=" expression
unit_expr     = unit_term { ("*" | "/") unit_term }
unit_term     = unit_atom [ "^" ["-"] NUMBER ]
unit_atom     = IDENTIFIER | "1" | "(" unit_expr ")"
expression    = or_expr
or_expr       = and_expr { "or" and_expr }
and_expr      = comp_expr { "and" comp_expr }
comp_expr     = add_expr { ("==" | "!=" | "<" | ">" | "<=" | ">=") add_expr }
add_expr      = mul_expr { ("+" | "-") mul_expr }
mul_expr      = unary_expr { ("*" | "/") unary_expr }
unary_expr    = ["-" | "not"] power_expr
power_expr    = primary ["^" unary_expr]
primary       = NUMBER | IDENTIFIER [ "'" ] | func_call | piecewise | "(" expression ")"
              | derivative | constant
func_call     = IDENTIFIER "(" expression { "," expression } ")"
derivative    = "d(" IDENTIFIER ")/d(" IDENTIFIER ")"  // legacy; V' preferred
piecewise     = "{" piece { piece } otherwise "}"
piece         = expression "when" expression
otherwise     = expression "otherwise"
constant      = "pi" | "e" | "inf" | "nan" | "true" | "false"
import_stmt   = "import" STRING "{" { import_item } "}"
import_item   = ("component" | "unit") IDENTIFIER [ "as" IDENTIFIER ]
unit_def      = "unit" IDENTIFIER "=" unit_expr
reset_stmt    = "reset" IDENT [ "at" "order" NUMBER ] "when" expr "{" IDENT "=" expr "}"
```
