/*
 * CellML Text 2.0 — SI unit handling
 *
 * Parses compact unit expressions (e.g. "mV", "mS/cm^2") and converts
 * them to/from libCellML Units definitions.
 */

#pragma once

#include <string>
#include <vector>

#include <libcellml/units.h>
#include <libcellml/model.h>

namespace cellmltext {

// A single factor inside a compound unit expression, e.g. in "mS/cm^2":
//   { baseUnit="siemens", prefix="milli", exponent=1 }
//   { baseUnit="metre",   prefix="centi", exponent=-2 }
struct UnitFactor {
    std::string cellmlUnit;       // CellML standard unit name
    std::string cellmlPrefix;     // CellML prefix name, or "" for none
    double      exponent   = 1.0;
    double      multiplier = 1.0;
};

// Result of parsing a text unit expression.
struct ParsedUnit {
    std::string textForm;               // original text, e.g. "mS/cm^2"
    std::string cellmlName;             // generated CellML name, e.g. "mS_per_cm2"
    std::vector<UnitFactor> factors;
    bool valid = false;
};

// --- Public API ----------------------------------------------------------

// Parse a text unit expression such as "mV", "mS/cm^2", "1/ms", etc.
// into its component factors.
ParsedUnit parseUnitExpression(const std::string &text);

// Generate a CellML-safe name from the text unit expression.
std::string unitTextToCellmlName(const std::string &text);

// Create a libCellML UnitsPtr from the parsed unit, and add it to the model
// if an identically-named units does not already exist.
libcellml::UnitsPtr ensureUnits(libcellml::ModelPtr &model,
                                const ParsedUnit &pu);

// Given a libCellML Units object, produce the compact text representation.
// Returns empty string if the units cannot be represented compactly.
std::string unitsToText(const libcellml::UnitsPtr &units);

// Checks whether the given name is a CellML built-in (standard) unit.
bool isStandardUnit(const std::string &name);

// Checks whether a Units object is auto-derivable from its inline text form
// (i.e., the parser can re-create it from the compact notation). This is true
// when all factors reference standard units or model-defined units, with
// multiplier 1.0.
bool isAutoDerived(const libcellml::UnitsPtr &units,
                   const libcellml::ModelPtr &model = nullptr);

} // namespace cellmltext
