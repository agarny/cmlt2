/*
 * CellML Text 2.0 — Expression AST
 *
 * Shared between the parser (text → AST), the MathML converter, and the
 * serializer (AST → text).
 */

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cellmltext {

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class ExprKind {
    Number,
    Identifier,
    BinaryOp,
    UnaryOp,
    FunctionCall,
    Derivative,
    Piecewise,
    Constant,
};

// -- Base ------------------------------------------------------------------
struct Expr {
    ExprKind kind;
    virtual ~Expr() = default;

protected:
    explicit Expr(ExprKind k) : kind(k) {}
};

// -- Concrete node types ---------------------------------------------------

struct NumberExpr : Expr {
    double value;
    explicit NumberExpr(double v) : Expr(ExprKind::Number), value(v) {}
};

struct IdentifierExpr : Expr {
    std::string name;
    explicit IdentifierExpr(std::string n)
        : Expr(ExprKind::Identifier), name(std::move(n)) {}
};

struct BinaryOpExpr : Expr {
    std::string op;   // "+", "-", "*", "/", "^", "==", "!=", "<", ">",
                      // "<=", ">=", "and", "or"
    ExprPtr left;
    ExprPtr right;
    BinaryOpExpr(std::string o, ExprPtr l, ExprPtr r)
        : Expr(ExprKind::BinaryOp), op(std::move(o)),
          left(std::move(l)), right(std::move(r)) {}
};

struct UnaryOpExpr : Expr {
    std::string op;   // "-", "not"
    ExprPtr operand;
    UnaryOpExpr(std::string o, ExprPtr e)
        : Expr(ExprKind::UnaryOp), op(std::move(o)), operand(std::move(e)) {}
};

struct FunctionCallExpr : Expr {
    std::string name;
    std::vector<ExprPtr> args;
    FunctionCallExpr(std::string n, std::vector<ExprPtr> a)
        : Expr(ExprKind::FunctionCall), name(std::move(n)),
          args(std::move(a)) {}
};

struct DerivativeExpr : Expr {
    std::string variable;   // the differentiated variable, e.g. "V"
    std::string bvar;       // the bound variable, e.g. "t"
    DerivativeExpr(std::string var, std::string bv)
        : Expr(ExprKind::Derivative), variable(std::move(var)),
          bvar(std::move(bv)) {}
};

struct PiecewiseExpr : Expr {
    // Each piece: (value_expr, condition_expr).
    std::vector<std::pair<ExprPtr, ExprPtr>> pieces;
    ExprPtr otherwise;   // may be nullptr if absent

    PiecewiseExpr() : Expr(ExprKind::Piecewise) {}
};

struct ConstantExpr : Expr {
    std::string name;     // "pi", "e", "inf", "nan", "true", "false"
    explicit ConstantExpr(std::string n)
        : Expr(ExprKind::Constant), name(std::move(n)) {}
};

// -- Helpers ---------------------------------------------------------------

inline ExprPtr makeNumber(double v) {
    return std::make_unique<NumberExpr>(v);
}
inline ExprPtr makeId(const std::string &n) {
    return std::make_unique<IdentifierExpr>(n);
}
inline ExprPtr makeBinOp(const std::string &op, ExprPtr l, ExprPtr r) {
    return std::make_unique<BinaryOpExpr>(op, std::move(l), std::move(r));
}
inline ExprPtr makeUnaryOp(const std::string &op, ExprPtr e) {
    return std::make_unique<UnaryOpExpr>(op, std::move(e));
}
inline ExprPtr makeCall(const std::string &name, std::vector<ExprPtr> args) {
    return std::make_unique<FunctionCallExpr>(name, std::move(args));
}
inline ExprPtr makeDeriv(const std::string &var, const std::string &bvar) {
    return std::make_unique<DerivativeExpr>(var, bvar);
}
inline ExprPtr makeConst(const std::string &name) {
    return std::make_unique<ConstantExpr>(name);
}

} // namespace cellmltext
