/*
 * CellML Text 2.0 — Serializer (implementation)
 *
 * Converts a libCellML Model → CellML Text 2.0 source.
 */

#include "serializer.h"
#include "ast.h"
#include "mathml.h"
#include "units.h"

#include <libcellml/component.h>
#include <libcellml/importsource.h>
#include <libcellml/model.h>
#include <libcellml/reset.h>
#include <libcellml/variable.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace cellmltext {

// ===================================================================
//  Expression AST → text string
// ===================================================================

static int precedence(const std::string &op) {
    if (op == "or")  return 1;
    if (op == "and") return 2;
    if (op == "==" || op == "!=" || op == "<" || op == ">"
        || op == "<=" || op == ">=") return 3;
    if (op == "+" || op == "-") return 4;
    if (op == "*" || op == "/") return 5;
    if (op == "^") return 7;
    return 0;
}

static std::string exprToText(const Expr *expr, int parentPrec = 0);

static std::string formatNumber(double v) {
    if (v == std::floor(v) && std::abs(v) < 1e15 && v != 0.0) {
        std::ostringstream os;
        os << static_cast<long long>(v);
        // Append .0 to avoid ambiguity with integers.
        return os.str() + ".0";
    }
    if (v == 0.0) return "0.0";
    std::ostringstream os;
    os << v;
    std::string s = os.str();
    // Ensure there's a decimal point.
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos
        && s.find('E') == std::string::npos)
        s += ".0";
    return s;
}

static std::string exprToText(const Expr *expr, int parentPrec) {
    if (!expr) return "???";

    switch (expr->kind) {
    case ExprKind::Number: {
        auto *n = static_cast<const NumberExpr *>(expr);
        return formatNumber(n->value);
    }
    case ExprKind::Identifier: {
        auto *id = static_cast<const IdentifierExpr *>(expr);
        return id->name;
    }
    case ExprKind::BinaryOp: {
        auto *b = static_cast<const BinaryOpExpr *>(expr);
        int prec = precedence(b->op);
        std::string left = exprToText(b->left.get(), prec);
        std::string right = exprToText(b->right.get(), prec + 1);
        std::string result;
        if (b->op == "^")
            result = left + "^" + right;
        else
            result = left + " " + b->op + " " + right;
        if (prec < parentPrec)
            result = "(" + result + ")";
        return result;
    }
    case ExprKind::UnaryOp: {
        auto *u = static_cast<const UnaryOpExpr *>(expr);
        if (u->op == "-") {
            std::string operand = exprToText(u->operand.get(), 6);
            return "-" + operand;
        }
        if (u->op == "not") {
            std::string operand = exprToText(u->operand.get(), 2);
            return "not " + operand;
        }
        return exprToText(u->operand.get(), parentPrec);
    }
    case ExprKind::FunctionCall: {
        auto *f = static_cast<const FunctionCallExpr *>(expr);
        std::string result = f->name + "(";
        for (size_t i = 0; i < f->args.size(); ++i) {
            if (i > 0) result += ", ";
            result += exprToText(f->args[i].get(), 0);
        }
        result += ")";
        return result;
    }
    case ExprKind::Derivative: {
        auto *d = static_cast<const DerivativeExpr *>(expr);
        return "d(" + d->variable + ")/d(" + d->bvar + ")";
    }
    case ExprKind::Piecewise: {
        auto *p = static_cast<const PiecewiseExpr *>(expr);
        std::string result = "{\n";
        for (auto &[val, cond] : p->pieces) {
            result += "    " + exprToText(val.get(), 0)
                    + "  when " + exprToText(cond.get(), 0) + "\n";
        }
        if (p->otherwise) {
            result += "    " + exprToText(p->otherwise.get(), 0) + "  otherwise\n";
        }
        result += "}";
        return result;
    }
    case ExprKind::Constant: {
        auto *c = static_cast<const ConstantExpr *>(expr);
        return c->name;
    }
    }
    return "???";
}

// ===================================================================
//  Serializer
// ===================================================================

std::string Serializer::serialize(const libcellml::ModelPtr &model) {
    output_.clear();
    errors_.clear();
    definedVarsCache_.clear();
    model_ = model;

    writeModel(model);
    return output_;
}

void Serializer::write(const std::string &text) {
    output_ += text;
}

void Serializer::writeLine(const std::string &text, int indent) {
    writeIndent(indent);
    output_ += text + "\n";
}

void Serializer::writeIndent(int indent) {
    for (int i = 0; i < indent; ++i)
        output_ += "  ";
}

void Serializer::newline() {
    output_ += "\n";
}

// ===================================================================
//  Model  —  Name { ... }
// ===================================================================

void Serializer::writeModel(const libcellml::ModelPtr &model) {
    writeLine(model->name() + " {");

    // Write unit definitions (non-standard, non-imported).
    writeUnitDefinitions(model, 1);

    // Write imports.
    writeImports(model, 1);

    // Write components (top-level only; children are nested inside).
    for (size_t i = 0; i < model->componentCount(); ++i) {
        auto comp = model->component(i);
        if (comp->isImport()) continue;
        writeComponent(comp, 1);
    }

    writeLine("}");
}

// ===================================================================
//  Component  —  nested with children inside
// ===================================================================

void Serializer::writeComponent(const libcellml::ComponentPtr &comp, int indent) {
    writeLine(comp->name() + " {", indent);

    // Variables.
    for (size_t i = 0; i < comp->variableCount(); ++i)
        writeVariable(comp->variable(i), comp, indent + 1);

    // Equations (from MathML).
    if (!comp->math().empty()) {
        if (comp->variableCount() > 0) newline();
        writeEquations(comp, indent + 1);
    }

    // Resets.
    if (comp->resetCount() > 0) {
        newline();
        writeResets(comp, indent + 1);
    }

    // Nested child components.
    for (size_t i = 0; i < comp->componentCount(); ++i) {
        auto child = comp->component(i);
        if (child->isImport()) continue;
        newline();
        writeComponent(child, indent + 1);
    }

    writeLine("}", indent);
}

// ===================================================================
//  Variable  —  either definition (units) or connection (comp.var)
// ===================================================================

void Serializer::writeVariable(const libcellml::VariablePtr &var,
                                const libcellml::ComponentPtr &ownerComp,
                                int indent) {
    // Determine if this variable should be written as a connection reference.
    if (var->equivalentVariableCount() > 0 && var->initialValue().empty()) {
        auto defined = getDefinedVarNames(ownerComp);
        if (defined.find(var->name()) == defined.end()) {
            // Not defined by any equation → write as connection.
            auto [compName, varName] = findConnectionTarget(var, ownerComp);
            if (!compName.empty()) {
                writeLine(var->name() + ": " + compName + "." + varName, indent);
                return;
            }
        }
    }

    // Write as a definition with units.
    std::string line = var->name() + ": ";

    auto units = var->units();
    // Look up the actual definition from the model if we only have a name stub.
    if (units && units->unitCount() == 0 && model_) {
        if (model_->hasUnits(units->name())) {
            units = model_->units(units->name());
        }
    }
    std::string unitText;
    if (units) {
        // For standard units, auto-derived units, and dimensionless: use compact form
        // with model-based flattening.  For non-auto-derived units: use their
        // defined name directly.
        if (isStandardUnit(units->name()) || units->name() == "dimensionless"
            || isAutoDerived(units, model_)) {
            unitText = unitsToText(units, model_);
        }
        if (unitText.empty())
            unitText = units->name();
    } else {
        unitText = "dimensionless";
    }
    line += unitText;

    // Initial value.
    std::string iv = var->initialValue();
    if (!iv.empty()) {
        line += " = " + iv;
    }

    writeLine(line, indent);
}

// ===================================================================
//  Connection detection helpers
// ===================================================================

std::set<std::string> Serializer::getDefinedVarNames(
    const libcellml::ComponentPtr &comp) {
    auto it = definedVarsCache_.find(comp->name());
    if (it != definedVarsCache_.end()) return it->second;

    std::set<std::string> names;
    std::string mathml = comp->math();
    if (!mathml.empty()) {
        std::vector<MathError> errors;
        auto equations = mathMLToEquations(mathml, &errors);

        for (auto &[lhs, rhs] : equations) {
            if (!lhs) continue;

            // Simple identifier on LHS: x = ...
            if (lhs->kind == ExprKind::Identifier)
                names.insert(static_cast<IdentifierExpr *>(lhs.get())->name);

            // Look for derivatives anywhere in LHS.
            std::function<void(const Expr *)> findDerivs;
            findDerivs = [&](const Expr *e) {
                if (!e) return;
                if (e->kind == ExprKind::Derivative) {
                    auto *d = static_cast<const DerivativeExpr *>(e);
                    names.insert(d->variable); // state variable
                    names.insert(d->bvar);     // bound variable (e.g. t)
                    return;
                }
                if (e->kind == ExprKind::BinaryOp) {
                    auto *b = static_cast<const BinaryOpExpr *>(e);
                    findDerivs(b->left.get());
                    findDerivs(b->right.get());
                }
                if (e->kind == ExprKind::UnaryOp) {
                    auto *u = static_cast<const UnaryOpExpr *>(e);
                    findDerivs(u->operand.get());
                }
            };
            findDerivs(lhs.get());
        }
    }

    definedVarsCache_[comp->name()] = names;
    return names;
}

std::pair<std::string, std::string> Serializer::findConnectionTarget(
    const libcellml::VariablePtr &var,
    const libcellml::ComponentPtr &ownerComp) {
    // BFS through the equivalence chain to find the "defining" variable
    // (the one with an initial value or computed by an equation).
    std::set<libcellml::VariablePtr> visited;
    std::vector<libcellml::VariablePtr> queue;

    for (size_t i = 0; i < var->equivalentVariableCount(); ++i)
        queue.push_back(var->equivalentVariable(i));
    visited.insert(var);

    size_t head = 0;
    while (head < queue.size()) {
        auto eqVar = queue[head++];
        if (visited.count(eqVar)) continue;
        visited.insert(eqVar);

        auto eqComp = std::dynamic_pointer_cast<libcellml::Component>(
            eqVar->parent());
        if (!eqComp) continue;

        // Is this variable a "definition"?
        if (!eqVar->initialValue().empty()) {
            return {eqComp->name(), eqVar->name()};
        }
        auto defined = getDefinedVarNames(eqComp);
        if (defined.count(eqVar->name())) {
            return {eqComp->name(), eqVar->name()};
        }

        // Enqueue further equivalences.
        for (size_t i = 0; i < eqVar->equivalentVariableCount(); ++i)
            queue.push_back(eqVar->equivalentVariable(i));
    }

    // No definition found — return the first direct equivalent.
    if (var->equivalentVariableCount() > 0) {
        auto eqVar = var->equivalentVariable(0);
        auto eqComp = std::dynamic_pointer_cast<libcellml::Component>(
            eqVar->parent());
        if (eqComp)
            return {eqComp->name(), eqVar->name()};
    }
    return {"", ""};
}

// ===================================================================
//  Equations (from MathML → text)
// ===================================================================

void Serializer::writeEquations(const libcellml::ComponentPtr &comp,
                                 int indent) {
    std::string mathml = comp->math();
    if (mathml.empty()) return;

    MathError err;
    std::vector<MathError> mathErrors;
    auto equations = mathMLToEquations(mathml, &mathErrors);

    for (auto &e : mathErrors)
        errors_.push_back({e.message});

    for (auto &[lhs, rhs] : equations) {
        if (!lhs || !rhs) continue;

        std::string lhsText = exprToText(lhs.get(), 0);
        std::string rhsText = exprToText(rhs.get(), 0);

        // Check if RHS is a piecewise — format specially.
        if (rhs && rhs->kind == ExprKind::Piecewise) {
            auto *pw = static_cast<PiecewiseExpr *>(rhs.get());
            writeIndent(indent);
            write(lhsText + " = {\n");
            for (auto &[val, cond] : pw->pieces) {
                writeIndent(indent + 1);
                write(exprToText(val.get(), 0) + "  when "
                      + exprToText(cond.get(), 0) + "\n");
            }
            if (pw->otherwise) {
                writeIndent(indent + 1);
                write(exprToText(pw->otherwise.get(), 0) + "  otherwise\n");
            }
            writeLine("}", indent);
        } else {
            writeLine(lhsText + " = " + rhsText, indent);
        }
    }
}

// ===================================================================
//  Resets
// ===================================================================

void Serializer::writeResets(const libcellml::ComponentPtr &comp, int indent) {
    for (size_t i = 0; i < comp->resetCount(); ++i) {
        auto reset = comp->reset(i);
        auto var = reset->variable();
        if (!var) continue;

        std::string line = "reset " + var->name()
                         + " at order " + std::to_string(reset->order())
                         + " when ";

        std::string testMathML = reset->testValue();
        if (!testMathML.empty()) {
            auto eqs = mathMLToEquations(testMathML);
            if (!eqs.empty() && eqs[0].first) {
                line += exprToText(eqs[0].first.get(), 0);
            } else {
                auto expr = mathMLFragmentToExpr(testMathML);
                if (expr) line += exprToText(expr.get(), 0);
                else line += "???";
            }
        }

        line += " {";
        writeLine(line, indent);

        std::string resetMathML = reset->resetValue();
        if (!resetMathML.empty()) {
            auto eqs = mathMLToEquations(resetMathML);
            if (!eqs.empty() && eqs[0].first) {
                std::string valText = exprToText(eqs[0].first.get(), 0);
                writeLine(var->name() + " = " + valText, indent + 1);
            }
        }

        writeLine("}", indent);
    }
}

// ===================================================================
//  Imports
// ===================================================================

void Serializer::writeImports(const libcellml::ModelPtr &model, int indent) {
    std::map<std::string, std::vector<std::string>> imports;

    std::function<void(const libcellml::ComponentPtr &)> walkComp;
    walkComp = [&](const libcellml::ComponentPtr &comp) {
        if (comp->isImport()) {
            auto is = comp->importSource();
            std::string url = is ? is->url() : "";
            std::string ref = comp->importReference();
            std::string entry = "component " + ref;
            if (ref != comp->name()) entry += " as " + comp->name();
            imports[url].push_back(entry);
        }
        for (size_t i = 0; i < comp->componentCount(); ++i)
            walkComp(comp->component(i));
    };

    for (size_t i = 0; i < model->componentCount(); ++i)
        walkComp(model->component(i));

    for (size_t i = 0; i < model->unitsCount(); ++i) {
        auto u = model->units(i);
        if (u->isImport()) {
            auto is = u->importSource();
            std::string url = is ? is->url() : "";
            std::string ref = u->importReference();
            std::string entry = "unit " + ref;
            if (ref != u->name()) entry += " as " + u->name();
            imports[url].push_back(entry);
        }
    }

    for (auto &[url, entries] : imports) {
        writeLine("import \"" + url + "\" {", indent);
        for (auto &e : entries)
            writeLine(e, indent + 1);
        writeLine("}", indent);
        newline();
    }
}

// ===================================================================
//  Unit definitions
// ===================================================================

void Serializer::writeUnitDefinitions(const libcellml::ModelPtr &model,
                                       int indent) {
    bool hasCustomUnits = false;
    for (size_t i = 0; i < model->unitsCount(); ++i) {
        auto u = model->units(i);
        if (u->isImport()) continue;
        if (isStandardUnit(u->name())) continue;

        // Skip auto-derived units (expressible via SI prefix + symbol inline).
        if (isAutoDerived(u, model)) continue;

        // Don't flatten user-unit references in definitions — referenced
        // units are either also defined or are standard/parseable.
        std::string text = unitsToText(u);
        if (!text.empty() && text != u->name()) {
            writeLine("unit " + u->name() + " = " + text, indent);
            hasCustomUnits = true;
        }
    }
    if (hasCustomUnits) newline();
}

} // namespace cellmltext
