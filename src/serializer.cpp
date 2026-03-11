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
        output_ += "    ";
}

void Serializer::newline() {
    output_ += "\n";
}

// ===================================================================
//  Model
// ===================================================================

void Serializer::writeModel(const libcellml::ModelPtr &model) {
    writeLine("model " + model->name());
    newline();

    // Write unit definitions (non-standard, non-imported).
    writeUnitDefinitions(model);

    // Write imports.
    writeImports(model);

    // Write components (top-level only; children are written recursively).
    for (size_t i = 0; i < model->componentCount(); ++i) {
        auto comp = model->component(i);
        if (comp->isImport()) continue;   // imported components handled above
        writeComponent(comp, 0);
        newline();
    }

    // Write connections.
    writeConnections(model);

    // Write encapsulation.
    writeEncapsulation(model);
}

// ===================================================================
//  Component
// ===================================================================

void Serializer::writeComponent(const libcellml::ComponentPtr &comp, int indent) {
    writeLine("component " + comp->name() + " {", indent);

    // Variables.
    for (size_t i = 0; i < comp->variableCount(); ++i)
        writeVariable(comp->variable(i), indent + 1);

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

    writeLine("}", indent);

    // Write child components (for encapsulated components that are nested in
    // the model hierarchy).
    for (size_t i = 0; i < comp->componentCount(); ++i) {
        if (comp->component(i)->isImport()) continue;
        newline();
        writeComponent(comp->component(i), indent);
    }
}

// ===================================================================
//  Variable
// ===================================================================

void Serializer::writeVariable(const libcellml::VariablePtr &var, int indent) {
    std::string line = var->name() + ": ";

    // Determine text unit.
    auto units = var->units();
    std::string unitText;
    if (units) {
        unitText = unitsToText(units);
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
//  Equations (from MathML → text)
// ===================================================================

void Serializer::writeEquations(const libcellml::ComponentPtr &comp, int indent) {
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

        // Parse test value MathML → expression.
        std::string testMathML = reset->testValue();
        if (!testMathML.empty()) {
            auto eqs = mathMLToEquations(testMathML);
            if (!eqs.empty() && eqs[0].first) {
                line += exprToText(eqs[0].first.get(), 0);
            } else {
                // Try as single expression.
                auto expr = mathMLFragmentToExpr(testMathML);
                if (expr) line += exprToText(expr.get(), 0);
                else line += "???";
            }
        }

        line += " {";
        writeLine(line, indent);

        // Parse reset value MathML.
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
//  Connections (variable equivalences → map statements)
// ===================================================================

void Serializer::writeConnections(const libcellml::ModelPtr &model) {
    // Collect all equivalences to avoid duplicates.
    std::set<std::pair<std::string, std::string>> written;

    std::function<void(const libcellml::ComponentPtr &)> walkComponent;
    walkComponent = [&](const libcellml::ComponentPtr &comp) {
        for (size_t vi = 0; vi < comp->variableCount(); ++vi) {
            auto var = comp->variable(vi);
            for (size_t ei = 0; ei < var->equivalentVariableCount(); ++ei) {
                auto eqVar = var->equivalentVariable(ei);
                if (!eqVar) continue;

                auto eqComp = std::dynamic_pointer_cast<libcellml::Component>(eqVar->parent());
                if (!eqComp) continue;

                // Create a canonical key to avoid writing both directions.
                std::string key1 = comp->name() + "." + var->name();
                std::string key2 = eqComp->name() + "." + eqVar->name();
                auto canonKey = (key1 < key2)
                    ? std::make_pair(key1, key2)
                    : std::make_pair(key2, key1);

                if (written.count(canonKey)) continue;
                written.insert(canonKey);

                writeLine("map " + key1 + " <-> " + key2);
            }
        }

        // Recurse into child components.
        for (size_t ci = 0; ci < comp->componentCount(); ++ci)
            walkComponent(comp->component(ci));
    };

    bool hasConnections = false;
    for (size_t i = 0; i < model->componentCount(); ++i) {
        walkComponent(model->component(i));
        if (!written.empty() && !hasConnections) {
            hasConnections = true;
        }
    }
    if (hasConnections) newline();
}

// ===================================================================
//  Encapsulation (hierarchy → group statements)
// ===================================================================

void Serializer::writeEncapsulation(const libcellml::ModelPtr &model) {
    std::function<void(const libcellml::ComponentPtr &)> writeGroup;
    writeGroup = [&](const libcellml::ComponentPtr &comp) {
        if (comp->componentCount() > 0) {
            write("group " + comp->name() + " contains {\n");
            for (size_t i = 0; i < comp->componentCount(); ++i)
                writeLine(comp->component(i)->name(), 1);
            writeLine("}");
            newline();
        }
        for (size_t i = 0; i < comp->componentCount(); ++i)
            writeGroup(comp->component(i));
    };

    for (size_t i = 0; i < model->componentCount(); ++i)
        writeGroup(model->component(i));
}

// ===================================================================
//  Imports
// ===================================================================

void Serializer::writeImports(const libcellml::ModelPtr &model) {
    // Group imported entities by their source URL.
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
        writeLine("import \"" + url + "\" {");
        for (auto &e : entries)
            writeLine(e, 1);
        writeLine("}");
        newline();
    }
}

// ===================================================================
//  Unit definitions
// ===================================================================

void Serializer::writeUnitDefinitions(const libcellml::ModelPtr &model) {
    for (size_t i = 0; i < model->unitsCount(); ++i) {
        auto u = model->units(i);
        if (u->isImport()) continue;
        if (isStandardUnit(u->name())) continue;

        std::string text = unitsToText(u);
        if (!text.empty() && text != u->name()) {
            writeLine("unit " + u->name() + " = " + text);
        } else {
            // Cannot express compactly — write as custom definition.
            writeLine("unit " + u->name() + " = " + u->name());
        }
    }
    bool hasCustomUnits = false;
    for (size_t i = 0; i < model->unitsCount(); ++i) {
        auto u = model->units(i);
        if (!u->isImport() && !isStandardUnit(u->name())) {
            hasCustomUnits = true;
            break;
        }
    }
    if (hasCustomUnits) newline();
}

} // namespace cellmltext
