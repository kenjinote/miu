#pragma once
// Pele Native C++ LaTeX-to-MathML Engine
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Pele {

struct MathBox {
    std::string svg;
    std::string html;
    double width = 0.0;
    double height = 0.0;
};

using MathResolver = std::function<MathBox(const std::string& tex, double fontSize)>;

namespace detail {

inline std::string mathTrim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}

inline std::string mathEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

inline uint32_t mathNextUtf8(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && i < s.size()) {
        uint32_t cp = (c & 0x1F) << 6;
        cp |= ((unsigned char)s[i++] & 0x3F);
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && i + 1 < s.size()) {
        uint32_t cp = (c & 0x0F) << 12;
        cp |= ((unsigned char)s[i++] & 0x3F) << 6;
        cp |= ((unsigned char)s[i++] & 0x3F);
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && i + 2 < s.size()) {
        uint32_t cp = (c & 0x07) << 18;
        cp |= ((unsigned char)s[i++] & 0x3F) << 12;
        cp |= ((unsigned char)s[i++] & 0x3F) << 6;
        cp |= ((unsigned char)s[i++] & 0x3F);
        return cp;
    }
    return c;
}

inline bool mathIsWideCodepoint(uint32_t cp) {
    return (cp >= 0x1100 && cp <= 0x11FF) ||
           (cp >= 0x2E80 && cp <= 0x9FFF) ||
           (cp >= 0xAC00 && cp <= 0xD7AF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0x10000);
}
// ============================================================================
// Zero-Dependency Native C++ LaTeX-to-MathML Typesetter & Box Sizer
// ============================================================================
struct MNode {
    std::string mml;
    double w = 0.0; // width in em
    double h = 1.0; // height in em
    bool isLargeOp = false;
};

inline std::string doubleStruckUtf8(char c) {
    switch (c) {
        case 'C': return "\xE2\x84\x82";
        case 'H': return "\xE2\x84\x8D";
        case 'N': return "\xE2\x84\x95";
        case 'P': return "\xE2\x84\x99";
        case 'Q': return "\xE2\x84\x9A";
        case 'R': return "\xE2\x84\x9D";
        case 'Z': return "\xE2\x84\xA4";
        default: break;
    }
    if (c >= 'A' && c <= 'Z') {
        uint32_t cp = 0x1D538u + uint32_t(c - 'A');
        char out[5] = {
            char(0xF0 | ((cp >> 18) & 0x07)),
            char(0x80 | ((cp >> 12) & 0x3F)),
            char(0x80 | ((cp >> 6) & 0x3F)),
            char(0x80 | (cp & 0x3F)),
            0
        };
        return std::string(out, 4);
    }
    if (c >= 'a' && c <= 'z') {
        uint32_t cp = 0x1D552u + uint32_t(c - 'a');
        char out[5] = {
            char(0xF0 | ((cp >> 18) & 0x07)),
            char(0x80 | ((cp >> 12) & 0x3F)),
            char(0x80 | ((cp >> 6) & 0x3F)),
            char(0x80 | (cp & 0x3F)),
            0
        };
        return std::string(out, 4);
    }
    return std::string(1, c);
}

struct LatexParser {
    std::string s;
    size_t pos = 0;
    bool displayMode = true;
    int recDepth = 0;

    LatexParser(const std::string& src, bool disp, int d = 0) : s(src), pos(0), displayMode(disp), recDepth(d) {}

    void skipSpaces() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) pos++;
    }

    std::string readCommand() {
        if (pos >= s.size() || s[pos] != '\\') return "";
        pos++;
        if (pos >= s.size()) return "";
        if (!std::isalpha(static_cast<unsigned char>(s[pos]))) {
            return std::string(1, s[pos++]);
        }
        size_t st = pos;
        while (pos < s.size() && std::isalpha(static_cast<unsigned char>(s[pos]))) pos++;
        return s.substr(st, pos - st);
    }

    std::string readRawGroup() {
        skipSpaces();
        if (pos < s.size() && s[pos] == '{') {
            pos++;
            int depth = 1;
            size_t st = pos;
            while (pos < s.size() && depth > 0) {
                if (s[pos] == '\\' && pos + 1 < s.size()) { pos += 2; continue; }
                if (s[pos] == '{') depth++;
                else if (s[pos] == '}') {
                    depth--;
                    if (depth == 0) {
                        std::string res = s.substr(st, pos - st);
                        pos++;
                        return res;
                    }
                }
                pos++;
            }
            return s.substr(st);
        }
        if (pos < s.size()) return std::string(1, s[pos++]);
        return "";
    }

    std::string readBracketOpt() {
        skipSpaces();
        if (pos >= s.size() || s[pos] != '[') return "";
        pos++;
        int depth = 1;
        size_t st = pos;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '\\' && pos + 1 < s.size()) { pos += 2; continue; }
            if (s[pos] == '[') depth++;
            else if (s[pos] == ']') {
                depth--;
                if (depth == 0) {
                    std::string res = s.substr(st, pos - st);
                    pos++;
                    return res;
                }
            }
            pos++;
        }
        return s.substr(st);
    }

    std::string readDelimToken() {
        skipSpaces();
        if (pos >= s.size()) return "";
        if (s[pos] == '\\') {
            std::string cmd = readCommand();
            if (cmd == "{" || cmd == "lbrace") return "{";
            if (cmd == "}" || cmd == "rbrace") return "}";
            if (cmd == "|" || cmd == "Vert" || cmd == "parallel") return "\xE2\x80\x96";
            if (cmd == "vert") return "|";
            if (cmd == "langle") return "\xE2\x9F\xA8";
            if (cmd == "rangle") return "\xE2\x9F\xA9";
            if (cmd == "lfloor") return "\xE2\x8C\x8A";
            if (cmd == "rfloor") return "\xE2\x8C\x8B";
            if (cmd == "lceil") return "\xE2\x8C\x88";
            if (cmd == "rceil") return "\xE2\x8C\x89";
            if (cmd == "backslash") return "\\";
            return cmd;
        }
        char c = s[pos++];
        if (c == '.') return "";
        return std::string(1, c);
    }

    MNode parseEnvironment(const std::string& env) {
        std::string endTag = "\\end{" + env + "}";
        size_t endPos = s.find(endTag, pos);
        std::string body;
        if (endPos != std::string::npos) {
            body = s.substr(pos, endPos - pos);
            pos = endPos + endTag.size();
        } else {
            body = s.substr(pos);
            pos = s.size();
        }
        // Optional column alignment spec for array: {ccc}
        if (env == "array" || env == "matrix*" || env == "pmatrix*" || env == "bmatrix*") {
            size_t b0 = 0;
            while (b0 < body.size() && (body[b0] == ' ' || body[b0] == '\t')) b0++;
            if (b0 < body.size() && body[b0] == '{') {
                size_t b1 = body.find('}', b0 + 1);
                if (b1 != std::string::npos) body = body.substr(b1 + 1);
            }
        }
        // Split body into rows by `\\` (outside nested braces/environments)
        std::vector<std::string> rows;
        {
            int depth = 0;
            size_t st = 0;
            for (size_t i = 0; i < body.size(); i++) {
                if (body[i] == '{') depth++;
                else if (body[i] == '}') depth = std::max(0, depth - 1);
                else if (depth == 0 && body[i] == '\\' && i + 1 < body.size() && body[i + 1] == '\\') {
                    rows.push_back(body.substr(st, i - st));
                    i++;
                    st = i + 1;
                }
            }
            std::string tail = mathTrim(body.substr(st));
            if (!tail.empty() || rows.empty()) rows.push_back(tail);
        }
        std::vector<std::vector<MNode>> grid;
        size_t maxCols = 0;
        for (const auto& rStr : rows) {
            std::vector<MNode> cols;
            int depth = 0;
            size_t st = 0;
            for (size_t i = 0; i < rStr.size(); i++) {
                if (rStr[i] == '\\' && i + 1 < rStr.size()) { i++; continue; }
                if (rStr[i] == '{') depth++;
                else if (rStr[i] == '}') depth = std::max(0, depth - 1);
                else if (depth == 0 && rStr[i] == '&') {
                    LatexParser sub(rStr.substr(st, i - st), false, recDepth + 1);
                    cols.push_back(sub.parseExpr());
                    st = i + 1;
                }
            }
            LatexParser sub(rStr.substr(st), false, recDepth + 1);
            cols.push_back(sub.parseExpr());
            maxCols = std::max(maxCols, cols.size());
            grid.push_back(std::move(cols));
        }
        std::vector<double> colW(maxCols, 0.0);
        double totalH = 0.0;
        std::string tableMml = "<mtable>";
        for (size_t r = 0; r < grid.size(); r++) {
            tableMml += "<mtr>";
            double rowH = 1.0;
            for (size_t c = 0; c < grid[r].size(); c++) {
                const auto& cell = grid[r][c];
                std::string alignAttr = (env == "cases" && c == 0) ? " columnalign=\"left\"" : "";
                tableMml += "<mtd" + alignAttr + ">" + cell.mml + "</mtd>";
                colW[c] = std::max(colW[c], cell.w);
                rowH = std::max(rowH, cell.h);
            }
            tableMml += "</mtr>";
            totalH += rowH + (r + 1 < grid.size() ? 0.35 : 0.0);
        }
        tableMml += "</mtable>";
        double totalW = 0.0;
        for (size_t c = 0; c < maxCols; c++) totalW += colW[c] + (c + 1 < maxCols ? 0.75 : 0.0);

        std::string lDelim, rDelim;
        if (env == "pmatrix" || env == "pmatrix*") { lDelim = "("; rDelim = ")"; }
        else if (env == "bmatrix" || env == "bmatrix*") { lDelim = "["; rDelim = "]"; }
        else if (env == "Bmatrix" || env == "Bmatrix*") { lDelim = "{"; rDelim = "}"; }
        else if (env == "vmatrix" || env == "vmatrix*") { lDelim = "|"; rDelim = "|"; }
        else if (env == "Vmatrix" || env == "Vmatrix*") { lDelim = "\xE2\x80\x96"; rDelim = "\xE2\x80\x96"; }
        else if (env == "cases") { lDelim = "{"; rDelim = ""; }

        MNode out;
        out.h = std::max(1.2, totalH + 0.2);
        out.w = totalW + (!lDelim.empty() ? 0.55 : 0.0) + (!rDelim.empty() ? 0.55 : 0.0);
        if (!lDelim.empty() || !rDelim.empty()) {
            out.mml = "<mrow>";
            if (!lDelim.empty()) out.mml += "<mo stretchy=\"true\">" + mathEsc(lDelim) + "</mo>";
            out.mml += tableMml;
            if (!rDelim.empty()) out.mml += "<mo stretchy=\"true\">" + mathEsc(rDelim) + "</mo>";
            out.mml += "</mrow>";
        } else {
            out.mml = tableMml;
        }
        return out;
    }

    MNode parseAtom() {
        skipSpaces();
        if (pos >= s.size() || recDepth > 64) {
            if (pos < s.size()) pos = s.size();
            return {"", 0.0, 1.0, false};
        }
        struct DepthGuard { int& d; DepthGuard(int& r) : d(r) { ++d; } ~DepthGuard() { --d; } } dg(recDepth);

        char c = s[pos];
        if (c == '{') {
            pos++;
            MNode inner = parseExpr('}');
            if (pos < s.size() && s[pos] == '}') pos++;
            return {"<mrow>" + inner.mml + "</mrow>", inner.w, inner.h, inner.isLargeOp};
        }
        if (c == '\\') {
            std::string cmd = readCommand();
            if (cmd.empty()) return {"", 0.0, 1.0, false};

            // Spacing
            if (cmd == "," || cmd == "thinspace") return {"<mspace width=\"0.17em\"/>", 0.17, 1.0, false};
            if (cmd == ":" || cmd == ">" || cmd == "medspace") return {"<mspace width=\"0.22em\"/>", 0.22, 1.0, false};
            if (cmd == ";" || cmd == "thickspace") return {"<mspace width=\"0.28em\"/>", 0.28, 1.0, false};
            if (cmd == "!") return {"", 0.0, 1.0, false};
            if (cmd == " ") return {"<mspace width=\"0.25em\"/>", 0.25, 1.0, false};
            if (cmd == "quad") return {"<mspace width=\"1em\"/>", 1.0, 1.0, false};
            if (cmd == "qquad") return {"<mspace width=\"2em\"/>", 2.0, 1.0, false};
            if (cmd == "displaystyle" || cmd == "textstyle" || cmd == "scriptstyle" || cmd == "limits" || cmd == "nolimits") {
                return {"", 0.0, 1.0, false};
            }

            // Environments
            if (cmd == "begin") {
                std::string env = mathTrim(readRawGroup());
                return parseEnvironment(env);
            }

            // \left ... \right
            if (cmd == "left") {
                std::string ld = readDelimToken();
                MNode inner = parseExpr(0, true);
                std::string rd;
                skipSpaces();
                if (s.compare(pos, 6, "\\right") == 0) {
                    pos += 6;
                    rd = readDelimToken();
                }
                std::string m = "<mrow>";
                double dw = 0.0;
                if (!ld.empty()) { m += "<mo stretchy=\"true\">" + mathEsc(ld) + "</mo>"; dw += 0.48; }
                m += inner.mml;
                if (!rd.empty()) { m += "<mo stretchy=\"true\">" + mathEsc(rd) + "</mo>"; dw += 0.48; }
                m += "</mrow>";
                return {m, inner.w + dw, std::max(1.15, inner.h), false};
            }
            if (cmd == "big" || cmd == "Big" || cmd == "bigg" || cmd == "Bigg" ||
                cmd == "bigl" || cmd == "bigr" || cmd == "Bigl" || cmd == "Bigr" ||
                cmd == "biggl" || cmd == "biggr" || cmd == "Biggl" || cmd == "Biggr") {
                std::string d = readDelimToken();
                return {"<mo stretchy=\"true\">" + mathEsc(d) + "</mo>", 0.5, 1.3, false};
            }

            // Fractions & Binomials
            if (cmd == "frac" || cmd == "dfrac" || cmd == "tfrac" || cmd == "cfrac") {
                MNode numNode = parseAtom();
                MNode denNode = parseAtom();
                double w = std::max(numNode.w, denNode.w) * 0.94 + 0.45;
                double h = (numNode.h + denNode.h) * 0.85 + 0.50;
                return {"<mfrac><mrow>" + numNode.mml + "</mrow><mrow>" + denNode.mml + "</mrow></mfrac>", w, h, false};
            }
            if (cmd == "binom" || cmd == "dbinom" || cmd == "tbinom") {
                MNode nNode = parseAtom();
                MNode kNode = parseAtom();
                double w = std::max(nNode.w, kNode.w) * 0.94 + 1.05;
                double h = (nNode.h + kNode.h) * 0.85 + 0.45;
                return {"<mrow><mo stretchy=\"true\">(</mo><mfrac linethickness=\"0\"><mrow>" + nNode.mml +
                        "</mrow><mrow>" + kNode.mml + "</mrow></mfrac><mo stretchy=\"true\">)</mo></mrow>", w, h, false};
            }

            // Roots
            if (cmd == "sqrt") {
                std::string opt = readBracketOpt();
                MNode arg = parseAtom();
                if (!opt.empty()) {
                    LatexParser sub(opt, false, recDepth + 1);
                    MNode deg = sub.parseExpr();
                    double w = arg.w + std::max(0.85, deg.w * 0.65 + 0.45);
                    double h = arg.h + 0.32;
                    return {"<mroot><mrow>" + arg.mml + "</mrow><mrow>" + deg.mml + "</mrow></mroot>", w, h, false};
                }
                return {"<msqrt><mrow>" + arg.mml + "</mrow></msqrt>", arg.w + 0.82, arg.h + 0.28, false};
            }

            // Overset / Underset / Stackrel
            if (cmd == "overset" || cmd == "stackrel") {
                MNode topNode = parseAtom();
                MNode baseNode = parseAtom();
                return {"<mover><mrow>" + baseNode.mml + "</mrow><mrow>" + topNode.mml + "</mrow></mover>",
                        std::max(baseNode.w, topNode.w * 0.75) + 0.1, baseNode.h + topNode.h * 0.65, false};
            }
            if (cmd == "underset") {
                MNode botNode = parseAtom();
                MNode baseNode = parseAtom();
                return {"<munder><mrow>" + baseNode.mml + "</mrow><mrow>" + botNode.mml + "</mrow></munder>",
                        std::max(baseNode.w, botNode.w * 0.75) + 0.1, baseNode.h + botNode.h * 0.65, false};
            }

            // Accents & Bars
            if (cmd == "hat" || cmd == "widehat" || cmd == "tilde" || cmd == "widetilde" ||
                cmd == "bar" || cmd == "overline" || cmd == "vec" || cmd == "overrightarrow" ||
                cmd == "overleftarrow" || cmd == "dot" || cmd == "ddot" || cmd == "overbrace") {
                MNode arg = parseAtom();
                std::string acc = "^";
                if (cmd == "tilde" || cmd == "widetilde") acc = "~";
                else if (cmd == "bar" || cmd == "overline") acc = "\xC2\xAF";
                else if (cmd == "vec" || cmd == "overrightarrow") acc = "\xE2\x86\x92";
                else if (cmd == "overleftarrow") acc = "\xE2\x86\x90";
                else if (cmd == "dot") acc = "\xCB\x99";
                else if (cmd == "ddot") acc = "\xC2\xA8";
                else if (cmd == "overbrace") acc = "\xE2\x8F\x9E";
                return {"<mover accent=\"true\"><mrow>" + arg.mml + "</mrow><mo>" + acc + "</mo></mover>",
                        arg.w, arg.h + 0.22, false};
            }
            if (cmd == "underline" || cmd == "underbrace") {
                MNode arg = parseAtom();
                std::string acc = (cmd == "underbrace") ? "\xE2\x8F\x9F" : "_";
                return {"<munder accentunder=\"true\"><mrow>" + arg.mml + "</mrow><mo>" + acc + "</mo></munder>",
                        arg.w, arg.h + 0.22, false};
            }

            // Text & Font Commands
            if (cmd == "text" || cmd == "textrm" || cmd == "textbf" || cmd == "textit" || cmd == "mbox" || cmd == "operatorname") {
                std::string txt = readRawGroup();
                double tw = double(txt.size()) * 0.56;
                if (cmd == "operatorname") {
                    return {"<mi mathvariant=\"normal\">" + mathEsc(txt) + "</mi>", tw + 0.15, 1.0, false};
                }
                return {"<mtext>" + mathEsc(txt) + "</mtext>", tw, 1.0, false};
            }
            if (cmd == "mathbb") {
                std::string txt = readRawGroup();
                std::string outStr;
                for (char ch : txt) outStr += doubleStruckUtf8(ch);
                return {"<mi>" + mathEsc(outStr) + "</mi>", double(txt.size()) * 0.72, 1.0, false};
            }
            if (cmd == "mathrm" || cmd == "mathbf" || cmd == "mathit" || cmd == "mathcal" ||
                cmd == "mathscr" || cmd == "mathfrak" || cmd == "mathsf" || cmd == "mathtt" ||
                cmd == "boldsymbol" || cmd == "bm") {
                std::string mv = "normal";
                if (cmd == "mathbf" || cmd == "boldsymbol" || cmd == "bm") mv = "bold";
                else if (cmd == "mathit") mv = "italic";
                else if (cmd == "mathcal" || cmd == "mathscr") mv = "script";
                else if (cmd == "mathfrak") mv = "fraktur";
                else if (cmd == "mathsf") mv = "sans-serif";
                else if (cmd == "mathtt") mv = "monospace";
                MNode arg = parseAtom();
                return {"<mstyle mathvariant=\"" + mv + "\">" + arg.mml + "</mstyle>", arg.w * 1.05, arg.h, arg.isLargeOp};
            }

            // Escaped literal delimiters
            if (cmd == "{" || cmd == "lbrace") return {"<mo stretchy=\"false\">{</mo>", 0.45, 1.0, false};
            if (cmd == "}" || cmd == "rbrace") return {"<mo stretchy=\"false\">}</mo>", 0.45, 1.0, false};
            if (cmd == "%") return {"<mo>%</mo>", 0.75, 1.0, false};
            if (cmd == "$") return {"<mtext>$</mtext>", 0.55, 1.0, false};
            if (cmd == "&") return {"<mtext>&amp;</mtext>", 0.70, 1.0, false};
            if (cmd == "#") return {"<mtext>#</mtext>", 0.65, 1.0, false};
            if (cmd == "_") return {"<mtext>_</mtext>", 0.55, 1.0, false};

            // Large Operators
            static const std::unordered_map<std::string, std::string> largeOps = {
                {"sum", "\xE2\x88\x91"}, {"prod", "\xE2\x88\x8F"}, {"coprod", "\xE2\x88\x90"},
                {"int", "\xE2\x88\xAB"}, {"iint", "\xE2\x88\xAC"}, {"iiint", "\xE2\x88\xAD"}, {"oint", "\xE2\x88\xAE"},
                {"bigcup", "\xE2\x8B\x83"}, {"bigcap", "\xE2\x8B\x82"}, {"bigoplus", "\xE2\xA8\x81"},
                {"bigotimes", "\xE2\xA8\x82"}, {"bigsqcup", "\xE2\xA8\x86"}, {"bigvee", "\xE2\x8B\x81"}, {"bigwedge", "\xE2\x8B\x80"}
            };
            auto loIt = largeOps.find(cmd);
            if (loIt != largeOps.end()) {
                return {"<mo>" + loIt->second + "</mo>", 1.20, 1.55, true};
            }

            // Standard Math Functions
            static const std::unordered_set<std::string> mathFuncs = {
                "sin", "cos", "tan", "cot", "sec", "csc", "arcsin", "arccos", "arctan",
                "sinh", "cosh", "tanh", "coth", "log", "ln", "lg", "exp",
                "lim", "limsup", "liminf", "max", "min", "sup", "inf",
                "arg", "deg", "det", "dim", "gcd", "hom", "ker", "Pr", "mod", "bmod"
            };
            if (mathFuncs.count(cmd)) {
                bool under = (cmd == "lim" || cmd == "limsup" || cmd == "liminf" ||
                              cmd == "max" || cmd == "min" || cmd == "sup" || cmd == "inf" ||
                              cmd == "det" || cmd == "Pr" || cmd == "gcd");
                return {"<mi mathvariant=\"normal\">" + cmd + "</mi>", double(cmd.size()) * 0.56 + 0.18, 1.0, under};
            }

            // Lowercase Greek & Symbols (<mi>)
            static const std::unordered_map<std::string, std::string> greekLower = {
                {"alpha", "\xCE\xB1"}, {"beta", "\xCE\xB2"}, {"gamma", "\xCE\xB3"}, {"delta", "\xCE\xB4"},
                {"epsilon", "\xCF\xB5"}, {"varepsilon", "\xCE\xB5"}, {"zeta", "\xCE\xB6"}, {"eta", "\xCE\xB7"},
                {"theta", "\xCE\xB8"}, {"vartheta", "\xCF\x91"}, {"iota", "\xCE\xB9"}, {"kappa", "\xCE\xBA"},
                {"lambda", "\xCE\xBB"}, {"mu", "\xCE\xBC"}, {"nu", "\xCE\xBD"}, {"xi", "\xCE\xBE"},
                {"pi", "\xCF\x80"}, {"varpi", "\xCF\x96"}, {"rho", "\xCF\x81"}, {"varrho", "\xCF\xB1"},
                {"sigma", "\xCF\x83"}, {"varsigma", "\xCF\x82"}, {"tau", "\xCF\x84"}, {"upsilon", "\xCF\x85"},
                {"phi", "\xCF\x95"}, {"varphi", "\xCF\x86"}, {"chi", "\xCF\x87"}, {"psi", "\xCF\x88"},
                {"omega", "\xCF\x89"}, {"infty", "\xE2\x88\x9E"}, {"partial", "\xE2\x88\x82"}, {"nabla", "\xE2\x88\x87"},
                {"hbar", "\xE2\x84\x8F"}, {"ell", "\xE2\x84\x93"}, {"Re", "\xE2\x84\x9C"}, {"Im", "\xE2\x84\x91"},
                {"wp", "\xE2\x84\x98"}, {"aleph", "\xE2\x84\xB5"}, {"emptyset", "\xE2\x88\x85"}, {"varnothing", "\xE2\x88\x85"},
                {"forall", "\xE2\x88\x80"}, {"exists", "\xE2\x88\x83"}, {"nexists", "\xE2\x88\x84"},
                {"neg", "\xC2\xAC"}, {"lnot", "\xC2\xAC"}, {"top", "\xE2\x8A\xA4"}, {"bot", "\xE2\x8A\xA5"},
                {"triangle", "\xE2\x96\xB3"}, {"angle", "\xE2\x88\xA0"}, {"prime", "\xE2\x80\xB2"},
                {"cdots", "\xE2\x8B\xAF"}, {"ldots", "\xE2\x80\xA6"}, {"dots", "\xE2\x80\xA6"},
                {"vdots", "\xE2\x8B\xAE"}, {"ddots", "\xE2\x8B\xB1"}
            };
            auto glIt = greekLower.find(cmd);
            if (glIt != greekLower.end()) {
                double w = (cmd == "cdots" || cmd == "ldots" || cmd == "dots") ? 1.15 : 0.68;
                return {"<mi>" + glIt->second + "</mi>", w, 1.0, false};
            }

            // Uppercase Greek (<mi mathvariant="normal">)
            static const std::unordered_map<std::string, std::string> greekUpper = {
                {"Gamma", "\xCE\x93"}, {"Delta", "\xCE\x94"}, {"Theta", "\xCE\x98"}, {"Lambda", "\xCE\x9B"},
                {"Xi", "\xCE\x9E"}, {"Pi", "\xCE\xA0"}, {"Sigma", "\xCE\xA3"}, {"Upsilon", "\xCE\xA5"},
                {"Phi", "\xCE\xA6"}, {"Psi", "\xCE\xA8"}, {"Omega", "\xCE\xA9"}
            };
            auto guIt = greekUpper.find(cmd);
            if (guIt != greekUpper.end()) {
                return {"<mi mathvariant=\"normal\">" + guIt->second + "</mi>", 0.78, 1.0, false};
            }

            // Operators & Relations (<mo>)
            static const std::unordered_map<std::string, std::string> mathOps = {
                {"pm", "\xC2\xB1"}, {"mp", "\xE2\x88\x93"}, {"times", "\xC3\x97"}, {"div", "\xC3\xB7"},
                {"cdot", "\xE2\x8B\x85"}, {"ast", "\xE2\x88\x97"}, {"star", "\xE2\x8B\x86"}, {"circ", "\xE2\x88\x98"},
                {"bullet", "\xE2\x88\x99"}, {"oplus", "\xE2\x8A\x95"}, {"ominus", "\xE2\x8A\x96"},
                {"otimes", "\xE2\x8A\x97"}, {"oslash", "\xE2\x8A\x98"}, {"odot", "\xE2\x8A\x99"},
                {"cap", "\xE2\x88\xA9"}, {"cup", "\xE2\x88\xAA"}, {"setminus", "\xE2\x88\x96"},
                {"wedge", "\xE2\x88\xA7"}, {"land", "\xE2\x88\xA7"}, {"vee", "\xE2\x88\xA8"}, {"lor", "\xE2\x88\xA8"},
                {"neq", "\xE2\x89\xA0"}, {"ne", "\xE2\x89\xA0"}, {"leq", "\xE2\x89\xA4"}, {"le", "\xE2\x89\xA4"},
                {"geq", "\xE2\x89\xA5"}, {"ge", "\xE2\x89\xA5"}, {"ll", "\xE2\x89\xAA"}, {"gg", "\xE2\x89\xAB"},
                {"approx", "\xE2\x89\x88"}, {"equiv", "\xE2\x89\xA1"}, {"sim", "\xE2\x88\xBC"}, {"simeq", "\xE2\x89\x83"},
                {"cong", "\xE2\x89\x85"}, {"propto", "\xE2\x88\x9D"}, {"doteq", "\xE2\x89\x90"},
                {"in", "\xE2\x88\x88"}, {"notin", "\xE2\x88\x89"}, {"ni", "\xE2\x88\x8B"},
                {"subset", "\xE2\x8A\x82"}, {"supset", "\xE2\x8A\x83"}, {"subseteq", "\xE2\x8A\x86"}, {"supseteq", "\xE2\x8A\x87"},
                {"vdash", "\xE2\x8A\xA2"}, {"dashv", "\xE2\x8A\xA3"}, {"models", "\xE2\x8A\xA8"},
                {"perp", "\xE2\x8A\xA5"}, {"parallel", "\xE2\x88\xA5"}, {"mid", "\xE2\x88\xA3"}, {"nmid", "\xE2\x88\xA4"},
                {"to", "\xE2\x86\x92"}, {"rightarrow", "\xE2\x86\x92"}, {"leftarrow", "\xE2\x86\x90"}, {"gets", "\xE2\x86\x90"},
                {"leftrightarrow", "\xE2\x86\x94"}, {"Rightarrow", "\xE2\x87\x92"}, {"Leftarrow", "\xE2\x87\x90"},
                {"Leftrightarrow", "\xE2\x87\x94"}, {"iff", "\xE2\x9F\xBA"}, {"implies", "\xE2\x9F\xB9"},
                {"impliedby", "\xE2\x9F\xB8"}, {"mapsto", "\xE2\x86\xA6"}, {"longmapsto", "\xE2\x9F\xBC"},
                {"longrightarrow", "\xE2\x9F\xB6"}, {"longleftarrow", "\xE2\x9F\xB5"},
                {"uparrow", "\xE2\x86\x91"}, {"downarrow", "\xE2\x86\x93"}, {"updownarrow", "\xE2\x86\x95"},
                {"Uparrow", "\xE2\x87\x91"}, {"Downarrow", "\xE2\x87\x93"},
                {"nearrow", "\xE2\x86\x97"}, {"searrow", "\xE2\x86\x98"}, {"swarrow", "\xE2\x86\x99"}, {"nwarrow", "\xE2\x86\x96"},
                {"langle", "\xE2\x9F\xA8"}, {"rangle", "\xE2\x9F\xA9"},
                {"lfloor", "\xE2\x8C\x8A"}, {"rfloor", "\xE2\x8C\x8B"},
                {"lceil", "\xE2\x8C\x88"}, {"rceil", "\xE2\x8C\x89"},
                {"|", "\xE2\x80\x96"}, {"Vert", "\xE2\x80\x96"}, {"vert", "|"}, {"backslash", "\\"}
            };
            auto opIt = mathOps.find(cmd);
            if (opIt != mathOps.end()) {
                return {"<mo>" + opIt->second + "</mo>", 1.05, 1.0, false};
            }

            // Fallback unknown command -> identifier
            return {"<mi>" + mathEsc(cmd) + "</mi>", double(cmd.size()) * 0.58, 1.0, false};
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t st = pos;
            while (pos < s.size() && (std::isdigit(static_cast<unsigned char>(s[pos])) ||
                   (s[pos] == '.' && pos + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[pos + 1]))))) {
                pos++;
            }
            std::string numStr = s.substr(st, pos - st);
            return {"<mn>" + numStr + "</mn>", double(numStr.size()) * 0.60, 1.0, false};
        }

        // ASCII letters
        if (std::isalpha(static_cast<unsigned char>(c))) {
            pos++;
            double w = std::isupper(static_cast<unsigned char>(c)) ? 0.74 : 0.60;
            return {"<mi>" + std::string(1, c) + "</mi>", w, 1.0, false};
        }

        // Multi-byte UTF-8 character (e.g. Japanese/Greek/Unicode directly in math)
        if (static_cast<unsigned char>(c) >= 0x80) {
            size_t st = pos;
            uint32_t cp = mathNextUtf8(s, pos);
            std::string u = s.substr(st, pos - st);
            double w = mathIsWideCodepoint(cp) ? 1.05 : 0.68;
            return {"<mi>" + mathEsc(u) + "</mi>", w, 1.0, false};
        }

        // Operators and punctuation
        pos++;
        if (c == '+' || c == '-' || c == '=' || c == '<' || c == '>' || c == '*' || c == '/') {
            return {"<mo>" + mathEsc(std::string(1, c)) + "</mo>", 1.02, 1.0, false};
        }
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '|') {
            return {"<mo stretchy=\"false\">" + mathEsc(std::string(1, c)) + "</mo>", 0.42, 1.0, false};
        }
        if (c == ',' || c == ';' || c == ':' || c == '!' || c == '?') {
            return {"<mo>" + mathEsc(std::string(1, c)) + "</mo>", 0.40, 1.0, false};
        }
        return {"<mo>" + mathEsc(std::string(1, c)) + "</mo>", 0.55, 1.0, false};
    }

    MNode parseFactor() {
        MNode base = parseAtom();
        if (base.mml.empty()) return base;

        bool hasSub = false, hasSup = false;
        MNode subNode, supNode;

        while (true) {
            skipSpaces();
            if (pos >= s.size()) break;
            if (s[pos] == '_' && !hasSub) {
                pos++;
                subNode = parseAtom();
                hasSub = true;
            } else if (s[pos] == '^' && !hasSup) {
                pos++;
                supNode = parseAtom();
                hasSup = true;
            } else if (s[pos] == '\'' && !hasSup) {
                std::string primes;
                int pCount = 0;
                while (pos < s.size() && s[pos] == '\'') {
                    primes += "\xE2\x80\xB2";
                    pCount++;
                    pos++;
                }
                supNode = {"<mo>" + primes + "</mo>", double(pCount) * 0.35, 0.8, false};
                hasSup = true;
            } else {
                break;
            }
        }

        if (!hasSub && !hasSup) return base;

        if (base.isLargeOp && displayMode) {
            double w = std::max({base.w, hasSub ? subNode.w * 0.78 : 0.0, hasSup ? supNode.w * 0.78 : 0.0}) + 0.20;
            double h = base.h + (hasSub ? subNode.h * 0.65 : 0.0) + (hasSup ? supNode.h * 0.65 : 0.0) + 0.15;
            if (hasSub && hasSup) {
                return {"<munderover><mrow>" + base.mml + "</mrow><mrow>" + subNode.mml + "</mrow><mrow>" + supNode.mml + "</mrow></munderover>", w, h, false};
            }
            if (hasSub) {
                return {"<munder><mrow>" + base.mml + "</mrow><mrow>" + subNode.mml + "</mrow></munder>", w, h, false};
            }
            return {"<mover><mrow>" + base.mml + "</mrow><mrow>" + supNode.mml + "</mrow></mover>", w, h, false};
        }

        double scriptW = std::max(hasSub ? subNode.w : 0.0, hasSup ? supNode.w : 0.0) * 0.75 + 0.10;
        double w = base.w + scriptW;
        double h = base.h + (hasSup ? std::max(0.28, (supNode.h - 0.5) * 0.65) : 0.0) +
                            (hasSub ? std::max(0.20, (subNode.h - 0.5) * 0.55) : 0.0);
        if (hasSub && hasSup) {
            return {"<msubsup><mrow>" + base.mml + "</mrow><mrow>" + subNode.mml + "</mrow><mrow>" + supNode.mml + "</mrow></msubsup>", w, h, false};
        }
        if (hasSub) {
            return {"<msub><mrow>" + base.mml + "</mrow><mrow>" + subNode.mml + "</mrow></msub>", w, h, false};
        }
        return {"<msup><mrow>" + base.mml + "</mrow><mrow>" + supNode.mml + "</mrow></msup>", w, h, false};
    }

    MNode parseExpr(char stopChar = 0, bool stopOnRight = false) {
        MNode out;
        out.h = 1.0;
        while (true) {
            skipSpaces();
            if (pos >= s.size()) break;
            if (stopChar != 0 && s[pos] == stopChar) break;
            if (stopOnRight && s.compare(pos, 6, "\\right") == 0 &&
                (pos + 6 >= s.size() || !std::isalpha(static_cast<unsigned char>(s[pos + 6])))) {
                break;
            }
            size_t before = pos;
            MNode f = parseFactor();
            if (pos == before) { pos++; continue; }
            out.mml += f.mml;
            out.w += f.w;
            out.h = std::max(out.h, f.h);
        }
        return out;
    }
};

inline MathBox typesetLatexMath(const std::string& tex, double fontSize = 16.0, bool displayMode = true) {
    MathBox box;
    std::string clean = mathTrim(tex);
    if (clean.empty()) return box;
    LatexParser parser(clean, displayMode);
    MNode root = parser.parseExpr();
    std::string dispAttr = displayMode ? " display=\"block\"" : "";
    box.html = "<math xmlns=\"http://www.w3.org/1998/Math/MathML\"" + dispAttr + "><mrow>" + root.mml + "</mrow></math>";
    box.width = std::ceil(std::max(fontSize * 1.1, root.w * fontSize + 14.0));
    box.height = std::ceil(std::max(fontSize * 1.35, root.h * fontSize + 6.0));
    return box;
}

inline MathResolver& activeMathResolver() {
    static MathResolver r = nullptr;
    return r;
}
inline bool looksLikeInlineMath(const std::string& inner) {
    if (inner.empty() || inner.front() == ' ' || inner.back() == ' ') return false;
    bool allDigits = true;
    for (char c : inner) {
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.' && c != ',') {
            allDigits = false;
            break;
        }
    }
    if (allDigits) return false;
    for (char c : inner) {
        if (c == '\\' || c == '^' || c == '_' || c == '{' || c == '}' || c == '=') return true;
    }
    if (inner.size() <= 16 && std::isalpha(static_cast<unsigned char>(inner.front()))) return true;
    return false;
}

} // namespace detail

inline MathBox typesetMath(const std::string& tex, double fontSize = 16.0, bool displayMode = true) {
    return detail::typesetLatexMath(tex, fontSize, displayMode);
}

inline std::string latexToMathML(const std::string& tex, bool displayMode = true) {
    return detail::typesetLatexMath(tex, 16.0, displayMode).html;
}

} // namespace Pele