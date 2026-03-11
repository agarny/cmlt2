/*
 * CellML Text 2.0 — MathML ↔ Expression AST conversion
 */

#pragma once

#include "ast.h"

#include <string>
#include <vector>

namespace cellmltext {

struct MathError {
    std::string message;
};

// Convert an expression AST to a MathML <math> string suitable for
// libCellML's Component::setMath().  |equations| is a list of (LHS, RHS)
// pairs; each pair produces one <apply><eq/>...</apply>.
std::string equationsToMathML(
    const std::vector<std::pair<ExprPtr, ExprPtr>> &equations,
    std::vector<MathError> *errors = nullptr);

// Convenience: single equation.
std::string equationToMathML(const ExprPtr &lhs, const ExprPtr &rhs,
                             std::vector<MathError> *errors = nullptr);

// Convert one expression AST node into its MathML fragment (no <math> wrapper).
std::string exprToMathML(const Expr *expr);

// Parse a MathML <math> string (from Component::math()) and return
// a list of (LHS, RHS) equation pairs as AST nodes.
std::vector<std::pair<ExprPtr, ExprPtr>>
mathMLToEquations(const std::string &mathml,
                  std::vector<MathError> *errors = nullptr);

// Parse a single MathML content-markup fragment into an expression AST node.
// (Operates on the child of an <apply> element.)
// |xml| is expected to be a well-formed XML fragment.
ExprPtr mathMLFragmentToExpr(const std::string &xml,
                             std::vector<MathError> *errors = nullptr);

} // namespace cellmltext
