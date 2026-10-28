#pragma once
#include "../foundation.h"

namespace Pele {
namespace TreeViewDiagram {

// ============================================================================
// src/diagrams/treeview/boxDrawing.ts
// ============================================================================

inline bool isBranchChar(uint16_t c) noexcept {
    return c == 0x2514 || c == 0x2517 || c == 0x251c || c == 0x2523;
}

inline bool isDashChar(uint16_t c) noexcept {
    return c == 0x2500 || c == 0x2501;
}

inline bool isPipeChar(uint16_t c) noexcept {
    return c == 0x2502 || c == 0x2503;
}

inline bool isAllBoxChar(uint16_t c) noexcept {
    return isBranchChar(c) || isDashChar(c) || isPipeChar(c);
}

inline bool hasAnyBoxChar(const std::string& s) noexcept {
    for (uint16_t c : toUtf16(s)) {
        if (isAllBoxChar(c)) return true;
    }
    return false;
}

inline bool isDecorationOnly(const std::string& line) noexcept {
    auto u16 = toUtf16(line);
    if (u16.empty()) return false;
    for (uint16_t c : u16) {
        if (!isSpace(c) && !isPipeChar(c)) return false;
    }
    return true;
}

inline bool isBoxOnly(const std::string& line) noexcept {
    auto u16 = toUtf16(line);
    if (u16.empty()) return false;
    for (uint16_t c : u16) {
        if (!isSpace(c) && !isAllBoxChar(c)) return false;
    }
    return true;
}

inline bool isMetadataLine(const std::string& line) {
    std::string s = trimStartStr(line);
    if (startsWith(s, "title ") || startsWith(s, "title\t")) return true;
    if (startsWith(s, "accTitle")) {
        size_t p = 8;
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) p++;
        if (p < s.size() && s[p] == ':') return true;
    }
    if (startsWith(s, "accDescr")) {
        size_t p = 8;
        while (p < s.size() && (s[p] == ' ' || s[p] == '\t')) p++;
        if (p < s.size() && (s[p] == ':' || s[p] == '{')) return true;
    }
    return false;
}

inline std::string preprocessBoxDrawing(const std::string& input) {
    if (!hasAnyBoxChar(input)) return input;
    auto lines = splitStr(input, '\n');

    int keywordIdx = -1;
    for (size_t i = 0; i < lines.size(); i++) {
        if (trimStr(lines[i]) == "treeView-beta") {
            keywordIdx = static_cast<int>(i);
            break;
        }
    }
    if (keywordIdx == -1) return input;

    auto passes = [](const std::string& line) {
        std::string t = trimStr(line);
        return t.empty() || startsWith(t, "%%") || isMetadataLine(line);
    };

    std::vector<std::string> contentLines;
    for (size_t i = static_cast<size_t>(keywordIdx + 1); i < lines.size(); i++) {
        const auto& line = lines[i];
        if (passes(line) || isDecorationOnly(line)) continue;
        contentLines.push_back(replaceAllStr(line, "\t", "    "));
    }
    bool anyBox = false;
    for (const auto& cl : contentLines) {
        if (hasAnyBoxChar(cl)) { anyBox = true; break; }
    }
    if (!anyBox) return input;

    int segmentWidth = 4;
    for (const auto& cl : contentLines) {
        auto u16 = toUtf16(cl);
        int found = -1;
        for (size_t j = 0; j < u16.size(); j++) {
            if (isBranchChar(u16[j])) { found = static_cast<int>(j); break; }
        }
        if (found > 0) {
            segmentWidth = found;
            break;
        }
    }

    std::vector<std::string> out;
    for (int i = 0; i <= keywordIdx; i++) out.push_back(lines[static_cast<size_t>(i)]);
    for (size_t i = static_cast<size_t>(keywordIdx + 1); i < lines.size(); i++) {
        const auto& line = lines[i];
        if (passes(line)) {
            out.push_back(line);
            continue;
        }
        if (isDecorationOnly(line)) continue;

        std::string normalized = replaceAllStr(line, "\t", "    ");
        auto u16 = toUtf16(normalized);
        int branchIdx = -1;
        for (size_t j = 0; j < u16.size(); j++) {
            if (isBranchChar(u16[j])) { branchIdx = static_cast<int>(j); break; }
        }
        if (branchIdx >= 0) {
            int depth = static_cast<int>(std::round(static_cast<double>(branchIdx) / static_cast<double>(segmentWidth))) + 1;
            size_t pos = static_cast<size_t>(branchIdx + 1);
            while (pos < u16.size() && isDashChar(u16[pos])) pos++;
            while (pos < u16.size() && u16[pos] == ' ') pos++;
            std::string content = trimEndStr(fromUtf16(u16.data() + pos, u16.size() - pos));
            if (content.empty()) {
                throw PeleError("Empty node", "syntax", "treeView", static_cast<int>(i + 1));
            }
            std::string indent;
            for (int d = 0; d < depth; d++) indent += "    ";
            out.push_back(indent + content);
        } else if (isBoxOnly(normalized)) {
            continue;
        } else if (hasAnyBoxChar(normalized)) {
            out.push_back(line);
        } else if (!normalized.empty() && isSpace(charCodeAt(normalized, 0))) {
            throw PeleError("Unexpected indentation without box-drawing characters.", "syntax", "treeView", static_cast<int>(i + 1));
        } else {
            out.push_back(line);
        }
    }

    std::string joined;
    for (size_t i = 0; i < out.size(); i++) {
        if (i > 0) joined.push_back('\n');
        joined += out[i];
    }
    return joined;
}

// ============================================================================
// src/diagrams/treeview/parser.ts
// ============================================================================

namespace T {
enum Kind {
    keyword = 0,
    accDescr = 1,
    accTitle = 2,
    title = 3,
    classAnnotation = 4,
    iconAnnotation = 5,
    descAnnotation = 6,
    indentation = 7,
    quotedName = 8,
    ws = 9,
    comment = 10,
    newline = 11,
    bareName = 12
};
} // namespace T

inline bool annotationAt(const std::string& src, int at) noexcept {
    if (startsWith(src, "##", static_cast<size_t>(at)) || startsWith(src, "icon(", static_cast<size_t>(at))) return true;
    if (!startsWith(src, ":::", static_cast<size_t>(at))) return false;
    int i = at + 3;
    while (isBlank(charCodeAt(src, i))) i++;
    int c = charCodeAt(src, i);
    return (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95;
}

inline int matchBareName(const std::string& src, int at, int) noexcept {
    int first = charCodeAt(src, at);
    if (first < 0 || isBlank(first) || first == 10 || first == 13 || first == 34 || first == 39) return -1;
    if (startsWith(src, ":::", static_cast<size_t>(at)) || startsWith(src, "icon(", static_cast<size_t>(at)) || startsWith(src, "##", static_cast<size_t>(at))) return -1;
    int n = static_cast<int>(src.size());
    int i = at + 1;
    while (i < n) {
        int c = charCodeAt(src, i);
        if (c == 10 || c == 13) break;
        if (isBlank(c)) {
            int j = i + 1;
            while (isBlank(charCodeAt(src, j))) j++;
            if (annotationAt(src, j)) break;
            i = j;
        } else {
            i++;
        }
    }
    return i;
}

inline TokenType makeTreeViewTitle() noexcept {
    TokenType t = common_tokens::makeTitle();
    t.match = [](const std::string& src, int at, int) -> int {
        int p = common_tokens::matchTabsSpaces(src, at);
        if (src.compare(p, 5, "title") != 0) return -1;
        p += 5;
        int n = static_cast<int>(src.size());
        if (p < n && (src[p] == '\t' || src[p] == ' ')) {
            return common_tokens::matchRestOfLineBeforeComment(src, p + 1);
        }
        // `(?! (?!%%)[^\n\r] )`
        if (p >= n || src[p] == '\n' || src[p] == '\r' || src.compare(p, 2, "%%") == 0) {
            return p;
        }
        return -1;
    };
    return t;
}

inline const std::vector<TokenType>& treeViewTokens() noexcept {
    static const std::vector<TokenType> types = []() {
        TokenType kw = common_tokens::keyword("treeView-beta");
        kw.longer = {T::bareName};

        TokenType classAnn;
        classAnn.name = "CLASS_ANNOTATION";
        classAnn.isRegex = true;
        classAnn.first = " \t";
        classAnn.match = [](const std::string& src, int at, int) -> int {
            int p = common_tokens::matchTabsSpaces(src, at);
            if (p == at || !startsWith(src, ":::", static_cast<size_t>(p))) return -1;
            p = common_tokens::matchTabsSpaces(src, p + 3);
            int c = charCodeAt(src, p);
            if (!isLetter(c) && c != '_') return -1;
            p++;
            int n = static_cast<int>(src.size());
            while (p < n && (isWord(charCodeAt(src, p)) || src[p] == '-')) p++;
            return p;
        };

        TokenType iconAnn;
        iconAnn.name = "ICON_ANNOTATION";
        iconAnn.isRegex = true;
        iconAnn.first = " \t";
        iconAnn.match = [](const std::string& src, int at, int) -> int {
            int p = common_tokens::matchTabsSpaces(src, at);
            if (p == at || !startsWith(src, "icon(", static_cast<size_t>(p))) return -1;
            p += 5;
            int n = static_cast<int>(src.size());
            while (p < n && (isWord(charCodeAt(src, p)) || src[p] == '-')) p++;
            if (p < n && src[p] == ':') {
                p++;
                int s = p;
                while (p < n && (isWord(charCodeAt(src, p)) || src[p] == '-')) p++;
                if (p == s) return -1;
            }
            if (p < n && src[p] == ')') return p + 1;
            return -1;
        };

        TokenType descAnn;
        descAnn.name = "DESC_ANNOTATION";
        descAnn.isRegex = true;
        descAnn.first = " \t";
        descAnn.match = [](const std::string& src, int at, int) -> int {
            int p = common_tokens::matchTabsSpaces(src, at);
            if (p == at || !startsWith(src, "##", static_cast<size_t>(p))) return -1;
            p += 2;
            int n = static_cast<int>(src.size());
            while (p < n && src[p] != '\n' && src[p] != '\r') p++;
            return p;
        };

        TokenType indentTok;
        indentTok.name = "INDENTATION";
        indentTok.isRegex = true;
        indentTok.first = " \t";
        indentTok.match = [](const std::string& src, int at, int) -> int {
            int p = common_tokens::matchTabsSpaces(src, at);
            return p > at ? p : -1;
        };

        TokenType quotedTok;
        quotedTok.name = "QUOTED_NAME";
        quotedTok.isRegex = true;
        quotedTok.first = "\"'";
        quotedTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            if (at >= n) return -1;
            char q = src[at];
            if (q != '"' && q != '\'') return -1;
            size_t end = src.find(q, static_cast<size_t>(at + 1));
            return end == std::string::npos ? -1 : static_cast<int>(end + 1);
        };

        TokenType wsTok = common_tokens::makeWhitespace();
        wsTok.name = "WS";

        TokenType commentTok;
        commentTok.name = "ML_COMMENT";
        commentTok.isRegex = true;
        commentTok.hidden = true;
        commentTok.first = "%";
        commentTok.match = [](const std::string& src, int at, int) -> int {
            if (src.compare(at, 2, "%%") != 0) return -1;
            int p = at + 2;
            int n = static_cast<int>(src.size());
            while (p < n && src[p] != '\n') p++;
            return p;
        };

        TokenType nlTok = common_tokens::makeNewline();
        nlTok.name = "NL";
        nlTok.hidden = true;

        TokenType bareTok;
        bareTok.name = "BARE_NAME";
        bareTok.isRegex = true;
        bareTok.match = matchBareName;

        return std::vector<TokenType>{
            kw,
            common_tokens::makeAccDescr(),
            common_tokens::makeAccTitle(),
            makeTreeViewTitle(),
            classAnn,
            iconAnn,
            descAnn,
            indentTok,
            quotedTok,
            wsTok,
            commentTok,
            nlTok,
            bareTok
        };
    }();
    return types;
}

inline std::string trimBlanksEnd(const std::string& text) noexcept {
    size_t end = text.size();
    while (end > 0 && isBlank(static_cast<unsigned char>(text[end - 1]))) end--;
    return end == text.size() ? text : text.substr(0, end);
}

struct TreeNodeAst {
    std::optional<int> indent;
    std::string name;
    std::optional<std::string> classAnnotation;
    std::optional<std::string> iconAnnotation;
    std::optional<std::string> descAnnotation;
};

struct TreeViewAst {
    std::optional<std::string> title;
    std::optional<std::string> accTitle;
    std::optional<std::string> accDescr;
    std::vector<TreeNodeAst> nodes;
};

inline std::string fixSingle(const std::string& text) noexcept {
    std::string s = trimStr(text);
    std::string out;
    int spaces = 0;
    char firstSpace = ' ';
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\t' || c == ' ') {
            if (spaces == 0) firstSpace = c;
            spaces++;
        } else {
            if (spaces == 1) out.push_back(firstSpace);
            else if (spaces >= 2) out.push_back(' ');
            spaces = 0;
            out.push_back(c);
        }
    }
    return out;
}

inline std::string fixTitleValue(const std::string& text) noexcept {
    size_t pos = text.find("title");
    if (pos == std::string::npos) return "";
    return fixSingle(text.substr(pos + 5));
}

inline std::string fixAccTitleValue(const std::string& text) noexcept {
    size_t pos = text.find(':');
    if (pos == std::string::npos) return "";
    return fixSingle(text.substr(pos + 1));
}

inline std::string fixAccDescrValue(const std::string& text) noexcept {
    size_t brace = text.find('{');
    size_t colon = text.find(':');
    if (colon != std::string::npos && (brace == std::string::npos || colon < brace)) {
        return fixSingle(text.substr(colon + 1));
    }
    if (brace != std::string::npos) {
        size_t close = text.find('}', brace + 1);
        std::string body = text.substr(brace + 1, close == std::string::npos ? std::string::npos : close - brace - 1);
        return cleanAccDescr(body);
    }
    return "";
}

inline TreeViewAst parseTreeView(const std::string& src) {
    const auto& types = treeViewTokens();
    Tokens toks = tokenize(src, types, "treeView");
    TokenReader r(toks, types, "treeView");
    TreeViewAst ast;
    r.expect(T::keyword);
    for (;;) {
        if (r.kind() == T::accDescr) ast.accDescr = fixAccDescrValue(r.take());
        else if (r.kind() == T::accTitle) ast.accTitle = fixAccTitleValue(r.take());
        else if (r.kind() == T::title) ast.title = fixTitleValue(r.take());
        else break;
    }
    while (r.kind() != -1) {
        TreeNodeAst node;
        if (r.kind() == T::indentation) node.indent = static_cast<int>(r.take().size());
        if (r.kind() == T::quotedName) {
            std::string text = r.take();
            node.name = text.size() >= 2 ? text.substr(1, text.size() - 2) : "";
        } else if (r.kind() == T::bareName) {
            node.name = trimBlanksEnd(r.take());
        } else {
            r.fail("a name");
        }
        for (;;) {
            if (r.kind() == T::classAnnotation) {
                std::string t = trimStr(r.take());
                node.classAnnotation = trimStr(t.substr(3));
            } else if (r.kind() == T::iconAnnotation) {
                std::string t = trimStr(r.take());
                node.iconAnnotation = t.substr(5, t.size() - 6);
            } else if (r.kind() == T::descAnnotation) {
                std::string t = trimStr(r.take());
                node.descAnnotation = trimStr(t.substr(2));
            } else {
                break;
            }
        }
        ast.nodes.push_back(std::move(node));
    }
    return ast;
}

// ============================================================================
// src/diagrams/treeview/model.ts & icons.ts
// ============================================================================

struct TreeNode {
    int id = 0;
    int level = 0;
    std::string name;
    std::string nodeType = "file"; // "file" | "directory"
    std::optional<std::string> icon;
    std::optional<std::string> cssClass;
    std::optional<std::string> description;
    std::vector<std::shared_ptr<TreeNode>> children;
};

struct TreeViewModel : public DiagramMeta {
    std::shared_ptr<TreeNode> root;
    int count = 1;
};

inline TreeViewModel buildTreeView(const TreeViewAst& ast, const std::string& fallbackTitle) noexcept {
    TreeViewModel model;
    model.root = std::make_shared<TreeNode>();
    model.root->id = 0;
    model.root->level = -1;
    model.root->name = "/";
    model.root->nodeType = "directory";
    model.count = 1;

    std::vector<std::shared_ptr<TreeNode>> stack = {model.root};
    for (const auto& n : ast.nodes) {
        int level = n.indent.value_or(0);
        std::string name = n.name;
        bool isDirectory = endsWith(name, "/");
        if (isDirectory) name.pop_back();

        while (level <= stack.back()->level) stack.pop_back();
        auto node = std::make_shared<TreeNode>();
        node->id = model.count++;
        node->level = level;
        node->name = name;
        node->nodeType = isDirectory ? "directory" : "file";
        if (n.classAnnotation.has_value() && !n.classAnnotation->empty()) node->cssClass = *n.classAnnotation;
        if (n.iconAnnotation.has_value()) node->icon = n.iconAnnotation->empty() ? "none" : *n.iconAnnotation;
        if (n.descAnnotation.has_value() && !n.descAnnotation->empty()) node->description = *n.descAnnotation;
        stack.back()->children.push_back(node);
        stack.push_back(node);
    }

    model.title = (ast.title.has_value() && !ast.title->empty()) ? *ast.title : fallbackTitle;
    if (ast.accTitle.has_value() && !ast.accTitle->empty()) {
        model.accTitle = trimStartStr(*ast.accTitle);
    }
    if (ast.accDescr.has_value() && !ast.accDescr->empty()) {
        model.accDescr = *ast.accDescr;
    }
    return model;
}

inline const std::string BUILTIN_PACK = "mermaid-treeview";

inline std::string builtinGlyph(const std::string& name) noexcept {
    if (name == "folder") return "M3 6.5A1.5 1.5 0 0 1 4.5 5h4.6l2.4 2.5h8A1.5 1.5 0 0 1 21 9v9a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18z";
    if (name == "file") return "M6.5 3h7L19 8.5v11a1.5 1.5 0 0 1-1.5 1.5h-11A1.5 1.5 0 0 1 5 19.5v-15A1.5 1.5 0 0 1 6.5 3zM13.5 3v5.5H19";
    return "";
}

struct IconConfig {
    bool showIcons = false;
    std::string defaultIconPack;
    const Config* filenameIcons = nullptr;
    const Config* extensionIcons = nullptr;
};

inline std::optional<std::string> ownIconMap(const Config* m, const std::string& key) noexcept {
    if (!m || !m->isMap()) return std::nullopt;
    const auto* v = m->get(key);
    if (v && v->isString()) return v->sVal;
    return std::nullopt;
}

inline std::optional<std::string> detectIcon(const std::string& name, const IconConfig& cfg) noexcept {
    auto byName = ownIconMap(cfg.filenameIcons, name);
    if (byName.has_value() && !byName->empty()) return byName;
    size_t dot = name.rfind('.');
    if (dot != std::string::npos && dot > 0) {
        std::string ext = toLowerAscii(name.substr(dot));
        auto byExt = ownIconMap(cfg.extensionIcons, ext);
        if (byExt.has_value()) return byExt;
        return ownIconMap(cfg.extensionIcons, ext.substr(1));
    }
    return std::nullopt;
}

inline std::string qualifyIcon(const std::string& icon, const std::string& defaultIconPack) noexcept {
    if (icon.find(':') != std::string::npos) return icon;
    if (!builtinGlyph(icon).empty() || defaultIconPack.empty()) return Pele::cat({BUILTIN_PACK, ":", icon});
    return Pele::cat({defaultIconPack, ":", icon});
}

inline std::optional<std::string> getNodeIcon(const TreeNode& node, const IconConfig& config) noexcept {
    if (node.icon.has_value()) {
        if (*node.icon == "none") return std::nullopt;
        if (!node.icon->empty()) return qualifyIcon(*node.icon, config.defaultIconPack);
    }
    if (!config.showIcons) return std::nullopt;
    if (node.nodeType == "file") {
        auto detected = detectIcon(node.name, config);
        if (detected.has_value()) {
            if (*detected == "none") return std::nullopt;
            if (!detected->empty()) return qualifyIcon(*detected, config.defaultIconPack);
        }
    }
    return Pele::cat({BUILTIN_PACK, ":", (node.nodeType == "directory" ? "folder" : "file")});
}

inline IconConfig getIconConfig(const Config& config) noexcept {
    IconConfig out;
    const auto* c = config.get("treeView");
    if (!c || !c->isMap()) return out;
    if (const auto* si = c->get("showIcons")) {
        if (si->isBool()) out.showIcons = si->bVal;
        else if (si->isNumber()) out.showIcons = (si->nVal != 0.0);
        else if (si->isString()) out.showIcons = !si->sVal.empty();
    }
    if (const auto* dip = c->get("defaultIconPack"); dip && dip->isString()) {
        out.defaultIconPack = dip->sVal;
    }
    if (const auto* fi = c->get("filenameIcons"); fi && fi->isMap()) {
        out.filenameIcons = fi;
    }
    if (const auto* ei = c->get("extensionIcons"); ei && ei->isMap()) {
        out.extensionIcons = ei;
    }
    return out;
}

// ============================================================================
// src/diagrams/treeview/render.ts
// ============================================================================

constexpr double ICON_GAP = 6.0;
constexpr double DESC_GAP = 16.0;
constexpr double GUIDE_INSET = 7.0;
constexpr double ELBOW = 4.0;
constexpr double HIGHLIGHT_OVERHANG = 4.0;

inline double treeSetting(const Config& config, const std::string& key, double fallback, double minVal, double maxVal) noexcept {
    const auto* tv = config.get("treeView");
    if (!tv || !tv->isMap()) return fallback;
    const auto* v = tv->get(key);
    if (!v || !v->isNumber()) return fallback;
    return (v->nVal >= minVal && v->nVal <= maxVal) ? v->nVal : fallback;
}

struct Row {
    const TreeNode* node = nullptr;
    double x = 0.0;
    double cy = 0.0;
    std::optional<std::string> icon;
    std::string text;
    double labelX = 0.0;
    bool highlighted = false;
    std::optional<Label> label;
    std::optional<Label> description;
    double top = 0.0;
    double height = 0.0;
};

inline double treeViewTitleRoom(const RenderOptions& options) noexcept {
    double available = options.maxWidth;
    double pad = options.hasPadding ? options.padding : 8.0;
    return available > 0.0 ? std::max(80.0, available - 2.0 * pad) : 4000.0;
}

inline bool hasHighlightClass(const std::optional<std::string>& cssClass) noexcept {
    if (!cssClass.has_value()) return false;
    std::istringstream iss(*cssClass);
    std::string tok;
    while (iss >> tok) {
        if (tok == "highlight") return true;
    }
    return false;
}

inline Rendered drawTreeView(const TreeViewModel& model, const Config& config, const RenderOptions& options, bool tight) noexcept {
    double size = options.hasFontSize ? options.fontSize : 16.0;
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.hasPadding ? options.padding : 8.0;
    IconConfig icons = getIconConfig(config);
    bool mono = false;
    std::string family = mono ? Pele::cat({" font-family=\"", FONT_MONO, "\""}) : "";

    double paddingX = treeSetting(config, "paddingX", 5.0, 0.0, 100.0);
    double paddingY = treeSetting(config, "paddingY", 2.0, 0.0, 100.0);
    double rowIndent = treeSetting(config, "rowIndent", 15.0, 0.0, 400.0);
    double lineThickness = treeSetting(config, "lineThickness", 1.0, 0.25, 12.0);

    int maxDepth = 0;
    std::vector<std::pair<const TreeNode*, int>> depths;
    for (const auto& ch : model.root->children) depths.push_back({ch.get(), 0});
    for (size_t i = 0; tight && i < depths.size(); i++) {
        auto [node, depth] = depths[i];
        maxDepth = std::max(maxDepth, depth);
        for (const auto& child : node->children) depths.push_back({child.get(), depth + 1});
    }
    double maxW = options.maxWidth > 0.0 ? options.maxWidth : 320.0;
    double step = tight ? std::min(rowIndent + paddingX, std::max(2.0, (maxW - 120.0) / std::max(1, maxDepth))) : (rowIndent + paddingX);
    double rowHeight = std::round(size * 1.5) + 2.0 * paddingY;
    double iconSize = size;

    Label title = layoutLabel(model.title, false, measurer, size, tight ? treeViewTitleRoom(options) : 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + 12.0 : 0.0;

    auto hasIcon = [&](const std::vector<std::shared_ptr<TreeNode>>& list) -> bool {
        for (const auto& ch : list) {
            if (getNodeIcon(*ch, icons).has_value()) return true;
        }
        return false;
    };

    std::vector<Row> rows;
    std::unordered_map<const TreeNode*, size_t> rowOf;
    struct PendingItem {
        const TreeNode* node;
        int depth;
        bool slot;
    };
    std::vector<PendingItem> pending;
    auto pushList = [&](const std::vector<std::shared_ptr<TreeNode>>& list, int depth) {
        bool slot = hasIcon(list);
        for (int i = static_cast<int>(list.size()) - 1; i >= 0; i--) {
            pending.push_back({list[static_cast<size_t>(i)].get(), depth, slot});
        }
    };
    pushList(model.root->children, 0);

    double labelRight = 0.0;
    bool anyHighlight = false;
    double rowsHeight = 0.0;
    while (!pending.empty()) {
        auto [node, depth, slot] = pending.back();
        pending.pop_back();
        double x = static_cast<double>(depth) * step;
        std::string text = decodeEntities(node->name);
        double labelX = x + (slot ? (iconSize + ICON_GAP) : 0.0);
        double room = std::max(40.0, maxW - 2.0 * pad - 2.0 * HIGHLIGHT_OVERHANG - labelX);
        int style = (node->nodeType == "directory" ? Style::Bold : 0) | (mono ? Style::Mono : 0);
        std::optional<Label> label = tight ? std::optional<Label>(literalLabel(text, measurer, size, room, style)) : std::nullopt;
        std::optional<Label> description = (tight && node->description.has_value())
            ? std::optional<Label>(literalLabel(decodeEntities(*node->description), measurer, size, room, Style::Italic))
            : std::nullopt;
        double width = label.has_value() ? label->width : measurer.width(text, size, style);
        if (labelX + width > labelRight) labelRight = labelX + width;
        bool highlighted = hasHighlightClass(node->cssClass);
        if (highlighted) anyHighlight = true;
        double lHeight = label.has_value() ? label->height : 0.0;
        double dHeight = description.has_value() ? description->height : 0.0;
        double height = tight ? std::max(rowHeight, lHeight + dHeight + 2.0 * paddingY) : rowHeight;
        double cy = rowsHeight + (tight ? (paddingY + std::max(iconSize, lHeight) / 2.0) : (height / 2.0));
        Row row{node, x, cy, getNodeIcon(*node, icons), text, labelX, highlighted, std::move(label), std::move(description), rowsHeight, height};
        rowsHeight += height;
        rowOf[node] = rows.size();
        rows.push_back(std::move(row));
        pushList(node->children, depth + 1);
    }

    double descX = labelRight + DESC_GAP;
    double width = labelRight;
    for (const auto& row : rows) {
        if (!row.node->description.has_value()) continue;
        double w = row.description.has_value() ? row.description->width : measurer.width(decodeEntities(*row.node->description), size, Style::Italic);
        width = std::max(width, (tight ? row.labelX : descX) + w);
    }
    width = std::max(width, title.width);
    double inset = pad + (anyHighlight ? HIGHLIGHT_OVERHANG : 0.0);
    double top = pad + titleHeight;
    double totalWidth = std::ceil(width + 2.0 * inset);
    double totalHeight = std::ceil(top + rowsHeight + pad);

    std::string guides;
    for (const auto& row : rows) {
        const auto& children = row.node->children;
        if (children.empty()) continue;
        double gx = inset + row.x + GUIDE_INSET;
        double tick = std::max(gx, inset + row.x + step - paddingX);
        double last = top + rows[rowOf[children.back().get()]].cy;
        double r = std::min(ELBOW, tick - gx);
        std::string d = Pele::cat({"M", num(gx), ",", num(top + row.top + row.height - 2.0), "V", num(last - r)});
        if (r > 0.0) Pele::appendCat(d, {"Q", num(gx), ",", num(last), " ", num(gx + r), ",", num(last), "H", num(tick)});
        for (size_t i = 0; i + 1 < children.size() && tick > gx; i++) {
            Pele::appendCat(d, {"M", num(gx), ",", num(top + rows[rowOf[children[i].get()]].cy), "H", num(tick)});
        }
        Pele::appendCat(guides, {"<path d=\"", d, "\"/>"});
    }

    double baseline = size * 0.35;
    std::string body;
    for (const auto& row : rows) {
        const TreeNode& node = *row.node;
        double cy = top + row.cy;
        bool directory = (node.nodeType == "directory");
        std::string inner;
        if (row.highlighted) {
            double x = inset + row.x - HIGHLIGHT_OVERHANG;
            Pele::appendCat(inner, {"<rect class=\"pele-tree-highlight\" x=\"", num(x), "\" y=\"", num(top + row.top + 1.0), "\" width=\"", num(totalWidth - inset + HIGHLIGHT_OVERHANG - x), "\" height=\"", num(row.height - 2.0), "\" fill=\"var(--_c)\" fill-opacity=\"0.14\"", ROUNDED, "/>"});
        }
        if (row.icon.has_value()) {
            std::string name = startsWith(*row.icon, Pele::cat({BUILTIN_PACK, ":"})) ? row.icon->substr(BUILTIN_PACK.size() + 1) : "";
            std::string glyph = builtinGlyph(name);
            if (glyph.empty()) glyph = builtinGlyph(directory ? "folder" : "file");
            std::string drawn = options.icons ? options.icons(*row.icon) : "";
            if (drawn.empty()) drawn = "<path d=\"" + glyph + "\" stroke=\"var(--_m)\" stroke-width=\"1.5\"/>";
            Pele::appendCat(inner, {"<svg class=\"pele-icon\" data-icon=\"", escText(*row.icon), "\" x=\"", num(inset + row.x), "\" y=\"", num(cy - iconSize / 2.0), "\" width=\"", num(iconSize), "\" height=\"", num(iconSize), "\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">", drawn, "</svg>"});
        }
        if (row.label.has_value()) {
            inner += leftLabelSvg(*row.label, inset + row.labelX, top + row.top + paddingY, " class=\"pele-label\"" + std::string(directory ? " font-weight=\"var(--_hw)\"" : "") + family);
        } else if (!row.text.empty()) {
            Pele::appendCat(inner, {"<text class=\"pele-label\" x=\"", num(inset + row.labelX), "\" y=\"", num(cy + baseline), "\"", (directory ? " font-weight=\"var(--_hw)\"" : ""), family, " xml:space=\"preserve\">", esc(row.text), "</text>"});
        }
        if (row.description.has_value()) {
            double lHeight = row.label.has_value() ? row.label->height : 0.0;
            inner += leftLabelSvg(*row.description, inset + row.labelX, top + row.top + paddingY + lHeight, " class=\"pele-tree-description\" font-style=\"italic\" fill=\"var(--_m)\"");
        } else if (node.description.has_value()) {
            Pele::appendCat(inner, {"<text class=\"pele-tree-description\" x=\"", num(inset + descX), "\" y=\"", num(cy + baseline), "\" font-style=\"italic\" fill=\"var(--_m)\" xml:space=\"preserve\">", esc(decodeEntities(*node.description)), "</text>"});
        }
        body += "<g class=\"pele-node pele-tree-" + std::string(directory ? "dir" : "file") + classNames(node.cssClass.value_or("")) + "\" data-id=\"" + escText(node.name) + "\">" + inner + "</g>";
    }

    std::string content =
        labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
        (!guides.empty() ? Pele::cat({"<g class=\"pele-tree-guides\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"", num(lineThickness), "\">", guides, "</g>"}) : "") +
        body;

    return {
        svgDocument("treeView", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered renderTreeViewModel(const TreeViewModel& model, const Config& config, const RenderOptions& options) noexcept {
    return tighten(options, [&](bool tight) {
        return drawTreeView(model, config, options, tight);
    });
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    std::string preprocessed = preprocessBoxDrawing(source);
    TreeViewModel model = buildTreeView(parseTreeView(preprocessed), title);
    return renderTreeViewModel(model, config, options);
}

} // namespace TreeViewDiagram

inline Rendered renderTreeView(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return TreeViewDiagram::render(source, config, title, options);
}

} // namespace Pele
