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
#include <libcellml/parser.h>
#include <libcellml/printer.h>
#include <libcellml/reset.h>
#include <libcellml/variable.h>

#include <cassert>
#include <cmath>
#include <fstream>
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

    Lexer lex("TestModel { }");
    auto tokens = lex.tokenize();

    CHECK(tokens.size() >= 4); // identifier, {, }, EOF
    CHECK_EQ(tokens[0].type, TokenType::Identifier);
    CHECK_EQ(tokens[0].value, std::string("TestModel"));
    CHECK_EQ(tokens[1].type, TokenType::LBrace);
}

static void testLexerOperators() {
    SUITE("Lexer/Operators");

    Lexer lex("a + b * c / d ^ 2 == e");
    auto tokens = lex.tokenize();

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

    Lexer lex("import as unit reset when at order otherwise and or not true false pi inf nan");
    auto tokens = lex.tokenize();

    CHECK_EQ(tokens[0].type, TokenType::Import);
    CHECK_EQ(tokens[1].type, TokenType::As);
    CHECK_EQ(tokens[2].type, TokenType::Unit);
    CHECK_EQ(tokens[3].type, TokenType::Reset);
    CHECK_EQ(tokens[4].type, TokenType::When);
    CHECK_EQ(tokens[5].type, TokenType::At);
    CHECK_EQ(tokens[6].type, TokenType::Order);
    CHECK_EQ(tokens[7].type, TokenType::Otherwise);
    CHECK_EQ(tokens[8].type, TokenType::And);
    CHECK_EQ(tokens[9].type, TokenType::Or);
    CHECK_EQ(tokens[10].type, TokenType::Not);
    CHECK_EQ(tokens[11].type, TokenType::True);
    CHECK_EQ(tokens[12].type, TokenType::False);
    CHECK_EQ(tokens[13].type, TokenType::Pi);
    CHECK_EQ(tokens[14].type, TokenType::Inf);
    CHECK_EQ(tokens[15].type, TokenType::Nan);
}

static void testLexerFormerKeywords() {
    SUITE("Lexer/FormerKeywords");

    // model, component, map, group, contains are now regular identifiers
    Lexer lex("model component map group contains");
    auto tokens = lex.tokenize();

    CHECK_EQ(tokens[0].type, TokenType::Identifier);
    CHECK_EQ(tokens[0].value, std::string("model"));
    CHECK_EQ(tokens[1].type, TokenType::Identifier);
    CHECK_EQ(tokens[1].value, std::string("component"));
    CHECK_EQ(tokens[2].type, TokenType::Identifier);
    CHECK_EQ(tokens[3].type, TokenType::Identifier);
    CHECK_EQ(tokens[4].type, TokenType::Identifier);
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
TestModel {
    membrane {
        V: mV = -75.0
        t: ms
    }
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
Test {
    C {
        x: dimensionless
        y: dimensionless

        x = 2 * y + 3
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    auto comp = model->component(0);
    CHECK(!comp->math().empty());

    std::string math = comp->math();
    CHECK(math.find("<math") != std::string::npos);
    CHECK(math.find("<apply><eq/>") != std::string::npos);
}

static void testParserDerivative() {
    SUITE("Parser/Derivative");

    std::string input = R"(
Test {
    C {
        V: mV
        t: ms

        d(V)/d(t) = -10.0
    }
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
Test {
    C {
        x: dimensionless
        V: mV

        x = {
            1.0  when V > 0
            0.0  otherwise
        }
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

static void testParserNested() {
    SUITE("Parser/Nested");

    std::string input = R"(
Test {
    parent {
        V: mV = -75.0

        child {
            V: parent.V
        }
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    CHECK(model != nullptr);
    CHECK_EQ(model->name(), std::string("Test"));
    CHECK_EQ(model->componentCount(), size_t(1));

    auto parent = model->component(0);
    CHECK_EQ(parent->name(), std::string("parent"));
    CHECK_EQ(parent->componentCount(), size_t(1));

    auto child = parent->component(0);
    CHECK_EQ(child->name(), std::string("child"));

    auto childV = child->variable("V");
    CHECK(childV != nullptr);
    if (childV) {
        CHECK(childV->equivalentVariableCount() > 0);
    }
}

static void testParserConnection() {
    SUITE("Parser/Connection");

    std::string input = R"(
Test {
    membrane {
        V: mV = -75.0
        I: channel.I

        channel {
            V: membrane.V
            I: uA/cm^2

            I = V * 0.5
        }
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    auto membrane = model->component(0);
    auto channel = membrane->component(0);

    // membrane.I connected to channel.I
    auto memI = membrane->variable("I");
    CHECK(memI != nullptr);
    if (memI) CHECK(memI->equivalentVariableCount() > 0);

    // channel.V connected to membrane.V
    auto chanV = channel->variable("V");
    CHECK(chanV != nullptr);
    if (chanV) CHECK(chanV->equivalentVariableCount() > 0);
}

static void testParserReset() {
    SUITE("Parser/Reset");

    std::string input = R"(
Test {
    neuron {
        V: mV = -65.0
        V_thresh: mV = -50.0
        V_reset: mV = -65.0

        reset V at order 1 when V > V_thresh {
            V = V_reset
        }
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    CHECK(parser.errors().empty());

    auto comp = model->component(0);
    CHECK_EQ(comp->resetCount(), size_t(1));

    auto reset = comp->reset(0);
    CHECK(reset != nullptr);
    if (reset) {
        CHECK_EQ(reset->order(), 1);
        auto var = reset->variable();
        CHECK(var != nullptr);
        if (var) CHECK_EQ(var->name(), std::string("V"));
    }
}

static void testParserCustomUnit() {
    SUITE("Parser/CustomUnit");

    std::string input = R"(
Test {
    unit beats_per_min = 1/60 * Hz

    heart {
        rate: beats_per_min = 72.0
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    CHECK(parser.errors().empty());

    // Check that the custom unit was created.
    CHECK(model->unitsCount() > 0);
    bool found = false;
    for (size_t i = 0; i < model->unitsCount(); ++i) {
        if (model->units(i)->name() == "beats_per_min") {
            found = true;
            break;
        }
    }
    CHECK(found);

    auto comp = model->component(0);
    auto rate = comp->variable("rate");
    CHECK(rate != nullptr);
    if (rate) CHECK_EQ(rate->initialValue(), std::string("72.0"));
}

static void testParserMultipleComponents() {
    SUITE("Parser/MultipleComponents");

    std::string input = R"(
Test {
    A {
        x: dimensionless = 1.0
    }
    B {
        y: dimensionless = 2.0
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    CHECK_EQ(model->componentCount(), size_t(2));
    CHECK_EQ(model->component(0)->name(), std::string("A"));
    CHECK_EQ(model->component(1)->name(), std::string("B"));
}

static void testParserDeepNesting() {
    SUITE("Parser/DeepNesting");

    std::string input = R"(
Test {
    L1 {
        x: mV = 1.0

        L2 {
            x: L1.x

            L3 {
                x: L1.x
            }
        }
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    CHECK(parser.errors().empty());

    auto L1 = model->component(0);
    CHECK_EQ(L1->componentCount(), size_t(1));
    auto L2 = L1->component(0);
    CHECK_EQ(L2->componentCount(), size_t(1));
    auto L3 = L2->component(0);
    CHECK_EQ(L3->name(), std::string("L3"));

    // L3's x should be connected (through equivalence chain)
    auto xL3 = L3->variable("x");
    CHECK(xL3 != nullptr);
    if (xL3) CHECK(xL3->equivalentVariableCount() > 0);
}

static void testParserMathFunctions() {
    SUITE("Parser/MathFunctions");

    std::string input = R"(
Test {
    math {
        x: dimensionless = 1.0
        y: dimensionless
        z: dimensionless

        y = sin(x) + cos(x) + exp(-x) + sqrt(abs(x))
        z = ln(x + 1) + min(x, 2.0) + max(x, 0.0)
    }
}
)";

    Parser parser;
    auto model = parser.parse(input);
    CHECK(parser.errors().empty());

    auto comp = model->component(0);
    std::string math = comp->math();
    CHECK(math.find("<sin/>") != std::string::npos);
    CHECK(math.find("<cos/>") != std::string::npos);
    CHECK(math.find("<exp/>") != std::string::npos);
    CHECK(math.find("<abs/>") != std::string::npos);
    CHECK(math.find("<ln/>") != std::string::npos);
    CHECK(math.find("<min/>") != std::string::npos);
    CHECK(math.find("<max/>") != std::string::npos);
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

    CHECK(text.find("TestModel {") != std::string::npos);
    CHECK(text.find("membrane {") != std::string::npos);
    CHECK(text.find("V:") != std::string::npos);
    // Should NOT contain the word "component"
    CHECK(text.find("component") == std::string::npos);
}

static void testSerializerConnection() {
    SUITE("Serializer/Connection");

    auto model = libcellml::Model::create("ConnTest");
    auto parent = libcellml::Component::create("membrane");
    auto child = libcellml::Component::create("channel");

    auto parentV = libcellml::Variable::create("V");
    parentV->setUnits("volt");
    parentV->setInitialValue(-75.0);
    parent->addVariable(parentV);

    auto childV = libcellml::Variable::create("V");
    childV->setUnits("volt");
    child->addVariable(childV);

    auto childI = libcellml::Variable::create("I");
    childI->setUnits("ampere");
    child->addVariable(childI);

    parent->addComponent(child);
    model->addComponent(parent);

    libcellml::Variable::addEquivalence(parentV, childV);

    Serializer ser;
    std::string text = ser.serialize(model);

    // Child's V should appear as a connection.
    CHECK(text.find("V: membrane.V") != std::string::npos);
    // Child's I (no equivalence) should appear with units.
    CHECK(text.find("I: A") != std::string::npos);
}

static void testSerializerNested() {
    SUITE("Serializer/Nested");

    auto model = libcellml::Model::create("NestTest");
    auto parent = libcellml::Component::create("parent");
    auto child = libcellml::Component::create("child");
    auto grandchild = libcellml::Component::create("grandchild");

    auto pv = libcellml::Variable::create("x");
    pv->setUnits("dimensionless");
    pv->setInitialValue(1.0);
    parent->addVariable(pv);

    child->addComponent(grandchild);
    parent->addComponent(child);
    model->addComponent(parent);

    Serializer ser;
    std::string text = ser.serialize(model);

    // Verify nesting structure
    CHECK(text.find("parent {") != std::string::npos);
    CHECK(text.find("child {") != std::string::npos);
    CHECK(text.find("grandchild {") != std::string::npos);
}

static void testSerializerReset() {
    SUITE("Serializer/Reset");

    auto model = libcellml::Model::create("ResetTest");
    auto comp = libcellml::Component::create("neuron");

    auto V = libcellml::Variable::create("V");
    V->setUnits("volt");
    V->setInitialValue(-0.065);
    comp->addVariable(V);

    auto Vreset = libcellml::Variable::create("V_reset");
    Vreset->setUnits("volt");
    Vreset->setInitialValue(-0.065);
    comp->addVariable(Vreset);

    auto reset = libcellml::Reset::create();
    reset->setVariable(V);
    reset->setTestVariable(V);
    reset->setOrder(1);
    reset->setTestValue(
        "<math xmlns=\"http://www.w3.org/1998/Math/MathML\""
        " xmlns:cellml=\"http://www.cellml.org/cellml/2.0#\">"
        "<apply><gt/><ci>V</ci><cn cellml:units=\"volt\">-0.05</cn></apply>"
        "</math>");
    reset->setResetValue(
        "<math xmlns=\"http://www.w3.org/1998/Math/MathML\""
        " xmlns:cellml=\"http://www.cellml.org/cellml/2.0#\">"
        "<ci>V_reset</ci>"
        "</math>");
    comp->addReset(reset);
    model->addComponent(comp);

    Serializer ser;
    std::string text = ser.serialize(model);

    CHECK(text.find("reset V at order 1 when") != std::string::npos);
}

// =====================================================================
//  Round-trip tests
// =====================================================================

static void testRoundTripSimple() {
    SUITE("RoundTrip/Simple");

    std::string input = R"(
RoundTripTest {
    main {
        x: dimensionless = 1.0
        y: dimensionless

        y = 2.0 * x + 3.0
    }
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
    CHECK(output.find("RoundTripTest") != std::string::npos);
    CHECK(output.find("main {") != std::string::npos);
    CHECK(output.find("x: dimensionless") != std::string::npos);
}

static void testRoundTripTextToXmlToText() {
    SUITE("RoundTrip/Text→XML→Text");

    std::string input = R"(
Noble62 {
    membrane {
        V: mV = -87.0
        t: ms
        Cm: uF/cm^2 = 12.0
        I_Na: uA/cm^2
        I_K: uA/cm^2

        Cm * d(V)/d(t) = -(I_Na + I_K)
    }
}
)";

    // Text → CellML XML
    std::vector<Error> errors;
    std::string xml = textToCellML(input, &errors);
    CHECK(!xml.empty());
    for (auto &e : errors)
        std::cerr << "  Error: " << e.message << "\n";
    CHECK(xml.find("<model") != std::string::npos);

    // CellML XML → Text
    errors.clear();
    std::string text = cellMLToText(xml, &errors);
    CHECK(!text.empty());
    for (auto &e : errors)
        std::cerr << "  Error: " << e.message << "\n";

    // Verify round-tripped text preserves key elements.
    CHECK(text.find("Noble62") != std::string::npos);
    CHECK(text.find("membrane") != std::string::npos);
    CHECK(text.find("V:") != std::string::npos);
    CHECK(text.find("-87.0") != std::string::npos);
}

static void testRoundTripXmlToTextToXml() {
    SUITE("RoundTrip/XML→Text→XML");

    // Start with a hand-crafted CellML XML model.
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<model xmlns="http://www.cellml.org/cellml/2.0#" name="RoundTrip">
  <component name="main">
    <variable name="x" units="dimensionless" initial_value="5.0"/>
    <variable name="y" units="dimensionless"/>
    <math xmlns="http://www.w3.org/1998/Math/MathML"
          xmlns:cellml="http://www.cellml.org/cellml/2.0#">
      <apply><eq/>
        <ci>y</ci>
        <apply><times/>
          <cn cellml:units="dimensionless">3.0</cn>
          <ci>x</ci>
        </apply>
      </apply>
    </math>
  </component>
</model>
)";

    // XML → Model via libCellML parser
    auto cellmlParser = libcellml::Parser::create();
    auto model1 = cellmlParser->parseModel(xml);
    CHECK(model1 != nullptr);
    CHECK_EQ(model1->name(), std::string("RoundTrip"));
    CHECK_EQ(model1->componentCount(), size_t(1));

    // Model → Text
    std::vector<Error> errors;
    std::string text = modelToText(model1, &errors);
    CHECK(!text.empty());
    CHECK(text.find("RoundTrip") != std::string::npos);
    CHECK(text.find("main") != std::string::npos);

    // Text → Model
    errors.clear();
    auto model2 = textToModel(text, &errors);
    CHECK(model2 != nullptr);
    for (auto &e : errors)
        std::cerr << "  Error: " << e.message << "\n";

    // Compare structural properties
    CHECK_EQ(model2->name(), std::string("RoundTrip"));
    CHECK_EQ(model2->componentCount(), size_t(1));

    auto comp2 = model2->component(0);
    CHECK_EQ(comp2->name(), std::string("main"));
    CHECK_EQ(comp2->variableCount(), size_t(2));

    auto x2 = comp2->variable("x");
    CHECK(x2 != nullptr);
    if (x2) {
        CHECK_EQ(x2->initialValue(), std::string("5.0"));
    }
    CHECK(!comp2->math().empty());

    // Model → XML (final)
    auto printer = libcellml::Printer::create();
    std::string finalXml = printer->printModel(model2);
    CHECK(!finalXml.empty());
    CHECK(finalXml.find("RoundTrip") != std::string::npos);
    CHECK(finalXml.find("main") != std::string::npos);
}

static void testRoundTripWithConnections() {
    SUITE("RoundTrip/Connections");

    std::string input = R"(
ConnModel {
    parent {
        V: mV = -75.0
        I: child.I

        child {
            V: parent.V
            I: uA/cm^2 = 0.0
        }
    }
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

    // Text → Model (again)
    errors.clear();
    auto model2 = textToModel(output, &errors);
    CHECK(model2 != nullptr);
    for (auto &e : errors)
        std::cerr << "  Error: " << e.message << "\n";

    // Verify structure preserved
    CHECK_EQ(model2->componentCount(), size_t(1));
    auto parent = model2->component(0);
    CHECK_EQ(parent->name(), std::string("parent"));
    CHECK_EQ(parent->componentCount(), size_t(1));

    auto child = parent->component(0);
    CHECK_EQ(child->name(), std::string("child"));
}

static void testRoundTripXmlWithHierarchy() {
    SUITE("RoundTrip/XMLHierarchy");

    // CellML XML with encapsulation and connections
    std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<model xmlns="http://www.cellml.org/cellml/2.0#" name="HierTest">
  <component name="parent">
    <variable name="V" units="volt" initial_value="-0.075"
              interface="public_and_private"/>
  </component>
  <component name="child">
    <variable name="V" units="volt" interface="public"/>
  </component>
  <connection component_1="parent" component_2="child">
    <map_variables variable_1="V" variable_2="V"/>
  </connection>
  <encapsulation>
    <component_ref component="parent">
      <component_ref component="child"/>
    </component_ref>
  </encapsulation>
</model>
)";

    // XML → Model
    auto cellmlParser = libcellml::Parser::create();
    auto model1 = cellmlParser->parseModel(xml);
    CHECK(model1 != nullptr);

    // Model → Text
    std::vector<Error> errors;
    std::string text = modelToText(model1, &errors);
    CHECK(!text.empty());

    // Text should show the connection as dot-notation
    CHECK(text.find("parent") != std::string::npos);
    CHECK(text.find("child") != std::string::npos);
    CHECK(text.find("V: parent.V") != std::string::npos);

    // Text → Model
    errors.clear();
    auto model2 = textToModel(text, &errors);
    CHECK(model2 != nullptr);
    for (auto &e : errors)
        std::cerr << "  Error: " << e.message << "\n";

    // Verify hierarchy
    auto parent = model2->component(0);
    CHECK_EQ(parent->name(), std::string("parent"));
    CHECK_EQ(parent->componentCount(), size_t(1));
    auto child = parent->component(0);
    CHECK_EQ(child->name(), std::string("child"));

    // Verify equivalence
    auto childV = child->variable("V");
    CHECK(childV != nullptr);
    if (childV) CHECK(childV->equivalentVariableCount() > 0);
}

// =====================================================================
//  Hodgkin-Huxley round-trip test
// =====================================================================

static void testRoundTripHodgkinHuxley() {
    SUITE("RoundTrip/HodgkinHuxley");

    std::string input = R"(
HodgkinHuxley1952 {
  membrane {
    V: mV = -75.0
    t: ms
    Cm: uF/cm^2 = 1.0
    I_Na: sodium_channel.I_Na
    I_K: potassium_channel.I_K
    I_L: leak_channel.I_L
    I_stim: uA/cm^2 = 0.0

    Cm * d(V)/d(t) = -(I_Na + I_K + I_L) + I_stim

    sodium_channel {
      V: membrane.V
      I_Na: uA/cm^2
      g_Na: mS/cm^2 = 120.0
      E_Na: mV = 50.0
      m: sodium_channel_m_gate.m
      h: sodium_channel_h_gate.h

      I_Na = g_Na * m^3 * h * (V - E_Na)

      sodium_channel_m_gate {
        V: membrane.V
        t: membrane.t
        m: dimensionless = 0.05
        alpha_m: 1/ms
        beta_m: 1/ms

        d(m)/d(t) = alpha_m * (1 - m) - beta_m * m
        alpha_m = {
          0.1 * (V + 25) / (exp((V + 25) / 10) - 1)  when V != -25
          1.0  otherwise
        }
        beta_m = 4.0 * exp(V / 18)
      }

      sodium_channel_h_gate {
        V: membrane.V
        t: membrane.t
        h: dimensionless = 0.6
        alpha_h: 1/ms
        beta_h: 1/ms

        d(h)/d(t) = alpha_h * (1 - h) - beta_h * h
        alpha_h = 0.07 * exp(V / 20)
        beta_h = 1 / (exp((V + 30) / 10) + 1)
      }
    }

    potassium_channel {
      V: membrane.V
      I_K: uA/cm^2
      g_K: mS/cm^2 = 36.0
      E_K: mV = -77.0
      n: potassium_channel_n_gate.n

      I_K = g_K * n^4 * (V - E_K)

      potassium_channel_n_gate {
        V: membrane.V
        t: membrane.t
        n: dimensionless = 0.325
        alpha_n: 1/ms
        beta_n: 1/ms

        d(n)/d(t) = alpha_n * (1 - n) - beta_n * n
        alpha_n = {
            0.01 * (V + 10) / (exp((V + 10) / 10) - 1)  when V != -10
            0.1  otherwise
        }
        beta_n = 0.125 * exp(V / 80)
      }
    }

    leak_channel {
      V: membrane.V
      I_L: uA/cm^2
      g_L: mS/cm^2 = 0.3
      E_L: mV = -54.4

      I_L = g_L * (V - E_L)
    }
  }
}
)";

    // Text → Model
    std::vector<Error> errors;
    auto model = textToModel(input, &errors);
    CHECK(model != nullptr);
    for (auto &e : errors)
        std::cerr << "  Parse error: " << e.message << "\n";
    CHECK(errors.empty());

    // Model → Text
    errors.clear();
    std::string output = modelToText(model, &errors);
    CHECK(!output.empty());

    // Verify no redundant unit definitions are emitted.
    CHECK(output.find("unit ") == std::string::npos);

    // Verify compact SI forms are used (not CellML names).
    CHECK(output.find("V: mV") != std::string::npos);
    CHECK(output.find("t: ms") != std::string::npos);
    CHECK(output.find("Cm: uF/cm^2") != std::string::npos);
    CHECK(output.find("alpha_m: 1/ms") != std::string::npos);

    // Verify key structure.
    CHECK(output.find("HodgkinHuxley1952") != std::string::npos);
    CHECK(output.find("membrane {") != std::string::npos);
    CHECK(output.find("sodium_channel {") != std::string::npos);
    CHECK(output.find("d(V)/d(t)") != std::string::npos);
    CHECK(output.find("d(m)/d(t)") != std::string::npos);

    // Verify stable round-trip: Text → Model → Text → Model → Text
    errors.clear();
    auto model2 = textToModel(output, &errors);
    CHECK(model2 != nullptr);
    CHECK(errors.empty());

    errors.clear();
    std::string output2 = modelToText(model2, &errors);
    CHECK_EQ(output, output2);
}

// =====================================================================
//  Unit multiplier tests
// =====================================================================

static void testUnitMultiplier() {
    SUITE("Units/Multiplier");

    auto pu = parseUnitExpression("3600*s");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(1));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("second"));
    CHECK(pu.factors[0].multiplier == 3600.0);
    CHECK(pu.factors[0].exponent == 1.0);
}

static void testUnitMultiplierFraction() {
    SUITE("Units/MultiplierFraction");

    auto pu = parseUnitExpression("1/60*Hz");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(1));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("hertz"));
    CHECK(std::abs(pu.factors[0].multiplier - 1.0/60.0) < 1e-10);
    CHECK(pu.factors[0].exponent == 1.0);
}

static void testUnitInlinePrefixed() {
    SUITE("Units/InlinePrefixed");

    // mg/L should parse: milligram per litre
    auto pu = parseUnitExpression("mg/L");
    CHECK(pu.valid);
    CHECK_EQ(pu.factors.size(), size_t(2));
    CHECK_EQ(pu.factors[0].cellmlUnit, std::string("gram"));
    CHECK_EQ(pu.factors[0].cellmlPrefix, std::string("milli"));
    CHECK(pu.factors[0].exponent == 1.0);
    CHECK_EQ(pu.factors[1].cellmlUnit, std::string("litre"));
    CHECK(pu.factors[1].exponent == -1.0);
}

static void testRoundTripCustomUnits() {
    SUITE("RoundTrip/CustomUnits");

    std::string input = R"(
PharmacokineticModel {
  unit h = 3600*s

  body {
    C: mg/L = 100.0
    t: h
    k_e: 1/h = 0.1
    C_half: mg/L

    d(C)/d(t) = -k_e * C
    C_half = C / 2.0
  }
}
)";

    // Text → Model
    std::vector<Error> errors;
    auto model = textToModel(input, &errors);
    CHECK(model != nullptr);
    CHECK(errors.empty());

    // Model → Text
    errors.clear();
    std::string output = modelToText(model, &errors);
    CHECK(!output.empty());

    // Verify the custom unit definitions are preserved.
    CHECK(output.find("unit h = 3600*s") != std::string::npos);
    CHECK(output.find("unit per_h = 1/h") != std::string::npos);

    // Verify inline SI prefixed units are used (no explicit definitions).
    CHECK(output.find("C: mg/L") != std::string::npos);
    CHECK(output.find("k_e: per_h") != std::string::npos);
    CHECK(output.find("t: h") != std::string::npos);

    // Verify stable round-trip.
    errors.clear();
    auto model2 = textToModel(output, &errors);
    CHECK(model2 != nullptr);
    CHECK(errors.empty());

    errors.clear();
    std::string output2 = modelToText(model2, &errors);
    CHECK_EQ(output, output2);
}

// =====================================================================
//  XML → Text round-trip tests (from CellML model files)
// =====================================================================

static std::string readFile(const std::string &path) {
    std::ifstream f(path);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

// Helper: parse CellML XML → Model → Text (pass1) → Model → Text (pass2),
// check structural properties and stable round-trip.
static void xmlRoundTrip(const std::string &filename,
                          const std::string &expectedModelName,
                          size_t minTopComponents,
                          const std::vector<std::string> &mustContain) {
    std::string path = std::string(TEST_MODELS_DIR) + "/" + filename;
    std::string xml = readFile(path);
    CHECK(!xml.empty());
    if (xml.empty()) return;

    // XML → Model via libCellML parser.
    auto cellmlParser = libcellml::Parser::create();
    auto model = cellmlParser->parseModel(xml);
    CHECK(model != nullptr);
    if (!model) return;
    CHECK_EQ(model->name(), expectedModelName);

    // Model → Text (pass 1).
    std::vector<Error> errors;
    std::string text1 = modelToText(model, &errors);
    CHECK(!text1.empty());
    for (auto &e : errors)
        std::cerr << "  Serializer error: " << e.message << "\n";

    // Check model name and top-level components.
    CHECK(text1.find(expectedModelName) != std::string::npos);
    CHECK(model->componentCount() >= minTopComponents);

    // Check required substrings.
    for (auto &s : mustContain)
        CHECK(text1.find(s) != std::string::npos);

    // No explicit unit definitions should be emitted for these models
    // (all units are auto-derivable from SI).
    CHECK(text1.find("unit ") == std::string::npos);

    // Text → Model (pass 2).
    errors.clear();
    auto model2 = textToModel(text1, &errors);
    CHECK(model2 != nullptr);
    for (auto &e : errors)
        std::cerr << "  Parse error (pass 2): L" << e.line << ":"
                  << e.column << " " << e.message << "\n";
    CHECK(errors.empty());
    if (!model2) return;

    // Model → Text (pass 2).
    errors.clear();
    std::string text2 = modelToText(model2, &errors);

    // Stable round-trip.
    CHECK_EQ(text1, text2);
}

static void testRoundTripXmlHodgkinHuxley1952() {
    SUITE("RoundTrip/XML/HodgkinHuxley1952");

    xmlRoundTrip(
        "hodgkin_huxley_squid_axon_model_1952.cellml",
        "hodgkin_huxley_squid_axon_model_1952",
        4,  // environment, membrane, sodium_channel, potassium_channel, leakage_current
        {
            "membrane {",
            "sodium_channel {",
            "potassium_channel {",
            "leakage_current {",
            "V: mV",
            "time: ms",
            "Cm: uF/cm^2",
            "g_Na: mS/cm^2",
            "d(V)/d(time)",
            "d(m)/d(time)",
            "d(h)/d(time)",
            "d(n)/d(time)",
        });
}

static void testRoundTripXmlNoble1962() {
    SUITE("RoundTrip/XML/Noble1962");

    xmlRoundTrip(
        "noble_model_1962.cellml",
        "noble_model_1962",
        4,  // environment, membrane, sodium_channel, potassium_channel, leakage_current
        {
            "membrane {",
            "sodium_channel {",
            "potassium_channel {",
            "leakage_current {",
            "V: mV",
            "time: ms",
            "alpha_m: 1/ms",
            "d(V)/d(time)",
            "d(m)/d(time)",
            "d(h)/d(time)",
            "d(n)/d(time)",
        });
}

static void testRoundTripXmlGarny2003() {
    SUITE("RoundTrip/XML/Garny2003");

    xmlRoundTrip(
        "garny_kohl_hunter_boyett_noble_rabbit_san_model_2003.cellml",
        "garny_2003",
        10,  // many components
        {
            "membrane {",
            "sodium_current {",
            "L_type_Ca_channel {",
            "V: mV",
            "d(V)/d(time)",
        });
}

static void testRoundTripXmlFabbri2017() {
    SUITE("RoundTrip/XML/Fabbri2017");

    xmlRoundTrip(
        "fabbri_fantini_wilders_severi_human_san_model_2017.cellml",
        "Human_SAN_Fabbri_Fantini_Wilders_Severi_2017",
        10,  // many components
        {
            "Membrane {",
            "i_Na {",
            "i_CaL {",
            "V: mV",
            "d(V_ode)/d(time)",
        });
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
    testLexerFormerKeywords();

    // Units.
    testUnitSimple();
    testUnitBare();
    testUnitCompound();
    testUnitPerMs();
    testUnitDimensionless();
    testUnitKg();
    testUnitMmolPerL();
    testUnitMultiplier();
    testUnitMultiplierFraction();
    testUnitInlinePrefixed();

    // MathML.
    testMathMLGeneration();
    testMathMLDerivative();
    testMathMLRoundTrip();

    // Parser.
    testParserMinimal();
    testParserEquation();
    testParserDerivative();
    testParserPiecewise();
    testParserNested();
    testParserConnection();
    testParserReset();
    testParserCustomUnit();
    testParserMultipleComponents();
    testParserDeepNesting();
    testParserMathFunctions();

    // Serializer.
    testSerializerBasic();
    testSerializerConnection();
    testSerializerNested();
    testSerializerReset();

    // Round-trip.
    testRoundTripSimple();
    testRoundTripTextToXmlToText();
    testRoundTripXmlToTextToXml();
    testRoundTripWithConnections();
    testRoundTripXmlWithHierarchy();
    testRoundTripHodgkinHuxley();
    testRoundTripCustomUnits();

    // XML → Text round-trips.
    testRoundTripXmlHodgkinHuxley1952();
    testRoundTripXmlNoble1962();
    testRoundTripXmlGarny2003();
    testRoundTripXmlFabbri2017();

    std::cout << "\n=============================\n";
    std::cout << "Tests: " << gTests
              << "  Passed: " << gPassed
              << "  Failed: " << gFailed << "\n";

    return gFailed > 0 ? 1 : 0;
}
