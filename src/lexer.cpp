/*
 * CellML Text 2.0 — Lexer (implementation)
 */

#include "lexer.h"

#include <cctype>
#include <unordered_map>

namespace cellmltext {

// ===================================================================
// Keyword table
// ===================================================================

static const std::unordered_map<std::string, TokenType> kKeywords = {
    {"model",         TokenType::Model},
    {"component",     TokenType::Component},
    {"map",           TokenType::Map},
    {"group",         TokenType::Group},
    {"contains",      TokenType::Contains},
    {"import",        TokenType::Import},
    {"as",            TokenType::As},
    {"unit",          TokenType::Unit},
    {"reset",         TokenType::Reset},
    {"when",          TokenType::When},
    {"at",            TokenType::At},
    {"order",         TokenType::Order},
    {"otherwise",     TokenType::Otherwise},
    {"and",           TokenType::And},
    {"or",            TokenType::Or},
    {"not",           TokenType::Not},
    {"true",          TokenType::True},
    {"false",         TokenType::False},
    {"pi",            TokenType::Pi},
    {"inf",           TokenType::Inf},
    {"nan",           TokenType::Nan},
    // Note: "e" is NOT a keyword — it is contextually resolved as
    //       the Euler constant only in expression context by the parser,
    //       because it is commonly used as a variable name.
};

// ===================================================================
// Lexer
// ===================================================================

Lexer::Lexer(const std::string &source) : src_(source) {}

char Lexer::peek() const {
    return atEnd() ? '\0' : src_[pos_];
}

char Lexer::peekNext() const {
    return (pos_ + 1 < src_.size()) ? src_[pos_ + 1] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { ++line_; column_ = 1; }
    else           { ++column_; }
    return c;
}

bool Lexer::match(char expected) {
    if (atEnd() || src_[pos_] != expected) return false;
    advance();
    return true;
}

bool Lexer::atEnd() const { return pos_ >= src_.size(); }

Token Lexer::makeToken(TokenType type, const std::string &value) {
    return Token{type, value, line_, column_};
}

void Lexer::skipLineComment() {
    while (!atEnd() && peek() != '\n') advance();
}

void Lexer::skipWhitespace() {
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r')
            advance();
        else
            break;
    }
}

Token Lexer::readNumber() {
    size_t startLine = line_, startCol = column_;
    size_t start = pos_;

    while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
        advance();

    // Decimal part.
    if (!atEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        advance(); // consume '.'
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
            advance();
    }

    // Exponent part (scientific notation).
    if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
        // Be careful: "e" could also be the Euler constant or a variable.
        // Only consume if followed by digit, '+', or '-'.
        char next = peekNext();
        if (std::isdigit(static_cast<unsigned char>(next))
            || next == '+' || next == '-') {
            advance(); // consume 'e'/'E'
            if (peek() == '+' || peek() == '-') advance();
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
                advance();
        }
    }

    std::string text = src_.substr(start, pos_ - start);
    return Token{TokenType::Number, text, startLine, startCol};
}

Token Lexer::readIdentOrKeyword() {
    size_t startLine = line_, startCol = column_;
    size_t start = pos_;

    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek()))
                        || peek() == '_'))
        advance();

    std::string text = src_.substr(start, pos_ - start);

    // Check keywords.
    auto it = kKeywords.find(text);
    if (it != kKeywords.end())
        return Token{it->second, text, startLine, startCol};

    return Token{TokenType::Identifier, text, startLine, startCol};
}

Token Lexer::readString() {
    size_t startLine = line_, startCol = column_;
    advance(); // consume opening '"'
    size_t start = pos_;

    while (!atEnd() && peek() != '"' && peek() != '\n')
        advance();

    std::string text = src_.substr(start, pos_ - start);

    if (!atEnd() && peek() == '"')
        advance(); // consume closing '"'
    else
        errors_.push_back({startLine, startCol, "Unterminated string literal"});

    return Token{TokenType::String, text, startLine, startCol};
}

Token Lexer::nextToken() {
    skipWhitespace();

    if (atEnd()) return makeToken(TokenType::Eof);

    size_t startLine = line_, startCol = column_;
    char c = peek();

    // Newline.
    if (c == '\n') {
        advance();
        return Token{TokenType::Newline, "\\n", startLine, startCol};
    }

    // Comment.
    if (c == '/' && peekNext() == '/') {
        skipLineComment();
        // Return a newline to mark end of comment line.
        return Token{TokenType::Newline, "\\n", startLine, startCol};
    }

    // String.
    if (c == '"')
        return readString();

    // Number.
    if (std::isdigit(static_cast<unsigned char>(c)))
        return readNumber();

    // Dot followed by digit → number (e.g. .5)
    if (c == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        return readNumber();
    }

    // Identifier or keyword.
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
        return readIdentOrKeyword();

    // Operators and delimiters.
    advance();
    switch (c) {
    case '+': return Token{TokenType::Plus,    "+",  startLine, startCol};
    case '-': return Token{TokenType::Minus,   "-",  startLine, startCol};
    case '*': return Token{TokenType::Star,    "*",  startLine, startCol};
    case '/': return Token{TokenType::Slash,   "/",  startLine, startCol};
    case '^': return Token{TokenType::Caret,   "^",  startLine, startCol};
    case '(': return Token{TokenType::LParen,  "(",  startLine, startCol};
    case ')': return Token{TokenType::RParen,  ")",  startLine, startCol};
    case '{': return Token{TokenType::LBrace,  "{",  startLine, startCol};
    case '}': return Token{TokenType::RBrace,  "}",  startLine, startCol};
    case ':': return Token{TokenType::Colon,   ":",  startLine, startCol};
    case ',': return Token{TokenType::Comma,   ",",  startLine, startCol};
    case '.': return Token{TokenType::Dot,     ".",  startLine, startCol};

    case '=':
        if (match('='))
            return Token{TokenType::EqEq, "==", startLine, startCol};
        return Token{TokenType::Equals, "=", startLine, startCol};

    case '!':
        if (match('='))
            return Token{TokenType::NotEq, "!=", startLine, startCol};
        errors_.push_back({startLine, startCol, "Unexpected character '!'"});
        return nextToken();

    case '<':
        if (match('-')) {
            if (match('>'))
                return Token{TokenType::Arrow, "<->", startLine, startCol};
            errors_.push_back({startLine, startCol, "Expected '>' to complete '<->'"});
            return nextToken();
        }
        if (match('='))
            return Token{TokenType::LessEq, "<=", startLine, startCol};
        return Token{TokenType::Less, "<", startLine, startCol};

    case '>':
        if (match('='))
            return Token{TokenType::GreaterEq, ">=", startLine, startCol};
        return Token{TokenType::Greater, ">", startLine, startCol};

    default:
        errors_.push_back({startLine, startCol,
            std::string("Unexpected character '") + c + "'"});
        return nextToken();
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.type == TokenType::Eof)
            break;
    }
    return tokens;
}

} // namespace cellmltext
