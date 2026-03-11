/*
 * CellML Text 2.0 — Parser
 *
 * Converts CellML Text 2.0 source into a libCellML Model.
 */

#pragma once

#include "ast.h"
#include "token.h"

#include <libcellml/model.h>

#include <string>
#include <vector>

namespace cellmltext {

struct ParseError {
    size_t line;
    size_t column;
    std::string message;
};

class Parser {
public:
    // Parse CellML Text 2.0 source and return a libCellML Model.
    libcellml::ModelPtr parse(const std::string &source);

    const std::vector<ParseError> &errors() const { return errors_; }

private:
    // Token stream helpers.
    const Token &current() const;
    const Token &peekToken() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string &context);
    void skipNewlines();
    void error(const std::string &msg);

    // Top-level.
    void parseModel();
    void parseTopLevel();
    void parseComponent();
    void parseMapStatement();
    void parseGroupStatement();
    void parseImportStatement();
    void parseUnitDef();

    // Inside component.
    void parseComponentBody(const libcellml::ComponentPtr &comp);
    void parseVarDecl(const libcellml::ComponentPtr &comp);
    void parseEquation(const libcellml::ComponentPtr &comp);
    void parseResetStatement(const libcellml::ComponentPtr &comp);

    // Unit expression (after ':').
    std::string parseUnitExpr();

    // Expression parser (recursive descent, Pratt-style precedence).
    ExprPtr parseExpression();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseComparison();
    ExprPtr parseAddition();
    ExprPtr parseMultiplication();
    ExprPtr parseUnary();
    ExprPtr parsePower();
    ExprPtr parsePrimary();
    ExprPtr parsePiecewise();
    ExprPtr parseFunctionCall(const std::string &name);

    // Post-process AST to recognise derivative patterns.
    ExprPtr transformDerivatives(ExprPtr expr);

    std::vector<Token> tokens_;
    size_t pos_ = 0;
    std::vector<ParseError> errors_;
    libcellml::ModelPtr model_;
};

} // namespace cellmltext
