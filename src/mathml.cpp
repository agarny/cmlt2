/*
 * CellML Text 2.0 — MathML ↔ Expression AST conversion (implementation)
 *
 * Uses pugixml to parse MathML content markup from libCellML and to
 * generate MathML from the expression AST.
 */

#include "mathml.h"

#include <pugixml.hpp>

#include <cmath>
#include <sstream>
#include <unordered_map>

namespace cellmltext {

// ===================================================================
//  Constants
// ===================================================================

static const char *kMathNS  = "http://www.w3.org/1998/Math/MathML";
static const char *kCellMLNS = "http://www.cellml.org/cellml/2.0#";

// ===================================================================
//  AST → MathML
// ===================================================================

// Forward declaration.
static void exprToMathMLImpl(std::ostringstream &os, const Expr *expr);

static void writeNumber(std::ostringstream &os, double v) {
    os << "<cn cellml:units=\"dimensionless\">";
    if (v == std::floor(v) && std::abs(v) < 1e15)
        os << static_cast<long long>(v);
    else
        os << v;
    os << "</cn>";
}

static void writeBinaryApply(std::ostringstream &os, const std::string &op,
                             const Expr *left, const Expr *right) {
    static const std::unordered_map<std::string, std::string> opMap = {
        {"+",  "plus"},   {"-",  "minus"},  {"*",  "times"},
        {"/",  "divide"}, {"^",  "power"},
        {"==", "eq"},     {"!=", "neq"},
        {"<",  "lt"},     {">",  "gt"},
        {"<=", "leq"},    {">=", "geq"},
        {"and","and"},    {"or", "or"},
    };
    auto it = opMap.find(op);
    std::string elem = (it != opMap.end()) ? it->second : op;

    os << "<apply><" << elem << "/>";
    exprToMathMLImpl(os, left);
    exprToMathMLImpl(os, right);
    os << "</apply>";
}

static void exprToMathMLImpl(std::ostringstream &os, const Expr *expr) {
    if (!expr) return;

    switch (expr->kind) {
    case ExprKind::Number: {
        auto *n = static_cast<const NumberExpr *>(expr);
        writeNumber(os, n->value);
        break;
    }
    case ExprKind::Identifier: {
        auto *id = static_cast<const IdentifierExpr *>(expr);
        os << "<ci>" << id->name << "</ci>";
        break;
    }
    case ExprKind::BinaryOp: {
        auto *b = static_cast<const BinaryOpExpr *>(expr);
        writeBinaryApply(os, b->op, b->left.get(), b->right.get());
        break;
    }
    case ExprKind::UnaryOp: {
        auto *u = static_cast<const UnaryOpExpr *>(expr);
        if (u->op == "-") {
            os << "<apply><minus/>";
            exprToMathMLImpl(os, u->operand.get());
            os << "</apply>";
        } else if (u->op == "not") {
            os << "<apply><not/>";
            exprToMathMLImpl(os, u->operand.get());
            os << "</apply>";
        }
        break;
    }
    case ExprKind::FunctionCall: {
        auto *f = static_cast<const FunctionCallExpr *>(expr);
        // Map text function name → MathML element.
        static const std::unordered_map<std::string, std::string> fnMap = {
            {"exp",   "exp"},     {"ln",    "ln"},
            {"log10", "log"},     {"log2",  "log"},
            {"sqrt",  "root"},    {"abs",   "abs"},
            {"floor", "floor"},   {"ceil",  "ceiling"},
            {"sin",   "sin"},     {"cos",   "cos"},     {"tan",   "tan"},
            {"asin",  "arcsin"},  {"acos",  "arccos"},  {"atan",  "arctan"},
            {"sinh",  "sinh"},    {"cosh",  "cosh"},    {"tanh",  "tanh"},
            {"sec",   "sec"},     {"csc",   "csc"},     {"cot",   "cot"},
            {"sech",  "sech"},    {"csch",  "csch"},    {"coth",  "coth"},
            {"min",   "min"},     {"max",   "max"},     {"rem",   "rem"},
        };

        auto it = fnMap.find(f->name);
        std::string elem = (it != fnMap.end()) ? it->second : f->name;

        if (f->name == "sqrt") {
            // MathML: <apply><root/><degree><cn ...>2</cn></degree> arg </apply>
            os << "<apply><root/>";
            if (!f->args.empty()) exprToMathMLImpl(os, f->args[0].get());
            os << "</apply>";
        } else if (f->name == "log10") {
            os << "<apply><log/><logbase><cn cellml:units=\"dimensionless\">10</cn></logbase>";
            if (!f->args.empty()) exprToMathMLImpl(os, f->args[0].get());
            os << "</apply>";
        } else if (f->name == "log2") {
            os << "<apply><log/><logbase><cn cellml:units=\"dimensionless\">2</cn></logbase>";
            if (!f->args.empty()) exprToMathMLImpl(os, f->args[0].get());
            os << "</apply>";
        } else {
            os << "<apply><" << elem << "/>";
            for (auto &a : f->args)
                exprToMathMLImpl(os, a.get());
            os << "</apply>";
        }
        break;
    }
    case ExprKind::Derivative: {
        auto *d = static_cast<const DerivativeExpr *>(expr);
        os << "<apply><diff/>"
           << "<bvar><ci>" << d->bvar << "</ci></bvar>"
           << "<ci>" << d->variable << "</ci>"
           << "</apply>";
        break;
    }
    case ExprKind::Piecewise: {
        auto *p = static_cast<const PiecewiseExpr *>(expr);
        os << "<piecewise>";
        for (auto &[val, cond] : p->pieces) {
            os << "<piece>";
            exprToMathMLImpl(os, val.get());
            exprToMathMLImpl(os, cond.get());
            os << "</piece>";
        }
        if (p->otherwise) {
            os << "<otherwise>";
            exprToMathMLImpl(os, p->otherwise.get());
            os << "</otherwise>";
        }
        os << "</piecewise>";
        break;
    }
    case ExprKind::Constant: {
        auto *c = static_cast<const ConstantExpr *>(expr);
        if (c->name == "pi")    os << "<pi/>";
        else if (c->name == "e")     os << "<exponentiale/>";
        else if (c->name == "true")  os << "<true/>";
        else if (c->name == "false") os << "<false/>";
        else if (c->name == "inf")   os << "<infinity/>";
        else if (c->name == "nan")   os << "<notanumber/>";
        break;
    }
    }
}

std::string exprToMathML(const Expr *expr) {
    std::ostringstream os;
    exprToMathMLImpl(os, expr);
    return os.str();
}

std::string equationToMathML(const ExprPtr &lhs, const ExprPtr &rhs,
                             std::vector<MathError> *) {
    std::ostringstream os;
    os << "<math xmlns=\"" << kMathNS << "\" xmlns:cellml=\"" << kCellMLNS << "\">";
    os << "<apply><eq/>";
    exprToMathMLImpl(os, lhs.get());
    exprToMathMLImpl(os, rhs.get());
    os << "</apply>";
    os << "</math>";
    return os.str();
}

std::string equationsToMathML(
    const std::vector<std::pair<ExprPtr, ExprPtr>> &equations,
    std::vector<MathError> *) {
    if (equations.empty()) return "";

    std::ostringstream os;
    os << "<math xmlns=\"" << kMathNS << "\" xmlns:cellml=\"" << kCellMLNS << "\">";
    for (auto &[lhs, rhs] : equations) {
        os << "<apply><eq/>";
        exprToMathMLImpl(os, lhs.get());
        exprToMathMLImpl(os, rhs.get());
        os << "</apply>";
    }
    os << "</math>";
    return os.str();
}

// ===================================================================
//  MathML → AST
// ===================================================================

static ExprPtr nodeToExpr(const pugi::xml_node &node);

static std::string localName(const pugi::xml_node &node) {
    std::string name = node.name();
    // Strip namespace prefix if any.
    auto pos = name.find(':');
    if (pos != std::string::npos)
        name = name.substr(pos + 1);
    return name;
}

// Get the child elements of a node (ignoring text nodes).
static std::vector<pugi::xml_node> childElements(const pugi::xml_node &node) {
    std::vector<pugi::xml_node> elems;
    for (auto child = node.first_child(); child; child = child.next_sibling()) {
        if (child.type() == pugi::node_element)
            elems.push_back(child);
    }
    return elems;
}

// Map MathML operator elements to text operators.
static const std::unordered_map<std::string, std::string> kMathMLBinOps = {
    {"plus",   "+"}, {"minus",  "-"}, {"times",  "*"},
    {"divide", "/"}, {"power",  "^"},
    {"eq",     "=="}, {"neq",   "!="}, {"lt", "<"}, {"gt", ">"},
    {"leq",    "<="}, {"geq",   ">="}, {"and", "and"}, {"or", "or"},
};

static const std::unordered_map<std::string, std::string> kMathMLFunctions = {
    {"exp",       "exp"},     {"ln",        "ln"},
    {"log",       "log10"},   // default log → log10; adjusted if logbase present
    {"root",      "sqrt"},    {"abs",       "abs"},
    {"floor",     "floor"},   {"ceiling",   "ceil"},
    {"sin",       "sin"},     {"cos",       "cos"},     {"tan",       "tan"},
    {"arcsin",    "asin"},    {"arccos",    "acos"},    {"arctan",    "atan"},
    {"sinh",      "sinh"},    {"cosh",      "cosh"},    {"tanh",      "tanh"},
    {"sec",       "sec"},     {"csc",       "csc"},     {"cot",       "cot"},
    {"sech",      "sech"},    {"csch",      "csch"},    {"coth",      "coth"},
    {"min",       "min"},     {"max",       "max"},     {"rem",       "rem"},
    {"not",       "not"},
};

static ExprPtr parseApply(const pugi::xml_node &node) {
    auto children = childElements(node);
    if (children.empty()) return nullptr;

    std::string opName = localName(children[0]);

    // --- diff (derivative) ---
    if (opName == "diff") {
        std::string variable, bvar;
        for (size_t i = 1; i < children.size(); ++i) {
            std::string cn = localName(children[i]);
            if (cn == "bvar") {
                auto bvarChildren = childElements(children[i]);
                for (auto &bc : bvarChildren) {
                    if (localName(bc) == "ci")
                        bvar = bc.child_value();
                }
            } else if (cn == "ci") {
                variable = children[i].child_value();
            }
        }
        return makeDeriv(variable, bvar);
    }

    // --- eq (used at equation level, handled by caller) ---
    if (opName == "eq" && children.size() == 3) {
        // Return a BinaryOp so the caller can split into LHS/RHS.
        return makeBinOp("==", nodeToExpr(children[1]), nodeToExpr(children[2]));
    }

    // --- Binary operators ---
    auto binIt = kMathMLBinOps.find(opName);
    if (binIt != kMathMLBinOps.end()) {
        // Handle n-ary operators (e.g. <plus/> with 3+ operands).
        if (children.size() == 2) {
            // Unary minus/plus.
            if (opName == "minus")
                return makeUnaryOp("-", nodeToExpr(children[1]));
            return nodeToExpr(children[1]);
        }
        ExprPtr result = nodeToExpr(children[1]);
        for (size_t i = 2; i < children.size(); ++i)
            result = makeBinOp(binIt->second, std::move(result), nodeToExpr(children[i]));
        return result;
    }

    // --- Functions ---
    auto fnIt = kMathMLFunctions.find(opName);
    if (fnIt != kMathMLFunctions.end()) {
        std::string fnName = fnIt->second;

        // Special handling for <log> with <logbase>.
        if (opName == "log") {
            double logBase = 10.0;
            std::vector<ExprPtr> args;
            for (size_t i = 1; i < children.size(); ++i) {
                std::string cn = localName(children[i]);
                if (cn == "logbase") {
                    auto lbChildren = childElements(children[i]);
                    if (!lbChildren.empty()) {
                        auto baseExpr = nodeToExpr(lbChildren[0]);
                        if (auto *num = dynamic_cast<NumberExpr *>(baseExpr.get()))
                            logBase = num->value;
                    }
                } else {
                    args.push_back(nodeToExpr(children[i]));
                }
            }
            if (logBase == 2.0) fnName = "log2";
            else if (logBase == 10.0) fnName = "log10";
            else fnName = "ln"; // fallback

            return makeCall(fnName, std::move(args));
        }

        // Special handling for <root> with <degree>.
        if (opName == "root") {
            std::vector<ExprPtr> args;
            for (size_t i = 1; i < children.size(); ++i) {
                std::string cn = localName(children[i]);
                if (cn == "degree") continue; // ignore degree for sqrt
                args.push_back(nodeToExpr(children[i]));
            }
            return makeCall("sqrt", std::move(args));
        }

        // Unary "not".
        if (opName == "not" && children.size() >= 2)
            return makeUnaryOp("not", nodeToExpr(children[1]));

        // General function call.
        std::vector<ExprPtr> args;
        for (size_t i = 1; i < children.size(); ++i)
            args.push_back(nodeToExpr(children[i]));
        return makeCall(fnName, std::move(args));
    }

    // Unknown operator — wrap as a generic call.
    std::vector<ExprPtr> args;
    for (size_t i = 1; i < children.size(); ++i)
        args.push_back(nodeToExpr(children[i]));
    return makeCall(opName, std::move(args));
}

static ExprPtr parsePiecewise(const pugi::xml_node &node) {
    auto pw = std::make_unique<PiecewiseExpr>();
    for (auto child = node.first_child(); child; child = child.next_sibling()) {
        if (child.type() != pugi::node_element) continue;
        std::string cn = localName(child);
        if (cn == "piece") {
            auto elems = childElements(child);
            if (elems.size() >= 2)
                pw->pieces.emplace_back(nodeToExpr(elems[0]), nodeToExpr(elems[1]));
        } else if (cn == "otherwise") {
            auto elems = childElements(child);
            if (!elems.empty())
                pw->otherwise = nodeToExpr(elems[0]);
        }
    }
    return pw;
}

static ExprPtr nodeToExpr(const pugi::xml_node &node) {
    if (!node) return nullptr;

    std::string name = localName(node);

    if (name == "cn") {
        double val = 0;
        try { val = std::stod(node.child_value()); } catch (...) {}
        return makeNumber(val);
    }
    if (name == "ci") {
        return makeId(node.child_value());
    }
    if (name == "apply") {
        return parseApply(node);
    }
    if (name == "piecewise") {
        return parsePiecewise(node);
    }
    // Constants.
    if (name == "pi")             return makeConst("pi");
    if (name == "exponentiale")   return makeConst("e");
    if (name == "true")           return makeConst("true");
    if (name == "false")          return makeConst("false");
    if (name == "infinity")       return makeConst("inf");
    if (name == "notanumber")     return makeConst("nan");

    return nullptr;
}

std::vector<std::pair<ExprPtr, ExprPtr>>
mathMLToEquations(const std::string &mathml, std::vector<MathError> *errors) {
    std::vector<std::pair<ExprPtr, ExprPtr>> result;
    if (mathml.empty()) return result;

    pugi::xml_document doc;
    auto parseResult = doc.load_string(mathml.c_str(),
        pugi::parse_default | pugi::parse_ws_pcdata);
    if (!parseResult) {
        if (errors)
            errors->push_back({"Failed to parse MathML: "
                                + std::string(parseResult.description())});
        return result;
    }

    // Find the <math> element (may have namespace prefix).
    pugi::xml_node mathNode;
    for (auto child = doc.first_child(); child; child = child.next_sibling()) {
        std::string n = localName(child);
        if (n == "math") { mathNode = child; break; }
    }
    if (!mathNode) mathNode = doc.first_child();

    // Each child <apply> of <math> should be an equation.
    for (auto child = mathNode.first_child(); child; child = child.next_sibling()) {
        if (child.type() != pugi::node_element) continue;
        std::string cn = localName(child);
        if (cn != "apply") continue;

        auto elems = childElements(child);
        if (elems.empty()) continue;

        std::string opName = localName(elems[0]);
        if (opName == "eq" && elems.size() >= 3) {
            ExprPtr lhs = nodeToExpr(elems[1]);
            ExprPtr rhs = nodeToExpr(elems[2]);
            result.emplace_back(std::move(lhs), std::move(rhs));
        } else {
            // Non-equation apply — wrap as a single expression.
            ExprPtr expr = parseApply(child);
            result.emplace_back(std::move(expr), nullptr);
        }
    }

    return result;
}

ExprPtr mathMLFragmentToExpr(const std::string &xml, std::vector<MathError> *errors) {
    if (xml.empty()) return nullptr;

    pugi::xml_document doc;
    auto res = doc.load_string(xml.c_str());
    if (!res) {
        if (errors)
            errors->push_back({"Failed to parse MathML fragment: "
                                + std::string(res.description())});
        return nullptr;
    }

    return nodeToExpr(doc.first_child());
}

} // namespace cellmltext
