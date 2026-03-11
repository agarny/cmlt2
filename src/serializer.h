/*
 * CellML Text 2.0 — Serializer
 *
 * Converts a libCellML Model into CellML Text 2.0 source.
 */

#pragma once

#include <libcellml/model.h>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace cellmltext {

struct SerializeError {
    std::string message;
};

class Serializer {
public:
    // Serialize a libCellML Model to CellML Text 2.0 format.
    std::string serialize(const libcellml::ModelPtr &model);

    const std::vector<SerializeError> &errors() const { return errors_; }

private:
    void writeModel(const libcellml::ModelPtr &model);
    void writeComponent(const libcellml::ComponentPtr &comp, int indent);
    void writeVariable(const libcellml::VariablePtr &var,
                       const libcellml::ComponentPtr &ownerComp, int indent);
    void writeEquations(const libcellml::ComponentPtr &comp, int indent);
    void writeResets(const libcellml::ComponentPtr &comp, int indent);
    void writeImports(const libcellml::ModelPtr &model, int indent);
    void writeUnitDefinitions(const libcellml::ModelPtr &model, int indent);

    // Connection detection helpers.
    std::set<std::string> getDefinedVarNames(const libcellml::ComponentPtr &comp);
    std::pair<std::string, std::string> findConnectionTarget(
        const libcellml::VariablePtr &var,
        const libcellml::ComponentPtr &ownerComp);

    void write(const std::string &text);
    void writeLine(const std::string &text, int indent = 0);
    void writeIndent(int indent);
    void newline();

    std::string output_;
    std::vector<SerializeError> errors_;
    std::unordered_map<std::string, std::set<std::string>> definedVarsCache_;
};

} // namespace cellmltext
