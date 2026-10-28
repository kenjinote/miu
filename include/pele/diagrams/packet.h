#pragma once
#include "../foundation.h"

namespace Pele {
namespace PacketDiagram {

struct PacketBlockAst {
    std::optional<int> start;
    std::optional<int> end;
    std::optional<int> bits;
    std::string label;
};

struct PacketAst {
    std::string title;
    std::string accTitle;
    std::string accDescr;
    std::vector<PacketBlockAst> blocks;
};

struct PacketBlock {
    int start = 0;
    int end = 0;
    int bits = 0;
    std::string label;
};

struct PacketModel : public DiagramMeta {
    int bitsPerRow = 32;
    std::vector<std::vector<PacketBlock>> rows;
};

namespace T {
    enum Type {
        packetBeta = 0,
        packet = 1,
        dash = 2,
        plus = 3,
        colon = 4,
        accDescr = 5,
        accTitle = 6,
        title = 7,
        intTok = 8,
        string = 9,
        newline = 10,
        whitespace = 11,
        yaml = 12,
        directive = 13,
        singleLineComment = 14
    };
}

inline const std::vector<TokenType>& packetTokens() noexcept {
    static const std::vector<TokenType> tokens = []() {
        TokenType pb;
        pb.name = "packet-beta";
        pb.literal = "packet-beta";

        TokenType dashTok;
        dashTok.name = "-";
        dashTok.literal = "-";
        dashTok.longer = {T::yaml};

        TokenType plusTok;
        plusTok.name = "+";
        plusTok.literal = "+";

        TokenType colonTok;
        colonTok.name = ":";
        colonTok.literal = ":";

        TokenType intTok;
        intTok.name = "INT";
        intTok.isRegex = true;
        intTok.first = "0123456789";
        intTok.match = [](const std::string& src, int at, int) -> int {
            int c = charCodeAt(src, at);
            if (c == 48) return at + 1;
            if (c >= 49 && c <= 57) {
                int end = at + 1;
                while (isDigit(charCodeAt(src, end))) end++;
                if (charCodeAt(src, end) != 46) return end;
                return end - 1 > at ? end - 1 : -1;
            }
            return -1;
        };

        return std::vector<TokenType>{
            pb,
            common_tokens::keyword("packet"),
            dashTok,
            plusTok,
            colonTok,
            common_tokens::makeAccDescr(),
            common_tokens::makeAccTitle(),
            common_tokens::makeTitle(),
            intTok,
            common_tokens::makeString(),
            common_tokens::makeNewline(),
            common_tokens::makeWhitespace(),
            common_tokens::makeYaml(),
            common_tokens::makeDirective(),
            common_tokens::makeSingleLineComment()
        };
    }();
    return tokens;
}

inline PacketAst parsePacket(const std::string& src) {
    auto safeSingle = [](const std::string& text) {
        std::string s = trimStr(text);
        std::string out;
        int spaces = 0;
        for (size_t idx = 0; idx < s.size(); idx++) {
            char c = s[idx];
            if (c == '\t' || c == ' ') {
                spaces++;
            } else {
                if (spaces == 1) out.push_back(s[idx - 1]);
                else if (spaces >= 2) out.push_back(' ');
                spaces = 0;
                out.push_back(c);
            }
        }
        return out;
    };
    auto safeTitleValue = [&](const std::string& text) -> std::string {
        size_t pos = text.find("title");
        if (pos == std::string::npos) return "";
        return safeSingle(text.substr(pos + 5));
    };
    auto safeAccTitleValue = [&](const std::string& text) -> std::string {
        size_t pos = text.find(':');
        if (pos == std::string::npos) return "";
        return safeSingle(text.substr(pos + 1));
    };
    auto safeAccDescrValue = [&](const std::string& text) -> std::string {
        size_t brace = text.find('{');
        size_t colon = text.find(':');
        if (colon != std::string::npos && (brace == std::string::npos || colon < brace)) {
            return safeSingle(text.substr(colon + 1));
        }
        if (brace != std::string::npos) {
            size_t close = text.find('}', brace + 1);
            std::string body = text.substr(brace + 1, close == std::string::npos ? std::string::npos : close - brace - 1);
            return cleanAccDescr(body);
        }
        return "";
    };

    const auto& tokTypes = packetTokens();
    Tokens toks = tokenize(src, tokTypes, "packet");
    TokenReader r(toks, tokTypes, "packet");
    PacketAst ast;

    auto endOfLine = [&]() {
        if (r.kind() == -1) return;
        if (r.kind() != T::newline) r.fail("a line break or the end of input");
        while (r.kind() == T::newline) r.i++;
    };

    while (r.kind() == T::newline) r.i++;
    if (!r.accept(T::packetBeta)) r.expect(T::packet);
    while (r.kind() != -1) {
        switch (r.kind()) {
            case T::newline:
                r.i++;
                break;
            case T::accDescr:
                ast.accDescr = safeAccDescrValue(r.take());
                endOfLine();
                break;
            case T::accTitle:
                ast.accTitle = safeAccTitleValue(r.take());
                endOfLine();
                break;
            case T::title:
                ast.title = safeTitleValue(r.take());
                endOfLine();
                break;
            case T::intTok:
            case T::plus: {
                PacketBlockAst block;
                if (r.accept(T::plus)) {
                    block.bits = std::stoi(r.expect(T::intTok));
                } else {
                    block.start = std::stoi(r.take());
                    if (r.accept(T::dash)) block.end = std::stoi(r.expect(T::intTok));
                }
                r.expect(T::colon);
                block.label = common_tokens::stringValue(r.expect(T::string));
                ast.blocks.push_back(std::move(block));
                endOfLine();
                break;
            }
            default:
                r.fail("a field, a title, or a line break");
        }
    }
    return ast;
}

constexpr size_t MAX_ROWS = 10000;

inline double packetOption(const Config& config, const std::string& key, double fallback, double min, double max) noexcept {
    const auto* pkt = config.get("packet");
    if (pkt && pkt->isMap()) {
        const auto* val = pkt->get(key);
        if (val && val->isNumber() && val->nVal >= min) {
            return std::min(val->nVal, max);
        }
    }
    return fallback;
}

inline std::string stripNewlineIndent(const std::string& s) noexcept {
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

inline PacketModel buildPacket(const PacketAst& ast, const Config& config, const std::string& title) {
    int bitsPerRow = static_cast<int>(std::floor(packetOption(config, "bitsPerRow", 32.0, 1.0, 4096.0)));
    std::vector<std::vector<PacketBlock>> rows;
    int lastBit = -1;
    std::vector<PacketBlock> word;
    int row = 1;

    for (const auto& block : ast.blocks) {
        if (block.start.has_value() && block.end.has_value() && *block.end < *block.start) {
            throw PeleError(
                "Packet block " + std::to_string(*block.start) + " - " + std::to_string(*block.end) + " is invalid. End must be greater than start.",
                "semantic",
                "packet"
            );
        }
        int start = block.start.value_or(lastBit + 1);
        if (start != lastBit + 1) {
            int endOrStart = block.end.value_or(start);
            throw PeleError(
                "Packet block " + std::to_string(start) + " - " + std::to_string(endOrStart) + " is not contiguous. It should start from " + std::to_string(lastBit + 1) + ".",
                "semantic",
                "packet"
            );
        }
        if (block.bits.has_value() && *block.bits == 0) {
            throw PeleError(
                "Packet block " + std::to_string(start) + " is invalid. Cannot have a zero bit field.",
                "semantic",
                "packet"
            );
        }
        int end = block.end.value_or(start + block.bits.value_or(1) - 1);
        int bits = block.bits.value_or(end - start + 1);
        lastBit = end;

        while (static_cast<int>(word.size()) <= bitsPerRow + 1 && rows.size() < MAX_ROWS) {
            int rowEnd = row * bitsPerRow - 1;
            if (end <= rowEnd) {
                word.push_back({start, end, bits, block.label});
                if (end == rowEnd) {
                    rows.push_back(std::move(word));
                    word.clear();
                    row++;
                }
                break;
            }
            word.push_back({start, rowEnd, rowEnd - start, block.label});
            rows.push_back(std::move(word));
            word.clear();
            row++;
            start = rowEnd + 1;
            bits = end - start;
        }
    }
    if (!word.empty()) rows.push_back(std::move(word));

    PacketModel model;
    model.title = !ast.title.empty() ? ast.title : title;
    model.accTitle = !ast.accTitle.empty() ? trimStartStr(ast.accTitle) : "";
    model.accDescr = !ast.accDescr.empty() ? stripNewlineIndent(ast.accDescr) : "";
    model.bitsPerRow = bitsPerRow;
    model.rows = std::move(rows);
    return model;
}

struct FittedText {
    std::string text;
    double size = 0.0;
    double width = 0.0;
    bool valid = false;
};

inline FittedText fitPacketText(const std::string& text, double max, double size, double min, const TextMeasurer& measurer, int style = 0) noexcept {
    if (text.empty() || max <= 0.0) return {};
    double width = measurer.width(text, size, style);
    if (width <= max) return {text, size, width, true};

    double fitted = std::max(min, std::floor((size * max) / width));
    while (fitted >= min) {
        width = measurer.width(text, fitted, style);
        if (width <= max) return {text, fitted, width, true};
        fitted -= 1.0;
    }

    std::vector<std::string> chars;
    size_t idx = 0;
    while (idx < text.size()) {
        size_t s = idx;
        nextCodepoint(text, idx);
        chars.push_back(text.substr(s, idx - s));
    }
    if (chars.empty()) return {};
    auto joinPrefix = [&](size_t len) {
        std::string res;
        for (size_t k = 0; k < len && k < chars.size(); k++) res += chars[k];
        return res;
    };
    const std::string ELLIPSIS = "\xE2\x80\xA6";
    int low = 0;
    int high = static_cast<int>(chars.size()) - 1;
    while (low < high) {
        int mid = (low + high + 1) >> 1;
        if (measurer.width(trimEndStr(joinPrefix(static_cast<size_t>(mid))) + ELLIPSIS, min, style) <= max) low = mid;
        else high = mid - 1;
    }
    if (low == 0) return {};
    std::string cut = trimEndStr(joinPrefix(static_cast<size_t>(low))) + ELLIPSIS;
    width = measurer.width(cut, min, style);
    if (width <= max) return {cut, min, width, true};
    return {};
}

inline double packetTitleRoom(const RenderOptions& options) noexcept {
    double available = options.maxWidth;
    return available > 0.0 ? std::max(80.0, available - 2.0 * options.padding) : 4000.0;
}

inline Rendered drawPacket(const PacketModel& model, const Config& config, const RenderOptions& options, bool down) noexcept {
    double size = options.fontSize;
    double labelSize = std::round(size * 0.875);
    double bitSize = std::round(size * 0.625);
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;

    const auto* pktCfg = config.get("packet");
    double rowHeight = packetOption(config, "rowHeight", 32.0, 1.0, 400.0);
    double bitWidth = packetOption(config, "bitWidth", 32.0, 1.0, 400.0);
    double paddingX = packetOption(config, "paddingX", 5.0, 0.0, 400.0);
    double paddingY = packetOption(config, "paddingY", 5.0, 0.0, 400.0);
    bool showBits = true;
    bool descending = false;
    if (pktCfg && pktCfg->isMap()) {
        if (const auto* sb = pktCfg->get("showBits"); sb && sb->isBool() && !sb->bVal) showBits = false;
        if (const auto* bo = pktCfg->get("bitOrder"); bo && bo->asString() == "descending") descending = true;
    }

    int bitsPerRow = model.bitsPerRow;
    std::vector<std::vector<PacketBlock>> rows = model.rows;
    if (down) {
        double maxW = options.maxWidth > 0.0 ? options.maxWidth : 320.0;
        double count = std::max(1.0, std::floor((maxW - 2.0 * pad) / std::min(bitWidth, 18.0)));
        bitsPerRow = std::min(bitsPerRow, std::max(8, static_cast<int>(std::pow(2.0, std::floor(std::log2(count))))));
        int last = (!model.rows.empty() && !model.rows.back().empty()) ? model.rows.back().back().end : 0;
        if (std::ceil(static_cast<double>(last + 1) / bitsPerRow) > 10000.0) bitsPerRow = model.bitsPerRow;
        bitWidth = std::min(bitWidth, std::max(18.0, (maxW - 2.0 * pad) / bitsPerRow));

        auto piece = [](const PacketBlock& block) { return block.bits == block.end - block.start; };
        std::vector<PacketBlock> whole;
        for (const auto& r : model.rows) {
            for (const auto& block : r) {
                if (!whole.empty()) {
                    auto& prev = whole.back();
                    if (piece(prev) && piece(block) && prev.label == block.label && block.start == prev.end + 1 && block.start % model.bitsPerRow == 0) {
                        prev.end = block.end;
                        prev.bits = prev.end - prev.start;
                        continue;
                    }
                }
                whole.push_back(block);
            }
        }
        rows.clear();
        for (const auto& block : whole) {
            for (int start = block.start; start <= block.end;) {
                int end = std::min(block.end, (start / bitsPerRow + 1) * bitsPerRow - 1);
                size_t index = static_cast<size_t>(start / bitsPerRow);
                if (rows.size() <= index) rows.resize(index + 1);
                PacketBlock b = block;
                b.start = start;
                b.end = end;
                rows[index].push_back(std::move(b));
                start = end + 1;
            }
        }
    }

    std::vector<std::vector<Label>> downLabels(rows.size());
    if (down) {
        for (size_t rIdx = 0; rIdx < rows.size(); rIdx++) {
            downLabels[rIdx].resize(rows[rIdx].size());
            for (size_t bIdx = 0; bIdx < rows[rIdx].size(); bIdx++) {
                const auto& block = rows[rIdx][bIdx];
                double room = std::max(1.0, static_cast<double>(block.end - block.start + 1) * bitWidth - std::min(paddingX, bitWidth - 1.0) - 6.0);
                downLabels[rIdx][bIdx] = literalLabel(trimStr(decodeEntities(block.label)), measurer, labelSize, room);
            }
        }
    }

    double numbers = showBits ? bitSize + 4.0 : 0.0;
    std::vector<double> rowHeights(rows.size(), rowHeight);
    if (down) {
        for (size_t rIdx = 0; rIdx < rows.size(); rIdx++) {
            double h = rowHeight;
            for (size_t bIdx = 0; bIdx < rows[rIdx].size(); bIdx++) {
                h = std::max(h, downLabels[rIdx][bIdx].height + 10.0);
            }
            rowHeights[rIdx] = h;
        }
    }
    std::vector<double> tops;
    tops.reserve(rowHeights.size());
    double rowsHeight = 0.0;
    for (double h : rowHeights) {
        tops.push_back(rowsHeight);
        rowsHeight += numbers + h + paddingY;
    }
    if (!rows.empty()) rowsHeight -= paddingY;

    double gap = std::min(paddingX, bitWidth - 1.0);
    double width = !rows.empty() ? static_cast<double>(bitsPerRow) * bitWidth - gap : 0.0;
    Label title = layoutLabel(model.title, false, measurer, size, down ? packetTitleRoom(options) : 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + (rowsHeight > 0.0 ? 12.0 : 0.0) : 0.0;

    std::unordered_map<std::string, std::map<double, FittedText>> fits;
    auto fit = [&](const std::string& label, double max) -> FittedText {
        auto& byWidth = fits[label];
        auto it = byWidth.find(max);
        if (it == byWidth.end()) {
            FittedText ft = fitPacketText(trimStr(decodeEntities(label)), max, labelSize, std::round(size * 0.5), measurer);
            byWidth[max] = ft;
            return ft;
        }
        return it->second;
    };

    double left = pad + std::max(0.0, (title.width - width) / 2.0);
    std::string fields;
    std::string bits;
    for (size_t index = 0; index < rows.size(); index++) {
        const auto& r = rows[index];
        double y = pad + tops[index] + numbers;
        double rh = rowHeights[index];
        for (size_t bIdx = 0; bIdx < r.size(); bIdx++) {
            const auto& block = r[bIdx];
            int count = block.end - block.start + 1;
            int first = block.start % bitsPerRow;
            double x = left + static_cast<double>(descending ? bitsPerRow - first - count : first) * bitWidth;
            double w = static_cast<double>(count) * bitWidth - gap;
            FittedText text = down ? FittedText{} : fit(block.label, w - 6.0);
            fields +=
                "<g class=\"pele-node\" data-id=\"" + std::to_string(block.start) + "-" + std::to_string(block.end) + "\">" +
                "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(w) + "\" height=\"" + num(rh) + "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"" + ROUNDED + "/>" +
                (down
                    ? labelSvg(downLabels[index][bIdx], x + w / 2.0, y + rh / 2.0, "")
                    : (text.valid && text.size <= rh
                        ? Pele::cat({"<text x=\"", num(x + w / 2.0), "\" y=\"", num(y + rh / 2.0 + text.size * 0.35), "\"", (text.size == labelSize ? "" : Pele::cat({" font-size=\"", num(text.size), "\""})), ">", esc(text.text), "</text>"})
                        : "")) +
                "</g>";
            if (!showBits) continue;
            int leading = descending ? block.end : block.start;
            int trailing = descending ? block.start : block.end;
            if (count == 1) {
                bits += "<text x=\"" + num(x + w / 2.0) + "\" y=\"" + num(y - 4.0) + "\" text-anchor=\"middle\">" + std::to_string(leading) + "</text>";
            } else {
                bits +=
                    "<text x=\"" + num(x) + "\" y=\"" + num(y - 4.0) + "\">" + std::to_string(leading) + "</text>" +
                    "<text x=\"" + num(x + w) + "\" y=\"" + num(y - 4.0) + "\" text-anchor=\"end\">" + std::to_string(trailing) + "</text>";
            }
        }
    }

    double totalWidth = std::ceil(std::max(width, title.width) + 2.0 * pad);
    double totalHeight = std::ceil(rowsHeight + titleHeight + 2.0 * pad);
    std::string content =
        (!fields.empty() ? Pele::cat({"<g class=\"pele-packet-fields\" font-size=\"", num(labelSize), "\" text-anchor=\"middle\">", fields, "</g>"}) : "") +
        (!bits.empty() ? Pele::cat({"<g class=\"pele-packet-bits\" font-size=\"", num(bitSize), "\" fill=\"var(--_m)\">", bits, "</g>"}) : "") +
        labelSvg(title, totalWidth / 2.0, totalHeight - pad - title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"");

    return {
        svgDocument("packet", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered renderPacketModel(const PacketModel& model, const Config& config, const RenderOptions& options) noexcept {
    Rendered across = drawPacket(model, config, options, false);
    double available = options.maxWidth;
    if (!(available > 0.0) || across.width <= available || available >= 640.0) return across;
    Rendered down = drawPacket(model, config, options, true);
    return down.width < across.width ? down : across;
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    PacketModel model = buildPacket(parsePacket(source), config, title);
    return renderPacketModel(model, config, options);
}

} // namespace PacketDiagram

inline Rendered renderPacket(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return PacketDiagram::render(source, config, title, options);
}

inline Rendered renderPacket(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
    RenderOptions opts = options;
    if (!opts.hasResponsive) {
        const auto* section = pre.config.get("packet");
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
        auto res = PacketDiagram::render(parsed, pre.config, pre.title, opts);
        for (auto& link : res.links) link.id = decodeEntities(link.id);
        return res;
    });
}

} // namespace Pele
