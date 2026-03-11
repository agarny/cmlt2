/*
 * CellML Text 2.0 — Public API
 *
 * This is the main header to include for using the CellML Text 2.0 library.
 * It provides functions to convert between CellML Text 2.0 (human-readable)
 * and CellML (XML via libCellML).
 */

#pragma once

#include <libcellml/model.h>

#include <string>
#include <vector>

namespace cellmltext {

// --- Error reporting ----------------------------------------------------

struct Error {
    size_t line   = 0;
    size_t column = 0;
    std::string message;
};

// --- Conversion functions -----------------------------------------------

// Parse a CellML Text 2.0 string and return a libCellML Model.
// Errors (if any) are appended to |errors|.
libcellml::ModelPtr textToModel(const std::string &cellmlText,
                                std::vector<Error> *errors = nullptr);

// Serialize a libCellML Model to CellML Text 2.0.
// Errors (if any) are appended to |errors|.
std::string modelToText(const libcellml::ModelPtr &model,
                        std::vector<Error> *errors = nullptr);

// --- Convenience: round-trip via CellML XML -----------------------------

// Convert CellML Text 2.0 → CellML XML string.
std::string textToCellML(const std::string &cellmlText,
                         std::vector<Error> *errors = nullptr);

// Convert CellML XML string → CellML Text 2.0.
std::string cellMLToText(const std::string &cellmlXml,
                         std::vector<Error> *errors = nullptr);

} // namespace cellmltext
