#include "compiler_internal.hpp"

std::string ltrim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

std::string rtrim(const std::string& s) {
    if (s.empty()) return s;
    size_t i = s.size() - 1;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        if (i == 0) return "";
        --i;
    }
    return s.substr(0, i + 1);
}

std::string trim(const std::string& s) { return rtrim(ltrim(s)); }

bool startsWith(const std::string& s, const std::string& prefix) { return s.rfind(prefix, 0) == 0; }

bool isIdentifierStartChar(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return std::isalpha(u) || c == '_';
}

bool isIdentifierChar(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_';
}

bool isValidIdentifier(const std::string& s) {
    if (s.empty()) return false;
    if (!isIdentifierStartChar(s[0])) return false;
    for (size_t i = 1; i < s.size(); ++i) {
        if (!isIdentifierChar(s[i])) return false;
    }
    return true;
}

bool isSupportedAssignmentOperator(const std::string& op) {
    return op == "+=" || op == "-=" || op == "*=" || op == "/=";
}

std::vector<LexToken> tokenizeLine(const std::string& line) {
    std::vector<LexToken> tokens;
    size_t i = 0;
    while (i < line.size()) {
        unsigned char u = static_cast<unsigned char>(line[i]);
        if (std::isspace(u)) {
            ++i;
            continue;
        }
        if (line[i] == '"') {
            std::string s;
            s.push_back(line[i++]);
            while (i < line.size()) {
                s.push_back(line[i]);
                if (line[i] == '"' && (s.size() < 2 || s[s.size() - 2] != '\\')) {
                    ++i;
                    break;
                }
                ++i;
            }
            tokens.push_back({LexTokenKind::String, s});
            continue;
        }
        if (isIdentifierStartChar(line[i])) {
            size_t j = i + 1;
            while (j < line.size() && isIdentifierChar(line[j])) ++j;
            tokens.push_back({LexTokenKind::Identifier, line.substr(i, j - i)});
            i = j;
            continue;
        }
        if (std::isdigit(u)) {
            size_t j = i + 1;
            while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j]))) ++j;
            tokens.push_back({LexTokenKind::Number, line.substr(i, j - i)});
            i = j;
            continue;
        }
        std::string symbol(1, line[i]);
        tokens.push_back({LexTokenKind::Symbol, symbol});
        ++i;
    }
    return tokens;
}

bool tryParseKeywordHeader(const std::string& line, const std::string& keyword, bool requireOpenBrace, std::string& nameOut) {
    auto t = tokenizeLine(trim(line));
    if (t.size() < 2) return false;
    if (t[0].kind != LexTokenKind::Identifier || t[0].text != keyword) return false;
    if (t[1].kind != LexTokenKind::Identifier || !isValidIdentifier(t[1].text)) return false;
    if (requireOpenBrace) {
        if (t.size() != 3) return false;
        if (t[2].kind != LexTokenKind::Symbol || t[2].text != "{") return false;
    }
    nameOut = t[1].text;
    return true;
}

bool tryParseVarDeclLine(const std::string& line, std::string& nameOut, bool& hasInitializer, std::string& initializerOut) {
    std::string t = trim(line);
    auto tok = tokenizeLine(t);
    if (tok.size() < 2) return false;
    if (tok[0].kind != LexTokenKind::Identifier || tok[0].text != "var") return false;
    if (tok[1].kind != LexTokenKind::Identifier || !isValidIdentifier(tok[1].text)) return false;
    nameOut = tok[1].text;
    hasInitializer = false;
    initializerOut.clear();
    size_t eq = t.find('=');
    if (eq != std::string::npos) {
        hasInitializer = true;
        initializerOut = trim(t.substr(eq + 1));
    }
    return true;
}

bool isArithmeticExpr(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"') return false;
    for (char c : t) {
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')') return true;
    }
    return false;
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string unwrapQuotedLiteral(std::string s) {
    s = trim(s);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
    return s;
}

std::vector<std::string> splitArgs(const std::string& raw) {
    std::vector<std::string> out;
    std::string cur;
    int quote = 0;
    int parenDepth = 0;
    int bracketDepth = 0;
    int braceDepth = 0;
    for (char c : raw) {
        if (c == '"' && (cur.empty() || cur.back() != '\\')) {
            quote = 1 - quote;
            cur.push_back(c);
            continue;
        }
        if (quote == 0) {
            if (c == '(') parenDepth++;
            else if (c == ')' && parenDepth > 0) parenDepth--;
            else if (c == '[') bracketDepth++;
            else if (c == ']' && bracketDepth > 0) bracketDepth--;
            else if (c == '{') braceDepth++;
            else if (c == '}' && braceDepth > 0) braceDepth--;
        }
        if (c == ',' && quote == 0 && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
            out.push_back(trim(cur));
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    if (!trim(cur).empty()) out.push_back(trim(cur));
    return out;
}

std::vector<ParamDef> parseParamDecls(const std::string& raw) {
    std::vector<ParamDef> out;
    std::string cur;
    int quote = 0;
    int depth = 0;
    for (char c : raw) {
        if (c == '"' && (cur.empty() || cur.back() != '\\')) {
            quote = 1 - quote;
            cur.push_back(c);
            continue;
        }
        if (quote == 0) {
            if (c == '(') depth++;
            if (c == ')') depth--;
        }
        if (c == ',' && quote == 0 && depth == 0) {
            std::string t = trim(cur);
            if (!t.empty()) {
                size_t eq = t.find('=');
                std::string ident = trim(eq == std::string::npos ? t : t.substr(0, eq));
                if (isValidIdentifier(ident)) {
                    ParamDef p;
                    p.name = ident;
                    if (eq != std::string::npos) {
                        p.hasDefault = true;
                        p.defaultValue = trim(t.substr(eq + 1));
                    }
                    out.push_back(std::move(p));
                }
            }
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    std::string t = trim(cur);
    if (!t.empty()) {
        size_t eq = t.find('=');
        std::string ident = trim(eq == std::string::npos ? t : t.substr(0, eq));
        if (isValidIdentifier(ident)) {
            ParamDef p;
            p.name = ident;
            if (eq != std::string::npos) {
                p.hasDefault = true;
                p.defaultValue = trim(t.substr(eq + 1));
            }
            out.push_back(std::move(p));
        }
    }
    return out;
}

std::string stripBlockComments(std::string s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    int depth = 0;
    while (i < s.size()) {
        if (depth > 0) {
            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
                depth++;
                i += 2;
                continue;
            }
            if (i + 1 < s.size() && s[i] == '*' && s[i + 1] == '/') {
                depth--;
                i += 2;
                continue;
            }
            i++;
            continue;
        }
        if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
            depth = 1;
            i += 2;
            continue;
        }
        out.push_back(s[i]);
        i++;
    }
    return out;
}

std::vector<std::string> mergeTripleQuoteLines(std::vector<std::string> lines) {
    std::vector<std::string> out;
    std::string acc;
    bool inRaw = false;
    for (const auto& line : lines) {
        std::string t = line;
        if (!inRaw) {
            size_t p = t.find("\"\"\"");
            if (p != std::string::npos) {
                size_t q = t.find("\"\"\"", p + 3);
                if (q != std::string::npos) {
                    out.push_back(t);
                    continue;
                }
                inRaw = true;
                acc = t;
                continue;
            }
            out.push_back(t);
            continue;
        }
        acc += "\n";
        acc += t;
        if (t.find("\"\"\"") != std::string::npos) {
            out.push_back(acc);
            acc.clear();
            inRaw = false;
        }
    }
    if (!acc.empty()) out.push_back(acc);
    return out;
}

PreprocessResult preprocessSource(const std::string& fileContent) {
    PreprocessResult pr;
    std::string stripped = stripBlockComments(fileContent);
    std::vector<std::string> rawLines;
    std::istringstream iss(stripped);
    std::string ln;
    while (std::getline(iss, ln)) {
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        rawLines.push_back(ln);
    }
    rawLines = mergeTripleQuoteLines(std::move(rawLines));
    for (auto& line : rawLines) {
        std::string t = trim(line);
        if (startsWith(t, "#!")) continue;
        if (startsWith(t, "#encoding")) {
            std::regex encRe(R"(^\s*#encoding\s+(\S+))");
            std::smatch m;
            if (std::regex_search(t, m, encRe)) pr.pragmaEncoding = m[1].str();
            continue;
        }
        if (t == "#pause-off" || startsWith(t, "#pause-off")) {
            pr.pragmaNoPause = true;
            continue;
        }
        if (t == "#pause-on" || startsWith(t, "#pause-on")) {
            pr.pragmaPauseOn = true;
            continue;
        }
        if (!t.empty() && t[0] == '#' && !startsWith(t, "#\"")) continue;
        pr.lines.push_back(line);
    }
    return pr;
}

PreprocessResult readAndPreprocess(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + file.string());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return preprocessSource(content);
}

std::string toBatchToken(std::string value) {
    value = trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') return value;
    return "%" + value + "%";
}

int netBraceDelta(const std::string& line) {
    int d = 0;
    int quote = 0;
    for (char c : line) {
        if (c == '"') quote = 1 - quote;
        if (quote) continue;
        if (c == '{') d++;
        if (c == '}') d--;
    }
    return d;
}

std::string formatDiagnostic(const std::string& message, const std::string& file, size_t line, const std::string& sourceLine) {
    std::ostringstream oss;
    oss << message;
    if (!file.empty()) {
        oss << " (" << file;
        if (line > 0) oss << ":" << line;
        oss << ")";
    }
    if (!trim(sourceLine).empty()) {
        oss << "\n  > " << trim(sourceLine);
    }
    return oss.str();
}

bool tryParseFunctionHeader(const std::string& lineIn, std::string& nameOut, std::vector<ParamDef>& paramsOut,
                            bool& exported, bool& isMacro, bool& isInline) {
    std::string line = trim(lineIn);
    exported = isMacro = isInline = false;
    while (true) {
        if (startsWith(line, "export ")) {
            exported = true;
            line = trim(line.substr(7));
            continue;
        }
        if (startsWith(line, "macro ")) {
            isMacro = true;
            line = trim(line.substr(6));
            continue;
        }
        if (startsWith(line, "inline ")) {
            isInline = true;
            line = trim(line.substr(7));
            continue;
        }
        break;
    }
    auto tokens = tokenizeLine(line);
    if (tokens.size() < 5) return false;
    if (tokens[0].kind != LexTokenKind::Identifier || tokens[0].text != "fn") return false;
    if (tokens[1].kind != LexTokenKind::Identifier || !isValidIdentifier(tokens[1].text)) return false;
    if (tokens[2].text != "(" || tokens[tokens.size() - 2].text != ")" || tokens.back().text != "{") return false;
    const size_t leftParen = line.find('(');
    const size_t rightParen = line.rfind(')');
    if (leftParen == std::string::npos || rightParen == std::string::npos || rightParen < leftParen) return false;
    nameOut = tokens[1].text;
    paramsOut = parseParamDecls(line.substr(leftParen + 1, rightParen - leftParen - 1));
    return true;
}
