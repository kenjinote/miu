#pragma once
#include "../foundation.h"

namespace Pele {
namespace InfoDiagram {

struct InfoAst {
    std::string title;
    std::string accTitle;
    std::string accDescr;
    bool hasAccTitle = false;
    bool hasAccDescr = false;
};

struct InfoModel : public DiagramMeta {
};

namespace T {
    enum Type {
        showInfo = 0,
        info = 1,
        accDescr = 2,
        accTitle = 3,
        title = 4,
        newline = 5,
        whitespace = 6,
        yaml = 7,
        directive = 8,
        singleLineComment = 9
    };
}

inline const std::vector<TokenType>& infoTokens() {
    static const std::vector<TokenType> tokens = {
        common_tokens::keyword("showInfo"),
        common_tokens::keyword("info"),
        common_tokens::makeAccDescr(),
        common_tokens::makeAccTitle(),
        common_tokens::makeTitle(),
        common_tokens::makeNewline(),
        common_tokens::makeWhitespace(),
        common_tokens::makeYaml(),
        common_tokens::makeDirective(),
        common_tokens::makeSingleLineComment()
    };
    return tokens;
}

inline InfoAst parseInfo(const std::string& src) {
    const auto& tokTypes = infoTokens();
    Tokens toks = tokenize(src, tokTypes, "info");
    TokenReader r(toks, tokTypes, "info");
    InfoAst ast;

    while (r.kind() == T::newline) r.i++;
    r.expect(T::info);
    while (r.kind() == T::newline) r.i++;
    if (r.accept(T::showInfo)) {
        while (r.kind() == T::newline) r.i++;
    }
    while (r.kind() != -1) {
        int kind = r.kind();
        if (kind == T::accDescr) {
            ast.accDescr = common_tokens::accDescrValue(r.take());
            ast.hasAccDescr = true;
        } else if (kind == T::accTitle) {
            ast.accTitle = common_tokens::accTitleValue(r.take());
            ast.hasAccTitle = true;
        } else if (kind == T::title) {
            ast.title = common_tokens::titleValue(r.take());
        } else {
            r.fail("a title or the end of input");
        }
        if (r.kind() == -1) break;
        if (r.kind() != T::newline) r.fail("a line break or the end of input");
        while (r.kind() == T::newline) r.i++;
    }
    return ast;
}

inline std::string stripNewlineIndent(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\n') {
            out.push_back('\n');
            i++;
            while (i < s.size()) {
                size_t next = i;
                int cp = charCodeAtUtf8(s, i, &next);
                if (!isSpace(cp)) break;
                i = next;
            }
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

inline InfoModel buildInfo(const InfoAst& ast, const std::string& title) {
    InfoModel model;
    model.title = !ast.title.empty() ? ast.title : title;
    model.accTitle = !ast.accTitle.empty() ? trimStartStr(ast.accTitle) : "";
    model.accDescr = !ast.accDescr.empty() ? stripNewlineIndent(ast.accDescr) : "";
    return model;
}

inline Rendered renderInfoModel(const InfoModel& model, const Config& /*config*/, const RenderOptions& options) {
    double size = options.fontSize;
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;

    Label title = layoutLabel(model.title, false, measurer, size, 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + 8.0 : 0.0;
    Label text = layoutLabel("Pele v1.12.0", false, measurer, size, 4000.0);

    double width = std::ceil(std::max(title.width, text.width) + 2.0 * pad);
    double height = std::ceil(titleHeight + text.height + 2.0 * pad);
    std::string content =
        labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
        labelSvg(text, width / 2.0, pad + titleHeight + text.height / 2.0, " class=\"pele-version\" fill=\"var(--_m)\"");

    return {svgDocument("info", width, height, size, options, model, content), width, height, {}};
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    InfoModel model = buildInfo(parseInfo(source), title);
    return renderInfoModel(model, config, options);
}

} // namespace InfoDiagram

inline Rendered renderInfo(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return InfoDiagram::render(source, config, title, options);
}

inline Rendered renderInfo(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = encodeEntities(pre.text) + "\n";
    RenderOptions opts = options;
    if (!opts.hasResponsive) {
        const auto* section = pre.config.get("info");
        bool fixed = (section && section->isMap() && section->get("useMaxWidth") && section->get("useMaxWidth")->isBool() && !section->get("useMaxWidth")->bVal);
        opts.responsive = !fixed;
        opts.hasResponsive = true;
    }
    IconResolver wrappedIcons;
    if (opts.icons) {
        auto orig = opts.icons;
        wrappedIcons = [orig](const std::string& name) { return orig(decodeEntities(name)); };
        opts.icons = wrappedIcons;
    }
    return withIcons(opts.icons, [&]() {
        auto res = InfoDiagram::render(parsed, pre.config, pre.title, opts);
        for (auto& link : res.links) link.id = decodeEntities(link.id);
        return res;
    });
}

} // namespace Pele
