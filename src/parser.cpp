/*
 * CellML Text 2.0 — Parser (implementation)
 *
 * Converts CellML Text 2.0 source → libCellML Model.
 */

#include "parser.h"
#include "lexer.h"
#include "mathml.h"
#include "units.h"

#include <libcellml/component.h>
#include <libcellml/importsource.h>
#include <libcellml/model.h>
#include <libcellml/reset.h>
#include <libcellml/variable.h>

#include <algorithm>
#include <cassert>
#include <unordered_map>

namespace cellmltext {

// ===================================================================
//  Token stream helpers
// ===================================================================

static const Token kEofToken{TokenType::Eof, "", 0, 0};

const Token &Parser::current() const {
    if (pos_ < tokens_.size()) return tokens_[pos_];
    return kEofToken;
}

const Token &Parser::peekToken() const {
    size_t next = pos_ + 1;
    while (next < tokens_.size() && tokens_[next].type == TokenType::Newline)
        ++next;
    if (next < tokens_.size()) return tokens_[next];
    return kEofToken;
}

Token Parser::advance() {
    Token tok = current();
    if (pos_ < tokens_.size()) ++pos_;
    return tok;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenType type, const std::string &context) {
    if (check(type)) return advance();
    error("Expected " + std::string(tokenTypeName(type))
          + " in " + context + ", got " + std::string(tokenTypeName(current().type))
          + " '" + current().value + "'");
    return current();
}

void Parser::skipNewlines() {
    while (check(TokenType::Newline)) advance();
}

void Parser::error(const std::string &msg) {
    auto &tok = current();
    errors_.push_back({tok.line, tok.column, msg});
}

// ===================================================================
//  Top-level parsing
// ===================================================================

libcellml::ModelPtr Parser::parse(const std::string &source) {
    Lexer lexer(source);
    tokens_ = lexer.tokenize();
    pos_ = 0;
    errors_.clear();
    model_ = libcellml::Model::create();
    pendingConnections_.clear();

    // Copy lexer errors.
    for (auto &le : lexer.errors())
        errors_.push_back({le.line, le.column, le.message});

    skipNewlines();
    parseModel();

    // Resolve dot-notation connections.
    resolveConnections();

    // Fix variable interfaces based on equivalences and hierarchy.
    model_->fixVariableInterfaces();

    return model_;
}

void Parser::parseModel() {
    // Support two forms:
    //   New:    Name { ... }
    //   Legacy: model Name [{ ... }]
    if (match(TokenType::Model)) {
        // Legacy: model Name
        Token name = expect(TokenType::Identifier, "model name");
        model_->setName(name.value);
        skipNewlines();
        if (match(TokenType::LBrace)) {
            parseModelBody();
            expect(TokenType::RBrace, "model body");
        } else {
            // Flat legacy format — top-level items until EOF.
            while (!check(TokenType::Eof)) {
                parseTopLevel();
                skipNewlines();
            }
        }
    } else if (check(TokenType::Identifier)) {
        // New: Name { ... }
        Token name = advance();
        model_->setName(name.value);
        skipNewlines();
        expect(TokenType::LBrace, "model body");
        parseModelBody();
        expect(TokenType::RBrace, "model body");
        skipNewlines();
    } else {
        error("Expected model name");
    }
}

void Parser::parseModelBody() {
    while (!check(TokenType::RBrace) && !check(TokenType::Eof)) {
        skipNewlines();
        if (check(TokenType::RBrace)) break;
        parseTopLevel();
    }
}

void Parser::parseTopLevel() {
    skipNewlines();
    if (check(TokenType::Eof)) return;

    switch (current().type) {
    case TokenType::Component:  parseComponent(nullptr);  break;
    case TokenType::Map:        parseMapStatement();    break;
    case TokenType::Group:      parseGroupStatement();  break;
    case TokenType::Import:     parseImportStatement(); break;
    case TokenType::Unit:       parseUnitDef();         break;
    default:
        error("Unexpected token '" + current().value + "' at top level");
        advance();
        break;
    }
}

// ===================================================================
//  component <name> { ... }
// ===================================================================

void Parser::parseComponent(const libcellml::ComponentPtr &parent) {
    expect(TokenType::Component, "component");
    Token name = expect(TokenType::Identifier, "component name");
    auto comp = libcellml::Component::create(name.value);

    skipNewlines();
    expect(TokenType::LBrace, "component body");

    parseComponentBody(comp);

    expect(TokenType::RBrace, "component body");

    if (parent) {
        parent->addComponent(comp);
    } else {
        model_->addComponent(comp);
    }
}

void Parser::parseComponentBody(const libcellml::ComponentPtr &comp) {
    // Collect equations as AST pairs, then convert to MathML at the end.
    std::vector<std::pair<ExprPtr, ExprPtr>> equations;

    while (!check(TokenType::RBrace) && !check(TokenType::Eof)) {
        skipNewlines();
        if (check(TokenType::RBrace)) break;

        // Nested component declaration.
        if (check(TokenType::Component)) {
            parseComponent(comp);
            skipNewlines();
            continue;
        }

        // Determine if this is a variable declaration, equation, or reset.
        if (check(TokenType::Reset)) {
            parseResetStatement(comp);
            continue;
        }

        // Variable declaration: IDENTIFIER ":" ...
        // Equation: expression "=" expression
        //
        // We need to look ahead: if we see IDENT followed by ":", it's a var decl.
        // Otherwise it's an equation.

        if (check(TokenType::Identifier)) {
            // Look ahead for ':' (var decl) vs '=' or other (equation).
            size_t savedPos = pos_;
            Token ident = advance();
            skipNewlines();
            if (check(TokenType::Colon)) {
                // Variable declaration.
                pos_ = savedPos;
                parseVarDecl(comp);
                continue;
            }
            // Restore and parse as equation.
            pos_ = savedPos;
        }

        // Parse as equation.
        ExprPtr lhs = parseExpression();
        lhs = transformDerivatives(std::move(lhs));

        if (!match(TokenType::Equals)) {
            error("Expected '=' in equation");
            skipNewlines();
            continue;
        }

        // Check for piecewise on RHS: { ... when ... otherwise ... }
        ExprPtr rhs;
        if (check(TokenType::LBrace)) {
            rhs = parsePiecewise();
        } else {
            rhs = parseExpression();
        }
        rhs = transformDerivatives(std::move(rhs));

        equations.emplace_back(std::move(lhs), std::move(rhs));
        skipNewlines();
    }

    // Convert equations to MathML and set on the component.
    if (!equations.empty()) {
        std::string mathml = equationsToMathML(equations);
        comp->setMath(mathml);
    }
}

// ===================================================================
//  Variable declaration:  name: unit [= initial_value]
// ===================================================================

void Parser::parseVarDecl(const libcellml::ComponentPtr &comp) {
    Token name = expect(TokenType::Identifier, "variable name");
    expect(TokenType::Colon, "variable declaration");

    // Check for connection reference: IDENTIFIER DOT IDENTIFIER
    // (dot never appears in unit expressions, so this is unambiguous)
    if (check(TokenType::Identifier)) {
        size_t savedPos = pos_;
        Token firstTok = advance();
        if (check(TokenType::Dot)) {
            advance(); // consume dot
            if (check(TokenType::Identifier)) {
                Token secondTok = advance();

                // This is a connection: name connects to firstTok.secondTok
                auto var = libcellml::Variable::create(name.value);
                var->setUnits("dimensionless"); // placeholder, resolved later
                comp->addVariable(var);

                pendingConnections_.push_back(
                    {comp, name.value, firstTok.value, secondTok.value});
                skipNewlines();
                return;
            }
        }
        // Not a connection — backtrack and parse as unit expression.
        pos_ = savedPos;
    }

    // Parse unit expression (consumes tokens until '=' or newline).
    std::string unitText = parseUnitExpr();
    auto pu = parseUnitExpression(unitText);

    // Determine the units name to assign.
    std::string unitsName;
    if (pu.valid) {
        // If it's a bare standard unit, use its CellML name directly.
        if (pu.factors.size() == 1 && pu.factors[0].exponent == 1.0
            && pu.factors[0].cellmlPrefix.empty()
            && pu.factors[0].multiplier == 1.0
            && isStandardUnit(pu.factors[0].cellmlUnit)) {
            unitsName = pu.factors[0].cellmlUnit;
        } else {
            unitsName = pu.cellmlName;
            ensureUnits(model_, pu);
        }
    } else {
        // Use the raw text as a custom unit reference.
        unitsName = unitText;
    }

    auto var = libcellml::Variable::create(name.value);
    var->setUnits(unitsName);

    // Optional initial value.
    if (match(TokenType::Equals)) {
        // Read the initial value (a number or identifier).
        if (check(TokenType::Number)) {
            var->setInitialValue(advance().value);
        } else if (check(TokenType::Minus)) {
            advance();
            if (check(TokenType::Number)) {
                var->setInitialValue("-" + advance().value);
            }
        } else if (check(TokenType::Identifier)) {
            var->setInitialValue(advance().value);
        }
    }

    comp->addVariable(var);
    skipNewlines();
}

// ===================================================================
//  Unit expression tokeniser (lightweight, returns string).
//  Reads tokens that form a unit expression context.
// ===================================================================

std::string Parser::parseUnitExpr() {
    // Collect everything until '=', newline, or '{'.
    std::string result;
    while (!check(TokenType::Equals) && !check(TokenType::Newline)
           && !check(TokenType::Eof) && !check(TokenType::LBrace)
           && !check(TokenType::RBrace)) {
        Token tok = advance();
        // Reconstruct text.
        if (tok.type == TokenType::Identifier || tok.type == TokenType::Number)
            result += tok.value;
        else if (tok.type == TokenType::Slash)  result += "/";
        else if (tok.type == TokenType::Star)   result += "*";
        else if (tok.type == TokenType::Caret)  result += "^";
        else if (tok.type == TokenType::LParen) result += "(";
        else if (tok.type == TokenType::RParen) result += ")";
        else if (tok.type == TokenType::Minus)  result += "-";
        else break;
    }
    return result;
}

// ===================================================================
//  Resolve dot-notation connections (post-processing).
//  For each pending connection like  V: membrane.V
//  we find the source variable, copy its units, and create CellML
//  variable equivalences along the encapsulation chain.
// ===================================================================

void Parser::resolveConnections() {
    // Build parent map: child component name → parent component.
    std::unordered_map<std::string, libcellml::ComponentPtr> parentMap;
    std::function<void(const libcellml::ComponentPtr &)> buildParentMap;
    buildParentMap = [&](const libcellml::ComponentPtr &comp) {
        for (size_t i = 0; i < comp->componentCount(); ++i) {
            auto child = comp->component(i);
            parentMap[child->name()] = comp;
            buildParentMap(child);
        }
    };
    for (size_t i = 0; i < model_->componentCount(); ++i)
        buildParentMap(model_->component(i));

    // Find component by name (recursive).
    std::function<libcellml::ComponentPtr(const libcellml::ComponentPtr &,
                                          const std::string &)> findCompIn;
    findCompIn = [&](const libcellml::ComponentPtr &root,
                     const std::string &name) -> libcellml::ComponentPtr {
        if (root->name() == name) return root;
        for (size_t i = 0; i < root->componentCount(); ++i) {
            auto f = findCompIn(root->component(i), name);
            if (f) return f;
        }
        return nullptr;
    };
    auto findComp = [&](const std::string &name) -> libcellml::ComponentPtr {
        for (size_t i = 0; i < model_->componentCount(); ++i) {
            auto f = findCompIn(model_->component(i), name);
            if (f) return f;
        }
        return nullptr;
    };

    // Test if `ancestor` is an ancestor of `descendant`.
    auto isAncestorOf = [&](const libcellml::ComponentPtr &ancestor,
                            const libcellml::ComponentPtr &descendant) -> bool {
        auto cur = descendant;
        while (true) {
            auto pIt = parentMap.find(cur->name());
            if (pIt == parentMap.end()) return false;
            if (pIt->second == ancestor) return true;
            cur = pIt->second;
        }
    };

    auto areEquivalent = [](const libcellml::VariablePtr &v1,
                            const libcellml::VariablePtr &v2) -> bool {
        for (size_t i = 0; i < v1->equivalentVariableCount(); ++i)
            if (v1->equivalentVariable(i) == v2) return true;
        return false;
    };

    for (auto &ref : pendingConnections_) {
        auto sourceComp = findComp(ref.sourceComponentName);
        if (!sourceComp) {
            error("Component '" + ref.sourceComponentName + "' not found");
            continue;
        }
        auto sourceVar = sourceComp->variable(ref.sourceVariableName);
        if (!sourceVar) {
            error("Variable '" + ref.sourceVariableName
                  + "' not found in '" + ref.sourceComponentName + "'");
            continue;
        }

        auto localVar = ref.component->variable(ref.variableName);
        if (!localVar) continue;

        // Copy units from source variable.
        if (auto su = sourceVar->units())
            localVar->setUnits(su->name());

        // --- Create equivalence chain along the encapsulation hierarchy ---

        // Case 1: source is a direct parent.
        auto pIt = parentMap.find(ref.component->name());
        if (pIt != parentMap.end() && pIt->second == sourceComp) {
            if (!areEquivalent(localVar, sourceVar))
                libcellml::Variable::addEquivalence(localVar, sourceVar);
            continue;
        }

        // Case 2: source is a direct child.
        bool isDirectChild = false;
        for (size_t i = 0; i < ref.component->componentCount(); ++i) {
            if (ref.component->component(i) == sourceComp) {
                isDirectChild = true; break;
            }
        }
        if (isDirectChild) {
            if (!areEquivalent(localVar, sourceVar))
                libcellml::Variable::addEquivalence(localVar, sourceVar);
            continue;
        }

        // Case 3: source is an ancestor — walk up creating a chain.
        if (isAncestorOf(sourceComp, ref.component)) {
            auto cur = ref.component;
            auto curVar = localVar;
            while (cur != sourceComp) {
                auto curParent = parentMap[cur->name()];
                auto pVar = curParent->variable(ref.sourceVariableName);
                if (!pVar) {
                    // Create intermediary variable.
                    pVar = libcellml::Variable::create(ref.sourceVariableName);
                    if (auto su = sourceVar->units())
                        pVar->setUnits(su->name());
                    curParent->addVariable(pVar);
                }
                if (!areEquivalent(curVar, pVar))
                    libcellml::Variable::addEquivalence(curVar, pVar);
                curVar = pVar;
                cur = curParent;
            }
            continue;
        }

        // Case 4: source is a descendant — walk up from source.
        if (isAncestorOf(ref.component, sourceComp)) {
            std::vector<libcellml::ComponentPtr> path;
            auto cur = sourceComp;
            while (cur != ref.component) {
                path.push_back(cur);
                auto bp = parentMap.find(cur->name());
                if (bp == parentMap.end()) break;
                cur = bp->second;
            }
            auto curVar = localVar;
            for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
                auto comp = path[static_cast<size_t>(i)];
                auto cVar = comp->variable(ref.sourceVariableName);
                if (!cVar) {
                    cVar = libcellml::Variable::create(ref.sourceVariableName);
                    if (auto su = sourceVar->units())
                        cVar->setUnits(su->name());
                    comp->addVariable(cVar);
                }
                if (!areEquivalent(curVar, cVar))
                    libcellml::Variable::addEquivalence(curVar, cVar);
                curVar = cVar;
            }
            continue;
        }

        // Case 5: sibling or other — direct equivalence.
        if (!areEquivalent(localVar, sourceVar))
            libcellml::Variable::addEquivalence(localVar, sourceVar);
    }
}

// ===================================================================
//  map comp1.var1 <-> comp2.var2
// ===================================================================

void Parser::parseMapStatement() {
    expect(TokenType::Map, "map statement");

    Token comp1 = expect(TokenType::Identifier, "map component 1");
    expect(TokenType::Dot, "map");
    Token var1 = expect(TokenType::Identifier, "map variable 1");

    expect(TokenType::Arrow, "map");

    Token comp2 = expect(TokenType::Identifier, "map component 2");
    expect(TokenType::Dot, "map");
    Token var2 = expect(TokenType::Identifier, "map variable 2");

    // Find or create the variables and set equivalence.
    auto findVar = [&](const std::string &compName,
                       const std::string &varName) -> libcellml::VariablePtr {
        // Search all components (including nested) for the named component.
        std::function<libcellml::ComponentPtr(const libcellml::ComponentPtr &)> findComp;
        findComp = [&](const libcellml::ComponentPtr &parent) -> libcellml::ComponentPtr {
            for (size_t i = 0; i < parent->componentCount(); ++i) {
                auto c = parent->component(i);
                if (c->name() == compName) return c;
                auto found = findComp(c);
                if (found) return found;
            }
            return nullptr;
        };

        // Search top-level model components.
        libcellml::ComponentPtr comp;
        for (size_t i = 0; i < model_->componentCount(); ++i) {
            auto c = model_->component(i);
            if (c->name() == compName) { comp = c; break; }
            auto found = findComp(c);
            if (found) { comp = found; break; }
        }

        if (!comp) {
            error("Component '" + compName + "' not found for map");
            return nullptr;
        }

        auto v = comp->variable(varName);
        if (!v) {
            error("Variable '" + varName + "' not found in component '"
                  + compName + "'");
        }
        return v;
    };

    auto v1 = findVar(comp1.value, var1.value);
    auto v2 = findVar(comp2.value, var2.value);

    if (v1 && v2) {
        libcellml::Variable::addEquivalence(v1, v2);
    }

    skipNewlines();
}

// ===================================================================
//  group <parent> contains { <child> ... }
// ===================================================================

void Parser::parseGroupStatement() {
    expect(TokenType::Group, "group statement");
    Token parentName = expect(TokenType::Identifier, "group parent");
    expect(TokenType::Contains, "group statement");
    skipNewlines();
    expect(TokenType::LBrace, "group body");

    // Find the parent component.
    libcellml::ComponentPtr parentComp;
    for (size_t i = 0; i < model_->componentCount(); ++i) {
        if (model_->component(i)->name() == parentName.value) {
            parentComp = model_->component(i);
            break;
        }
    }
    if (!parentComp) {
        error("Component '" + parentName.value + "' not found for group");
    }

    skipNewlines();
    while (!check(TokenType::RBrace) && !check(TokenType::Eof)) {
        skipNewlines();
        if (check(TokenType::RBrace)) break;

        Token childName = expect(TokenType::Identifier, "group child");

        if (parentComp) {
            // Find the child component at top level and move it under parent.
            libcellml::ComponentPtr childComp;
            for (size_t i = 0; i < model_->componentCount(); ++i) {
                if (model_->component(i)->name() == childName.value) {
                    childComp = model_->component(i);
                    break;
                }
            }
            if (childComp) {
                // Take it from the model and add under parent.
                model_->removeComponent(childName.value);
                parentComp->addComponent(childComp);
            } else {
                // Maybe already nested? Check recursively.
                error("Component '" + childName.value
                      + "' not found at top level for group");
            }
        }

        skipNewlines();
    }

    expect(TokenType::RBrace, "group body");
    skipNewlines();
}

// ===================================================================
//  import "url" { component <name> [as <local>]; ... }
// ===================================================================

void Parser::parseImportStatement() {
    expect(TokenType::Import, "import statement");
    Token url = expect(TokenType::String, "import URL");
    skipNewlines();
    expect(TokenType::LBrace, "import body");

    auto importSource = libcellml::ImportSource::create();
    importSource->setUrl(url.value);

    skipNewlines();
    while (!check(TokenType::RBrace) && !check(TokenType::Eof)) {
        skipNewlines();
        if (check(TokenType::RBrace)) break;

        if (match(TokenType::Component)) {
            Token name = expect(TokenType::Identifier, "import component name");
            std::string localName = name.value;
            if (match(TokenType::As)) {
                Token alias = expect(TokenType::Identifier, "import alias");
                localName = alias.value;
            }

            auto comp = libcellml::Component::create(localName);
            comp->setSourceComponent(importSource, name.value);
            model_->addComponent(comp);

        } else if (match(TokenType::Unit)) {
            Token name = expect(TokenType::Identifier, "import unit name");
            std::string localName = name.value;
            if (match(TokenType::As)) {
                Token alias = expect(TokenType::Identifier, "import alias");
                localName = alias.value;
            }

            auto units = libcellml::Units::create(localName);
            units->setSourceUnits(importSource, name.value);
            model_->addUnits(units);

        } else {
            error("Expected 'component' or 'unit' in import block");
            advance();
        }

        skipNewlines();
    }

    expect(TokenType::RBrace, "import body");
    skipNewlines();
}

// ===================================================================
//  unit <name> = <unit_expr>
// ===================================================================

void Parser::parseUnitDef() {
    expect(TokenType::Unit, "unit definition");
    Token name = expect(TokenType::Identifier, "unit name");
    expect(TokenType::Equals, "unit definition");

    std::string unitText = parseUnitExpr();
    auto pu = parseUnitExpression(unitText);

    if (pu.valid) {
        // Override the generated name with the user-specified name.
        auto units = libcellml::Units::create(name.value);
        for (auto &f : pu.factors) {
            if (f.cellmlPrefix.empty())
                units->addUnit(f.cellmlUnit, f.exponent);
            else
                units->addUnit(f.cellmlUnit, f.cellmlPrefix, f.exponent, f.multiplier);
        }
        model_->addUnits(units);
    } else {
        error("Invalid unit expression: " + unitText);
    }
    skipNewlines();
}

// ===================================================================
//  reset <var> at order <n> when <cond> { <var> = <expr> }
// ===================================================================

void Parser::parseResetStatement(const libcellml::ComponentPtr &comp) {
    expect(TokenType::Reset, "reset statement");
    Token varName = expect(TokenType::Identifier, "reset variable");
    expect(TokenType::At, "reset");
    expect(TokenType::Order, "reset");
    Token orderTok = expect(TokenType::Number, "reset order");
    expect(TokenType::When, "reset");

    // Parse the test condition.
    ExprPtr testExpr = parseExpression();

    skipNewlines();
    expect(TokenType::LBrace, "reset body");
    skipNewlines();

    // Parse the reset assignment: var = expr
    Token resetVarName = expect(TokenType::Identifier, "reset value variable");
    expect(TokenType::Equals, "reset value");
    ExprPtr resetExpr = parseExpression();

    skipNewlines();
    expect(TokenType::RBrace, "reset body");

    // Build the reset.
    auto resetVar = comp->variable(varName.value);
    if (!resetVar) {
        error("Reset variable '" + varName.value + "' not found in component");
        return;
    }

    auto reset = libcellml::Reset::create();
    reset->setVariable(resetVar);
    reset->setTestVariable(resetVar);
    reset->setOrder(std::stoi(orderTok.value));

    // Test value: MathML wrapping the test expression as an equation
    // (the test_value in CellML is: "when <test_variable> <op> <value>").
    // We store it as a MathML expression (the condition itself).
    std::string testMathML = "<math xmlns=\"http://www.w3.org/1998/Math/MathML\""
                             " xmlns:cellml=\"http://www.cellml.org/cellml/2.0#\">"
                             + exprToMathML(testExpr.get()) + "</math>";
    reset->setTestValue(testMathML);

    // Reset value.
    std::string resetMathML = "<math xmlns=\"http://www.w3.org/1998/Math/MathML\""
                              " xmlns:cellml=\"http://www.cellml.org/cellml/2.0#\">"
                              + exprToMathML(resetExpr.get()) + "</math>";
    reset->setResetValue(resetMathML);

    comp->addReset(reset);
    skipNewlines();
}

// ===================================================================
//  Expression parser — recursive descent
// ===================================================================

ExprPtr Parser::parseExpression() {
    return parseOr();
}

ExprPtr Parser::parseOr() {
    auto left = parseAnd();
    while (match(TokenType::Or)) {
        auto right = parseAnd();
        left = makeBinOp("or", std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseAnd() {
    auto left = parseComparison();
    while (match(TokenType::And)) {
        auto right = parseComparison();
        left = makeBinOp("and", std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseComparison() {
    auto left = parseAddition();

    while (check(TokenType::EqEq) || check(TokenType::NotEq)
           || check(TokenType::Less) || check(TokenType::Greater)
           || check(TokenType::LessEq) || check(TokenType::GreaterEq)) {
        Token op = advance();
        auto right = parseAddition();
        left = makeBinOp(op.value, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseAddition() {
    auto left = parseMultiplication();

    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        Token op = advance();
        auto right = parseMultiplication();
        left = makeBinOp(op.value, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseMultiplication() {
    auto left = parseUnary();

    while (check(TokenType::Star) || check(TokenType::Slash)) {
        Token op = advance();
        auto right = parseUnary();
        left = makeBinOp(op.value, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (match(TokenType::Minus)) {
        auto operand = parseUnary();
        return makeUnaryOp("-", std::move(operand));
    }
    if (match(TokenType::Not)) {
        auto operand = parseUnary();
        return makeUnaryOp("not", std::move(operand));
    }
    return parsePower();
}

ExprPtr Parser::parsePower() {
    auto base = parsePrimary();

    if (match(TokenType::Caret)) {
        auto exp = parseUnary();  // right-associative
        return makeBinOp("^", std::move(base), std::move(exp));
    }
    return base;
}

ExprPtr Parser::parsePrimary() {
    // Number literal.
    if (check(TokenType::Number)) {
        Token tok = advance();
        return makeNumber(std::stod(tok.value));
    }

    // Constants.
    if (check(TokenType::Pi))    { advance(); return makeConst("pi"); }
    if (check(TokenType::Inf))   { advance(); return makeConst("inf"); }
    if (check(TokenType::Nan))   { advance(); return makeConst("nan"); }
    if (check(TokenType::True))  { advance(); return makeConst("true"); }
    if (check(TokenType::False)) { advance(); return makeConst("false"); }

    // Identifier (variable, function call, or 'd' for derivative).
    if (check(TokenType::Identifier)) {
        Token tok = advance();

        // Check for function call: IDENT '('.
        if (check(TokenType::LParen)) {
            return parseFunctionCall(tok.value);
        }

        // Just a variable reference.
        return makeId(tok.value);
    }

    // Parenthesised expression.
    if (match(TokenType::LParen)) {
        auto expr = parseExpression();
        expect(TokenType::RParen, "parenthesised expression");
        return expr;
    }

    // Piecewise (bare '{' in expression context).
    if (check(TokenType::LBrace)) {
        return parsePiecewise();
    }

    error("Unexpected token in expression: " + current().value);
    advance();
    return makeNumber(0);
}

// ===================================================================
//  Function call:  name ( arg1, arg2, ... )
//  Also handles d(V)/d(t) derivative pattern.
// ===================================================================

ExprPtr Parser::parseFunctionCall(const std::string &name) {
    expect(TokenType::LParen, "function call");

    // Handle "d" specially: d(V)/d(t) derivative.
    if (name == "d") {
        // Parse d(variable)
        Token varTok = expect(TokenType::Identifier, "derivative variable");
        expect(TokenType::RParen, "derivative d(var)");

        // Check if followed by /d(
        if (check(TokenType::Slash)) {
            size_t savedPos = pos_;
            advance(); // consume '/'

            if (check(TokenType::Identifier) && current().value == "d") {
                advance(); // consume 'd'
                if (match(TokenType::LParen)) {
                    Token bvarTok = expect(TokenType::Identifier, "derivative bound variable");
                    expect(TokenType::RParen, "derivative d(bvar)");
                    return makeDeriv(varTok.value, bvarTok.value);
                }
            }
            // Not a derivative pattern — backtrack.
            pos_ = savedPos;
        }

        // Just a function call d(x).
        std::vector<ExprPtr> args;
        args.push_back(makeId(varTok.value));
        return makeCall("d", std::move(args));
    }

    // Regular function call.
    std::vector<ExprPtr> args;
    if (!check(TokenType::RParen)) {
        args.push_back(parseExpression());
        while (match(TokenType::Comma)) {
            args.push_back(parseExpression());
        }
    }
    expect(TokenType::RParen, "function call");
    return makeCall(name, std::move(args));
}

// ===================================================================
//  Piecewise: { expr when cond \n expr when cond \n expr otherwise }
// ===================================================================

ExprPtr Parser::parsePiecewise() {
    expect(TokenType::LBrace, "piecewise");
    skipNewlines();

    auto pw = std::make_unique<PiecewiseExpr>();

    while (!check(TokenType::RBrace) && !check(TokenType::Eof)) {
        skipNewlines();
        if (check(TokenType::RBrace)) break;

        ExprPtr value = parseExpression();
        value = transformDerivatives(std::move(value));

        if (match(TokenType::When)) {
            ExprPtr condition = parseExpression();
            condition = transformDerivatives(std::move(condition));
            pw->pieces.emplace_back(std::move(value), std::move(condition));
        } else if (match(TokenType::Otherwise)) {
            pw->otherwise = std::move(value);
        } else {
            error("Expected 'when' or 'otherwise' in piecewise expression");
            break;
        }
        skipNewlines();
    }

    expect(TokenType::RBrace, "piecewise");
    return pw;
}

// ===================================================================
//  Post-processing: recognise d(x)/d(y) in the AST and convert to
//  DerivativeExpr.  This catches cases where the expression parser
//  builds BinaryOp("/", Call("d", [x]), Call("d", [y])).
// ===================================================================

ExprPtr Parser::transformDerivatives(ExprPtr expr) {
    if (!expr) return expr;

    switch (expr->kind) {
    case ExprKind::BinaryOp: {
        auto *bin = static_cast<BinaryOpExpr *>(expr.get());
        bin->left = transformDerivatives(std::move(bin->left));
        bin->right = transformDerivatives(std::move(bin->right));

        // Check for d(x)/d(y) pattern.
        if (bin->op == "/") {
            auto *leftCall = dynamic_cast<FunctionCallExpr *>(bin->left.get());
            auto *rightCall = dynamic_cast<FunctionCallExpr *>(bin->right.get());
            if (leftCall && rightCall
                && leftCall->name == "d" && rightCall->name == "d"
                && leftCall->args.size() == 1 && rightCall->args.size() == 1) {
                auto *leftId = dynamic_cast<IdentifierExpr *>(leftCall->args[0].get());
                auto *rightId = dynamic_cast<IdentifierExpr *>(rightCall->args[0].get());
                if (leftId && rightId) {
                    return makeDeriv(leftId->name, rightId->name);
                }
            }
        }
        break;
    }
    case ExprKind::UnaryOp: {
        auto *u = static_cast<UnaryOpExpr *>(expr.get());
        u->operand = transformDerivatives(std::move(u->operand));
        break;
    }
    case ExprKind::FunctionCall: {
        auto *f = static_cast<FunctionCallExpr *>(expr.get());
        for (auto &a : f->args)
            a = transformDerivatives(std::move(a));
        break;
    }
    case ExprKind::Piecewise: {
        auto *pw = static_cast<PiecewiseExpr *>(expr.get());
        for (auto &[v, c] : pw->pieces) {
            v = transformDerivatives(std::move(v));
            c = transformDerivatives(std::move(c));
        }
        if (pw->otherwise)
            pw->otherwise = transformDerivatives(std::move(pw->otherwise));
        break;
    }
    default:
        break;
    }
    return expr;
}

} // namespace cellmltext
