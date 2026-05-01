#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "compiler_api.hpp"

namespace fs = std::filesystem;

struct ParamDef {
    std::string name;
    std::string defaultValue;
    bool hasDefault = false;
};

struct FunctionDef {
    std::string name;
    std::vector<ParamDef> paramDefs;
    std::vector<std::string> body;
    bool exported = false;
    bool isMacro = false;
    bool isInline = false;
};

struct ClassDef {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<FunctionDef> methods;
};

struct EnumDef {
    std::string name;
    std::vector<std::string> variants;
};

struct StructDef {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields;
};

struct Program {
    std::string moduleName;
    std::vector<std::string> topLevelLines;
    std::vector<FunctionDef> functions;
    std::vector<ClassDef> classes;
    std::vector<EnumDef> enums;
    std::vector<StructDef> structs;
};

struct PreprocessResult {
    std::vector<std::string> lines;
    bool pragmaNoPause = false;
    bool pragmaPauseOn = false;
    std::string pragmaEncoding;
};

struct LoopLabels {
    std::string breakLabel;
    std::string continueLabel;
};

enum class LexTokenKind {
    Identifier,
    Number,
    String,
    Symbol
};

struct LexToken {
    LexTokenKind kind;
    std::string text;
};

std::string ltrim(const std::string& s);
std::string rtrim(const std::string& s);
std::string trim(const std::string& s);
bool startsWith(const std::string& s, const std::string& prefix);
bool isArithmeticExpr(const std::string& s);
bool isIdentifierStartChar(char c);
bool isIdentifierChar(char c);
bool isValidIdentifier(const std::string& s);
bool isSupportedAssignmentOperator(const std::string& op);
std::vector<LexToken> tokenizeLine(const std::string& line);
bool tryParseKeywordHeader(const std::string& line, const std::string& keyword, bool requireOpenBrace, std::string& nameOut);
bool tryParseVarDeclLine(const std::string& line, std::string& nameOut, bool& hasInitializer, std::string& initializerOut);
std::string replaceAll(std::string s, const std::string& from, const std::string& to);
std::string unwrapQuotedLiteral(std::string s);
std::vector<std::string> splitArgs(const std::string& raw);
std::vector<ParamDef> parseParamDecls(const std::string& raw);
std::string stripBlockComments(std::string s);
std::vector<std::string> mergeTripleQuoteLines(std::vector<std::string> lines);
PreprocessResult preprocessSource(const std::string& fileContent);
PreprocessResult readAndPreprocess(const fs::path& file);
std::string toBatchToken(std::string value);
int netBraceDelta(const std::string& line);
std::string formatDiagnostic(const std::string& message, const std::string& file, size_t line, const std::string& sourceLine);
bool tryParseFunctionHeader(const std::string& lineIn, std::string& nameOut, std::vector<ParamDef>& paramsOut,
                            bool& exported, bool& isMacro, bool& isInline);

class Compiler {
public:
    std::string compileFile(const fs::path& input, const CompileOptions& options = {});

private:
    std::vector<fs::path> importOrder_;
    std::unordered_set<std::string> imported_;
    std::unordered_set<std::string> importing_;
    std::vector<std::string> importStack_;
    std::unordered_map<std::string, Program> programs_;
    std::unordered_map<std::string, std::vector<std::string>> sourceLinesByFile_;
    std::unordered_map<std::string, std::string> objectClass_;
    std::unordered_map<std::string, std::unordered_set<std::string>> exportsByModule_;
    std::vector<std::vector<std::string>> helperBlocks_;
    std::unordered_map<std::string, std::vector<FunctionDef>> overloadsByModName_;
    std::unordered_map<std::string, StructDef> structDefs_;
    int tryCounter_ = 0;
    int loopCounter_ = 0;
    int ifCounter_ = 0;
    int forCounter_ = 0;
    int matchCounter_ = 0;
    bool pragmaNoPause_ = false;
    bool pragmaPauseOn_ = false;
    std::string pragmaEncoding_;
    bool stdlibEmitted_ = false;
    bool verbose_ = false;
    bool debugEcho_ = false;
    bool emitMetadataHeader_ = false;
    std::string compilerVersion_ = "unknown";
    fs::path rootInputPath_;

    void compileImportsRecursive(const fs::path& file);
    void checkUnknownModuleFunctionRefs();
    Program parseProgram(const std::vector<std::string>& lines);
    void buildOverloadMap();
    static std::string mangleFunctionLabel(const std::string& modulePrefix, const std::string& fname, size_t arity);
    const FunctionDef* resolveOverload(const std::string& mod, const std::string& name, const std::vector<std::string>& given,
                                       std::vector<std::string>& paddedOut);
    void checkCrossModuleExport(const std::string& callerMod, const std::string& calleeMod, const std::string& fnName);

    std::string replaceArrayAccess(std::string s);
    std::string replaceThisAccess(std::string s);
    std::string replaceInterpolationSyntax(std::string s);
    std::string replaceLenExpr(std::string s);
    std::string normalizeExpr(std::string s);
    std::vector<std::string> transpileLine(const std::string& rawLine, const std::string& moduleName,
                                           const std::vector<LoopLabels>* loopStack);
    std::vector<std::string> expandMacroBody(const FunctionDef& fn, const std::vector<std::string>& args,
                                             const std::string& moduleName, const std::vector<LoopLabels>* loopStack);
    std::vector<std::string> emitCallLines(const std::string& callerMod, const std::string& calleeMod,
                                           const std::string& fname, const std::vector<std::string>& argsGiven,
                                           const std::string* retVar, const std::vector<LoopLabels>* loopStack);
    std::vector<std::string> transpileLines(const std::vector<std::string>& rawLines, const std::string& moduleName,
                                            const std::vector<LoopLabels>* loopStack, const std::string& sourceFile);

    void emitFunction(std::ostringstream& out, const std::string& fullName, const FunctionDef& fn,
                      const std::string& moduleName, const std::string& sourceFile);
    void emitClassMethod(std::ostringstream& out, const ClassDef& c, const FunctionDef& m, const std::string& sourceFile);
    void emitRuntimeStdlib(std::ostringstream& out);
    std::string emitBatch();
};
