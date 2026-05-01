#include "compiler_internal.hpp"
#include <iostream>

std::string Compiler::compileFile(const fs::path& input, const CompileOptions& options) {
        importOrder_.clear();
        imported_.clear();
        importing_.clear();
        importStack_.clear();
        programs_.clear();
        sourceLinesByFile_.clear();
        objectClass_.clear();
        exportsByModule_.clear();
        helperBlocks_.clear();
        tryCounter_ = 0;
        loopCounter_ = 0;
        ifCounter_ = 0;
        forCounter_ = 0;
        matchCounter_ = 0;
        pragmaNoPause_ = false;
        pragmaPauseOn_ = false;
        pragmaEncoding_.clear();
        stdlibEmitted_ = false;
        verbose_ = options.verbose;
        debugEcho_ = options.debugEcho;
        emitMetadataHeader_ = options.emitMetadataHeader;
        compilerVersion_ = options.compilerVersion;
        rootInputPath_ = fs::absolute(input);
        compileImportsRecursive(fs::absolute(input));
        buildOverloadMap();
        checkUnknownModuleFunctionRefs();
        return emitBatch();
    }

void Compiler::compileImportsRecursive(const fs::path& file) {
        const auto key = fs::absolute(file).lexically_normal().string();
        if (importing_.count(key)) {
            std::ostringstream cycle;
            bool inCycle = false;
            for (const auto& p : importStack_) {
                if (p == key) inCycle = true;
                if (inCycle) {
                    if (cycle.tellp() > 0) cycle << " -> ";
                    cycle << p;
                }
            }
            if (cycle.tellp() > 0) cycle << " -> ";
            cycle << key;
            throw std::runtime_error("Cyclic import detected: " + cycle.str());
        }
        if (imported_.count(key)) return;
        importing_.insert(key);
        importStack_.push_back(key);
        if (verbose_) {
            std::cerr << "[batppc] resolving import: " << key << "\n";
        }

        auto pr = readAndPreprocess(fs::path(key));
        if (pr.pragmaNoPause) pragmaNoPause_ = true;
        if (pr.pragmaPauseOn) pragmaPauseOn_ = true;
        if (!pr.pragmaEncoding.empty()) pragmaEncoding_ = pr.pragmaEncoding;
        const auto& lines = pr.lines;
        sourceLinesByFile_[key] = lines;
        std::regex importRe(R"REGEX(^\s*import\s+"([^"]+)")REGEX");
        std::smatch m;
        for (size_t lineNo = 0; lineNo < lines.size(); ++lineNo) {
            const auto& raw = lines[lineNo];
            std::string line = trim(raw);
            if (std::regex_search(line, m, importRe)) {
                try {
                    fs::path dep = fs::path(key).parent_path() / m[1].str();
                    compileImportsRecursive(fs::absolute(dep));
                } catch (const std::exception& e) {
                    throw std::runtime_error(formatDiagnostic(e.what(), key, lineNo + 1, raw));
                }
            }
        }

        try {
            programs_[key] = parseProgram(lines);
        } catch (const std::exception& e) {
            throw std::runtime_error(formatDiagnostic(e.what(), key, 0, ""));
        }
        importOrder_.push_back(fs::path(key));
        imported_.insert(key);
        importing_.erase(key);
        importStack_.pop_back();
        if (verbose_) {
            std::cerr << "[batppc] loaded module: " << key << "\n";
        }
    }

void Compiler::checkUnknownModuleFunctionRefs() {
        std::regex modCallRe(R"(\b([A-Za-z_]\w*)::([A-Za-z_]\w*)\s*\()");
        for (const auto& path : importOrder_) {
            const std::string fileKey = path.string();
            auto itLines = sourceLinesByFile_.find(fileKey);
            if (itLines == sourceLinesByFile_.end()) continue;
            for (size_t i = 0; i < itLines->second.size(); ++i) {
                const std::string& raw = itLines->second[i];
                std::string line = trim(raw);
                if (line.empty() || startsWith(line, "//")) continue;
                std::smatch m;
                std::string probe = line;
                while (std::regex_search(probe, m, modCallRe)) {
                    const std::string mod = m[1].str();
                    const std::string fn = m[2].str();
                    const std::string key = mod + "::" + fn;
                    if (!overloadsByModName_.count(key)) {
                        std::cerr << "[batppc] warning: "
                                  << formatDiagnostic("Unknown module function reference '" + key + "'", fileKey, i + 1, raw)
                                  << "\n";
                    }
                    probe = m.suffix().str();
                }
            }
        }
    }

Program Compiler::parseProgram(const std::vector<std::string>& lines) {
        Program p;

        size_t i = 0;
        while (i < lines.size()) {
            std::string line = trim(lines[i]);
            if (line.empty() || startsWith(line, "//")) {
                ++i;
                continue;
            }
            std::string parsedName;
            if (tryParseKeywordHeader(line, "module", false, parsedName)) {
                p.moduleName = parsedName;
                ++i;
                continue;
            }
            if (tryParseKeywordHeader(line, "enum", true, parsedName)) {
                EnumDef e;
                e.name = parsedName;
                ++i;
                while (i < lines.size()) {
                    std::string t = trim(lines[i]);
                    if (t == "}" || t == "};") {
                        ++i;
                        break;
                    }
                    if (t.empty() || startsWith(t, "//")) {
                        ++i;
                        continue;
                    }
                    while (!t.empty() && (t.back() == ',' || t.back() == ';')) {
                        t.pop_back();
                        t = rtrim(t);
                    }
                    t = trim(t);
                    if (!t.empty()) e.variants.push_back(t);
                    ++i;
                }
                p.enums.push_back(std::move(e));
                continue;
            }
            if (tryParseKeywordHeader(line, "struct", true, parsedName)) {
                StructDef s;
                s.name = parsedName;
                ++i;
                int depth = 1;
                while (i < lines.size() && depth > 0) {
                    std::string t = trim(lines[i]);
                    if (t == "{") {
                        depth++;
                        ++i;
                        continue;
                    }
                    if (t == "}" || t == "};") {
                        depth--;
                        ++i;
                        continue;
                    }
                    std::string fieldName;
                    bool hasInit = false;
                    std::string initVal;
                    if (depth == 1 && tryParseVarDeclLine(t, fieldName, hasInit, initVal)) {
                        s.fields.push_back({fieldName, hasInit ? initVal : "\"\""});
                        ++i;
                        continue;
                    }
                    ++i;
                }
                p.structs.push_back(std::move(s));
                continue;
            }
            {
                std::string fnName;
                std::vector<ParamDef> fnParams;
                bool ex = false, macro = false, inl = false;
                if (tryParseFunctionHeader(lines[i], fnName, fnParams, ex, macro, inl)) {
                    FunctionDef f;
                    f.name = fnName;
                    f.paramDefs = std::move(fnParams);
                    f.exported = ex;
                    f.isMacro = macro;
                    f.isInline = inl;
                    ++i;
                    int depth = 1;
                    while (i < lines.size()) {
                        std::string bodyLine = lines[i];
                        int d = netBraceDelta(bodyLine);
                        if (depth + d == 0 && trim(bodyLine) == "}") {
                            depth += d;
                            ++i;
                            break;
                        }
                        f.body.push_back(bodyLine);
                        depth += d;
                        ++i;
                    }
                    p.functions.push_back(std::move(f));
                    continue;
                }
            }
            if (tryParseKeywordHeader(line, "class", true, parsedName)) {
                ClassDef c;
                c.name = parsedName;
                ++i;
                int depth = 1;
                while (i < lines.size() && depth > 0) {
                    std::string t = trim(lines[i]);
                    if (t == "{") {
                        depth++;
                        ++i;
                        continue;
                    }
                    if (t == "}") {
                        depth--;
                        ++i;
                        continue;
                    }
                    std::string fieldName;
                    bool hasInit = false;
                    std::string initVal;
                    if (depth == 1 && tryParseVarDeclLine(t, fieldName, hasInit, initVal)) {
                        c.fields.push_back({fieldName, hasInit ? initVal : "\"\""});
                        ++i;
                        continue;
                    }
                    if (depth == 1) {
                        std::string mn;
                        std::vector<ParamDef> mp;
                        bool mex, mmacro, minl;
                        if (tryParseFunctionHeader(lines[i], mn, mp, mex, mmacro, minl)) {
                            FunctionDef f;
                            f.name = mn;
                            f.paramDefs = std::move(mp);
                            f.isMacro = mmacro;
                            f.isInline = minl;
                            ++i;
                            int fDepth = 1;
                            while (i < lines.size()) {
                                int d = netBraceDelta(lines[i]);
                                if (fDepth + d == 0 && trim(lines[i]) == "}") {
                                    fDepth += d;
                                    ++i;
                                    break;
                                }
                                f.body.push_back(lines[i]);
                                fDepth += d;
                                ++i;
                            }
                            c.methods.push_back(std::move(f));
                            continue;
                        }
                    }
                    ++i;
                }
                p.classes.push_back(std::move(c));
                continue;
            }
            if (!startsWith(line, "import ")) {
                p.topLevelLines.push_back(lines[i]);
            }
            ++i;
        }
        return p;
    }

    std::unordered_map<std::string, std::vector<FunctionDef>> overloadsByModName_;
    std::unordered_map<std::string, StructDef> structDefs_;

void Compiler::buildOverloadMap() {
        overloadsByModName_.clear();
        exportsByModule_.clear();
        structDefs_.clear();
        for (const auto& path : importOrder_) {
            const Program& pr = programs_.at(path.string());
            std::string mod = pr.moduleName.empty() ? "main" : pr.moduleName;
            (void)mod;
            for (const auto& fn : pr.functions) {
                std::string key = (pr.moduleName.empty() ? "main" : pr.moduleName) + "::" + fn.name;
                overloadsByModName_[key].push_back(fn);
                if (fn.exported) {
                    exportsByModule_[pr.moduleName.empty() ? "main" : pr.moduleName].insert(fn.name);
                }
            }
            for (const auto& s : pr.structs) {
                structDefs_[s.name] = s;
            }
        }
    }

std::string Compiler::mangleFunctionLabel(const std::string& modulePrefix, const std::string& fname, size_t arity) {
        return modulePrefix + "__" + fname + "__" + std::to_string(arity);
    }

const FunctionDef* Compiler::resolveOverload(const std::string& mod, const std::string& name, const std::vector<std::string>& given,
                                       std::vector<std::string>& paddedOut) {
        std::vector<std::string> gv;
        for (const auto& g : given) gv.push_back(trim(g));
        std::string key = mod + "::" + name;
        auto it = overloadsByModName_.find(key);
        if (it == overloadsByModName_.end()) return nullptr;
        for (const auto& fn : it->second) {
            const auto& pd = fn.paramDefs;
            if (gv.size() > pd.size()) continue;
            bool ok = true;
            paddedOut.clear();
            for (size_t i = 0; i < gv.size(); ++i) paddedOut.push_back(gv[i]);
            for (size_t i = gv.size(); i < pd.size(); ++i) {
                if (!pd[i].hasDefault) {
                    ok = false;
                    break;
                }
                paddedOut.push_back(pd[i].defaultValue);
            }
            if (ok) return &fn;
        }
        return nullptr;
    }

void Compiler::checkCrossModuleExport(const std::string& callerMod, const std::string& calleeMod, const std::string& fnName) {
        if (calleeMod == callerMod) return;
        auto it = exportsByModule_.find(calleeMod);
        if (it == exportsByModule_.end() || !it->second.count(fnName)) {
            throw std::runtime_error("Function '" + calleeMod + "::" + fnName + "' is not exported or unknown.");
        }
    }
