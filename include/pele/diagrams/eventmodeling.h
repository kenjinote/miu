#pragma once
#include "../foundation.h"

namespace Pele {
namespace EventModelingDiagram {

// ============================================================================
// src/diagrams/eventmodeling/parser.ts
// ============================================================================

namespace T {
enum Kind {
    keyword = 0,
    resetframe = 1,
    readmodel = 2,
    processor = 3,
    timeframe = 4,
    command = 5,
    entity = 6,
    event = 7,
    jsobj = 8,
    figma = 9,
    given = 10,
    json = 11,
    salt = 12,
    html = 13,
    text = 14,
    data = 15,
    note = 16,
    when = 17,
    then = 18,
    rmo = 19,
    cmd = 20,
    evt = 21,
    pcr = 22,
    uri = 23,
    arrow = 24,
    gwt = 25,
    ui = 26,
    md = 27,
    tf = 28,
    open = 29,
    close = 30,
    rf = 31,
    tick = 32,
    dot = 33,
    id = 34,
    frameId = 35,
    inlineData = 36,
    blockData = 37,
    accDescr = 38,
    accTitle = 39,
    title = 40,
    ws = 41,
    yaml = 42,
    directive = 43,
    singleLineComment = 44,
    mlComment = 45,
    slComment = 46
};
} // namespace T

inline TokenType makeEmTitle() {
    TokenType t = common_tokens::makeTitle();
    t.name = "EM_TITLE";
    t.match = [](const std::string& src, int at, int) -> int {
        int p = common_tokens::matchTabsSpaces(src, at);
        if (src.compare(p, 5, "title") != 0) return -1;
        p += 5;
        if (p < static_cast<int>(src.size()) && (src[p] == '\t' || src[p] == ' ')) {
            return common_tokens::matchRestOfLineBeforeComment(src, p + 1);
        }
        if (p < static_cast<int>(src.size()) && isWord(charCodeAt(src, p))) return -1;
        return p;
    };
    return t;
}

inline const std::vector<TokenType>& eventModelingTokens() {
    static const std::vector<TokenType> types = []() {
        auto word = [](const std::string& text) {
            TokenType t;
            t.name = text;
            t.literal = text;
            t.isRegex = false;
            t.longer = {T::id};
            return t;
        };
        auto sign = [](const std::string& text) {
            TokenType t;
            t.name = text;
            t.literal = text;
            t.isRegex = false;
            return t;
        };

        TokenType kw = common_tokens::keyword("eventmodeling");
        kw.longer = {T::id};

        TokenType idTok;
        idTok.name = "EM_ID";
        idTok.isRegex = true;
        idTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            if (at >= n) return -1;
            int c = charCodeAt(src, at);
            if (!isLetter(c) && c != '_') return -1;
            int p = at + 1;
            while (p < n && isWord(charCodeAt(src, p))) p++;
            return p;
        };

        TokenType fidTok;
        fidTok.name = "EM_FID";
        fidTok.isRegex = true;
        fidTok.first = "0123456789";
        fidTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            int p = at;
            while (p < n && p - at < 3 && isDigit(charCodeAt(src, p))) p++;
            return p > at ? p : -1;
        };

        TokenType inlineTok;
        inlineTok.name = "EM_DATA_INLINE";
        inlineTok.isRegex = true;
        inlineTok.first = "{\"'" ;
        inlineTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            if (at >= n) return -1;
            char open = src[at];
            char close = (open == '{') ? '}' : (open == '"' || open == '\'') ? open : 0;
            if (!close) return -1;
            int p = at + 1;
            int lastClose = -1;
            while (p < n && src[p] != '\n' && src[p] != '\r') {
                if (src[p] == close) lastClose = p;
                p++;
            }
            return lastClose >= at + 1 ? (lastClose + 1) : -1;
        };

        TokenType blockTok;
        blockTok.name = "EM_DATA_BLOCK";
        blockTok.isRegex = true;
        blockTok.first = "{";
        blockTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            if (at >= n || src[at] != '{') return -1;
            int p = common_tokens::matchTabsSpaces(src, at + 1);
            if (p < n && src[p] == '\r') p++;
            if (p >= n || src[p] != '\n') return -1;
            p++;
            while (p < n) {
                if (src[p] == '}') {
                    int q = p + 1;
                    if (q < n && src[q] == '\r') q++;
                    if (q < n && src[q] == '\n') return q + 1;
                    if (q >= n || isSpace(charCodeAt(src, q))) return p + 1;
                }
                size_t nl = src.find('\n', static_cast<size_t>(p));
                if (nl == std::string::npos) return -1;
                p = static_cast<int>(nl + 1);
            }
            return -1;
        };

        TokenType accDescrTok = common_tokens::makeAccDescr();
        accDescrTok.name = "EM_ACC_DESCR";

        TokenType accTitleTok = common_tokens::makeAccTitle();
        accTitleTok.name = "EM_ACC_TITLE";

        TokenType wsTok;
        wsTok.name = "EM_WS";
        wsTok.isRegex = true;
        wsTok.hidden = true;
        wsTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            int p = at;
            while (p < n && isSpace(charCodeAt(src, p))) p++;
            return p > at ? p : -1;
        };

        TokenType yamlTok = common_tokens::makeYaml();
        yamlTok.name = "EM_YAML";

        TokenType dirTok = common_tokens::makeDirective();
        dirTok.name = "EM_DIRECTIVE";

        TokenType slcTok = common_tokens::makeSingleLineComment();
        slcTok.name = "EM_SINGLE_LINE_COMMENT";

        TokenType mlcTok;
        mlcTok.name = "EM_ML_COMMENT";
        mlcTok.isRegex = true;
        mlcTok.hidden = true;
        mlcTok.first = "/";
        mlcTok.match = [](const std::string& src, int at, int) -> int {
            if (!startsWith(src, "/*", static_cast<size_t>(at))) return -1;
            size_t close = src.find("*/", static_cast<size_t>(at + 2));
            return close == std::string::npos ? -1 : static_cast<int>(close + 2);
        };

        TokenType slSlashTok;
        slSlashTok.name = "EM_SL_COMMENT";
        slSlashTok.isRegex = true;
        slSlashTok.hidden = true;
        slSlashTok.first = "/";
        slSlashTok.match = [](const std::string& src, int at, int) -> int {
            if (!startsWith(src, "//", static_cast<size_t>(at))) return -1;
            int p = at + 2;
            int n = static_cast<int>(src.size());
            while (p < n && src[p] != '\n' && src[p] != '\r') p++;
            return p;
        };

        return std::vector<TokenType>{
            kw,
            word("resetframe"), word("readmodel"), word("processor"), word("timeframe"),
            word("command"), word("entity"), word("event"), word("jsobj"), word("figma"),
            word("given"), word("json"), word("salt"), word("html"), word("text"),
            word("data"), word("note"), word("when"), word("then"), word("rmo"),
            word("cmd"), word("evt"), word("pcr"), word("uri"),
            sign("->>"),
            word("gwt"),
            word("ui"),
            word("md"),
            word("tf"),
            sign("[["),
            sign("]]"),
            word("rf"),
            sign("`"),
            sign("."),
            idTok,
            fidTok,
            inlineTok,
            blockTok,
            accDescrTok,
            accTitleTok,
            makeEmTitle(),
            wsTok,
            yamlTok,
            dirTok,
            slcTok,
            mlcTok,
            slSlashTok
        };
    }();
    return types;
}

struct EmFrameAst {
    bool isReset = false;
    std::string name;
    std::string modelEntityType;
    std::string entityIdentifier;
    std::vector<std::string> sourceFrames;
    std::optional<std::string> dataReference;
    std::optional<std::string> dataType;
    std::optional<std::string> dataInlineValue;
    int offset = 0;
};

struct EmDataEntityAst {
    std::string name;
    std::optional<std::string> dataType;
    std::string dataBlockValue;
};

struct EmNoteEntityAst {
    std::string sourceFrame;
    std::optional<std::string> dataType;
    std::string dataBlockValue;
};

struct EmGwtAst {
    std::string sourceFrame;
    std::vector<std::string> givenStatements;
    std::vector<std::string> whenStatements;
    std::vector<std::string> thenStatements;
};

struct EventModelAst {
    std::optional<std::string> title;
    std::optional<std::string> accTitle;
    std::optional<std::string> accDescr;
    std::vector<std::string> modelEntities;
    std::vector<EmFrameAst> frames;
    std::vector<EmDataEntityAst> dataEntities;
    std::vector<EmNoteEntityAst> noteEntities;
    std::vector<EmGwtAst> gwtEntities;
    std::string source;
};

inline bool isTimeFrameKind(int k) {
    return k == T::tf || k == T::timeframe;
}

inline bool isResetFrameKind(int k) {
    return k == T::rf || k == T::resetframe;
}

inline bool isEntityTypeKind(int k) {
    return k == T::rmo || k == T::readmodel || k == T::ui || k == T::cmd ||
           k == T::command || k == T::evt || k == T::event || k == T::pcr || k == T::processor;
}

inline bool isDataTypeKind(int k) {
    return k == T::json || k == T::jsobj || k == T::figma || k == T::salt ||
           k == T::uri || k == T::md || k == T::html || k == T::text;
}

inline EventModelAst parseEventModel(const std::string& src) {
    const auto& types = eventModelingTokens();
    Tokens toks = tokenize(src, types, "eventmodeling");
    TokenReader r(toks, types, "eventmodeling");
    EventModelAst ast;
    ast.source = src;

    auto qualifiedName = [&]() -> std::string {
        std::string name = r.expect(T::id);
        while (r.accept(T::dot)) name += "." + r.expect(T::id);
        return name;
    };
    auto parseDataType = [&]() -> std::optional<std::string> {
        if (!r.accept(T::tick)) return std::nullopt;
        if (!isDataTypeKind(r.kind())) r.fail("a data type");
        std::string dt = r.take();
        r.expect(T::tick);
        return dt;
    };
    auto parseStatements = [&]() -> std::vector<std::string> {
        std::vector<std::string> out;
        do {
            if (!isEntityTypeKind(r.kind())) r.fail("an entity type");
            r.take();
            out.push_back(r.expect(T::id));
        } while (isEntityTypeKind(r.kind()));
        return out;
    };

    r.expect(T::keyword);
    while (r.kind() != -1) {
        int kind = r.kind();
        if (kind == T::accDescr) {
            ast.accDescr = r.take();
        } else if (kind == T::accTitle) {
            ast.accTitle = r.take();
        } else if (kind == T::title) {
            ast.title = r.take();
        } else if (kind == T::entity) {
            r.i++;
            ast.modelEntities.push_back(qualifiedName());
        } else if (isTimeFrameKind(kind) || isResetFrameKind(kind)) {
            EmFrameAst frame;
            frame.isReset = isResetFrameKind(kind);
            frame.offset = toks.starts[r.i];
            r.i++;
            frame.name = r.expect(T::frameId);
            if (!isEntityTypeKind(r.kind())) r.fail("an entity type");
            frame.modelEntityType = r.take();
            frame.entityIdentifier = qualifiedName();
            while (r.accept(T::arrow)) frame.sourceFrames.push_back(r.expect(T::frameId));
            if (r.accept(T::open)) {
                frame.dataReference = r.expect(T::id);
                r.expect(T::close);
            }
            if (r.kind() == T::tick || r.kind() == T::inlineData) {
                auto dt = parseDataType();
                if (dt.has_value()) frame.dataType = dt;
                frame.dataInlineValue = r.expect(T::inlineData);
            }
            ast.frames.push_back(std::move(frame));
        } else if (kind == T::data) {
            r.i++;
            EmDataEntityAst entity;
            entity.name = r.expect(T::id);
            entity.dataType = parseDataType();
            entity.dataBlockValue = r.expect(T::blockData);
            ast.dataEntities.push_back(std::move(entity));
        } else if (kind == T::note) {
            r.i++;
            EmNoteEntityAst note;
            note.sourceFrame = r.expect(T::frameId);
            note.dataType = parseDataType();
            note.dataBlockValue = r.expect(T::blockData);
            ast.noteEntities.push_back(std::move(note));
        } else if (kind == T::gwt) {
            r.i++;
            EmGwtAst gwt;
            gwt.sourceFrame = r.expect(T::frameId);
            r.expect(T::given);
            gwt.givenStatements = parseStatements();
            if (r.accept(T::when)) gwt.whenStatements = parseStatements();
            r.expect(T::then);
            gwt.thenStatements = parseStatements();
            ast.gwtEntities.push_back(std::move(gwt));
        } else {
            r.fail("a frame, an entity, a data block, a note, or a specification");
        }
    }
    return ast;
}

// ============================================================================
// src/diagrams/eventmodeling/model.ts
// ============================================================================

struct EmLane {
    int index = 0;
    std::string label;
    std::string group; // "trigger" | "state" | "stream"
    std::optional<std::string> ns;
};

struct EmBox {
    std::string id;
    std::string kind; // "ui" | "processor" | "command" | "readmodel" | "event"
    std::string entity;
    std::string name;
    std::optional<std::string> ns;
    bool reset = false;
    EmLane lane;
    std::optional<std::string> data;
    std::optional<std::string> dataType;
};

struct EmRelation {
    size_t sourceIdx = 0;
    size_t targetIdx = 0;
};

struct EmNote {
    std::string frame;
    std::string text;
    std::optional<std::string> dataType;
};

struct EmSpecification {
    std::string frame;
    std::vector<std::string> given;
    std::vector<std::string> when;
    std::vector<std::string> then;
};

struct EventModelingModel : public DiagramMeta {
    std::vector<EmBox> boxes;
    std::vector<EmLane> lanes;
    std::vector<EmRelation> relations;
    std::vector<EmNote> notes;
    std::vector<EmSpecification> specifications;
    std::vector<std::string> entities;
};

inline std::string mapEntityKind(const std::string& raw) {
    if (raw == "ui") return "ui";
    if (raw == "pcr" || raw == "processor") return "processor";
    if (raw == "rmo" || raw == "readmodel") return "readmodel";
    if (raw == "cmd" || raw == "command") return "command";
    return "event";
}

inline std::string groupOfKind(const std::string& kind) {
    if (kind == "ui" || kind == "processor") return "trigger";
    if (kind == "event") return "stream";
    return "state";
}

inline std::string blockText(const std::string& raw) {
    size_t open = raw.find('{');
    size_t close = raw.rfind('}');
    std::string inner;
    if (open != std::string::npos && close != std::string::npos && close > open) {
        inner = raw.substr(open + 1, close - open - 1);
    } else if (open == std::string::npos && raw.size() >= 2) {
        inner = raw.substr(1, raw.size() - 2);
    }
    inner = replaceAllStr(inner, "\r", "");
    auto lines = splitStr(inner, '\n');
    while (!lines.empty() && trimStr(lines.front()).empty()) lines.erase(lines.begin());
    while (!lines.empty() && trimStr(lines.back()).empty()) lines.pop_back();
    size_t indent = std::numeric_limits<size_t>::max();
    for (const auto& line : lines) {
        if (trimStr(line).empty()) continue;
        size_t lead = line.size() - trimStartStr(line).size();
        indent = std::min(indent, lead);
    }
    if (indent == std::numeric_limits<size_t>::max()) indent = 0;
    std::string out;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) out.push_back('\n');
        const auto& line = lines[i];
        out += trimEndStr(line.substr(std::min(indent, line.size())));
    }
    return out;
}

inline EventModelingModel buildEventModel(const EventModelAst& ast, const std::string& fallbackTitle) {
    std::unordered_set<std::string> ids;
    for (const auto& frame : ast.frames) {
        if (ids.count(frame.name)) {
            int line = 1;
            for (int k = 0; k < frame.offset && k < static_cast<int>(ast.source.size()); k++) {
                if (ast.source[k] == '\n') line++;
            }
            throw PeleError("Duplicate event modeling frame ID \"" + frame.name + "\" on line " + std::to_string(line), "semantic", "eventmodeling", line);
        }
        ids.insert(frame.name);
    }

    std::unordered_map<std::string, std::string> dataMap;
    for (const auto& entity : ast.dataEntities) {
        if (!dataMap.count(entity.name)) dataMap[entity.name] = entity.dataBlockValue;
    }

    std::unordered_map<std::string, EmLane> laneOf;
    std::vector<EmLane> laneList;
    std::unordered_map<std::string, int> nextIdx;

    auto getLane = [&](const std::string& kind, const std::optional<std::string>& ns) -> EmLane {
        std::string group = groupOfKind(kind);
        std::string key = ns.has_value() ? (group + " " + *ns) : group;
        auto it = laneOf.find(key);
        if (it != laneOf.end()) return it->second;

        int first = 0;
        std::string label;
        std::string prefix;
        if (group == "trigger") { first = 0; label = "UI/Automation"; prefix = "UI/A: "; }
        else if (group == "state") { first = 100; label = "Command/Read Model"; prefix = "C/RM: "; }
        else { first = 200; label = "Events"; prefix = "Stream: "; }

        int index = first;
        if (ns.has_value()) {
            auto nit = nextIdx.find(group);
            index = (nit != nextIdx.end()) ? nit->second : (first + 1);
            nextIdx[group] = index + 1;
        }
        EmLane found{index, ns.has_value() ? (prefix + *ns) : label, group, ns};
        laneOf[key] = found;
        laneList.push_back(found);
        return found;
    };

    EventModelingModel model;
    for (size_t index = 0; index < ast.frames.size(); index++) {
        const auto& frame = ast.frames[index];
        auto parts = splitStr(frame.entityIdentifier, '.');
        std::optional<std::string> ns = (parts.size() == 2) ? std::optional<std::string>(parts[0]) : std::nullopt;
        std::string kind = mapEntityKind(frame.modelEntityType);
        std::optional<std::string> text;
        if (frame.dataInlineValue.has_value()) text = blockText(*frame.dataInlineValue);
        if (frame.dataReference.has_value()) {
            auto dit = dataMap.find(*frame.dataReference);
            if (dit != dataMap.end()) text = blockText(dit->second);
        }
        EmBox box;
        box.id = frame.name;
        box.kind = kind;
        box.entity = frame.entityIdentifier;
        box.name = (parts.size() == 2) ? parts[1] : frame.entityIdentifier;
        box.ns = ns;
        box.reset = frame.isReset;
        box.lane = getLane(kind, ns);
        box.data = text;
        box.dataType = frame.dataType;
        model.boxes.push_back(std::move(box));

        if (!frame.sourceFrames.empty()) {
            std::unordered_set<std::string> wanted(frame.sourceFrames.begin(), frame.sourceFrames.end());
            for (size_t sIdx = 0; sIdx < model.boxes.size(); sIdx++) {
                if (wanted.count(model.boxes[sIdx].id)) {
                    model.relations.push_back({sIdx, index});
                }
            }
        } else if (index > 0 && !model.boxes[index].reset) {
            for (int i = static_cast<int>(index) - 1; i >= 0; i--) {
                if (model.boxes[static_cast<size_t>(i)].lane.index != model.boxes[index].lane.index) {
                    model.relations.push_back({static_cast<size_t>(i), index});
                    break;
                }
            }
        }
    }

    std::stable_sort(laneList.begin(), laneList.end(), [](const EmLane& a, const EmLane& b) {
        return a.index < b.index;
    });
    model.lanes = std::move(laneList);

    std::string parsedTitle = ast.title.has_value() ? common_tokens::titleValue(*ast.title) : "";
    model.title = !parsedTitle.empty() ? parsedTitle : fallbackTitle;
    if (ast.accTitle.has_value()) {
        model.accTitle = trimStartStr(common_tokens::accTitleValue(*ast.accTitle));
    }
    if (ast.accDescr.has_value()) {
        model.accDescr = common_tokens::accDescrValue(*ast.accDescr);
    }
    for (const auto& note : ast.noteEntities) {
        model.notes.push_back({note.sourceFrame, blockText(note.dataBlockValue), note.dataType});
    }
    for (const auto& gwt : ast.gwtEntities) {
        model.specifications.push_back({gwt.sourceFrame, gwt.givenStatements, gwt.whenStatements, gwt.thenStatements});
    }
    model.entities = ast.modelEntities;
    return model;
}

// ============================================================================
// src/diagrams/eventmodeling/render.ts
// ============================================================================

constexpr double BOX_PAD_X = 12.0;
constexpr double BOX_PAD_Y = 10.0;
constexpr double BOX_MIN_WIDTH = 96.0;
constexpr double BOX_GAP = 16.0;
constexpr double STAGGER = 80.0;
constexpr double LANE_PAD = 12.0;
constexpr double LANE_GAP = 8.0;
constexpr size_t DATA_COLUMNS = 48;
constexpr size_t NOTE_COLUMNS = 80;
constexpr double CARD_GAP = 8.0;
constexpr double RAIL_GAP = 8.0;
constexpr double CARD_MIN = 120.0;
constexpr double ARROW_ROOM = 20.0;

inline std::optional<int> entitySeries(const std::string& kind) {
    if (kind == "command") return 0;
    if (kind == "event") return 1;
    if (kind == "readmodel") return 2;
    if (kind == "processor") return 6;
    return std::nullopt;
}

inline std::vector<std::string> hardWrap(const std::string& text, size_t columns) {
    std::vector<std::string> out;
    for (const auto& line : splitStr(decodeEntities(text), '\n')) {
        std::vector<std::string> chars;
        size_t idx = 0;
        while (idx < line.size()) {
            size_t s = idx;
            nextCodepoint(line, idx);
            chars.push_back(line.substr(s, idx - s));
        }
        if (chars.size() <= columns) {
            out.push_back(line);
            continue;
        }
        for (size_t i = 0; i < chars.size(); i += columns) {
            std::string slice;
            for (size_t k = i; k < i + columns && k < chars.size(); k++) slice += chars[k];
            out.push_back(std::move(slice));
        }
    }
    return out;
}

inline std::string renderLinesSvg(const std::vector<std::string>& rows, double x, double y, double step, const std::string& attrs) {
    std::string body;
    for (size_t i = 0; i < rows.size(); i++) {
        if (!rows[i].empty()) {
            body += "<tspan x=\"" + num(x) + "\" y=\"" + num(y + static_cast<double>(i) * step) + "\">" + esc(rows[i]) + "</tspan>";
        }
    }
    return body.empty() ? "" : ("<text" + attrs + " xml:space=\"preserve\">" + body + "</text>");
}

inline std::string joinVec(const std::vector<std::string>& v, const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) out += sep;
        out += v[i];
    }
    return out;
}

inline Rendered verticalEventModel(const EventModelingModel& model, const RenderOptions& options) {
    double size = options.hasFontSize ? options.fontSize : 16.0;
    double small = std::round(size * 0.875);
    double mono = std::round(size * 0.75);
    double pad = options.hasPadding ? options.padding : 8.0;
    const TextMeasurer& measurer = getMeasurer(options);
    double maxW = options.maxWidth > 0.0 ? options.maxWidth : 320.0;
    double room = std::max(160.0, maxW - 2.0 * pad);

    std::unordered_map<size_t, std::pair<size_t, size_t>> runs;
    std::vector<size_t> runsOrder;
    std::unordered_set<size_t> passed;
    for (const auto& relation : model.relations) {
        size_t s = relation.sourceIdx;
        size_t t = relation.targetIdx;
        if (s == t) continue;
        size_t lo = std::min(s, t);
        size_t hi = std::max(s, t);
        if (hi - lo < 2) {
            passed.insert(hi);
            continue;
        }
        auto it = runs.find(t);
        if (it != runs.end()) {
            it->second.first = std::min(it->second.first, s);
            it->second.second = std::max(it->second.second, s);
        } else {
            runs[t] = {lo, hi};
            runsOrder.push_back(t);
        }
    }

    std::stable_sort(runsOrder.begin(), runsOrder.end(), [&](size_t a, size_t b) {
        return runs[a].first < runs[b].first;
    });
    std::unordered_map<size_t, size_t> railOf;
    std::vector<size_t> railEnds;
    for (size_t target : runsOrder) {
        auto [from, to] = runs[target];
        size_t rail = railEnds.size();
        for (size_t rIdx = 0; rIdx < railEnds.size(); rIdx++) {
            if (railEnds[rIdx] < from) { rail = rIdx; break; }
        }
        if (rail == railEnds.size()) railEnds.push_back(to);
        else railEnds[rail] = to;
        railOf[target] = rail;
    }

    double gutter = 24.0 + RAIL_GAP * std::max(0, static_cast<int>(railEnds.size()) - 1);
    double cardWidth = std::max(CARD_MIN, room - gutter);
    double textRoom = cardWidth - 2.0 * BOX_PAD_X;
    Label title = layoutLabel(model.title, false, measurer, size, room, Style::Bold);
    double y = pad + (title.height > 0.0 ? title.height + 12.0 : 0.0);
    double bottom = pad + title.height;
    double width = std::max(title.width, cardWidth + gutter);
    std::string nodes, edges, cards;

    struct VPlaced {
        double y = 0.0;
        double h = 0.0;
        double w = 0.0;
        size_t index = 0;
        double lane = 0.0;
    };
    std::vector<VPlaced> placed(model.boxes.size());
    auto textFn = [&](const std::string& raw, double fontSize, int style = 0) -> Label {
        return literalLabel(decodeEntities(raw), measurer, fontSize, textRoom, style);
    };

    for (size_t index = 0; index < model.boxes.size(); index++) {
        const auto& box = model.boxes[index];
        double laneRoom = passed.count(index) ? (textRoom - ARROW_ROOM) : textRoom;
        Label lane = breakLabel(layoutLabel(box.lane.label, false, measurer, small, laneRoom, Style::Bold), measurer, laneRoom);
        Label name = layoutLabel(box.name, false, measurer, small, textRoom - 36.0, Style::Bold);
        std::optional<Label> data = (box.data.has_value() && !box.data->empty())
            ? std::optional<Label>(textFn(*box.data, mono, Style::Mono))
            : std::nullopt;
        double dataW = data.has_value() ? data->width : 0.0;
        double w = std::max({cardWidth, lane.width + 2.0 * BOX_PAD_X, name.width + 2.0 * BOX_PAD_X + 36.0, dataW + 2.0 * BOX_PAD_X});
        double headH = lane.height + 12.0;
        double h = 2.0 * BOX_PAD_Y + name.height + (data.has_value() ? (data->height + 6.0) : 0.0);
        auto series = entitySeries(box.kind);
        double cy = y + headH;
        std::string frame = "x=\"" + num(pad) + "\" y=\"" + num(cy) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\"";
        std::string dataLinesSvg;
        if (data.has_value()) {
            std::vector<std::string> dRows;
            for (const auto& l : data->lines) {
                std::string rowStr;
                for (const auto& sp : l) rowStr += sp.text;
                dRows.push_back(std::move(rowStr));
            }
            dataLinesSvg = renderLinesSvg(dRows, pad + BOX_PAD_X, cy + BOX_PAD_Y + name.height + 6.0 + data->lineHeight / 2.0 + mono * 0.35, data->lineHeight, " class=\"pele-em-data\" font-family=\"" + FONT_MONO + "\" font-size=\"" + num(mono) + "\"");
        }
        nodes += "<g class=\"pele-cluster pele-em-lane\" data-id=\"" + escText(box.lane.label) + "\">" +
            leftLabelSvg(lane, pad + BOX_PAD_X, y, " class=\"pele-cluster-label\" font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\" fill=\"var(--_m)\"") +
            "<g class=\"pele-node pele-em-" + box.kind + (box.reset ? " pele-em-reset" : "") + "\" data-id=\"" + escText(box.id) + "\"><rect " + frame + " fill=\"var(--_bg)\" stroke=\"var(--_b)\"" + ROUNDED + "/>" +
            (!series.has_value() ? "" : ("<rect " + frame + " fill=\"" + seriesColor(*series) + "\" fill-opacity=\"0.22\" stroke=\"" + seriesColor(*series) + "\"" + ROUNDED + "/>")) +
            labelSvg(name, pad + BOX_PAD_X + name.width / 2.0, cy + BOX_PAD_Y + name.height / 2.0, " class=\"pele-label\" font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\"") +
            "<text class=\"pele-em-frame\" x=\"" + num(pad + w - BOX_PAD_X) + "\" y=\"" + num(cy + BOX_PAD_Y + small) + "\" text-anchor=\"end\" font-size=\"" + num(mono) + "\" fill=\"var(--_m)\">" + esc(box.id) + "</text>" +
            dataLinesSvg + "</g></g>";
        placed[index] = {cy, h, w, index, lane.width};
        width = std::max(width, w + gutter);
        bottom = cy + h;
        y = bottom + 28.0;
    }

    for (const auto& relation : model.relations) {
        if (relation.sourceIdx == relation.targetIdx) continue;
        const auto& s = placed[relation.sourceIdx];
        const auto& t = placed[relation.targetIdx];
        bool down = t.y > s.y;
        double dir = down ? 1.0 : -1.0;
        double sy = down ? (s.y + s.h) : s.y;
        double ty = down ? t.y : (t.y + t.h);
        std::string d, head;
        if (std::abs(static_cast<int>(s.index) - static_cast<int>(t.index)) == 1) {
            double cx = pad + std::max(std::min(s.w, t.w) / 2.0, BOX_PAD_X + (down ? t.lane : s.lane) + 16.0);
            d = "M" + num(cx) + "," + num(sy) + "V" + num(ty - dir * markerTrim("arrow_point"));
            head = marker("arrow_point", cx, ty, 0.0, dir, "var(--_l)");
        } else {
            size_t rIdx = railOf.count(relation.targetIdx) ? railOf[relation.targetIdx] : 0;
            double rail = pad + width - gutter + 12.0 + RAIL_GAP * static_cast<double>(rIdx);
            double fromY = s.y + s.h / 2.0 - 5.0;
            double toY = t.y + t.h / 2.0 + 5.0;
            d = "M" + num(pad + s.w) + "," + num(fromY) + "H" + num(rail) + "V" + num(toY) + "H" + num(pad + t.w + markerTrim("arrow_point"));
            head = marker("arrow_point", pad + t.w, toY, -1.0, 0.0, "var(--_l)");
        }
        edges += "<g class=\"pele-edge\" data-id=\"" + escText(model.boxes[relation.sourceIdx].id) + "-" + escText(model.boxes[relation.targetIdx].id) + "\"><path d=\"" + d + "\" fill=\"none\" stroke=\"var(--_l)\"/>" + head + "</g>";
    }

    std::unordered_map<std::string, std::string> names;
    for (const auto& box : model.boxes) names[box.id] = box.name;
    auto addCard = [&](const std::string& kind, const std::string& heading, const std::string& frame, const std::string& raw) {
        std::string hText = heading + " \xC2\xB7 " + frame + (names.count(frame) ? (" " + names[frame]) : "");
        Label head = layoutLabel(hText, false, measurer, small, textRoom, Style::Bold);
        Label body = textFn(raw, small);
        double w = std::max({cardWidth, head.width + 2.0 * BOX_PAD_X, body.width + 2.0 * BOX_PAD_X});
        double h = 2.0 * BOX_PAD_Y + head.height + body.height;
        std::vector<std::string> bRows;
        for (const auto& l : body.lines) {
            std::string rowStr;
            for (const auto& sp : l) rowStr += sp.text;
            bRows.push_back(std::move(rowStr));
        }
        cards += "<g class=\"pele-em-" + kind + "\" data-id=\"" + escText(frame) + "\"><rect x=\"" + num(pad) + "\" y=\"" + num(y) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" fill=\"var(--_bg)\" stroke=\"var(--_b)\"" + ROUNDED + "/>" +
            labelSvg(head, pad + BOX_PAD_X + head.width / 2.0, y + BOX_PAD_Y + head.height / 2.0, " font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\"") +
            renderLinesSvg(bRows, pad + BOX_PAD_X, y + BOX_PAD_Y + head.height + body.lineHeight / 2.0 + small * 0.35, body.lineHeight, " font-size=\"" + num(small) + "\"") + "</g>";
        bottom = y + h;
        y = bottom + CARD_GAP;
        width = std::max(width, w);
    };
    for (const auto& note : model.notes) addCard("note", "Note", note.frame, note.text);
    for (const auto& spec : model.specifications) {
        std::vector<std::string> rows = {"Given  " + joinVec(spec.given, ", ")};
        if (!spec.when.empty()) rows.push_back("When  " + joinVec(spec.when, ", "));
        rows.push_back("Then  " + joinVec(spec.then, ", "));
        addCard("specification", "Given / When / Then", spec.frame, joinVec(rows, "\n"));
    }

    double totalWidth = std::ceil(width + 2.0 * pad);
    double height = std::ceil(bottom + pad);
    std::string content = labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") + edges + nodes + cards;
    return {svgDocument("eventmodeling", totalWidth, height, size, options, model, content), totalWidth, height, {}};
}

inline Rendered drawEventModel(const EventModelingModel& model, const Config& config, const RenderOptions& options, bool down) {
    (void)config;
    if (down) return verticalEventModel(model, options);
    double size = options.hasFontSize ? options.fontSize : 16.0;
    double nameSize = std::round(size * 0.875);
    double small = std::round(size * 0.8125);
    double mono = std::round(size * 0.75);
    double monoStep = std::round(mono * 1.4);
    double smallStep = std::round(small * 1.5);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.hasPadding ? options.padding : 8.0;

    auto widest = [&](const std::vector<std::string>& rows, double fontSize, int style) -> double {
        double w = 0.0;
        for (const auto& row : rows) w = std::max(w, measurer.width(row, fontSize, style));
        return w;
    };

    struct Band {
        EmLane lane;
        Label label;
        double right = -std::numeric_limits<double>::infinity();
        double y = 0.0;
        double h = 0.0;
    };
    std::vector<Band> bands;
    std::unordered_map<int, size_t> bandByLaneIdx;
    double labelColumn = 0.0;
    for (const auto& lane : model.lanes) {
        Label label = layoutLabel(lane.label, false, measurer, small, 4000.0, Style::Bold);
        labelColumn = std::max(labelColumn, label.width);
        bandByLaneIdx[lane.index] = bands.size();
        bands.push_back({lane, std::move(label), -std::numeric_limits<double>::infinity(), 0.0, 0.0});
    }
    labelColumn += 2.0 * LANE_PAD;

    struct Placed {
        const EmBox* box = nullptr;
        Label name;
        std::vector<std::string> data;
        double x = 0.0;
        double y = 0.0;
        double w = 0.0;
        double h = 0.0;
    };
    std::vector<Placed> placed;
    placed.reserve(model.boxes.size());
    for (size_t i = 0; i < model.boxes.size(); i++) {
        const auto& box = model.boxes[i];
        auto& band = bands[bandByLaneIdx[box.lane.index]];
        Label name = layoutLabel(box.name, false, measurer, nameSize, 4000.0, Style::Bold);
        std::vector<std::string> data = (box.data.has_value() && !box.data->empty()) ? hardWrap(*box.data, DATA_COLUMNS) : std::vector<std::string>{};
        double w = std::max(BOX_MIN_WIDTH, std::max(name.width, widest(data, mono, Style::Mono)) + 2.0 * BOX_PAD_X);
        double h = 2.0 * BOX_PAD_Y + name.height + (!data.empty() ? (6.0 + static_cast<double>(data.size()) * monoStep) : 0.0);
        double x = 0.0;
        if (!placed.empty()) {
            const auto& prev = placed.back();
            double stagger = prev.x + std::max(prev.w / 2.0, prev.w - STAGGER);
            x = (prev.box->lane.index == box.lane.index) ? (band.right + BOX_GAP) : std::max(stagger, band.right + BOX_GAP);
        }
        band.right = x + w;
        band.h = std::max(band.h, h);
        placed.push_back({&box, std::move(name), std::move(data), x, 0.0, w, h});
    }

    double contentRight = 0.0;
    for (const auto& item : placed) contentRight = std::max(contentRight, item.x + item.w);
    double laneWidth = labelColumn + contentRight + LANE_PAD;

    Label title = layoutLabel(model.title, false, measurer, size, 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + 12.0 : 0.0;
    double left = pad;
    double top = pad + titleHeight;

    double y = top;
    std::string lanesSvg;
    for (auto& band : bands) {
        band.y = y;
        band.h += 2.0 * LANE_PAD;
        lanesSvg +=
            "<g class=\"pele-cluster pele-em-lane\" data-id=\"" + escText(band.lane.label) + "\">"
            "<rect x=\"" + num(left) + "\" y=\"" + num(y) + "\" width=\"" + num(laneWidth) + "\" height=\"" + num(band.h) + "\" fill=\"var(--_s)\"" + ROUNDED + "/>" +
            labelSvg(band.label, left + LANE_PAD + band.label.width / 2.0, y + band.h / 2.0, " class=\"pele-cluster-label\" font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\" fill=\"var(--_m)\"") +
            "</g>";
        y += band.h + LANE_GAP;
    }
    double bottom = !bands.empty() ? (y - LANE_GAP) : top;

    std::string nodesSvg;
    for (auto& item : placed) {
        const auto& band = bands[bandByLaneIdx[item.box->lane.index]];
        item.x += left + labelColumn;
        item.y = band.y + (band.h - item.h) / 2.0;
        auto series = entitySeries(item.box->kind);
        std::string frame = "x=\"" + num(item.x) + "\" y=\"" + num(item.y) + "\" width=\"" + num(item.w) + "\" height=\"" + num(item.h) + "\"";
        std::string shape = !series.has_value()
            ? ("<rect " + frame + " fill=\"var(--_bg)\" stroke=\"var(--_b)\"" + ROUNDED + "/>")
            : ("<rect " + frame + " fill=\"var(--_bg)\"" + ROUNDED + "/><rect " + frame + " fill=\"" + seriesColor(*series) + "\" fill-opacity=\"0.22\" stroke=\"" + seriesColor(*series) + "\"" + ROUNDED + "/>");
        double nameY = item.y + BOX_PAD_Y + item.name.height / 2.0;
        std::string text =
            labelSvg(item.name, item.x + item.w / 2.0, nameY, " class=\"pele-label\" font-size=\"" + num(nameSize) + "\" font-weight=\"var(--_hw)\"") +
            renderLinesSvg(
                item.data,
                item.x + BOX_PAD_X,
                item.y + BOX_PAD_Y + item.name.height + 6.0 + monoStep / 2.0 + mono * 0.35,
                monoStep,
                " class=\"pele-em-data\" font-family=\"" + FONT_MONO + "\" font-size=\"" + num(mono) + "\""
            );
        nodesSvg += "<g class=\"pele-node pele-em-" + item.box->kind + (item.box->reset ? " pele-em-reset" : "") + "\" data-id=\"" + escText(item.box->id) + "\">" + shape + text + "</g>";
    }

    std::string edgesSvg;
    double trim = markerTrim("arrow_point");
    for (const auto& relation : model.relations) {
        if (relation.sourceIdx == relation.targetIdx) continue;
        const auto& s = placed[relation.sourceIdx];
        const auto& t = placed[relation.targetIdx];
        std::string d, head;
        if (s.box->lane.index == t.box->lane.index) {
            bool forward = t.x >= s.x;
            double sx = forward ? (s.x + s.w) : s.x;
            double tx = forward ? t.x : (t.x + t.w);
            double sy = s.y + s.h / 2.0;
            double ty = t.y + t.h / 2.0;
            double dir = tx >= sx ? 1.0 : -1.0;
            d = "M" + num(sx) + "," + num(sy) + "L" + num(tx - dir * trim) + "," + num(ty);
            head = marker("arrow_point", tx, ty, dir, 0.0, "var(--_l)");
        } else {
            bool downDir = t.y > s.y;
            double from = std::max(s.x, t.x);
            double to = std::min(s.x + s.w, t.x + t.w);
            auto inset = [](double w) { return std::min(24.0, w / 4.0); };
            double sx = (to - from >= 16.0) ? ((from + to) / 2.0) : (t.x > s.x ? (s.x + s.w - inset(s.w)) : (s.x + inset(s.w)));
            double tx = (to - from >= 16.0) ? sx : (t.x > s.x ? (t.x + inset(t.w)) : (t.x + t.w - inset(t.w)));
            double sy = downDir ? (s.y + s.h) : s.y;
            double ty = downDir ? t.y : (t.y + t.h);
            double dir = downDir ? 1.0 : -1.0;
            double end = ty - dir * trim;
            double mid = (sy + end) / 2.0;
            d = (sx == tx)
                ? ("M" + num(sx) + "," + num(sy) + "V" + num(end))
                : ("M" + num(sx) + "," + num(sy) + "C" + num(sx) + "," + num(mid) + " " + num(tx) + "," + num(mid) + " " + num(tx) + "," + num(end));
            head = marker("arrow_point", tx, ty, 0.0, dir, "var(--_l)");
        }
        edgesSvg += "<g class=\"pele-edge\" data-id=\"" + escText(s.box->id) + "-" + escText(t.box->id) + "\"><path d=\"" + d + "\" fill=\"none\" stroke=\"var(--_l)\"/>" + head + "</g>";
    }

    std::unordered_map<std::string, std::string> names;
    for (const auto& box : model.boxes) names[box.id] = box.name;
    std::string cardsSvg;
    double cardsRight = 0.0;
    auto addCard = [&](const std::string& kind, const std::string& heading, const std::string& frame, const std::vector<std::string>& rows) {
        std::string hText = heading + " \xC2\xB7 " + frame + (names.count(frame) ? (" " + names[frame]) : "");
        Label head = layoutLabel(hText, false, measurer, small, 4000.0, Style::Bold);
        double w = std::max(head.width, widest(rows, small, 0)) + 2.0 * BOX_PAD_X;
        double h = 2.0 * BOX_PAD_Y + head.height + static_cast<double>(rows.size()) * smallStep;
        double cy = bottom + (cardsSvg.empty() && bands.empty() ? 0.0 : CARD_GAP);
        cardsSvg +=
            "<g class=\"pele-em-" + kind + "\" data-id=\"" + escText(frame) + "\">"
            "<rect x=\"" + num(left) + "\" y=\"" + num(cy) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" fill=\"var(--_bg)\" stroke=\"var(--_b)\"" + ROUNDED + "/>" +
            labelSvg(head, left + BOX_PAD_X + head.width / 2.0, cy + BOX_PAD_Y + head.height / 2.0, " font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\"") +
            renderLinesSvg(rows, left + BOX_PAD_X, cy + BOX_PAD_Y + head.height + smallStep / 2.0 + small * 0.35, smallStep, " font-size=\"" + num(small) + "\"") +
            "</g>";
        bottom = cy + h;
        cardsRight = std::max(cardsRight, w);
    };
    for (const auto& note : model.notes) addCard("note", "Note", note.frame, hardWrap(note.text, NOTE_COLUMNS));
    for (const auto& spec : model.specifications) {
        std::vector<std::string> rawRows = {"Given  " + joinVec(spec.given, ", ")};
        if (!spec.when.empty()) rawRows.push_back("When  " + joinVec(spec.when, ", "));
        rawRows.push_back("Then  " + joinVec(spec.then, ", "));
        std::vector<std::string> wrapped;
        for (const auto& r : rawRows) {
            for (auto& w : hardWrap(r, NOTE_COLUMNS)) wrapped.push_back(std::move(w));
        }
        addCard("specification", "Given / When / Then", spec.frame, wrapped);
    }

    double width = std::max({!bands.empty() ? laneWidth : 0.0, cardsRight, title.width});
    double totalWidth = std::ceil(width + 2.0 * pad);
    double totalHeight = std::ceil(bottom + pad);
    std::string content =
        labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
        lanesSvg + edgesSvg + nodesSvg + cardsSvg;

    return {
        svgDocument("eventmodeling", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

template <typename DrawFn>
inline Rendered turnToFit(const RenderOptions& options, DrawFn&& draw) {
    Rendered across = draw(false);
    double available = options.maxWidth;
    if (!(available > 0.0) || across.width <= available) return across;
    if (available >= 640.0) return across;
    Rendered down = draw(true);
    return down.width < across.width ? down : across;
}

inline Rendered renderEventModel(const EventModelingModel& model, const Config& config, const RenderOptions& options) {
    return turnToFit(options, [&](bool down) {
        return drawEventModel(model, config, options, down);
    });
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    EventModelingModel model = buildEventModel(parseEventModel(source), title);
    return renderEventModel(model, config, options);
}

} // namespace EventModelingDiagram

inline Rendered renderEventModeling(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return EventModelingDiagram::render(source, config, title, options);
}

} // namespace Pele
