/*
 * CellML Text 2.0 — Tests
 *
 * A lightweight test harness (no external framework dependency).
 */

#include "api.h"
#include "ast.h"
#include "lexer.h"
#include "mathml.h"
#include "parser.h"
#include "serializer.h"
#include "units.h"

#include <libcellml/component.h>
#include <libcellml/model.h>
#include <libcellml/printer.h>
#include <libcellml/variable.h>

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

using namespace cellmltext;

// --- Minimal test framework ---

static int gTests    = 0;
static int gPassed   = 0;
static int gFailed   = 0;
static std::string gCurrentSuite;

#define SUITE(name) gCurrentSuite = name

#define CHECK(cond)                                                           \
    do {                                                                      \
        ++gTests;                                                             \
        if (cond) {                                                           \
            ++gPassed;                                                        \
        } else {                                                              \
            ++gFailed;                                                        \
            std::cerr << "FAIL [" << gCurrentSuite << "] " << __FILE__        \
                      << ":" << __LINE__ << "  (" #cond ")\n";                \
        }                                                                     \
    } while (0)

#define CHECK_EQ(a, b)                                                        \
    do {                                                                      \
        ++gTests;                                                             \
        if ((a) == (b)) {                                                     \
            ++gPassed;                                                        \
        } else {                                                              \
            ++gFailed;                                                        \
            std::cerr << "FAIL [" << gCurrentSuite << "] " << __FILE__        \
                      << ":" << __LINE__ << "\n  expected: " << (b)           \
                      << "\n  got:      " << (a) << "\n";                     \
        }                                                                     \
    } while (0)

// =====================================================================
//  Lexer tests
// =====================================================================

static void testLexerBasic() {
    SUITE("Lexer/Basic");

    Lexer lex("model HodgkinHuxley1952");
    auto tokens = lex.tokenize();

    CHECK(tokens.size() >= 3); // model, identifier, EOF
    CHECK_EQ(tokens[0].type, TokenType::Model);
    CHECK_EQ(tokens[1].type, TokenType::Identifier);
    CHECK_EQ(tokens[1].value, std::string("HodgkinHuxley1952"));
}

static void testLexerOperators() {
    SUITE("Lexer/Operators");

    Lexer lex("a + b * c / d ^ 2 == e <-> f");
    auto tokens = lex.tokenize();

    // a + b * c / d ^ 2 == e <-> f
    CHECK_EQ(tokens[0].type, TokenType::Identifier); // a
    CHECK_EQ(tokens[1].type, TokenType::Plus);
    CHECK_EQ(tokens[2].type, TokenType::Identifier); // b
    CHECK_EQ(tokens[3].type, TokenType::Star);
    CHECK_EQ(tokens[4].type, TokenType::Identifier); // c
    CHECK_EQ(tokens[5].type, TokenType::Slash);
    CHECK_EQ(tokens[6].type, TokenType::Identifier); // d
    CHECK_EQ(tokens[7].type, TokenType::Caret);
    CHECK_EQ(tokens[8].type, TokenType::Number);     // 2
    CHECK_EQ(tokens[9].type, TokenType::EqEq);
    CHECK_EQ(tokens[10].type, TokenType::Identifier); // e
    CHECK_EQ(tokens[11].type, TokenType::Arrow);
    CHECK_EQ(tokens[12].type, TokenType::Identifier); // f
}

static void testLexerNumbers() {
    SUITE("Lexer/Numbers");

    Lexer lex("42 3.14 1e-3 2.5E+10");
    auto tokens = lex.tokenize();

    CHECK_EQ(tokens[0].value, std::string("42"));
    CHECK_EQ(tokens[1].value, std::string("3.14"));
    CHECK_EQ(tokens[2].value, std::string("1e-3"));
    CHECK_EQ(tokens[3].value, std::string("2.5E+10"));
}

static void testLexerString() {
    SUITE("Lexer/String");

    Lexer lex("import \"path/to/model.cellml\"");
    auto tokens = lex.tokenize();

    CHECK_EQ(tokens[0].type, TokenType::Import);
    CHECK_EQ(tokens[1].type, TokenType::String);
    CHECK_EQ(tokens[1].value, std::string("path/to/model.cellml"));
}

static void testLexerKeywords() {
    SUITE("Lexer/Keywords");

    Lexer lex("model component map group contains import as unit reset when at order otherwise and or not true false pi inf nan");
    auto tokens = lex.tokenize();

    CHECK_EQ(tokens[0].type, TokenType::Model);
    CHECK_EQ(tokens[1].type, TokenType::Component);
    CHECK_EQ(tokens[2].type, TokenType::Map);
    CHECK_EQ(tokens[3].type, TokenType::Group);
    CHECK_EQ(tokens[4].type, TokenType::Contains);
    CHECK_EQ(tokens[5].type, TokenType::Import);
    CHECK_EQ(tokens[6].type, TokenType::As);
    CHECK_EQ(tokens[7].type, TokenType::Unit);
    CHECK_EQ(tokens[8].type, TokenType::Reset);
    CHECK_EQ(tokens[9].type, TokenType::When);
    CHECK_EQ(tokens[10].type, TokenType::At);
    CHECK_EQ(tokens[11].type, TokenType::Order);
    CHECK_EQ(tokens[12].type, TokenType::Otherwise);
    CHECK_EQ(tokens[13].type, TokenType::And);
    CHECK_EQ(tokens[14].type, TokenType::Or);
    CHECK_EQ(tokens[15].type, TokenType::Not);
    CHECK_EQ(tokens[16].type, TokenType::True);
    CHECK_EQ(tokens[17].type, TokenType::False);
    CHECK_EQ(tokens[18].type, TokenType::Pi);
    CHECK_EQ(tokens[19].type, TokenType::Inf);
    CHECK_EQ(tokens[20].type, TokenType::Nan);
}

// =====================================================================
//  Unit parsing tests
// =====================================================================

static void testUnitSimple() {
    SUITE("Units/Simple");

    auto pu = parseUnitExpression("mV");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(1));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("volt"));
    CHECK_EQ(pu.factors[0].cellmlPrefix, std::string("milli"));
    CHECK(pu.factors[0].exponent == 1.0);
}

static void testUnitBare() {
    SUITE("Units/Bare");

    auto pu = parseUnitExpression("V");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(1));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("volt"));
    CHECK(pu.factors[0].cellmlPrefix.empty());
}

static void testUnitCompound() {
    SUITE("Units/Compound");

    auto pu = parseUnitExpression("mS/cm^2");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(2));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("siemens"));
    CHECK_EQ(pu.factors[0].cellmlPrefix, std::string("milli"));
    CHECK(pu.factors[0].exponent == 1.0);
    CHECK_EQ(pu.factors[1].cellmlUnit, std::string("metre"));
    CHECK_EQ(pu.factors[1].cellmlPrefix, std::string("centi"));
    CHECK(pu.factors[1].exponent == -2.0);
}

static void testUnitPerMs() {
    SUITE("Units/1/ms");

    auto pu = parseUnitExpression("1/ms");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(1));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("second"));
    CHECK_EQ(pu.factors[0].cellmlPrefix, std::string("milli"));
    CHECK(pu.factors[0].exponent == -1.0);
}

static void testUnitDimensionless() {
    SUITE("Units/Dimensionless");

    auto pu1 = parseUnitExpression("1");
    CHECK(pu1.valid);
    CHECK_EQ(pu1.cellmlName, std::string("dimensionless"));

    auto pu2 = parseUnitExpression("dimensionless");
    CHECK(pu2.valid);
    CHECK_EQ(pu2.cellmlName, std::string("dimensionless"));
}

static void testUnitKg() {
    SUITE("Units/kg");

    auto pu = parseUnitExpression("kg");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(1));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("kilogram"));
    CHECK(pu.factors[0].cellmlPrefix.empty());
}

static void testUnitMmolPerL() {
    SUITE("Units/mmol/L");

    auto pu = parseUnitExpression("mmol/L");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(2));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("mole"));
    CHECK_EQ(pu.factors[0].cellmlPrefix, std::string("milli"));
    CHECK(pu.factors[0].exponent == 1.0);
    CHECK_EQ(pu.factors[1].cellmlUnit, std::string("litre"));
    CHECK(pu.factors[1].exponent == -1.0);
}

// =====================================================================
//  MathML conversion tests
// =====================================================================

static void testMathMLGeneration() {
    SUITE("MathML/Generation");

    // Test: x = 2 * y + 3
    auto lhs = makeId("x");
    auto rhs = makeBinOp("+",
        makeBinOp("*", makeNumber(2), makeId("y")),
        makeNumber(3));

    std::string ml = equationToMathML(lhs, rhs);
    CHECK(!ml.empty());
    CHECK(ml.find("<apply><eq/>") != std::string::npos);
    CHECK(ml.find("<ci>x</ci>") != std::string::npos);
    CHECK(ml.find("<apply><plus/>") != std::string::npos);
}

static void testMathMLDerivative() {
    SUITE("MathML/Derivative");

    auto lhs = makeDeriv("V", "t");
    auto rhs = makeUnaryOp("-", makeId("I_ion"));

    std::string ml = equationToMathML(lhs, rhs);
    CHECK(ml.find("<apply><diff/>") != std::string::npos);
    CHECK(ml.find("<bvar><ci>t</ci></bvar>") != std::string::npos);
    CHECK(ml.find("<ci>V</ci>") != std::string::npos);
}

static void testMathMLRoundTrip() {
    SUITE("MathML/RoundTrip");

    // Generate: x = sin(y) + exp(z)
    auto lhs = makeId("x");
    std::vector<ExprPtr> sinArgs;
    sinArgs.push_back(makeId("y"));
    std::vector<ExprPtr> expArgs;
    expArgs.push_back(makeId("z"));
    auto rhs = makeBinOp("+",
        makeCall("sin", std::move(sinArgs)),
        makeCall("exp", std::move(expArgs)));

    std::string ml = equationToMathML(lhs, rhs);

    // Parse back.
    auto equations = mathMLToEquations(ml);
    CHECK_EQ(equations.size(), size_t(1));
    if (!equations.empty()) {
        CHECK(equations[0].first != nullptr);
        CHECK(equations[0].second != nullptr);
    }
}

// =====================================================================
//  Parser tests
// =====================================================================

static void testParserMinimal() {
    SUITE("Parser/Minimal");

    std::string input = R"(
model TestModel

component membrane {
    V: mV = -75.0
    t: ms
}
)";

    Parser parser;
    auto model = parser.parse(input);
    CHECK(model != nullptr);
    CHECK_EQ(model->name(), std::string("TestModel"));
    CHECK_EQ(model->componentCount(), size_t(1));

    auto comp = model->component(0);
    CHECK_EQ(comp->name(), std::string("membrane"));
    CHECK_EQ(comp->variableCount(), size_t(2));

    auto v = comp->variable("V");
    CHECK(v != nullptr);
    if (v) {
        CHECK_EQ(v->initialValue(), std::string("-75.0"));
    }
}

static void testParserEquation() {
    SUITE("Parser/Equation");

    std::string input = R"(
model Test

component C {
    x: dimensionless
    y: dimensionless

    x = 2 * y + 3
}
)";

    Parser parser;
    auto model = parser.parse(input);
    auto comp = model->component(0);
    CHECK(!comp->math().empty());

    // The math should contain valid MathML.
    std::string math = comp->math();
    CHECK(math.find("<math") != std::string::npos);
    CHECK(math.find("<apply><eq/>") != std::string::npos);
}

static void testParserDerivative() {
    SUITE("Parser/Derivative");

    std::string input = R"(
model Test

component C {
    V: mV
    t: ms

    d(V)/d(t) = -10.0
}
)";

    Parser parser;
    auto model = parser.parse(input);
    auto comp = model->component(0);
    std::string math = comp->math();
    CHECK(math.find("<diff/>") != std::string::npos);
    CHECK(math.find("<bvar>") != std::string::npos);
}

static void testParserPiecewise() {
    SUITE("Parser/Piecewise");

    std::string input = R"(
model Test

component C {
    x: dimensionless
    V: mV

    x = {
        1.0  when V > 0
        0.0  otherwise
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    auto comp = model->component(0);
    std::string math = comp->math();
    CHECK(math.find("<piecewise>") != std::string::npos);
    CHECK(math.find("<piece>") != std::string::npos);
    CHECK(math.find("<otherwise>") != std::string::npos);
}

static void testParserMap() {
    SUITE("Parser/Map");

    std::string input = R"(
model Test

component A {
    V: mV
}

component B {
    V: mV
}

map A.V <-> B.V
)";

    Parser parser;
    auto model = parser.parse(input);
    auto compA = model->component("A");
    auto compB = model->component("B");
    CHECK(compA != nullptr);
    CHECK(compB != nullptr);

    if (compA && compB) {
        auto vA = compA->variable("V");
        CHECK(vA != nullptr);
        if (vA)
            CHECK(vA->equivalentVariableCount() > 0);
    }
}

static void testParserGroup() {
    SUITE("Parser/Group");

    std::string input = R"(
model Test

component parent {
    x: dimensionless
}

component child {
    y: dimensionless
}

group parent contains {
    child
}
)";

    Parser parser;
    auto model = parser.parse(input);

    // After grouping, 'child' should be under 'parent'.
    auto parentComp = model->component("parent");
    CHECK(parentComp != nullptr);
    if (parentComp) {
        CHECK_EQ(parentComp->componentCount(), size_t(1));
        if (parentComp->componentCount() > 0)
            CHECK_EQ(parentComp->component(0)->name(), std::string("child"));
    }
}

// =====================================================================
//  Serializer tests
// =====================================================================

static void testSerializerBasic() {
    SUITE("Serializer/Basic");

    auto model = libcellml::Model::create("TestModel");
    auto comp = libcellml::Component::create("membrane");
    auto v = libcellml::Variable::create("V");
    v->setUnits("volt");
    v->setInitialValue(-75.0);
    comp->addVariable(v);
    model->addComponent(comp);

    Serializer ser;
    std::string text = ser.serialize(model);

    CHECK(text.find("model TestModel") != std::string::npos);
    CHECK(text.find("component membrane") != std::string::npos);
    CHECK(text.find("V:") != std::string::npos);
}

// =====================================================================
//  Round-trip tests
// =====================================================================

static void testRoundTripSimple() {
    SUITE("RoundTrip/Simple");

    std::string input = R"(
model RoundTripTest

component main {
    x: dimensionless = 1.0
    y: dimensionless

    y = 2.0 * x + 3.0
}
)";

    // Text → Model
    std::vector<Error> errors;
    auto model = textToModel(input, &errors);
    CHECK(model != nullptr);
    for (auto &e : errors)
        std::cerr << "  Error: " << e.message << "\n";

    // Model → Text
    std::string output = modelToText(model, &errors);
    CHECK(!output.empty());

    // Verify key elements are preserved.
    CHECK(output.find("model RoundTripTest") != std::string::npos);
    CHECK(output.find("component main") != std::string::npos);
}

static void testRoundTripCellMLXML() {
    SUITE("RoundTrip/CellML-XML");

    std::string input = R"(
model Noble62

component membrane {
    V: mV = -87.0
    t: ms
    Cm: uF/cm^2 = 12.0
    I_Na: uA/cm^2
    I_K: uA/cm^2

    Cm * d(V)/d(t) = -(I_Na + I_K)
}
)";

    // Text → CellML XML
    std::vector<Error> errors;
    std::string xml = textToCellML(input, &errors);
    CHECK(!xml.empty());
    for (auto &e : errors)
        std::cerr << "  Error: " << e.message << "\n";

    // Should be valid XML.
    CHECK(xml.find("<?xml") != std::string::npos || xml.find("<model") != std::string::npos);
}

// =====================================================================
//  Main
// =====================================================================

int main() {
    std::cout << "CellML Text 2.0 — Test Suite\n";
    std::cout << "=============================\n\n";

    // Lexer.
    testLexerBasic();
    testLexerOperators();
    testLexerNumbers();
    testLexerString();
    testLexerKeywords();

    // Units.
    testUnitSimple();
    testUnitBare();
    testUnitCompound();
    testUnitPerMs();
    testUnitDimensionless();
    testUnitKg();
    testUnitMmolPerL();

    // MathML.
    testMathMLGeneration();
    testMathMLDerivative();
    testMathMLRoundTrip();

    // Parser.
    testParserMinimal();
    testParserEquation();
    testParserDerivative();
    testParserPiecewise();
    testParserMap();
    testParserGroup();

    // Serializer.
    testSerializerBasic();

    // Round-trip.
    testRoundTripSimple();
    testRoundTripCellMLXML();

    std::cout << "\n=============================\n";
    std::cout << "Tests: " << gTests
              << "  Passed: " << gPassed
              << "  Failed: " << gFailed << "\n";

    return gFailed > 0 ? 1 : 0;
}
