/*
 * CellML Text 2.0 — Token definitions
 */

#pragma once

#include <ostream>
#include <string>

namespace cellmltext {

enum class TokenType {
    // Literals
    Number,         // 42, 3.14, 1e-3
    String,         // "path/to/file.cellml"
    Identifier,     // V, I_Na, membrane, etc.

    // Keywords
    Import,         // import
    As,             // as
    Unit,           // unit
    Reset,          // reset
    When,           // when
    At,             // at
    Order,          // order
    Otherwise,      // otherwise
    And,            // and
    Or,             // or
    Not,            // not

    // Constants
    True,           // true
    False,          // false
    Pi,             // pi
    Euler,          // e
    Inf,            // inf
    Nan,            // nan

    // Operators
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Caret,          // ^
    Equals,         // =
    EqEq,           // ==
    NotEq,          // !=
    Less,           // <
    Greater,        // >
    LessEq,         // <=
    GreaterEq,      // >=

    // Delimiters
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    Colon,          // :
    Comma,          // ,
    Dot,            // .

    // Special
    Newline,
    Eof,
};

struct Token {
    TokenType type;
    std::string value;     // literal text of the token
    size_t line   = 1;
    size_t column = 1;
};

inline const char *tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::Number:     return "Number";
        case TokenType::String:     return "String";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Import:     return "import";
        case TokenType::As:         return "as";
        case TokenType::Unit:       return "unit";
        case TokenType::Reset:      return "reset";
        case TokenType::When:       return "when";
        case TokenType::At:         return "at";
        case TokenType::Order:      return "order";
        case TokenType::Otherwise:  return "otherwise";
        case TokenType::And:        return "and";
        case TokenType::Or:         return "or";
        case TokenType::Not:        return "not";
        case TokenType::True:       return "true";
        case TokenType::False:      return "false";
        case TokenType::Pi:         return "pi";
        case TokenType::Euler:      return "e";
        case TokenType::Inf:        return "inf";
        case TokenType::Nan:        return "nan";
        case TokenType::Plus:       return "+";
        case TokenType::Minus:      return "-";
        case TokenType::Star:       return "*";
        case TokenType::Slash:      return "/";
        case TokenType::Caret:      return "^";
        case TokenType::Equals:     return "=";
        case TokenType::EqEq:       return "==";
        case TokenType::NotEq:      return "!=";
        case TokenType::Less:       return "<";
        case TokenType::Greater:    return ">";
        case TokenType::LessEq:     return "<=";
        case TokenType::GreaterEq:  return ">=";
        case TokenType::LParen:     return "(";
        case TokenType::RParen:     return ")";
        case TokenType::LBrace:     return "{";
        case TokenType::RBrace:     return "}";
        case TokenType::Colon:      return ":";
        case TokenType::Comma:      return ",";
        case TokenType::Dot:        return ".";
        case TokenType::Newline:    return "Newline";
        case TokenType::Eof:        return "EOF";
    }
    return "?";
}

inline std::ostream &operator<<(std::ostream &os, TokenType t) {
    return os << tokenTypeName(t);
}

} // namespace cellmltext
