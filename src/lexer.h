/*
 * CellML Text 2.0 — Lexer
 */

#pragma once

#include "token.h"

#include <string>
#include <vector>

namespace cellmltext {

struct LexError {
    size_t line;
    size_t column;
    std::string message;
};

class Lexer {
public:
    explicit Lexer(const std::string &source);

    std::vector<Token> tokenize();
    const std::vector<LexError> &errors() const { return errors_; }

private:
    Token nextToken();
    Token makeToken(TokenType type, const std::string &value = "");
    Token readNumber();
    Token readIdentOrKeyword();
    Token readString();

    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    void skipLineComment();
    void skipWhitespace();
    bool atEnd() const;

    const std::string src_;
    size_t pos_    = 0;
    size_t line_   = 1;
    size_t column_ = 1;
    std::vector<LexError> errors_;
};

} // namespace cellmltext
