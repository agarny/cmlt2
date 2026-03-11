/*
 * CellML Text 2.0 — Public API (implementation)
 */

#include "api.h"
#include "parser.h"
#include "serializer.h"

#include <libcellml/issue.h>
#include <libcellml/parser.h>
#include <libcellml/printer.h>

namespace cellmltext {

libcellml::ModelPtr textToModel(const std::string &cellmlText,
                                std::vector<Error> *errors) {
    Parser parser;
    auto model = parser.parse(cellmlText);

    if (errors) {
        for (auto &pe : parser.errors())
            errors->push_back({pe.line, pe.column, pe.message});
    }
    return model;
}

std::string modelToText(const libcellml::ModelPtr &model,
                        std::vector<Error> *errors) {
    Serializer serializer;
    std::string text = serializer.serialize(model);

    if (errors) {
        for (auto &se : serializer.errors())
            errors->push_back({0, 0, se.message});
    }
    return text;
}

std::string textToCellML(const std::string &cellmlText,
                         std::vector<Error> *errors) {
    auto model = textToModel(cellmlText, errors);
    if (!model) return "";

    auto printer = libcellml::Printer::create();
    return printer->printModel(model);
}

std::string cellMLToText(const std::string &cellmlXml,
                         std::vector<Error> *errors) {
    auto parser = libcellml::Parser::create();
    auto model = parser->parseModel(cellmlXml);

    if (!model) {
        if (errors)
            errors->push_back({0, 0, "Failed to parse CellML XML"});
        return "";
    }

    // Report any libCellML parse errors.
    if (errors) {
        for (size_t i = 0; i < parser->errorCount(); ++i) {
            auto issue = parser->error(i);
            errors->push_back({0, 0, issue->description()});
        }
    }

    return modelToText(model, errors);
}

} // namespace cellmltext
