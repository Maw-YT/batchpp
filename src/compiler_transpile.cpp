#include "compiler_internal.hpp"
#include <functional>

std::string Compiler::replaceArrayAccess(std::string s) {
    std::regex arrNestedAccess(R"(%([A-Za-z_]\w*)\[(\d+)\]\[(\d+)\]%)");
    while (std::regex_search(s, arrNestedAccess)) {
        s = std::regex_replace(s, arrNestedAccess, "%__arr_$1_$2_$3%");
    }
    std::regex arrAccess(R"(%([A-Za-z_]\w*)\[(\d+)\]%)");
    while (std::regex_search(s, arrAccess)) s = std::regex_replace(s, arrAccess, "%__arr_$1_$2%");
    return s;
}

std::string Compiler::replaceThisAccess(std::string s) {
    std::regex thisAccess(R"(%this\.([A-Za-z_]\w*)%)");
    while (std::regex_search(s, thisAccess)) s = std::regex_replace(s, thisAccess, "!__obj_%__this%_$1!");
    return s;
}

std::string Compiler::replaceInterpolationSyntax(std::string s) {
    std::regex thisInterp(R"(\$\{this\.([A-Za-z_]\w*)\})");
    while (std::regex_search(s, thisInterp)) s = std::regex_replace(s, thisInterp, "!__obj_%__this%_$1!");

    std::regex arrNestedInterp(R"(\$\{([A-Za-z_]\w*)\[(\d+)\]\[(\d+)\]\})");
    while (std::regex_search(s, arrNestedInterp)) s = std::regex_replace(s, arrNestedInterp, "%__arr_$1_$2_$3%");

    std::regex arrInterp(R"(\$\{([A-Za-z_]\w*)\[(\d+)\]\})");
    while (std::regex_search(s, arrInterp)) s = std::regex_replace(s, arrInterp, "%__arr_$1_$2%");

    std::regex lenInterp(R"(\$\{len\(([A-Za-z_]\w*)\)\})");
    while (std::regex_search(s, lenInterp)) {
        s = std::regex_replace(s, lenInterp, "%__arr_$1_len%", std::regex_constants::format_first_only);
    }

    std::regex varInterp(R"(\$\{([A-Za-z_]\w*)\})");
    while (std::regex_search(s, varInterp)) s = std::regex_replace(s, varInterp, "%$1%");
    return s;
}

std::string Compiler::replaceLenExpr(std::string s) {
    std::regex lenRe(R"(len\s*\(\s*([A-Za-z_]\w*)\s*\))");
    while (std::regex_search(s, lenRe)) {
        s = std::regex_replace(s, lenRe, "%__arr_$1_len%", std::regex_constants::format_first_only);
    }
    return s;
}

std::string Compiler::normalizeExpr(std::string s) {
    s = replaceInterpolationSyntax(s);
    s = replaceArrayAccess(s);
    s = replaceThisAccess(s);
    s = replaceLenExpr(s);
    return s;
}

std::vector<std::string> Compiler::transpileLine(const std::string& rawLine, const std::string& moduleName,
                                                 const std::vector<LoopLabels>* loopStack) {
    std::string line = trim(rawLine);
    if (line.empty() || startsWith(line, "//")) return {};
    std::smatch m;
    const std::string callerMod = moduleName.empty() ? "main" : moduleName;
    auto tryParseCallExpr = [&](const std::string& raw, std::string& calleeModOut, std::string& fnameOut,
                                std::vector<std::string>& argsOut) -> bool {
        std::string s = trim(raw);
        if (s.empty()) return false;
        size_t openPos = std::string::npos;
        int quote = 0;
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '"' && (i == 0 || s[i - 1] != '\\')) {
                quote = 1 - quote;
                continue;
            }
            if (quote == 0 && c == '(') {
                openPos = i;
                break;
            }
        }
        if (openPos == std::string::npos || openPos == 0) return false;
        if (trim(s.substr(s.size() - 1)) != ")") return false;

        size_t closePos = s.size() - 1;
        int depth = 0;
        quote = 0;
        bool foundMatch = false;
        for (size_t i = openPos; i < s.size(); ++i) {
            char c = s[i];
            if (c == '"' && (i == 0 || s[i - 1] != '\\')) {
                quote = 1 - quote;
                continue;
            }
            if (quote != 0) continue;
            if (c == '(') depth++;
            else if (c == ')') {
                depth--;
                if (depth == 0) {
                    closePos = i;
                    foundMatch = true;
                    break;
                }
            }
        }
        if (!foundMatch || closePos != s.size() - 1) return false;

        std::string head = trim(s.substr(0, openPos));
        std::string argsRaw = s.substr(openPos + 1, closePos - openPos - 1);
        if (head.empty()) return false;
        size_t dc = head.find("::");
        if (dc != std::string::npos) {
            calleeModOut = trim(head.substr(0, dc));
            fnameOut = trim(head.substr(dc + 2));
        } else {
            calleeModOut = callerMod;
            fnameOut = head;
        }
        if (!isValidIdentifier(fnameOut)) return false;
        if (!calleeModOut.empty() && !isValidIdentifier(calleeModOut)) return false;
        argsOut = splitArgs(argsRaw);
        return true;
    };

    if (line == "break" || line == "break;") {
        if (!loopStack || loopStack->empty()) throw std::runtime_error("'break' outside of loop.");
        return {"goto :" + loopStack->back().breakLabel};
    }
    if (line == "continue" || line == "continue;") {
        if (!loopStack || loopStack->empty()) throw std::runtime_error("'continue' outside of loop.");
        return {"goto :" + loopStack->back().continueLabel};
    }

    std::regex assertRe(R"(^assert\s+(.+?)(?:\s*,\s*(.+))?\s*$)");
    if (std::regex_match(line, m, assertRe)) {
        std::string cond = normalizeExpr(trim(m[1].str()));
        std::string msg = m[2].matched ? unwrapQuotedLiteral(trim(m[2].str())) : "Assertion failed";
        return {"if not " + cond + " (", "  echo " + msg, "  exit /b 1", ")"};
    }

    std::regex mapDecl(R"(^map\s+([A-Za-z_]\w*)\s*=\s*\{(.*)\}\s*$)");
    if (std::regex_match(line, m, mapDecl)) {
        std::string mapName = m[1].str();
        std::string inner = m[2].str();
        std::vector<std::string> out;
        std::vector<std::string> keys;
        std::string cur;
        int quote = 0;
        int d = 0;
        for (size_t i = 0; i < inner.size(); ++i) {
            char c = inner[i];
            if (c == '"' && (cur.empty() || cur.back() != '\\')) quote = 1 - quote;
            if (quote == 0) {
                if (c == '{') d++;
                if (c == '}') d--;
            }
            if (c == ',' && quote == 0 && d == 0) {
                std::string pair = trim(cur);
                if (!pair.empty()) {
                    size_t col = pair.find(':');
                    if (col != std::string::npos) {
                        std::string k = unwrapQuotedLiteral(trim(pair.substr(0, col)));
                        std::string v = unwrapQuotedLiteral(normalizeExpr(trim(pair.substr(col + 1))));
                        std::string kn = replaceAll(k, " ", "_");
                        out.push_back("set \"__map_" + mapName + "_" + kn + "=" + v + "\"");
                        keys.push_back(kn);
                    }
                }
                cur.clear();
                continue;
            }
            cur.push_back(c);
        }
        std::string pair = trim(cur);
        if (!pair.empty()) {
            size_t col = pair.find(':');
            if (col != std::string::npos) {
                std::string k = unwrapQuotedLiteral(trim(pair.substr(0, col)));
                std::string v = unwrapQuotedLiteral(normalizeExpr(trim(pair.substr(col + 1))));
                std::string kn = replaceAll(k, " ", "_");
                out.push_back("set \"__map_" + mapName + "_" + kn + "=" + v + "\"");
                keys.push_back(kn);
            }
        }
        std::string keyList;
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i) keyList += " ";
            keyList += keys[i];
        }
        out.insert(out.begin(), "set \"__map_" + mapName + "_keys=" + keyList + "\"");
        return out;
    }

    std::regex stDecl(R"(^st\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\(\)\s*$)");
    if (std::regex_match(line, m, stDecl)) {
        std::string inst = m[1].str();
        std::string stName = m[2].str();
        auto it = structDefs_.find(stName);
        if (it == structDefs_.end()) throw std::runtime_error("Unknown struct '" + stName + "'.");
        std::vector<std::string> out;
        for (const auto& fld : it->second.fields) {
            std::string rhs = unwrapQuotedLiteral(normalizeExpr(fld.second));
            out.push_back("set \"__st_" + inst + "_" + fld.first + "=" + rhs + "\"");
        }
        return out;
    }

    std::regex sliceRe(R"(^slice\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*,\s*(\d+)\s*,\s*(\d+)\s*$)");
    if (std::regex_match(line, m, sliceRe)) {
        std::string dst = m[1].str();
        std::string src = m[2].str();
        int start = std::stoi(m[3].str());
        int count = std::stoi(m[4].str());
        std::vector<std::string> out;
        out.push_back("set /a __sl_i=0");
        out.push_back("set \"__arr_" + dst + "_len=" + std::to_string(count) + "\"");
        for (int i = 0; i < count; ++i) out.push_back("set \"__arr_" + dst + "_" + std::to_string(i) + "=!__arr_" + src + "_" + std::to_string(start + i) + "!\"");
        return out;
    }

    std::regex arrAssign(R"(^([A-Za-z_]\w*)\[(\d+)\]\s*=\s*(.+)\s*$)");
    std::regex arrNestedAssign(R"(^([A-Za-z_]\w*)\[(\d+)\]\[(\d+)\]\s*=\s*(.+)\s*$)");
    if (std::regex_match(line, m, arrNestedAssign)) {
        return {"set \"__arr_" + m[1].str() + "_" + m[2].str() + "_" + m[3].str() + "=" +
                unwrapQuotedLiteral(normalizeExpr(trim(m[4].str()))) + "\""};
    }

    std::regex arrNestedDynSecondAssign(R"(^([A-Za-z_]\w*)\[(\d+)\]\[([A-Za-z_]\w*)\]\s*=\s*(.+)\s*$)");
    if (std::regex_match(line, m, arrNestedDynSecondAssign)) {
        return {"call :__lib_arr_set2 " + m[1].str() + " " + m[2].str() + " %" + m[3].str() + "% " +
                unwrapQuotedLiteral(normalizeExpr(trim(m[4].str())))};
    }

    if (std::regex_match(line, m, arrAssign)) {
        return {"set \"__arr_" + m[1].str() + "_" + m[2].str() + "=" + unwrapQuotedLiteral(normalizeExpr(trim(m[3].str()))) + "\""};
    }

    std::regex arrDynAssign(R"(^([A-Za-z_]\w*)\[([A-Za-z_]\w*)\]\s*=\s*(.+)\s*$)");
    if (std::regex_match(line, m, arrDynAssign)) {
        return {"call :__lib_arr_set " + m[1].str() + " %" + m[2].str() + "% " + unwrapQuotedLiteral(normalizeExpr(trim(m[3].str())))};
    }

    std::regex arrDecl(R"(^arr\s+([A-Za-z_]\w*)\s*=\s*\[(.*)\]\s*$)");
    if (std::regex_match(line, m, arrDecl)) {
        std::string name = m[1].str();
        std::function<std::vector<std::string>(const std::string&, const std::string&)> emitArrayLiteral;
        emitArrayLiteral = [&](const std::string& arrName, const std::string& innerRaw) -> std::vector<std::string> {
            auto items = splitArgs(innerRaw);
            std::vector<std::string> out;
            out.push_back("set \"__arr_" + arrName + "_len=" + std::to_string(items.size()) + "\"");
            for (size_t i = 0; i < items.size(); ++i) {
                std::string item = trim(items[i]);
                if (item.size() >= 2 && item.front() == '[' && item.back() == ']') {
                    std::string childName = arrName + "_" + std::to_string(i);
                    auto child = emitArrayLiteral(childName, item.substr(1, item.size() - 2));
                    out.insert(out.end(), child.begin(), child.end());
                    out.push_back("set \"__arr_" + arrName + "_" + std::to_string(i) + "=" + childName + "\"");
                } else {
                    out.push_back("set \"__arr_" + arrName + "_" + std::to_string(i) + "=" +
                                  unwrapQuotedLiteral(normalizeExpr(item)) + "\"");
                }
            }
            return out;
        };
        return emitArrayLiteral(name, m[2].str());
    }

    std::regex varDecl(R"(^((?:let|var|const))\s+([A-Za-z_]\w*)(?:\s*=\s*(.+))?\s*$)");
    if (std::regex_match(line, m, varDecl)) {
        std::string name = m[2].str();
        std::string rhs = m[3].matched ? normalizeExpr(trim(m[3].str())) : "\"\"";
        if (isArithmeticExpr(rhs)) return {"set /a " + name + "=" + rhs};
        return {"set \"" + name + "=" + unwrapQuotedLiteral(rhs) + "\""};
    }

    std::regex setOpRe(R"(^set\s+([A-Za-z_]\w*)\s*(\+=|-=|\*=|/=)\s*(.+)\s*$)");
    if (std::regex_match(line, m, setOpRe)) {
        std::string name = m[1].str();
        std::string assignmentOp = m[2].str();
        if (!isValidIdentifier(name) || !isSupportedAssignmentOperator(assignmentOp)) {
            throw std::runtime_error("Invalid assignment expression.");
        }
        std::string arithmeticOp = assignmentOp.substr(0, 1);
        std::string rhs = normalizeExpr(trim(m[3].str()));
        return {"set /a " + name + "=" + name + arithmeticOp + "(" + rhs + ")"};
    }

    std::regex varOpRe(R"(^([A-Za-z_]\w*)\s*(\+=|-=|\*=|/=)\s*(.+)\s*$)");
    if (std::regex_match(line, m, varOpRe)) {
        std::string name = m[1].str();
        std::string assignmentOp = m[2].str();
        if (!isValidIdentifier(name) || !isSupportedAssignmentOperator(assignmentOp)) {
            throw std::runtime_error("Invalid assignment expression.");
        }
        std::string arithmeticOp = assignmentOp.substr(0, 1);
        std::string rhs = normalizeExpr(trim(m[3].str()));
        return {"set /a " + name + "=" + name + arithmeticOp + "(" + rhs + ")"};
    }

    std::regex setIncDecRe(R"(^set\s+([A-Za-z_]\w*)(\+\+|--)\s*$)");
    if (std::regex_match(line, m, setIncDecRe)) return {"set /a " + m[1].str() + "=" + m[1].str() + ((m[2].str() == "++") ? "+1" : "-1")};

    std::regex varIncDecRe(R"(^([A-Za-z_]\w*)(\+\+|--)\s*$)");
    if (std::regex_match(line, m, varIncDecRe)) return {"set /a " + m[1].str() + "=" + m[1].str() + ((m[2].str() == "++") ? "+1" : "-1")};

    std::regex pushRe(R"(^([A-Za-z_]\w*)\.push\((.*)\)\s*$)");
    if (std::regex_match(line, m, pushRe)) {
        std::string name = m[1].str();
        std::string value = normalizeExpr(trim(m[2].str()));
        return {
            "set /a __arr_" + name + "_idx=%__arr_" + name + "_len%",
            "set \"__arr_" + name + "_%__arr_" + name + "_idx%=" + value + "\"",
            "set /a __arr_" + name + "_len=%__arr_" + name + "_len%+1"
        };
    }

    std::regex inputSafePromptRe(R"(^input\?\s+([A-Za-z_]\w*)(?:\s+(.+))?\s*$)");
    if (std::regex_match(line, m, inputSafePromptRe)) {
        std::string name = m[1].str();
        std::string prompt = m[2].matched ? trim(m[2].str()) : "";
        prompt = replaceAll(normalizeExpr(prompt), "\"", "");
        if (!prompt.empty()) return {"set \"__input_ok=1\"", "set /p \"" + name + "=" + prompt + "\"", "if errorlevel 1 set \"__input_ok=0\""};
        return {"set \"__input_ok=1\"", "set /p \"" + name + "=\"", "if errorlevel 1 set \"__input_ok=0\""};
    }

    std::regex inputPromptRe(R"(^input\s+([A-Za-z_]\w*)(?:\s+(.+))?\s*$)");
    if (std::regex_match(line, m, inputPromptRe)) {
        std::string name = m[1].str();
        std::string prompt = m[2].matched ? trim(m[2].str()) : "";
        prompt = replaceAll(normalizeExpr(prompt), "\"", "");
        if (!prompt.empty()) return {"set /p \"" + name + "=" + prompt + "\""};
        return {"set /p \"" + name + "=\""};
    }

    std::regex tokenRe(R"(^token\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*,\s*(\d+)\s*$)");
    if (std::regex_match(line, m, tokenRe)) {
        return {
            "set \"" + m[1].str() + "=\"",
            "set /a __tok_i=0",
            "for %%T in (%" + m[2].str() + "%) do (",
            "  set /a __tok_i+=1",
            "  if !__tok_i!==" + m[3].str() + " set \"" + m[1].str() + "=%%T\"",
            ")"
        };
    }

    std::regex afterRe(R"(^after\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*,\s*\"([^\"]*)\"\s*$)");
    if (std::regex_match(line, m, afterRe)) {
        return {
            "set \"" + m[1].str() + "=%" + m[2].str() + ":*" + m[3].str() + "=%\"",
            "if /i \"%" + m[1].str() + "%\"==\"%" + m[2].str() + "%\" set \"" + m[1].str() + "=\""
        };
    }

    std::regex startsWithRe(R"(^startswith\s+([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*)\s*,\s*\"([^\"]*)\"\s*$)");
    if (std::regex_match(line, m, startsWithRe)) {
        std::string outVar = m[1].str();
        std::string srcVar = m[2].str();
        std::string prefix = m[3].str();
        return {"set \"" + outVar + "=0\"",
                "if /i \"!" + srcVar + ":~0," + std::to_string(prefix.size()) + "!\"==\"" + prefix + "\" set \"" + outVar + "=1\""};
    }

    std::regex objNew(R"(^obj\s+([A-Za-z_]\w*)\s*=\s*new\s+([A-Za-z_]\w*)\((.*)\)\s*$)");
    if (std::regex_match(line, m, objNew)) {
        std::string obj = m[1].str();
        std::string cls = m[2].str();
        objectClass_[obj] = cls;
        auto args = splitArgs(m[3].str());
        std::ostringstream call;
        call << "call :__class_" << cls << "_init " << obj;
        for (const auto& a : args) call << " " << a;
        return {"set \"__obj_" + obj + "_class=" + cls + "\"", call.str()};
    }

    std::regex methodCall(R"(^([A-Za-z_]\w*)\.([A-Za-z_]\w*)\((.*)\)\s*$)");
    if (std::regex_match(line, m, methodCall)) {
        std::string obj = m[1].str();
        auto it = objectClass_.find(obj);
        if (it == objectClass_.end()) throw std::runtime_error("Unknown object '" + obj + "' in method call.");
        auto args = splitArgs(m[3].str());
        std::ostringstream call;
        call << "call :__class_" << it->second << "_" << m[2].str() << " " << obj;
        for (const auto& a : args) call << " " << a;
        return {call.str()};
    }

    std::regex setReWithRhs(R"(^set\s+([A-Za-z_]\w*)\s*=\s*(.+)\s*$)");
    if (std::regex_match(line, m, setReWithRhs)) {
        std::string var = m[1].str();
        std::string rhsRaw = trim(m[2].str());
        std::string calleeMod;
        std::string fname;
        std::vector<std::string> args;
        if (tryParseCallExpr(rhsRaw, calleeMod, fname, args)) {
            return emitCallLines(callerMod, calleeMod, fname, args, &var, loopStack);
        }
    }

    {
        std::string calleeMod;
        std::string fname;
        std::vector<std::string> args;
        if (tryParseCallExpr(line, calleeMod, fname, args)) {
            return emitCallLines(callerMod, calleeMod, fname, args, nullptr, loopStack);
        }
    }

    std::regex setRe(R"(^set\s+([A-Za-z_]\w*)\s*=\s*(.+)\s*$)");
    if (std::regex_match(line, m, setRe)) {
        std::string rhs = normalizeExpr(m[2].str());
        if (isArithmeticExpr(rhs)) return {"set /a " + m[1].str() + "=" + rhs};
        return {"set \"" + m[1].str() + "=" + unwrapQuotedLiteral(rhs) + "\""};
    }

    std::regex thisSet(R"(^this\.([A-Za-z_]\w*)\s*=\s*(.+)\s*$)");
    if (std::regex_match(line, m, thisSet)) {
        return {"set \"__obj_%__this%_" + m[1].str() + "=" + unwrapQuotedLiteral(normalizeExpr(m[2].str())) + "\""};
    }

    std::regex returnRe(R"(^return\s+(.+)\s*$)");
    if (std::regex_match(line, m, returnRe)) {
        std::string rhs = normalizeExpr(m[1].str());
        if (isArithmeticExpr(rhs)) return {"set /a __ret=" + rhs, "exit /b 0"};
        return {"set \"__ret=" + unwrapQuotedLiteral(trim(m[1].str())) + "\"", "exit /b 0"};
    }

    std::regex throwRe(R"(^throw(?:\s+(.+))?\s*$)");
    if (std::regex_match(line, m, throwRe)) {
        std::string msg = m[1].matched ? normalizeExpr(trim(m[1].str())) : "\"error\"";
        msg = unwrapQuotedLiteral(msg);
        return {"set \"__throw_msg=" + msg + "\"", "exit /b 1"};
    }

    return {normalizeExpr(line)};
}

std::vector<std::string> Compiler::expandMacroBody(const FunctionDef& fn, const std::vector<std::string>& args,
                                                   const std::string& moduleName, const std::vector<LoopLabels>* loopStack) {
    if (args.size() != fn.paramDefs.size()) throw std::runtime_error("Macro argument count mismatch");
    std::vector<std::string> out;
    for (std::string bl : fn.body) {
        for (size_t i = 0; i < fn.paramDefs.size(); ++i) bl = replaceAll(bl, fn.paramDefs[i].name, args[i]);
        auto lines = transpileLine(bl, moduleName, loopStack);
        out.insert(out.end(), lines.begin(), lines.end());
    }
    return out;
}

std::vector<std::string> Compiler::emitCallLines(const std::string& callerMod, const std::string& calleeMod, const std::string& fname,
                                                 const std::vector<std::string>& argsGiven, const std::string* retVar,
                                                 const std::vector<LoopLabels>* loopStack) {
    std::vector<std::string> padded;
    const FunctionDef* fd = resolveOverload(calleeMod, fname, argsGiven, padded);
    if (!fd) throw std::runtime_error("No overload for '" + calleeMod + "::" + fname + "'.");
    checkCrossModuleExport(callerMod, calleeMod, fname);
    if (fd->isMacro || fd->isInline) {
        auto inner = expandMacroBody(*fd, padded, callerMod, loopStack);
        if (retVar) inner.push_back("set \"" + *retVar + "=!__ret!\"");
        return inner;
    }
    std::ostringstream oss;
    oss << "call :__fn_" << mangleFunctionLabel(calleeMod, fname, fd->paramDefs.size());
    for (const auto& a : padded) oss << " " << normalizeExpr(a);
    std::vector<std::string> out = {oss.str()};
    if (retVar) out.push_back("set \"" + *retVar + "=!__ret!\"");
    return out;
}

static bool canFlattenBatchIfBranch(const std::vector<std::string>& lines)
{
    bool any = false;
    for (const auto& raw : lines) {
        std::string l = trim(raw);
        if (l.empty())
            continue;
        any = true;
        if (startsWith(l, "call :"))
            continue;
        if (startsWith(l, "goto :"))
            continue;
        if (startsWith(l, "echo "))
            continue;
        if (startsWith(l, "set \""))
            continue;
        return false;
    }
    return any;
}

static std::string negateBatchCondition(std::string cond)
{
    cond = trim(cond);
    if (startsWith(cond, "/i "))
        return "/i not " + cond.substr(3);
    return "not " + cond;
}

std::vector<std::string> Compiler::transpileLines(const std::vector<std::string>& rawLines, const std::string& moduleName,
                                                  const std::vector<LoopLabels>* loopStack, const std::string& sourceFile) {
    std::vector<std::string> out;
    std::regex tryStartRe(R"(^\s*try\s*\{\s*$)");
    std::regex whileStartRe(R"(^\s*while\s+(.+)\s*\{\s*$)");
    std::regex ifStartRe(R"(^\s*if\s+(.+)\s*\{\s*$)");
    std::regex elseIfLineRe(R"(^\s*else\s+if\s+(.+)\s*\{\s*$)");
    std::regex elseLineRe(R"(^\s*else\s*\{\s*$)");
    std::regex blockStartRe(R"(^\s*\{\s*$)");
    std::regex forRangeRe(R"(^\s*for\s+([A-Za-z_]\w*)\s*=\s*(.+)\s+to\s+(.+)\s*\{\s*$)");
    std::regex forArrRe(R"(^\s*for\s+([A-Za-z_]\w*)\s+in\s+arr\s+([A-Za-z_]\w*)\s*\{\s*$)");
    std::regex forInRe(R"(^\s*for\s+([A-Za-z_]\w*)\s+in\s+(.+)\s*\{\s*$)");
    std::regex matchStartRe(R"(^\s*match\s+(.+)\s*\{\s*$)");
    std::regex catchStartRe(R"(^\s*catch\s+([A-Za-z_]\w*)\s*\{\s*$)");
    std::regex inlineCatchRe(R"(^\s*\}\s*catch\s+([A-Za-z_]\w*)\s*\{\s*$)");
    std::regex finallyStartRe(R"(^\s*finally\s*\{\s*$)");
    std::regex inlineFinallyRe(R"(^\s*\}\s*finally\s*\{\s*$)");
    std::smatch m;

    auto pushLoop = [&](const LoopLabels& L) {
        std::vector<LoopLabels> v;
        if (loopStack) v = *loopStack;
        v.push_back(L);
        return v;
    };

    auto parseBody = [&](size_t& i) {
        int depth = 1;
        std::vector<std::string> body;
        while (i < rawLines.size()) {
            int d = netBraceDelta(rawLines[i]);
            if (depth + d == 0 && trim(rawLines[i]) == "}") {
                ++i;
                break;
            }
            body.push_back(rawLines[i]);
            depth += d;
            ++i;
        }
        return body;
    };

    size_t i = 0;
    while (i < rawLines.size()) {
        std::string line = trim(rawLines[i]);
        try {

        if (std::regex_match(line, blockStartRe)) {
            ++i;
            auto body = parseBody(i);
            auto scopedLines = transpileLines(body, moduleName, loopStack, sourceFile);
            out.push_back("setlocal EnableExtensions EnableDelayedExpansion");
            out.insert(out.end(), scopedLines.begin(), scopedLines.end());
            out.push_back("endlocal");
            continue;
        }

        if (std::regex_match(line, m, ifStartRe)) {
            std::vector<std::pair<std::string, std::vector<std::string>>> branches;
            branches.push_back({normalizeExpr(trim(m[1].str())), {}});
            ++i;
            branches.back().second = parseBody(i);
            while (i < rawLines.size()) {
                while (i < rawLines.size() && trim(rawLines[i]).empty()) ++i;
                if (i >= rawLines.size()) break;
                std::string nx = trim(rawLines[i]);
                if (std::regex_match(nx, m, elseIfLineRe)) {
                    branches.push_back({normalizeExpr(trim(m[1].str())), {}});
                    ++i;
                    branches.back().second = parseBody(i);
                    continue;
                }
                if (std::regex_match(nx, elseLineRe)) {
                    branches.push_back({"", {}});
                    ++i;
                    branches.back().second = parseBody(i);
                }
                break;
            }
            std::vector<std::vector<std::string>> bodies;
            bodies.reserve(branches.size());
            for (const auto& br : branches)
                bodies.push_back(transpileLines(br.second, moduleName, loopStack, sourceFile));

            static int flatIfSkipSeq = 0;
            if (loopStack && branches.size() == 1 && !branches[0].first.empty() &&
                canFlattenBatchIfBranch(bodies[0])) {
                std::string skipLab = "__if_sk_" + std::to_string(++flatIfSkipSeq);
                out.push_back("if " + negateBatchCondition(branches[0].first) + " goto :" + skipLab);
                for (const auto& l : bodies[0]) {
                    std::string tl = trim(l);
                    if (!tl.empty())
                        out.push_back(tl);
                }
                out.push_back(":" + skipLab);
                continue;
            }

            for (size_t bi = 0; bi < branches.size(); ++bi) {
                const auto& body = bodies[bi];
                if (bi == 0)
                    out.push_back("if " + branches[bi].first + " (");
                else if (!branches[bi].first.empty())
                    out.push_back(") else if " + branches[bi].first + " (");
                else
                    out.push_back(") else (");
                for (const auto& l : body)
                    out.push_back("  " + l);
            }
            out.push_back(")");
            continue;
        }

        if (std::regex_match(line, m, forRangeRe)) {
            std::string iv = m[1].str();
            std::string from = normalizeExpr(trim(m[2].str()));
            std::string to = normalizeExpr(trim(m[3].str()));
            ++i;
            auto body = parseBody(i);
            int id = ++forCounter_;
            std::string ent = "__for_ent_" + std::to_string(id);
            std::string cont = "__for_cont_" + std::to_string(id);
            std::string brk = "__for_brk_" + std::to_string(id);
            auto loopCtx = pushLoop({brk, cont});
            auto bodyLines = transpileLines(body, moduleName, &loopCtx, sourceFile);
            std::vector<std::string> block = {":" + ent, "set /a " + iv + "=" + from, "set /a __for_to=" + to, ":" + cont,
                                              "if !" + iv + "! gtr !__for_to! goto :" + brk};
            block.insert(block.end(), bodyLines.begin(), bodyLines.end());
            block.push_back("set /a " + iv + "+=1");
            block.push_back("goto :" + cont);
            block.push_back(":" + brk);
            block.push_back("exit /b 0");
            helperBlocks_.push_back(std::move(block));
            out.push_back("call :" + ent);
            continue;
        }

        if (std::regex_match(line, m, forArrRe)) {
            std::string iv = m[1].str(), arr = m[2].str();
            ++i;
            auto body = parseBody(i);
            int id = ++forCounter_;
            std::string ent = "__forarr_ent_" + std::to_string(id);
            std::string cont = "__forarr_cont_" + std::to_string(id);
            std::string brk = "__forarr_brk_" + std::to_string(id);
            std::string idx = "__forarr_i_" + std::to_string(id);
            auto loopCtx = pushLoop({brk, cont});
            auto bodyLines = transpileLines(body, moduleName, &loopCtx, sourceFile);
            std::vector<std::string> block = {":" + ent, "set /a " + idx + "=0", ":" + cont,
                                              "if !" + idx + "! geq !__arr_" + arr + "_len! goto :" + brk,
                                              "call set \"" + iv + "=%%__arr_" + arr + "_!" + idx + "!%%\""};
            block.insert(block.end(), bodyLines.begin(), bodyLines.end());
            block.push_back("set /a " + idx + "+=1");
            block.push_back("goto :" + cont);
            block.push_back(":" + brk);
            block.push_back("exit /b 0");
            helperBlocks_.push_back(std::move(block));
            out.push_back("call :" + ent);
            continue;
        }

        if (std::regex_match(line, m, forInRe)) {
            std::string iv = m[1].str();
            std::string listExpr = normalizeExpr(trim(m[2].str()));
            if (!listExpr.empty() && listExpr.find('%') == std::string::npos && listExpr.find('!') == std::string::npos) {
                if (std::isalpha(static_cast<unsigned char>(listExpr[0])) || listExpr[0] == '_') listExpr = "%" + listExpr + "%";
            }
            ++i;
            auto body = parseBody(i);
            int id = ++forCounter_;
            std::string blk = "__forin_blk_" + std::to_string(id);
            std::string brk = "__forin_brk_" + std::to_string(id);
            std::string cont = "__forin_cont_" + std::to_string(id);
            auto loopCtx = pushLoop({brk, cont});
            auto bodyLines = transpileLines(body, moduleName, &loopCtx, sourceFile);
            std::vector<std::string> block = {":" + blk, "for %%F in (" + listExpr + ") do (", "  set \"" + iv + "=%%F\""};
            for (const auto& l : bodyLines) block.push_back("  " + l);
            block.push_back(")");
            block.push_back("exit /b 0");
            helperBlocks_.push_back(std::move(block));
            out.push_back("call :" + blk);
            continue;
        }

        if (std::regex_match(line, m, matchStartRe)) {
            std::string disc = trim(m[1].str());
            ++i;
            auto inner = parseBody(i);
            int mid = ++matchCounter_;
            std::string done = "__match_done_" + std::to_string(mid);
            std::regex caseLineRe(R"(^\s*case\s+(.+?)\s*:\s*\{\s*$)");
            std::regex defaultLineRe(R"(^\s*default\s*:\s*\{\s*$)");
            size_t j = 0;
            while (j < inner.size()) {
                std::string L = trim(inner[j]);
                if (std::regex_match(L, m, caseLineRe)) {
                    std::string lit = unwrapQuotedLiteral(trim(m[1].str()));
                    ++j;
                    size_t k = j;
                    auto cb = [&]() {
                        int d = 1;
                        std::vector<std::string> b;
                        while (k < inner.size()) {
                            int dd = netBraceDelta(inner[k]);
                            if (d + dd == 0 && trim(inner[k]) == "}") {
                                ++k;
                                break;
                            }
                            b.push_back(inner[k]);
                            d += dd;
                            ++k;
                        }
                        return b;
                    }();
                    auto lines = transpileLines(cb, moduleName, loopStack, sourceFile);
                    out.push_back("if /i " + disc + "==\"" + lit + "\" (");
                    for (const auto& l : lines) out.push_back("  " + l);
                    out.push_back("  goto :" + done);
                    out.push_back(")");
                    j = k;
                    continue;
                }
                if (std::regex_match(L, m, defaultLineRe)) {
                    ++j;
                    size_t k = j;
                    int d = 1;
                    std::vector<std::string> cb;
                    while (k < inner.size()) {
                        int dd = netBraceDelta(inner[k]);
                        if (d + dd == 0 && trim(inner[k]) == "}") {
                            ++k;
                            break;
                        }
                        cb.push_back(inner[k]);
                        d += dd;
                        ++k;
                    }
                    auto lines = transpileLines(cb, moduleName, loopStack, sourceFile);
                    out.insert(out.end(), lines.begin(), lines.end());
                    break;
                }
                ++j;
            }
            out.push_back(":" + done);
            continue;
        }

        if (std::regex_match(line, m, whileStartRe)) {
            std::string cond = normalizeExpr(trim(m[1].str()));
            ++i;
            auto body = parseBody(i);
            int id = ++loopCounter_;
            std::string ent = "__while_ent_" + std::to_string(id);
            std::string start = "__while_start_" + std::to_string(id);
            std::string brk = "__while_brk_" + std::to_string(id);
            auto loopCtx = pushLoop({brk, start});
            auto bodyLines = transpileLines(body, moduleName, &loopCtx, sourceFile);
            std::vector<std::string> block = {":" + ent, ":" + start, "if not " + cond + " goto :" + brk};
            block.insert(block.end(), bodyLines.begin(), bodyLines.end());
            block.push_back("goto :" + start);
            block.push_back(":" + brk);
            block.push_back("exit /b 0");
            helperBlocks_.push_back(std::move(block));
            out.push_back("call :" + ent);
            continue;
        }

        if (!std::regex_match(line, tryStartRe)) {
            auto ls = transpileLine(rawLines[i], moduleName, loopStack);
            out.insert(out.end(), ls.begin(), ls.end());
            ++i;
            continue;
        }

        ++i;
        int tDepth = 1;
        std::vector<std::string> tryBody;
        std::string errName;
        bool sawInlineCatch = false;
        while (i < rawLines.size()) {
            std::string t = trim(rawLines[i]);
            if (tDepth == 1 && std::regex_match(t, m, inlineCatchRe)) {
                errName = m[1].str();
                sawInlineCatch = true;
                ++i;
                break;
            }
            int d = netBraceDelta(rawLines[i]);
            if (tDepth + d == 0 && t == "}") {
                ++i;
                break;
            }
            tryBody.push_back(rawLines[i]);
            tDepth += d;
            ++i;
        }

        while (i < rawLines.size() && trim(rawLines[i]).empty()) ++i;
        auto parseCatchBody = [&](size_t& idx) {
            std::vector<std::string> body;
            int depth = 1;
            while (idx < rawLines.size()) {
                std::string t = trim(rawLines[idx]);
                if (depth == 1 && std::regex_match(t, m, inlineFinallyRe)) {
                    break;
                }
                int d = netBraceDelta(rawLines[idx]);
                if (depth + d == 0 && t == "}") {
                    ++idx;
                    break;
                }
                body.push_back(rawLines[idx]);
                depth += d;
                ++idx;
            }
            return body;
        };

        std::vector<std::string> catchBody;
        if (sawInlineCatch) {
            catchBody = parseCatchBody(i);
        } else {
            std::string catchLine = (i < rawLines.size()) ? trim(rawLines[i]) : "";
            if (i < rawLines.size() && std::regex_match(catchLine, m, catchStartRe)) {
                errName = m[1].str();
                ++i;
                catchBody = parseCatchBody(i);
            } else {
                throw std::runtime_error("try block must be followed by catch <name> { ... }");
            }
        }

        std::vector<std::string> finallyBody;
        while (i < rawLines.size() && trim(rawLines[i]).empty()) ++i;
        if (i < rawLines.size()) {
            std::string finLine = trim(rawLines[i]);
            if (std::regex_match(finLine, finallyStartRe) || std::regex_match(finLine, inlineFinallyRe)) {
                ++i;
                finallyBody = parseBody(i);
            }
        }

        auto tryLines = transpileLines(tryBody, moduleName, loopStack, sourceFile);
        auto catchLines = transpileLines(catchBody, moduleName, loopStack, sourceFile);
        int tid = ++tryCounter_;
        std::string tryLabel = "__try_block_" + std::to_string(tid);
        std::string catchLabel = "__catch_block_" + std::to_string(tid);
        std::vector<std::string> tb = {":" + tryLabel};
        tb.insert(tb.end(), tryLines.begin(), tryLines.end());
        if (tryLines.empty() || trim(tryLines.back()) != "exit /b 0") tb.push_back("exit /b 0");
        helperBlocks_.push_back(std::move(tb));

        std::vector<std::string> cb = {":" + catchLabel};
        cb.insert(cb.end(), catchLines.begin(), catchLines.end());
        if (catchLines.empty() || trim(catchLines.back()) != "exit /b 0") cb.push_back("exit /b 0");
        helperBlocks_.push_back(std::move(cb));

        out.push_back("set \"__throw_msg=\"");
        out.push_back("call :" + tryLabel);
        out.push_back("set \"__try_ec=%errorlevel%\"");
        out.push_back("if not \"%__try_ec%\"==\"0\" (");
        out.push_back("  set \"" + errName + ".code=%__try_ec%\"");
        out.push_back("  set \"" + errName + ".message=%__throw_msg%\"");
        out.push_back("  if not defined " + errName + ".message set \"" + errName + ".message=Unhandled error\"");
        out.push_back("  call :" + catchLabel);
        out.push_back(")");

        if (!finallyBody.empty()) {
            auto finLines = transpileLines(finallyBody, moduleName, loopStack, sourceFile);
            std::string finLabel = "__finally_block_" + std::to_string(tid);
            std::vector<std::string> fb = {":" + finLabel};
            fb.insert(fb.end(), finLines.begin(), finLines.end());
            fb.push_back("exit /b 0");
            helperBlocks_.push_back(std::move(fb));
            out.push_back("call :" + finLabel);
        }
        } catch (const std::exception& e) {
            const size_t shownLine = (i < rawLines.size()) ? (i + 1) : rawLines.size();
            const std::string shownSource = (i < rawLines.size()) ? rawLines[i] : "";
            throw std::runtime_error(formatDiagnostic(e.what(), sourceFile, shownLine, shownSource));
        }
    }
    return out;
}
