/*
 * CellML Text 2.0 — SI unit handling (implementation)
 */

#include "units.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace cellmltext {

// ===================================================================
// Lookup tables
// ===================================================================

// Map SI symbol → CellML standard unit name.
// Ordered longest-first so that greedy matching works correctly.
static const std::vector<std::pair<std::string, std::string>> kSiSymbols = {
    // Multi-character symbols first (longest match).
    {"dimensionless", "dimensionless"},
    {"ohm",  "ohm"},
    {"mol",  "mole"},
    {"kat",  "katal"},
    {"rad",  "radian"},
    {"Wb",   "weber"},
    {"Hz",   "hertz"},
    {"Pa",   "pascal"},
    {"Bq",   "becquerel"},
    {"Gy",   "gray"},
    {"Sv",   "sievert"},
    {"lm",   "lumen"},
    {"lx",   "lux"},
    {"cd",   "candela"},
    {"sr",   "steradian"},
    {"kg",   "kilogram"},
    // Single-character symbols.
    {"m",    "metre"},
    {"g",    "gram"},
    {"s",    "second"},
    {"A",    "ampere"},
    {"K",    "kelvin"},
    {"N",    "newton"},
    {"J",    "joule"},
    {"W",    "watt"},
    {"C",    "coulomb"},
    {"V",    "volt"},
    {"F",    "farad"},
    {"S",    "siemens"},
    {"T",    "tesla"},
    {"H",    "henry"},
    {"L",    "litre"},
};

// Build a quick lookup set from the above.
static std::unordered_map<std::string, std::string> buildSymbolMap() {
    std::unordered_map<std::string, std::string> m;
    for (auto &p : kSiSymbols) m[p.first] = p.second;
    return m;
}
static const auto &symbolMap() {
    static auto m = buildSymbolMap();
    return m;
}

// Map prefix symbol → CellML prefix name.
// Two-character prefixes first.
static const std::vector<std::pair<std::string, std::string>> kPrefixes = {
    {"da", "deca"},
    {"Y",  "yotta"}, {"Z",  "zetta"}, {"E",  "exa"},   {"P",  "peta"},
    {"T",  "tera"},  {"G",  "giga"},  {"M",  "mega"},  {"k",  "kilo"},
    {"h",  "hecto"},
    {"d",  "deci"},  {"c",  "centi"}, {"m",  "milli"}, {"u",  "micro"},
    {"n",  "nano"},  {"p",  "pico"},  {"f",  "femto"}, {"a",  "atto"},
    {"z",  "zepto"}, {"y",  "yocto"},
};

// Map CellML prefix name → SI symbol.
static std::unordered_map<std::string, std::string> buildPrefixNameToSymbol() {
    std::unordered_map<std::string, std::string> m;
    for (auto &p : kPrefixes) m[p.second] = p.first;
    return m;
}
static const auto &prefixNameToSymbol() {
    static auto m = buildPrefixNameToSymbol();
    return m;
}

// Map CellML standard unit name → SI symbol.
static std::unordered_map<std::string, std::string> buildCellmlToSymbol() {
    std::unordered_map<std::string, std::string> m;
    for (auto &p : kSiSymbols) m[p.second] = p.first;
    return m;
}
static const auto &cellmlToSymbol() {
    static auto m = buildCellmlToSymbol();
    return m;
}

// ===================================================================
// CellML standard unit names.
// ===================================================================

static const std::unordered_set<std::string> kStandardUnits = {
    "ampere", "becquerel", "candela", "coulomb", "dimensionless",
    "farad", "gram", "gray", "henry", "hertz", "joule", "katal",
    "kelvin", "kilogram", "litre", "lumen", "lux", "metre", "mole",
    "newton", "ohm", "pascal", "radian", "second", "siemens",
    "sievert", "steradian", "tesla", "volt", "watt", "weber",
};

bool isStandardUnit(const std::string &name) {
    return kStandardUnits.count(name) != 0;
}

// ===================================================================
// Decompose a single unit atom token (e.g. "mV", "cm", "kg") into
// (CellML unit name, CellML prefix name).
// ===================================================================

struct AtomDecomp {
    std::string unit;    // CellML standard unit name
    std::string prefix;  // CellML prefix name, or ""
    bool valid = false;
};

static AtomDecomp decomposeAtom(const std::string &tok) {
    // 1) Check if the entire token is a known SI symbol.
    auto &sm = symbolMap();
    auto it = sm.find(tok);
    if (it != sm.end())
        return {it->second, "", true};

    // 2) Try prefix decomposition (longest prefix first).
    for (auto &[psym, pname] : kPrefixes) {
        if (tok.size() > psym.size() && tok.substr(0, psym.size()) == psym) {
            std::string rest = tok.substr(psym.size());
            auto it2 = sm.find(rest);
            if (it2 != sm.end()) {
                // Special case: "kg" is already handled as a standalone
                // symbol above, but "mg" = milli + gram, not milli + "g" →
                // This is fine because "kg" matched first.
                return {it2->second, pname, true};
            }
        }
    }

    // 3) Check if the token is a CellML standard unit name itself
    //    (e.g. "dimensionless", "ohm" already handled above, but also
    //    allow full names like "volt", "second", etc.).
    if (kStandardUnits.count(tok))
        return {tok, "", true};

    return {{}, {}, false};
}

// ===================================================================
// Parse a text unit expression.
// Grammar:
//   unit_expr = unit_term { ("*" | "/") unit_term }
//   unit_term = unit_atom [ "^" ["-"] NUMBER ]
//   unit_atom = IDENTIFIER | "1" | "(" unit_expr ")"
//
// We do a lightweight parse directly on the string, not on tokens,
// because this is called from both the parser and the serializer.
// ===================================================================

struct UnitLexer {
    const std::string &src;
    size_t pos = 0;

    explicit UnitLexer(const std::string &s) : src(s) {}

    void skipWs() {
        while (pos < src.size() && src[pos] == ' ') ++pos;
    }

    bool atEnd() const { return pos >= src.size(); }

    char peek() const { return pos < src.size() ? src[pos] : '\0'; }
    char advance() { return src[pos++]; }

    bool match(char c) {
        skipWs();
        if (peek() == c) { advance(); return true; }
        return false;
    }

    // Read a contiguous identifier-like run (letters, digits, _).
    std::string readAtom() {
        skipWs();
        size_t start = pos;
        while (pos < src.size() && (std::isalnum(static_cast<unsigned char>(src[pos]))
                                    || src[pos] == '_'))
            ++pos;
        return src.substr(start, pos - start);
    }

    // Read a number (integer, float, or scientific notation).
    double readNumber() {
        skipWs();
        size_t start = pos;
        if (pos < src.size() && src[pos] == '-') ++pos;
        while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
            ++pos;
        if (pos < src.size() && src[pos] == '.') {
            ++pos;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                ++pos;
        }
        // Scientific notation: e/E followed by optional sign and digits.
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            ++pos;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) ++pos;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                ++pos;
        }
        return std::stod(src.substr(start, pos - start));
    }
};

static bool parseUnitTerm(UnitLexer &lex, std::vector<UnitFactor> &factors,
                          double sign);

static bool parseUnitExpr(UnitLexer &lex, std::vector<UnitFactor> &factors) {
    if (!parseUnitTerm(lex, factors, 1.0))
        return false;

    while (!lex.atEnd()) {
        lex.skipWs();
        char c = lex.peek();
        if (c == '*') {
            lex.advance();
            if (!parseUnitTerm(lex, factors, 1.0)) return false;
        } else if (c == '/') {
            lex.advance();
            if (!parseUnitTerm(lex, factors, -1.0)) return false;
        } else {
            break;
        }
    }
    return true;
}

static bool parseUnitTerm(UnitLexer &lex, std::vector<UnitFactor> &factors,
                          double sign) {
    lex.skipWs();

    // Handle parenthesised sub-expression.
    if (lex.peek() == '(') {
        lex.advance();
        size_t startIdx = factors.size();
        if (!parseUnitExpr(lex, factors)) return false;
        lex.skipWs();
        if (lex.peek() != ')') return false;
        lex.advance();

        // Apply exponent to newly added factors.
        double exp = 1.0;
        lex.skipWs();
        if (lex.peek() == '^') {
            lex.advance();
            exp = lex.readNumber();
        }
        for (size_t i = startIdx; i < factors.size(); ++i)
            factors[i].exponent *= exp * sign;
        return true;
    }

    // Handle numeric values (multipliers like 3600, 1e-3, or "1" as a
    // dimensionless base).  Numbers are treated as pure numeric factors;
    // the expression-level * and / operators combine them with unit atoms.
    if (std::isdigit(static_cast<unsigned char>(lex.peek()))) {
        double num = lex.readNumber();
        factors.push_back({"", "", sign, num});
        return true;
    }

    // Read atom.
    std::string atom = lex.readAtom();
    if (atom.empty()) return false;

    AtomDecomp ad = decomposeAtom(atom);
    if (!ad.valid) {
        // Treat as a custom/unknown unit reference.
        ad.unit = atom;
        ad.prefix = "";
        ad.valid = true;
    }

    double exp = 1.0;
    lex.skipWs();
    if (lex.peek() == '^') {
        lex.advance();
        lex.skipWs();
        bool negExp = false;
        if (lex.peek() == '-') { negExp = true; lex.advance(); }
        else if (lex.peek() == '(') {
            // Handle ^(-2) notation.
            lex.advance();
            lex.skipWs();
            negExp = (lex.peek() == '-');
            if (negExp) lex.advance();
            exp = lex.readNumber();
            if (negExp) exp = -exp;
            lex.skipWs();
            if (lex.peek() == ')') lex.advance();
            factors.push_back({ad.unit, ad.prefix, exp * sign, 1.0});
            return true;
        }
        exp = lex.readNumber();
        if (negExp) exp = -exp;
    }

    factors.push_back({ad.unit, ad.prefix, exp * sign, 1.0});
    return true;
}

// ===================================================================
// Public: parseUnitExpression
// ===================================================================

ParsedUnit parseUnitExpression(const std::string &text) {
    ParsedUnit pu;
    pu.textForm = text;

    if (text.empty()) return pu;

    // Handle "dimensionless" and "1" directly.
    if (text == "dimensionless" || text == "1") {
        pu.cellmlName = "dimensionless";
        pu.factors.push_back({"dimensionless", "", 1.0, 1.0});
        pu.valid = true;
        return pu;
    }

    UnitLexer lex(text);
    if (!parseUnitExpr(lex, pu.factors) || pu.factors.empty())
        return pu;

    // Post-process: fold pure numeric factors into multipliers.
    double accumulatedMultiplier = 1.0;
    std::vector<UnitFactor> realFactors;
    for (auto &f : pu.factors) {
        if (f.cellmlUnit.empty()) {
            // Pure number factor.
            accumulatedMultiplier *= std::pow(f.multiplier, f.exponent);
        } else {
            realFactors.push_back(f);
        }
    }
    if (!realFactors.empty() && accumulatedMultiplier != 1.0) {
        realFactors[0].multiplier *= accumulatedMultiplier;
    } else if (realFactors.empty()) {
        // All factors were numbers — treat as dimensionless.
        realFactors.push_back({"dimensionless", "", 1.0, accumulatedMultiplier});
    }
    pu.factors = realFactors;

    pu.valid = true;
    pu.cellmlName = unitTextToCellmlName(text);
    return pu;
}

// ===================================================================
// Generate a CellML-safe name from the text expression.
// ===================================================================

std::string unitTextToCellmlName(const std::string &text) {
    if (text == "1" || text == "dimensionless")
        return "dimensionless";

    // First check if it's a simple unit (single atom, possibly prefixed).
    // Parse it and see if we get a single factor with exponent 1.
    UnitLexer lex(text);
    std::vector<UnitFactor> factors;
    if (parseUnitExpr(lex, factors) && factors.size() == 1
        && factors[0].exponent == 1.0 && factors[0].multiplier == 1.0) {
        // Simple unit: just use the CellML name with prefix.
        std::string name = factors[0].cellmlPrefix.empty()
                             ? factors[0].cellmlUnit
                             : factors[0].cellmlPrefix + "_" + factors[0].cellmlUnit;
        return name;
    }

    // Build a descriptive name from the text.
    std::string name;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
            name += c;
        else if (c == '/')
            name += "_per_";
        else if (c == '*')
            name += "_";
        else if (c == '^')
            name += "";
        else if (c == '-')
            name += "neg";
        else if (c == '(' || c == ')')
            ; // skip
        else if (c == ' ')
            ; // skip
        else
            name += '_';
    }

    // Clean up double underscores and trailing underscores.
    std::string cleaned;
    bool lastUnderscore = false;
    for (char c : name) {
        if (c == '_') {
            if (!lastUnderscore && !cleaned.empty()) {
                cleaned += c;
                lastUnderscore = true;
            }
        } else {
            cleaned += c;
            lastUnderscore = false;
        }
    }
    while (!cleaned.empty() && cleaned.back() == '_')
        cleaned.pop_back();

    // CellML names must not start with a digit — strip leading digits/underscores.
    if (!cleaned.empty() && std::isdigit(static_cast<unsigned char>(cleaned[0]))) {
        size_t start = 0;
        while (start < cleaned.size()
               && (std::isdigit(static_cast<unsigned char>(cleaned[start]))
                   || cleaned[start] == '_'))
            ++start;
        cleaned = cleaned.substr(start);
    }
    if (cleaned.empty())
        cleaned = "unit";

    return cleaned;
}

// ===================================================================
// Create/ensure libCellML Units from a parsed unit.
// ===================================================================

libcellml::UnitsPtr ensureUnits(libcellml::ModelPtr &model,
                                const ParsedUnit &pu) {
    if (!pu.valid) return nullptr;

    // Dimensionless is built-in.
    if (pu.cellmlName == "dimensionless") return nullptr;

    // Check if this is a bare standard unit (no prefix, single factor, exp 1).
    if (pu.factors.size() == 1 && pu.factors[0].exponent == 1.0
        && pu.factors[0].cellmlPrefix.empty()
        && pu.factors[0].multiplier == 1.0
        && isStandardUnit(pu.factors[0].cellmlUnit)) {
        return nullptr;   // built-in, no definition needed
    }

    // Already defined in the model?
    if (model->hasUnits(pu.cellmlName))
        return model->units(pu.cellmlName);

    auto units = libcellml::Units::create(pu.cellmlName);
    for (auto &f : pu.factors) {
        if (f.cellmlPrefix.empty() && f.multiplier == 1.0) {
            units->addUnit(f.cellmlUnit, f.exponent);
        } else if (f.cellmlPrefix.empty()) {
            units->addUnit(f.cellmlUnit, "", f.exponent, f.multiplier);
        } else {
            units->addUnit(f.cellmlUnit, f.cellmlPrefix, f.exponent, f.multiplier);
        }
    }
    model->addUnits(units);
    return units;
}

// ===================================================================
// Reverse: CellML Units → compact text.
// ===================================================================

static std::string formatMultiplier(double m) {
    // Use scientific notation for very small or very large values.
    if (m == 0.0) return "0";
    double absm = std::abs(m);
    if (absm >= 1.0 && absm < 1e15 && m == std::floor(m)) {
        std::ostringstream os;
        os << static_cast<long long>(m);
        return os.str();
    }
    // Check for clean 1e-N forms.
    std::ostringstream os;
    os << m;
    return os.str();
}

// Map CellML prefix name → log10 exponent for multiplier conversion.
static double prefixNameToExponent(const std::string &name) {
    static const std::unordered_map<std::string, double> m = {
        {"yotta", 24}, {"zetta", 21}, {"exa", 18}, {"peta", 15},
        {"tera", 12}, {"giga", 9}, {"mega", 6}, {"kilo", 3},
        {"hecto", 2}, {"deca", 1},
        {"deci", -1}, {"centi", -2}, {"milli", -3}, {"micro", -6},
        {"nano", -9}, {"pico", -12}, {"femto", -15}, {"atto", -18},
        {"zepto", -21}, {"yocto", -24},
    };
    auto it = m.find(name);
    return (it != m.end()) ? it->second : 0.0;
}

// Recursively flatten unit factors, resolving any non-standard unit references
// through the model until all factors reference standard (SI) units.
static void flattenFactors(
    const libcellml::ModelPtr &model,
    const std::string &ref, const std::string &prefix,
    double exponent, double multiplier,
    std::vector<UnitFactor> &out, double &outMultiplier,
    int depth = 0) {
    // Standard unit — no further resolution needed.
    if (isStandardUnit(ref) || depth > 10) {
        outMultiplier *= multiplier;
        out.push_back({ref, prefix, exponent, 1.0});
        return;
    }
    // Try to resolve through the model.
    if (model && model->hasUnits(ref)) {
        auto refUnits = model->units(ref);
        size_t n = refUnits->unitCount();
        if (n > 0) {
            // Convert outer prefix to a multiplier.
            if (!prefix.empty()) {
                double pExp = prefixNameToExponent(prefix);
                outMultiplier *= std::pow(10.0, pExp * exponent);
            }
            outMultiplier *= multiplier;
            for (size_t i = 0; i < n; ++i) {
                std::string subRef, subPrefix, id;
                double subExp, subMult;
                refUnits->unitAttributes(i, subRef, subPrefix, subExp, subMult, id);
                flattenFactors(model, subRef, subPrefix,
                               subExp * exponent, subMult,
                               out, outMultiplier, depth + 1);
            }
            return;
        }
    }
    // Cannot resolve — keep as-is.
    outMultiplier *= multiplier;
    out.push_back({ref, prefix, exponent, 1.0});
}

std::string unitsToText(const libcellml::UnitsPtr &units,
                        const libcellml::ModelPtr &model) {
    if (!units) return "";

    std::string name = units->name();
    if (name == "dimensionless") return "dimensionless";
    if (isStandardUnit(name)) {
        auto &c2s = cellmlToSymbol();
        auto it = c2s.find(name);
        return (it != c2s.end()) ? it->second : name;
    }

    size_t count = units->unitCount();
    if (count == 0) return name;

    // Collect factors, flattening user-unit references when possible.
    std::vector<UnitFactor> allFactors;
    auto &c2s = cellmlToSymbol();
    auto &p2s = prefixNameToSymbol();
    double overallMultiplier = 1.0;

    for (size_t i = 0; i < count; ++i) {
        std::string ref, prefix, id;
        double exponent, multiplier;
        units->unitAttributes(i, ref, prefix, exponent, multiplier, id);

        if (model && !isStandardUnit(ref)) {
            flattenFactors(model, ref, prefix, exponent, multiplier,
                           allFactors, overallMultiplier);
        } else {
            overallMultiplier *= multiplier;
            allFactors.push_back({ref, prefix, exponent, 1.0});
        }
    }

    // Sort into numerator and denominator by exponent sign.
    std::vector<UnitFactor> numerator, denominator;
    for (auto &f : allFactors) {
        if (f.exponent > 0)
            numerator.push_back(f);
        else
            denominator.push_back(f);
    }

    auto formatAtom = [&](const UnitFactor &f, bool inDenom) -> std::string {
        auto cit = c2s.find(f.cellmlUnit);
        std::string sym = (cit != c2s.end()) ? cit->second : f.cellmlUnit;
        auto pit = p2s.find(f.cellmlPrefix);
        std::string psym = (pit != p2s.end()) ? pit->second : "";
        std::string result = psym + sym;

        double exp = inDenom ? -f.exponent : f.exponent;
        if (exp != 1.0) {
            if (exp == std::floor(exp))
                result += "^" + std::to_string(static_cast<int>(exp));
            else {
                std::ostringstream oss;
                oss << exp;
                result += "^" + oss.str();
            }
        }
        return result;
    };

    std::string result;

    // Prefix overall multiplier if not 1.0.
    if (overallMultiplier != 1.0) {
        result = formatMultiplier(overallMultiplier) + "*";
    }

    if (numerator.empty() && overallMultiplier == 1.0) {
        result += "1";
    } else {
        for (size_t i = 0; i < numerator.size(); ++i) {
            if (i > 0) result += "*";
            result += formatAtom(numerator[i], false);
        }
    }

    if (!denominator.empty()) {
        result += "/";
        if (denominator.size() > 1) result += "(";
        for (size_t i = 0; i < denominator.size(); ++i) {
            if (i > 0) result += "*";
            result += formatAtom(denominator[i], true);
        }
        if (denominator.size() > 1) result += ")";
    }

    return result;
}

// ===================================================================
// Check if a Units object is auto-derivable from SI symbols/prefixes.
// Returns true if the unit needs no explicit definition in the text.
// ===================================================================

static bool isAutoDerivedImpl(const libcellml::UnitsPtr &units,
                              const libcellml::ModelPtr &model,
                              int depth) {
    if (!units || depth > 10) return false;
    size_t count = units->unitCount();
    if (count == 0) return false;

    for (size_t i = 0; i < count; ++i) {
        std::string ref, prefix, id;
        double exponent, multiplier;
        units->unitAttributes(i, ref, prefix, exponent, multiplier, id);

        if (multiplier != 1.0) return false;
        if (isStandardUnit(ref)) continue;
        // Recursively check referenced user units.
        if (model && model->hasUnits(ref)) {
            if (!isAutoDerivedImpl(model->units(ref), model, depth + 1))
                return false;
            continue;
        }
        return false;
    }
    return true;
}

bool isAutoDerived(const libcellml::UnitsPtr &units,
                   const libcellml::ModelPtr &model) {
    return isAutoDerivedImpl(units, model, 0);
}

} // namespace cellmltext
