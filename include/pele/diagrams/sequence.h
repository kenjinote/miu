#pragma once
#include "../foundation.h"
#include "../util.h"
#include "../text.h"
#include "../svg.h"
#include "../common_parse.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Pele::SequenceDiagram {

inline std::string num(double value) noexcept {
    long long n = static_cast<long long>(std::floor(value * 100.0 + 0.5));
    if (n == 0) return "0";
    bool neg = n < 0;
    unsigned long long absN = neg ? static_cast<unsigned long long>(-n) : static_cast<unsigned long long>(n);
    unsigned long long whole = absN / 100;
    unsigned long long frac = absN % 100;
    std::string res = (neg ? "-" : "") + std::to_string(whole);
    if (frac == 0) return res;
    res.push_back('.');
    if (frac % 10 == 0) {
        res.push_back(static_cast<char>('0' + (frac / 10)));
    } else {
        if (frac < 10) res.push_back('0');
        res += std::to_string(frac);
    }
    return res;
}

// ============================================================================
// Port of src/diagrams/sequence/types.ts
// ============================================================================

namespace LINETYPE {
constexpr int SOLID = 0;
constexpr int DOTTED = 1;
constexpr int NOTE = 2;
constexpr int SOLID_CROSS = 3;
constexpr int DOTTED_CROSS = 4;
constexpr int SOLID_OPEN = 5;
constexpr int DOTTED_OPEN = 6;
constexpr int LOOP_START = 10;
constexpr int LOOP_END = 11;
constexpr int ALT_START = 12;
constexpr int ALT_ELSE = 13;
constexpr int ALT_END = 14;
constexpr int OPT_START = 15;
constexpr int OPT_END = 16;
constexpr int ACTIVE_START = 17;
constexpr int ACTIVE_END = 18;
constexpr int PAR_START = 19;
constexpr int PAR_AND = 20;
constexpr int PAR_END = 21;
constexpr int RECT_START = 22;
constexpr int RECT_END = 23;
constexpr int SOLID_POINT = 24;
constexpr int DOTTED_POINT = 25;
constexpr int AUTONUMBER = 26;
constexpr int CRITICAL_START = 27;
constexpr int CRITICAL_OPTION = 28;
constexpr int CRITICAL_END = 29;
constexpr int BREAK_START = 30;
constexpr int BREAK_END = 31;
constexpr int PAR_OVER_START = 32;
constexpr int BIDIRECTIONAL_SOLID = 33;
constexpr int BIDIRECTIONAL_DOTTED = 34;
constexpr int SOLID_TOP = 41;
constexpr int SOLID_BOTTOM = 42;
constexpr int STICK_TOP = 43;
constexpr int STICK_BOTTOM = 44;
constexpr int SOLID_ARROW_TOP_REVERSE = 45;
constexpr int SOLID_ARROW_BOTTOM_REVERSE = 46;
constexpr int STICK_ARROW_TOP_REVERSE = 47;
constexpr int STICK_ARROW_BOTTOM_REVERSE = 48;
constexpr int SOLID_TOP_DOTTED = 51;
constexpr int SOLID_BOTTOM_DOTTED = 52;
constexpr int STICK_TOP_DOTTED = 53;
constexpr int STICK_BOTTOM_DOTTED = 54;
constexpr int SOLID_ARROW_TOP_REVERSE_DOTTED = 55;
constexpr int SOLID_ARROW_BOTTOM_REVERSE_DOTTED = 56;
constexpr int STICK_ARROW_TOP_REVERSE_DOTTED = 57;
constexpr int STICK_ARROW_BOTTOM_REVERSE_DOTTED = 58;
constexpr int CENTRAL_CONNECTION = 59;
constexpr int CENTRAL_CONNECTION_REVERSE = 60;
constexpr int CENTRAL_CONNECTION_DUAL = 61;
} // namespace LINETYPE

namespace PLACEMENT {
constexpr int LEFTOF = 0;
constexpr int RIGHTOF = 1;
constexpr int OVER = 2;
} // namespace PLACEMENT

struct SeqText {
    std::string text;
    std::optional<bool> wrap;
    std::string type;
    bool hasType = false;
};

struct SeqBoxData {
    std::string text;
    bool hasText = false;
    std::string color;
    std::optional<bool> wrap;
};

struct SeqBox {
    std::string name;
    bool hasName = false;
    bool wrap = false;
    std::string fill;
    std::vector<std::string> actorKeys;
};

struct SeqActor {
    std::shared_ptr<SeqBox> box;
    std::string name;
    std::string description;
    bool wrap = false;
    std::string prevActor;
    bool hasPrevActor = false;
    std::string nextActor;
    bool hasNextActor = false;
    Pele::FlatMap<std::string, std::string> links;
    Pele::FlatMap<std::string, std::string> properties;
    std::string details;
    std::string type;
};

struct SeqNumbering {
    double start = 0.0;
    bool hasStart = false;
    double step = 0.0;
    bool hasStep = false;
    bool visible = false;
};

struct SeqMessage {
    std::string id;
    std::string from;
    bool hasFrom = false;
    std::string to;
    bool hasTo = false;
    std::string message;
    SeqNumbering numbering;
    bool isNumbering = false;
    bool wrap = false;
    int type = -1;
    bool hasType = false;
    bool activate = false;
    int placement = 0;
    bool hasPlacement = false;
    int centralConnection = 0;
};

struct SeqNote {
    std::vector<std::string> actor;
    bool singleActor = true;
    int placement = 0;
    std::string message;
    bool wrap = false;
};

struct SeqStatement {
    std::string type;
    std::string actor;
    std::vector<std::string> actorPair;
    bool hasActorPair = false;
    std::string draw;
    bool hasDraw = false;
    SeqText description;
    bool hasDescription = false;
    std::string config;
    bool hasConfig = false;
    int signalType = -1;
    bool hasSignalType = false;
    std::string from;
    bool hasFrom = false;
    std::string to;
    bool hasTo = false;
    SeqText msg;
    bool hasMsg = false;
    bool activate = false;
    int centralConnection = 0;
    bool hasCentralConnection = false;
    int placement = 0;
    bool hasPlacement = false;
    SeqText text;
    bool hasText = false;
    SeqBoxData boxData;
    bool hasBoxData = false;
    std::string boxText;
    SeqText loopText;
    bool hasLoopText = false;
    SeqText optText;
    bool hasOptText = false;
    SeqText altText;
    bool hasAltText = false;
    SeqText parText;
    bool hasParText = false;
    SeqText criticalText;
    bool hasCriticalText = false;
    SeqText optionText;
    bool hasOptionText = false;
    SeqText breakText;
    bool hasBreakText = false;
    SeqText color;
    bool hasColor = false;
    double sequenceIndex = 0.0;
    bool hasSequenceIndex = false;
    double sequenceIndexStep = 0.0;
    bool hasSequenceIndexStep = false;
    bool sequenceVisible = false;
};

struct SeqItem {
    enum Kind { Empty, Statement, String, List } kind = Empty;
    SeqStatement stmt;
    std::string str;
    std::vector<SeqItem> list;

    static SeqItem makeEmpty() {
        SeqItem it;
        it.kind = List;
        return it;
    }
    static SeqItem makeStmt(const SeqStatement& s) {
        SeqItem it;
        it.kind = Statement;
        it.stmt = s;
        return it;
    }
    static SeqItem makeStr(const std::string& s) {
        SeqItem it;
        it.kind = String;
        it.str = s;
        return it;
    }
    static SeqItem makeList(std::vector<SeqItem> l) {
        SeqItem it;
        it.kind = List;
        it.list = std::move(l);
        return it;
    }
};

// ============================================================================
// Port of src/diagrams/sequence/colors.ts
// ============================================================================

namespace detail_colors {

enum class K {
    Bad,
    Number,
    Percent,
    Angle,
    None
};

inline K kind(const std::string& term) noexcept {
    std::string lower = toLowerAscii(term);
    if (lower == "none") return K::None;
    size_t n = lower.size();
    if (n == 0) return K::Bad;
    size_t i = 0;
    if (lower[i] == '+' || lower[i] == '-') i++;
    if (i < n && isDigit(static_cast<unsigned char>(lower[i]))) {
        while (i < n && isDigit(static_cast<unsigned char>(lower[i]))) i++;
        if (i < n && lower[i] == '.') {
            i++;
            if (i >= n || !isDigit(static_cast<unsigned char>(lower[i]))) return K::Bad;
            while (i < n && isDigit(static_cast<unsigned char>(lower[i]))) i++;
        }
    } else if (i < n && lower[i] == '.') {
        i++;
        if (i >= n || !isDigit(static_cast<unsigned char>(lower[i]))) return K::Bad;
        while (i < n && isDigit(static_cast<unsigned char>(lower[i]))) i++;
    } else {
        return K::Bad;
    }
    if (i < n && lower[i] == 'e') {
        size_t ePos = i + 1;
        if (ePos < n && (lower[ePos] == '+' || lower[ePos] == '-')) ePos++;
        if (ePos >= n || !isDigit(static_cast<unsigned char>(lower[ePos]))) return K::Bad;
        while (ePos < n && isDigit(static_cast<unsigned char>(lower[ePos]))) ePos++;
        i = ePos;
    }
    std::string unit = lower.substr(i);
    if (unit.empty()) return K::Number;
    if (unit == "%") return K::Percent;
    if (unit == "deg" || unit == "grad" || unit == "rad" || unit == "turn") return K::Angle;
    return K::Bad;
}

inline bool isAlpha(const std::string& term, bool modern) noexcept {
    K k = kind(term);
    return k == K::Number || k == K::Percent || (modern && k == K::None);
}

inline std::vector<std::string> splitWhitespace(const std::string& s) noexcept {
    std::vector<std::string> out;
    size_t i = 0;
    size_t n = s.size();
    while (i < n) {
        size_t start = i;
        while (i < n) {
            size_t next = i;
            int cp = charCodeAtUtf8(s, i, &next);
            if (isSpace(cp)) break;
            i = next;
        }
        out.push_back(s.substr(start, i - start));
        while (i < n) {
            size_t next = i;
            int cp = charCodeAtUtf8(s, i, &next);
            if (!isSpace(cp)) break;
            i = next;
        }
    }
    if (out.empty()) out.push_back("");
    return out;
}

inline bool isFunction(const std::string& color) noexcept {
    bool hsl = startsWith(color, "hsl");
    size_t open = color.find('(');
    if (open == std::string::npos) return false;
    std::string name = color.substr(0, open);
    if (name != "rgb" && name != "rgba" && name != "hsl" && name != "hsla") return false;
    if (color.size() > 120 || color.find(')') != color.size() - 1) return false;
    std::string inner = trimStr(color.substr(open + 1, color.size() - open - 2));
    std::vector<std::string> parts;
    std::string alpha;
    bool hasAlpha = false;
    bool legacy = inner.find(',') != std::string::npos;
    if (legacy) {
        for (const auto& seg : splitStr(inner, ',')) {
            parts.push_back(trimStr(seg));
        }
        if (parts.size() == 4) {
            alpha = parts.back();
            hasAlpha = true;
            parts.pop_back();
        }
    } else {
        auto slash = splitStr(inner, '/');
        if (slash.size() > 2) return false;
        parts = splitWhitespace(trimStr(slash[0]));
        if (slash.size() == 2) {
            alpha = trimStr(slash[1]);
            hasAlpha = true;
        }
    }
    if (parts.size() != 3 || (hasAlpha && !isAlpha(alpha, !legacy))) return false;
    K kinds[3] = {kind(parts[0]), kind(parts[1]), kind(parts[2])};
    if (legacy) {
        if (hsl) return (kinds[0] == K::Number || kinds[0] == K::Angle) && kinds[1] == K::Percent && kinds[2] == K::Percent;
        return (kinds[0] == K::Number || kinds[0] == K::Percent) && kinds[1] == kinds[0] && kinds[2] == kinds[0];
    }
    for (int i = 0; i < 3; i++) {
        K k = kinds[i];
        bool ok = k == K::Number || k == K::None || (hsl && i == 0 ? k == K::Angle : k == K::Percent);
        if (!ok) return false;
    }
    return true;
}

inline bool isCssColor(const std::string& color) noexcept {
    if (color.find('(') != std::string::npos) return isFunction(color);
    static const Pele::FlatSet<std::string> keywords = []() {
        const std::string KEYWORDS =
            "aliceblue antiquewhite aqua aquamarine azure beige bisque black blanchedalmond blue blueviolet brown burlywood "
            "cadetblue chartreuse chocolate coral cornflowerblue cornsilk crimson cyan darkblue darkcyan darkgoldenrod darkgray "
            "darkgreen darkgrey darkkhaki darkmagenta darkolivegreen darkorange darkorchid darkred darksalmon darkseagreen "
            "darkslateblue darkslategray darkslategrey darkturquoise darkviolet deeppink deepskyblue dimgray dimgrey dodgerblue "
            "firebrick floralwhite forestgreen fuchsia gainsboro ghostwhite gold goldenrod gray green greenyellow grey honeydew "
            "hotpink indianred indigo ivory khaki lavender lavenderblush lawngreen lemonchiffon lightblue lightcoral lightcyan "
            "lightgoldenrodyellow lightgray lightgreen lightgrey lightpink lightsalmon lightseagreen lightskyblue lightslategray "
            "lightslategrey lightsteelblue lightyellow lime limegreen linen magenta maroon mediumaquamarine mediumblue "
            "mediumorchid mediumpurple mediumseagreen mediumslateblue mediumspringgreen mediumturquoise mediumvioletred "
            "midnightblue mintcream mistyrose moccasin navajowhite navy oldlace olive olivedrab orange orangered orchid "
            "palegoldenrod palegreen paleturquoise palevioletred papayawhip peachpuff peru pink plum powderblue purple "
            "rebeccapurple red rosybrown royalblue saddlebrown salmon sandybrown seagreen seashell sienna silver skyblue "
            "slateblue slategray slategrey snow springgreen steelblue tan teal thistle tomato turquoise violet wheat white "
            "whitesmoke yellow yellowgreen transparent currentcolor inherit initial unset revert "
            "accentcolor accentcolortext activetext buttonborder buttonface buttontext canvas canvastext field fieldtext "
            "graytext highlight highlighttext linktext mark marktext selecteditem selecteditemtext visitedtext "
            "activeborder activecaption appworkspace background buttonhighlight buttonshadow captiontext inactiveborder "
            "inactivecaption inactivecaptiontext infobackground infotext menu menutext scrollbar threeddarkshadow threedface "
            "threedhighlight threedlightshadow threedshadow window windowframe windowtext";
        Pele::FlatSet<std::string> set;
        for (const auto& w : splitStr(KEYWORDS, ' ')) {
            if (!w.empty()) set.insert(w);
        }
        return set;
    }();
    return keywords.count(toLowerAscii(color)) > 0;
}

} // namespace detail_colors

// ============================================================================
// Port of src/diagrams/sequence/db.ts
// ============================================================================

struct SeqDbOptions {
    std::optional<bool> wrap;
    bool sequenceWrap = false;
};

namespace detail_db {

[[noreturn]] inline void fail(const std::string& message) {
    throw PeleError(message, "semantic", "sequence");
}

inline SeqText extractWrap(const std::string& raw) noexcept {
    std::string text = trimStr(raw);
    std::optional<bool> wrap;
    size_t prefixLen = 0;
    if (startsWith(text, ":wrap:")) {
        wrap = true;
        prefixLen = 6;
    } else if (startsWith(text, "wrap:")) {
        wrap = true;
        prefixLen = 5;
    } else if (startsWith(text, ":nowrap:")) {
        wrap = false;
        prefixLen = 8;
    } else if (startsWith(text, "nowrap:")) {
        wrap = false;
        prefixLen = 7;
    }
    if (wrap.has_value()) {
        text = trimStr(text.substr(prefixLen));
    }
    return {text, wrap, "", false};
}

inline bool hasLineTerminator(const std::string& s) noexcept {
    for (size_t i = 0; i < s.size();) {
        size_t next = i;
        int cp = charCodeAtUtf8(s, i, &next);
        if (cp == '\r' || cp == 0x2028 || cp == 0x2029) return true;
        i = next;
    }
    return false;
}

inline std::optional<std::pair<std::string, std::string>> splitBox(const std::string& str) noexcept {
    // Check /^(?:rgba?|hsla?)\s*\(/
    size_t fnLen = 0;
    if (startsWith(str, "rgba") || startsWith(str, "hsla")) fnLen = 4;
    else if (startsWith(str, "rgb") || startsWith(str, "hsl")) fnLen = 3;
    if (fnLen > 0) {
        size_t p = fnLen;
        while (p < str.size()) {
            size_t next = p;
            int cp = charCodeAtUtf8(str, p, &next);
            if (!isSpace(cp)) break;
            p = next;
        }
        if (p < str.size() && str[p] == '(') {
            size_t match0Len = p + 1;
            size_t close = str.rfind(')');
            if (close != std::string::npos && close >= match0Len) {
                if (hasLineTerminator(str.substr(match0Len))) return std::nullopt;
                return std::make_pair(str.substr(0, close + 1), str.substr(close + 1));
            }
        }
    }
    if (hasLineTerminator(str)) return std::nullopt;
    size_t wLen = 0;
    while (wLen < str.size() && isWord(static_cast<unsigned char>(str[wLen]))) wLen++;
    return std::make_pair(str.substr(0, wLen), str.substr(wLen));
}

} // namespace detail_db

class SeqDb {
public:
    std::vector<std::string> actorOrder;
    std::unordered_map<std::string, SeqActor> actors;
    Pele::FlatMap<std::string, int> createdActors;
    Pele::FlatMap<std::string, int> destroyedActors;
    std::vector<std::shared_ptr<SeqBox>> boxes;
    std::vector<SeqMessage> messages;
    std::vector<SeqNote> notes;
    std::string title;
    std::string accTitle;
    std::string accDescr;

private:
    std::string prevActor_;
    bool hasPrevActor_ = false;
    std::shared_ptr<SeqBox> currentBox_;
    std::string lastCreated_;
    bool hasLastCreated_ = false;
    std::string lastDestroyed_;
    bool hasLastDestroyed_ = false;
    std::optional<bool> wrapEnabled_;
    bool sequenceWrap_ = false;
    Pele::FlatMap<std::string, int> active_;

public:
    explicit SeqDb(const SeqDbOptions& options = {})
        : wrapEnabled_(options.wrap), sequenceWrap_(options.sequenceWrap) {}

    DiagramMeta asMeta() const {
        return {title, accTitle, accDescr};
    }

    bool autoWrap() const {
        return wrapEnabled_.value_or(sequenceWrap_);
    }

    void setAccTitle(const std::string& text) {
        accTitle = trimStartStr(text);
    }

    void setAccDescription(const std::string& text) {
        // text.replace(/\n\s+/g, '\n')
        std::string out;
        size_t i = 0;
        while (i < text.size()) {
            if (text[i] == '\n') {
                out.push_back('\n');
                i++;
                while (i < text.size()) {
                    size_t next = i;
                    int cp = charCodeAtUtf8(text, i, &next);
                    if (!isSpace(cp)) break;
                    i = next;
                }
            } else {
                out.push_back(text[i++]);
            }
        }
        accDescr = out;
    }

    void setDiagramTitle(const std::string& text) {
        title = text;
    }

    SeqText parseMessage(const std::string& str) const {
        return detail_db::extractWrap(str);
    }

    SeqBoxData parseBoxData(const std::string& str) const {
        auto match = detail_db::splitBox(str);
        std::string color = (match && !match->first.empty()) ? trimStr(match->first) : "transparent";
        std::string rawTitle;
        bool hasTitle = false;
        if (match && !match->second.empty()) {
            rawTitle = trimStr(match->second);
            hasTitle = true;
        }
        if (!detail_colors::isCssColor(color)) {
            color = "transparent";
            rawTitle = trimStr(str);
            hasTitle = true;
        }
        if (!hasTitle) return {"", false, color, std::nullopt};
        auto [text, wrap, _, __] = detail_db::extractWrap(rawTitle);
        return {text, !text.empty(), color, wrap};
    }

    void addBox(const SeqBoxData& data) {
        currentBox_ = std::make_shared<SeqBox>();
        currentBox_->name = data.text;
        currentBox_->hasName = data.hasText;
        currentBox_->wrap = data.wrap.value_or(autoWrap());
        currentBox_->fill = data.color;
        boxes.push_back(currentBox_);
    }

    void addActor(
        const std::string& id,
        const std::string& name,
        const SeqText* descriptionIn,
        const std::string* typeIn,
        const std::string* metadata = nullptr
    ) {
        auto assignedBox = currentBox_;
        YamlValue doc;
        bool hasDoc = false;
        if (metadata != nullptr) {
            YamlValue parsed;
            try {
                std::string yamlInput = metadata->find('\n') != std::string::npos
                    ? (*metadata + "\n")
                    : ("{\n" + *metadata + "\n}");
                parsed = parseYaml(yamlInput);
            } catch (const std::exception& err) {
                throw PeleError(Pele::cat({"Config of participant \"", id, "\" is not valid YAML. ", err.what()}),  "syntax",  "sequence");
            }
            if (parsed.isMap()) {
                doc = std::move(parsed);
                hasDoc = true;
            }
        }
        std::string type = typeIn ? *typeIn : "";
        bool hasType = (typeIn != nullptr && !typeIn->empty());
        if (hasDoc) {
            if (const auto* dt = doc.get("type"); dt && !dt->isNull()) {
                type = dt->asString();
                hasType = true;
            }
        }
        SeqText desc;
        bool hasDesc = false;
        if (descriptionIn != nullptr) {
            desc = *descriptionIn;
            hasDesc = true;
        }
        if (hasDoc) {
            if (const auto* da = doc.get("alias"); da && !da->isNull()) {
                std::string aliasStr = da->asString();
                if (!aliasStr.empty() && (!hasDesc || desc.text == name)) {
                    desc.text = aliasStr;
                    if (descriptionIn) desc.wrap = descriptionIn->wrap;
                    desc.type = type;
                    desc.hasType = hasType;
                    hasDesc = true;
                }
            }
        }

        auto oldIt = actors.find(id);
        if (oldIt != actors.end()) {
            auto& old = oldIt->second;
            if (currentBox_ && old.box && currentBox_ != old.box) {
                std::string oldBoxName = old.box->hasName ? old.box->name : "undefined";
                std::string curBoxName = currentBox_->hasName ? currentBox_->name : "undefined";
                detail_db::fail(Pele::cat({"A same participant should only be defined in one Box: ", old.name, " can't be in '", oldBoxName, "' and in '", curBoxName, "' at the same time."}));
            }
            assignedBox = old.box ? old.box : currentBox_;
            old.box = assignedBox;
            if (name == old.name && !hasDesc) return;
        } else {
            actorOrder.push_back(id);
        }

        if (!hasDesc || !hasType) {
            desc.text = name;
            desc.wrap = std::nullopt;
            desc.type = type;
            desc.hasType = hasType;
        }

        SeqActor act;
        act.box = assignedBox;
        act.name = name;
        act.description = desc.text;
        act.wrap = desc.wrap.value_or(autoWrap());
        act.prevActor = prevActor_;
        act.hasPrevActor = hasPrevActor_;
        act.type = hasType ? type : "participant";
        actors[id] = std::move(act);

        if (hasPrevActor_) {
            auto prevIt = actors.find(prevActor_);
            if (prevIt != actors.end()) {
                prevIt->second.nextActor = id;
                prevIt->second.hasNextActor = true;
            }
        }
        if (currentBox_) currentBox_->actorKeys.push_back(id);
        prevActor_ = id;
        hasPrevActor_ = true;
    }

    bool addSignal(
        const std::string* from,
        const std::string* to,
        const SeqText* message,
        int type,
        bool hasType,
        bool activate = false,
        int centralConnection = 0
    ) {
        if (hasType && type == LINETYPE::ACTIVE_END) {
            int count = (from && !from->empty()) ? active_[*from] : 0;
            if (count < 1) {
                detail_db::fail(Pele::cat({"Trying to inactivate an inactive participant (", (from ? *from : "undefined"), ")"}));
            }
            active_[*from] = count - 1;
        } else if (hasType && type == LINETYPE::ACTIVE_START && from && !from->empty()) {
            active_[*from] = active_[*from] + 1;
        }
        SeqMessage m;
        m.id = std::to_string(messages.size());
        if (from) {
            m.from = *from;
            m.hasFrom = true;
        }
        if (to) {
            m.to = *to;
            m.hasTo = true;
        }
        m.message = message ? message->text : "";
        m.wrap = (message && message->wrap.has_value()) ? *message->wrap : autoWrap();
        m.type = type;
        m.hasType = hasType;
        m.activate = activate;
        m.centralConnection = centralConnection;
        messages.push_back(std::move(m));
        return true;
    }

    void addNote(const std::vector<std::string>& actorPair, bool singleActor, int placement, const SeqText& message) {
        bool wrap = message.wrap.value_or(autoWrap());
        notes.push_back({actorPair, singleActor, placement, message.text, wrap});
        SeqMessage m;
        m.id = std::to_string(messages.size());
        m.from = actorPair[0];
        m.hasFrom = true;
        m.to = actorPair.size() > 1 ? actorPair[1] : actorPair[0];
        m.hasTo = true;
        m.message = message.text;
        m.wrap = wrap;
        m.type = LINETYPE::NOTE;
        m.hasType = true;
        m.placement = placement;
        m.hasPlacement = true;
        messages.push_back(std::move(m));
    }

    void addLinks(const std::string& actorId, const SeqText& text) {
        auto it = actors.find(actorId);
        if (it == actors.end()) return;
        std::string t = trimStr(text.text);
        if (t.size() < 2 || t.front() != '{' || t.back() != '}') return;
        try {
            YamlValue parsed = parseYaml(t);
            if (parsed.isMap()) {
                for (const auto& [k, v] : parsed.mVal) {
                    if (v.isString()) it->second.links[k] = v.sVal;
                }
            }
        } catch (...) {}
    }

    void addALink(const std::string& actorId, const SeqText& text) {
        auto it = actors.find(actorId);
        if (it == actors.end()) return;
        size_t sep = text.text.find('@');
        if (sep == std::string::npos || sep == 0) return;
        it->second.links[trimStr(text.text.substr(0, sep - 1))] = trimStr(text.text.substr(sep + 1));
    }

    void addProperties(const std::string& actorId, const SeqText& text) {
        auto it = actors.find(actorId);
        if (it == actors.end()) return;
        std::string t = trimStr(text.text);
        if (t.size() < 2 || t.front() != '{' || t.back() != '}') return;
        try {
            YamlValue parsed = parseYaml(t);
            if (parsed.isMap()) {
                for (const auto& [k, v] : parsed.mVal) {
                    if (v.isString()) it->second.properties[k] = v.sVal;
                }
            }
        } catch (...) {}
    }

    void addDetails(const std::string& actorId, const SeqText& text) {
        auto it = actors.find(actorId);
        if (it != actors.end()) it->second.details = text.text;
    }

    void apply(const SeqItem& root) {
        std::vector<const std::vector<SeqItem>*> lists;
        std::vector<size_t> at;
        const SeqItem* item = &root;
        while (true) {
            if (item != nullptr) {
                if (item->kind == SeqItem::List) {
                    lists.push_back(&item->list);
                    at.push_back(0);
                } else if (item->kind == SeqItem::Statement) {
                    applyOne(item->stmt);
                }
            }
            if (lists.empty()) return;
            size_t depth = lists.size() - 1;
            const auto* list = lists[depth];
            if (at[depth] < list->size()) {
                item = &(*list)[at[depth]++];
            } else {
                lists.pop_back();
                at.pop_back();
                item = nullptr;
            }
        }
    }

private:
    void applyOne(const SeqStatement& param) {
        const std::string& actor = param.actor;
        const std::string& t = param.type;
        if (t == "sequenceIndex") {
            SeqMessage m;
            m.id = std::to_string(messages.size());
            m.isNumbering = true;
            m.numbering.start = param.sequenceIndex;
            m.numbering.hasStart = param.hasSequenceIndex;
            m.numbering.step = param.sequenceIndexStep;
            m.numbering.hasStep = param.hasSequenceIndexStep;
            m.numbering.visible = param.sequenceVisible;
            m.wrap = false;
            m.type = param.signalType;
            m.hasType = param.hasSignalType;
            messages.push_back(std::move(m));
        } else if (t == "addParticipant") {
            addActor(
                actor,
                actor,
                param.hasDescription ? &param.description : nullptr,
                param.hasDraw ? &param.draw : nullptr,
                param.hasConfig ? &param.config : nullptr
            );
        } else if (t == "createParticipant") {
            if (actors.count(actor) > 0) {
                detail_db::fail(
                    "It is not possible to have actors with the same id, even if one is destroyed before the next is created. Use 'AS' aliases to simulate the behavior"
                );
            }
            lastCreated_ = actor;
            hasLastCreated_ = true;
            addActor(
                actor,
                actor,
                param.hasDescription ? &param.description : nullptr,
                param.hasDraw ? &param.draw : nullptr,
                param.hasConfig ? &param.config : nullptr
            );
            createdActors[actor] = static_cast<int>(messages.size());
        } else if (t == "destroyParticipant") {
            lastDestroyed_ = actor;
            hasLastDestroyed_ = true;
            destroyedActors[actor] = static_cast<int>(messages.size());
        } else if (t == "activeStart" || t == "activeEnd" || t == "centralConnection" || t == "centralConnectionReverse") {
            addSignal(&actor, nullptr, nullptr, param.signalType, param.hasSignalType);
        } else if (t == "addNote") {
            if (param.hasActorPair) {
                addNote(param.actorPair, false, param.placement, param.text);
            } else {
                addNote({actor, actor}, true, param.placement, param.text);
            }
        } else if (t == "addLinks") {
            addLinks(actor, param.text);
        } else if (t == "addALink") {
            addALink(actor, param.text);
        } else if (t == "addProperties") {
            addProperties(actor, param.text);
        } else if (t == "addDetails") {
            addDetails(actor, param.text);
        } else if (t == "addMessage") {
            if (hasLastCreated_) {
                if (!param.hasTo || param.to != lastCreated_) {
                    detail_db::fail(Pele::cat({"The created participant ", lastCreated_, " does not have an associated creating message after its declaration. Please check the sequence diagram."}));
                }
                hasLastCreated_ = false;
                lastCreated_.clear();
            } else if (hasLastDestroyed_) {
                if ((!param.hasTo || param.to != lastDestroyed_) && (!param.hasFrom || param.from != lastDestroyed_)) {
                    detail_db::fail(Pele::cat({"The destroyed participant ", lastDestroyed_, " does not have an associated destroying message after its declaration. Please check the sequence diagram."}));
                }
                hasLastDestroyed_ = false;
                lastDestroyed_.clear();
            }
            addSignal(
                param.hasFrom ? &param.from : nullptr,
                param.hasTo ? &param.to : nullptr,
                param.hasMsg ? &param.msg : nullptr,
                param.signalType,
                param.hasSignalType,
                param.activate,
                param.hasCentralConnection ? param.centralConnection : 0
            );
        } else if (t == "boxStart") {
            addBox(param.boxData);
        } else if (t == "boxEnd") {
            currentBox_.reset();
        } else if (t == "loopStart") {
            addSignal(nullptr, nullptr, param.hasLoopText ? &param.loopText : nullptr, param.signalType, param.hasSignalType);
        } else if (t == "rectStart") {
            addSignal(nullptr, nullptr, param.hasColor ? &param.color : nullptr, param.signalType, param.hasSignalType);
        } else if (t == "optStart") {
            addSignal(nullptr, nullptr, param.hasOptText ? &param.optText : nullptr, param.signalType, param.hasSignalType);
        } else if (t == "altStart" || t == "else") {
            addSignal(nullptr, nullptr, param.hasAltText ? &param.altText : nullptr, param.signalType, param.hasSignalType);
        } else if (t == "parStart" || t == "and") {
            addSignal(nullptr, nullptr, param.hasParText ? &param.parText : nullptr, param.signalType, param.hasSignalType);
        } else if (t == "criticalStart") {
            addSignal(nullptr, nullptr, param.hasCriticalText ? &param.criticalText : nullptr, param.signalType, param.hasSignalType);
        } else if (t == "option") {
            addSignal(nullptr, nullptr, param.hasOptionText ? &param.optionText : nullptr, param.signalType, param.hasSignalType);
        } else if (t == "breakStart") {
            addSignal(nullptr, nullptr, param.hasBreakText ? &param.breakText : nullptr, param.signalType, param.hasSignalType);
        } else if (
            t == "loopEnd" || t == "rectEnd" || t == "optEnd" || t == "altEnd" ||
            t == "parEnd" || t == "criticalEnd" || t == "breakEnd"
        ) {
            addSignal(nullptr, nullptr, nullptr, param.signalType, param.hasSignalType);
        }
    }
};

// ============================================================================
// Port of src/diagrams/sequence/tokens.ts & lexer.ts
// ============================================================================

namespace detail_lexer {

enum T : int {
    T_END = 0,
    T_ERROR = 1,
    T_INVALID = 2,
    T_NEWLINE = 3,
    T_SD = 4,
    T_create = 5,
    T_box = 6,
    T_restOfLine = 7,
    T_end = 8,
    T_autonumber = 9,
    T_NUM = 10,
    T_off = 11,
    T_activate = 12,
    T_deactivate = 13,
    T_title = 14,
    T_legacy_title = 15,
    T_acc_title = 16,
    T_acc_title_value = 17,
    T_acc_descr = 18,
    T_acc_descr_value = 19,
    T_acc_descr_multiline_value = 20,
    T_loop = 21,
    T_rect = 22,
    T_opt = 23,
    T_alt = 24,
    T_par = 25,
    T_par_over = 26,
    T_critical = 27,
    T_break = 28,
    T_option = 29,
    T_and = 30,
    T_else = 31,
    T_participant = 32,
    T_AS = 33,
    T_participant_actor = 34,
    T_destroy = 35,
    T_note = 36,
    T_over = 37,
    T_links = 38,
    T_link = 39,
    T_properties = 40,
    T_details = 41,
    T_COMMA = 42,
    T_left_of = 43,
    T_right_of = 44,
    T_PLUS = 45,
    T_MINUS = 46,
    T_CENTRAL = 47,
    T_ACTOR = 48,
    T_CONFIG_START = 49,
    T_CONFIG_CONTENT = 50,
    T_CONFIG_END = 51,
    T_TXT = 52,
    T_SOLID_OPEN_ARROW = 53,
    T_DOTTED_OPEN_ARROW = 54,
    T_SOLID_ARROW = 55,
    T_BIDIRECTIONAL_SOLID_ARROW = 56,
    T_DOTTED_ARROW = 57,
    T_BIDIRECTIONAL_DOTTED_ARROW = 58,
    T_SOLID_CROSS = 59,
    T_DOTTED_CROSS = 60,
    T_SOLID_POINT = 61,
    T_DOTTED_POINT = 62,
    T_SOLID_ARROW_TOP = 63,
    T_SOLID_ARROW_BOTTOM = 64,
    T_STICK_ARROW_TOP = 65,
    T_STICK_ARROW_BOTTOM = 66,
    T_SOLID_ARROW_TOP_DOTTED = 67,
    T_SOLID_ARROW_BOTTOM_DOTTED = 68,
    T_STICK_ARROW_TOP_DOTTED = 69,
    T_STICK_ARROW_BOTTOM_DOTTED = 70,
    T_SOLID_ARROW_TOP_REVERSE = 71,
    T_SOLID_ARROW_BOTTOM_REVERSE = 72,
    T_STICK_ARROW_TOP_REVERSE = 73,
    T_STICK_ARROW_BOTTOM_REVERSE = 74,
    T_SOLID_ARROW_TOP_REVERSE_DOTTED = 75,
    T_SOLID_ARROW_BOTTOM_REVERSE_DOTTED = 76,
    T_STICK_ARROW_TOP_REVERSE_DOTTED = 77,
    T_STICK_ARROW_BOTTOM_REVERSE_DOTTED = 78
};

inline const std::vector<std::string>& tokenNames() noexcept {
    static const std::vector<std::string> names = {
        "$end",
        "INVALID",
        "INVALID",
        "NEWLINE",
        "SD",
        "create",
        "box",
        "restOfLine",
        "end",
        "autonumber",
        "NUM",
        "off",
        "activate",
        "deactivate",
        "title",
        "legacy_title",
        "acc_title",
        "acc_title_value",
        "acc_descr",
        "acc_descr_value",
        "acc_descr_multiline_value",
        "loop",
        "rect",
        "opt",
        "alt",
        "par",
        "par_over",
        "critical",
        "break",
        "option",
        "and",
        "else",
        "participant",
        "AS",
        "participant_actor",
        "destroy",
        "note",
        "over",
        "links",
        "link",
        "properties",
        "details",
        ",",
        "left_of",
        "right_of",
        "+",
        "-",
        "()",
        "ACTOR",
        "CONFIG_START",
        "CONFIG_CONTENT",
        "CONFIG_END",
        "TXT",
        "SOLID_OPEN_ARROW",
        "DOTTED_OPEN_ARROW",
        "SOLID_ARROW",
        "BIDIRECTIONAL_SOLID_ARROW",
        "DOTTED_ARROW",
        "BIDIRECTIONAL_DOTTED_ARROW",
        "SOLID_CROSS",
        "DOTTED_CROSS",
        "SOLID_POINT",
        "DOTTED_POINT",
        "SOLID_ARROW_TOP",
        "SOLID_ARROW_BOTTOM",
        "STICK_ARROW_TOP",
        "STICK_ARROW_BOTTOM",
        "SOLID_ARROW_TOP_DOTTED",
        "SOLID_ARROW_BOTTOM_DOTTED",
        "STICK_ARROW_TOP_DOTTED",
        "STICK_ARROW_BOTTOM_DOTTED",
        "SOLID_ARROW_TOP_REVERSE",
        "SOLID_ARROW_BOTTOM_REVERSE",
        "STICK_ARROW_TOP_REVERSE",
        "STICK_ARROW_BOTTOM_REVERSE",
        "SOLID_ARROW_TOP_REVERSE_DOTTED",
        "SOLID_ARROW_BOTTOM_REVERSE_DOTTED",
        "STICK_ARROW_TOP_REVERSE_DOTTED",
        "STICK_ARROW_BOTTOM_REVERSE_DOTTED",
    };
    return names;
}

struct Tokens {
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
};

enum S : int {
    S_INITIAL = 0,
    S_ID = 1,
    S_ALIAS = 2,
    S_LINE = 3,
    S_CONFIG = 4,
    S_acc_title = 5,
    S_acc_descr = 6,
    S_acc_descr_multiline = 7
};

constexpr uint8_t C_ActorStart = 1;
constexpr uint8_t C_ActorRest = 2;
constexpr uint8_t C_Id = 4;
constexpr uint8_t C_IdWide = 8;
constexpr uint8_t C_Line = 16;
constexpr uint8_t C_Menu = 32;

inline const std::array<uint8_t, 128>& classTable() noexcept {
    static const std::array<uint8_t, 128> tab = []() {
        std::array<uint8_t, 128> t{};
        const std::string actorStartEx = "/\\+()<->:\n,;";
        const std::string actorRestEx = "+<->:\n,;";
        const std::string idEx = "<>:\n,;@";
        const std::string lineEx = "#\n;";
        const std::string menuEx = "/\\+()<>:\n,;-";
        for (int c = 0; c < 128; c++) {
            char ch = static_cast<char>(c);
            bool ws = c == 32 || (c >= 9 && c <= 13);
            uint8_t bits = 0;
            if (actorStartEx.find(ch) == std::string::npos) bits |= C_ActorStart;
            if (actorRestEx.find(ch) == std::string::npos) bits |= C_ActorRest;
            if (idEx.find(ch) == std::string::npos) bits |= C_IdWide | (ws ? 0 : C_Id);
            if (lineEx.find(ch) == std::string::npos) bits |= C_Line;
            if (menuEx.find(ch) == std::string::npos && !ws) bits |= C_Menu;
            t[c] = bits;
        }
        return t;
    }();
    return tab;
}

inline Tokens tokenize(const std::string& src) {
    const size_t n = src.size();
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
    std::vector<int> stack;
    int state = S_INITIAL;
    size_t p = 0;
    size_t nextNl = std::string::npos;
    const auto& CLASS_TAB = classTable();

    auto charAt = [&](size_t idx) -> int {
        return idx < n ? static_cast<unsigned char>(src[idx]) : -1;
    };
    auto isSpaceAt = [&](size_t idx, size_t* nextOut = nullptr) -> bool {
        if (idx >= n) {
            if (nextOut) *nextOut = idx;
            return false;
        }
        unsigned char b = static_cast<unsigned char>(src[idx]);
        if (b < 128) {
            if (nextOut) *nextOut = idx + 1;
            return b == 32 || (b >= 9 && b <= 13);
        }
        size_t next = idx;
        int cp = charCodeAtUtf8(src, idx, &next);
        if (nextOut) *nextOut = next;
        return isSpace(cp);
    };

    auto emit = [&](int type, size_t s, size_t e) {
        types.push_back(type);
        texts.push_back(src.substr(s, e - s));
        starts.push_back(s);
        p = e;
    };
    auto emitText = [&](int type, size_t s, size_t e, const std::string& text) {
        types.push_back(type);
        texts.push_back(text);
        starts.push_back(s);
        p = e;
    };
    auto push = [&](int s) {
        stack.push_back(state);
        state = s;
    };
    auto pop = [&]() {
        if (!stack.empty()) {
            state = stack.back();
            stack.pop_back();
        }
    };
    auto eol = [&](size_t q) -> size_t {
        if (nextNl == std::string::npos || nextNl < q) {
            size_t found = src.find('\n', q);
            nextNl = (found == std::string::npos) ? n : found;
        }
        return nextNl;
    };
    auto has = [&](size_t q, uint8_t bit) -> bool {
        if (q >= n) return false;
        unsigned char c = static_cast<unsigned char>(src[q]);
        if (c < 128) return (CLASS_TAB[c] & bit) != 0;
        if (bit == C_Id || bit == C_Menu) {
            size_t start = q;
            while (start > 0 && (static_cast<unsigned char>(src[start]) & 0xC0) == 0x80) start--;
            return !isSpace(charCodeAtUtf8(src, start));
        }
        return true;
    };
    auto ci = [&](size_t q, const char* wordLit) -> bool {
        for (size_t k = 0; wordLit[k] != '\0'; k++) {
            int w = static_cast<unsigned char>(wordLit[k]);
            int c = charAt(q + k);
            if (c < 0) return false;
            if (w >= 97 && w <= 122 ? (c | 32) != w : c != w) return false;
        }
        return true;
    };
    auto word = [&](const char* lit) -> bool {
        size_t len = std::strlen(lit);
        return ci(p, lit) && !isWord(charAt(p + len));
    };
    auto skipWs = [&](size_t q) -> size_t {
        while (q < n) {
            size_t next = q;
            if (!isSpaceAt(q, &next)) break;
            q = next;
        }
        return q;
    };
    auto skipSameLine = [&]() -> bool {
        size_t q = p;
        while (q < n) {
            if (src[q] == '\n') break;
            size_t next = q;
            if (!isSpaceAt(q, &next)) break;
            q = next;
        }
        if (q == p) return false;
        p = q;
        return true;
    };
    auto lineRun = [&](size_t q) -> size_t {
        while (q < n && has(q, C_Line)) q++;
        return q;
    };
    auto begin = [&](int type, size_t len, int next) {
        emit(type, p, p + len);
        push(next);
    };
    auto keyword = [&](const char* lit, int type, int next = -1) -> bool {
        if (!word(lit)) return false;
        size_t len = std::strlen(lit);
        emit(type, p, p + len);
        if (next >= 0) push(next);
        return true;
    };
    auto menu = [&](const char* lit, int type) -> bool {
        if (!ci(p, lit)) return false;
        size_t len = std::strlen(lit);
        size_t q = p + len;
        int c = charAt(q);
        if (c != 32 && c != 9) return false;
        while (c == 32 || c == 9) c = charAt(++q);
        if (q >= n || !has(q, C_Menu)) return false;
        emit(type, p, p + len);
        return true;
    };

    auto arrowAhead = [&](size_t q) -> bool {
        int c = charAt(q);
        int d = charAt(q + 1);
        if (c == 45) {
            if (d == 120 || d == 88 || d == 41 || d == 92 || d == 47 || d == 45) return true;
            int e = charAt(q + 2);
            return d == 124 && (e == 92 || e == 47);
        }
        if (c == 47 || c == 92) return (d == 124 || d == c) && charAt(q + 2) == 45;
        return c == 40 && d == 41;
    };

    auto actor = [&]() {
        size_t q = p + 1;
        while (q < n && has(q, C_ActorStart)) q++;
        while (q < n && !arrowAhead(q)) {
            size_t r = q;
            while (charAt(r) == 45) r++;
            if (r >= n || !has(r, C_ActorRest)) break;
            r++;
            while (r < n && has(r, C_ActorRest)) r++;
            q = r;
        }
        emitText(T_ACTOR, p, q, trimStr(src.substr(p, q - p)));
    };

    auto number = [&]() -> bool {
        size_t q = p;
        while (isDigit(charAt(q))) q++;
        bool whole = q > p;
        if (charAt(q) == 46 && isDigit(charAt(q + 1))) {
            size_t r = isDigit(charAt(q + 2)) ? q + 3 : q + 2;
            int after = charAt(r);
            if (after == 32 || after == 10) {
                emit(T_NUM, p, r);
                return true;
            }
        }
        int after = charAt(q);
        if (whole && (after == 32 || after == 10)) {
            emit(T_NUM, p, q);
            return true;
        }
        return false;
    };

    auto dash = [&]() {
        int d = charAt(p + 1);
        int e = charAt(p + 2);
        if (d == 62) emit(e == 62 ? T_SOLID_ARROW : T_SOLID_OPEN_ARROW, p, p + (e == 62 ? 3 : 2));
        else if (d == 120 || d == 88) emit(T_SOLID_CROSS, p, p + 2);
        else if (d == 41) emit(T_SOLID_POINT, p, p + 2);
        else if (d == 124 && e == 92) emit(T_SOLID_ARROW_TOP, p, p + 3);
        else if (d == 124 && e == 47) emit(T_SOLID_ARROW_BOTTOM, p, p + 3);
        else if (d == 92 && e == 92) emit(T_STICK_ARROW_TOP, p, p + 3);
        else if (d == 47 && e == 47) emit(T_STICK_ARROW_BOTTOM, p, p + 3);
        else if (d == 45) {
            int f = charAt(p + 3);
            if (e == 62) emit(f == 62 ? T_DOTTED_ARROW : T_DOTTED_OPEN_ARROW, p, p + (f == 62 ? 4 : 3));
            else if (e == 120 || e == 88) emit(T_DOTTED_CROSS, p, p + 3);
            else if (e == 41) emit(T_DOTTED_POINT, p, p + 3);
            else if (e == 124 && f == 92) emit(T_SOLID_ARROW_TOP_DOTTED, p, p + 4);
            else if (e == 124 && f == 47) emit(T_SOLID_ARROW_BOTTOM_DOTTED, p, p + 4);
            else if (e == 92 && f == 92) emit(T_STICK_ARROW_TOP_DOTTED, p, p + 4);
            else if (e == 47 && f == 47) emit(T_STICK_ARROW_BOTTOM_DOTTED, p, p + 4);
            else emit(T_MINUS, p, p + 1);
        } else emit(T_MINUS, p, p + 1);
    };

    auto reverse = [&](int c) -> bool {
        int d = charAt(p + 1);
        if ((d != 124 && d != c) || charAt(p + 2) != 45) return false;
        bool dotted = charAt(p + 3) == 45;
        bool top = c == 47;
        int type;
        if (d == 124) {
            if (dotted) type = top ? T_SOLID_ARROW_TOP_REVERSE_DOTTED : T_SOLID_ARROW_BOTTOM_REVERSE_DOTTED;
            else type = top ? T_SOLID_ARROW_TOP_REVERSE : T_SOLID_ARROW_BOTTOM_REVERSE;
        } else if (dotted) type = top ? T_STICK_ARROW_TOP_REVERSE_DOTTED : T_STICK_ARROW_BOTTOM_REVERSE_DOTTED;
        else type = top ? T_STICK_ARROW_TOP_REVERSE : T_STICK_ARROW_BOTTOM_REVERSE;
        emit(type, p, p + (dotted ? 4 : 3));
        return true;
    };

    auto title = [&]() -> bool {
        if (!ci(p, "title")) return false;
        size_t q = p + 5;
        bool legacy = charAt(q) == 58;
        if (legacy) q++;
        size_t e = q;
        while (e < n && charAt(e) != 10) {
            size_t next = e;
            if (!isSpaceAt(e, &next)) break;
            e = next;
        }
        if (e == n || charAt(e) == 10) {
            p = e;
            return true;
        }
        int sep = charAt(q);
        size_t afterSep = q;
        bool sepSpace = isSpaceAt(q, &afterSep);
        if (!sepSpace || sep == 13 || !has(q + 1, C_Line)) return false;
        emit(legacy ? T_legacy_title : T_title, p, lineRun(q + 1));
        return true;
    };

    auto acc = [&]() -> bool {
        bool descr = ci(p, "accdescr");
        if (!descr && !ci(p, "acctitle")) return false;
        size_t q = skipWs(p + 8);
        int c = charAt(q);
        if (c == 58) {
            emit(descr ? T_acc_descr : T_acc_title, p, skipWs(q + 1));
            push(descr ? S_acc_descr : S_acc_title);
            return true;
        }
        if (descr && c == 123) {
            p = skipWs(q + 1);
            push(S_acc_descr_multiline);
            return true;
        }
        return false;
    };

    auto keywords = [&](int c) -> bool {
        switch (c | 32) {
            case 97:
                return (
                    keyword("actor", T_participant_actor, S_ID) ||
                    keyword("alt", T_alt, S_LINE) ||
                    keyword("and", T_and, S_LINE) ||
                    keyword("activate", T_activate, S_ID) ||
                    acc() ||
                    keyword("autonumber", T_autonumber)
                );
            case 98:
                return keyword("box", T_box, S_LINE) || keyword("break", T_break, S_LINE);
            case 99:
                return keyword("create", T_create) || keyword("critical", T_critical, S_LINE);
            case 100:
                return (
                    keyword("destroy", T_destroy, S_ID) || menu("details", T_details) || keyword("deactivate", T_deactivate, S_ID)
                );
            case 101:
                return keyword("else", T_else, S_LINE) || keyword("end", T_end);
            case 108:
                return (
                    keyword("loop", T_loop, S_LINE) || keyword("left of", T_left_of) || menu("links", T_links) || menu("link", T_link)
                );
            case 110:
                return keyword("note", T_note);
            case 111:
                return (
                    keyword("opt", T_opt, S_LINE) || keyword("option", T_option, S_LINE) || keyword("over", T_over) || keyword("off", T_off)
                );
            case 112:
                return (
                    keyword("participant", T_participant, S_ID) ||
                    keyword("par", T_par, S_LINE) ||
                    keyword("par_over", T_par_over, S_LINE) ||
                    menu("properties", T_properties)
                );
            case 114:
                return keyword("rect", T_rect, S_LINE) || keyword("right of", T_right_of);
            case 115:
                return keyword("sequencediagram", T_SD);
            case 116:
                return title();
            default:
                return false;
        }
    };

    auto initial = [&]() {
        int c = charAt(p);
        if (c == 10) {
            size_t q = p + 1;
            while (charAt(q) == 10) q++;
            emit(T_NEWLINE, p, q);
            return;
        }
        size_t nextWs = p;
        if (isSpaceAt(p, &nextWs)) {
            p = skipWs(nextWs);
            return;
        }
        if (c == 35 || (c == 37 && charAt(p + 1) != 123)) {
            p = eol(p);
            return;
        }
        if ((isDigit(c) || c == 46) && number()) return;
        if (c < 128 && (c | 32) >= 97 && (c | 32) <= 122 && keywords(c)) return;
        if (c == 44) emit(T_COMMA, p, p + 1);
        else if (c == 59) emit(T_NEWLINE, p, p + 1);
        else if (has(p, C_ActorStart)) actor();
        else if (c == 45) dash();
        else if (c == 60 && startsWith(src, "<<->>", p)) emit(T_BIDIRECTIONAL_SOLID_ARROW, p, p + 5);
        else if (c == 60 && startsWith(src, "<<-->>", p)) emit(T_BIDIRECTIONAL_DOTTED_ARROW, p, p + 6);
        else if ((c == 47 || c == 92) && reverse(c)) return;
        else if (c == 58) emit(T_TXT, p, lineRun(p + 1));
        else if (c == 43) emit(T_PLUS, p, p + 1);
        else if (c == 40 && charAt(p + 1) == 41) emit(T_CENTRAL, p, p + 2);
        else emit(T_INVALID, p, p + 1);
    };

    auto id = [&]() -> bool {
        if (skipSameLine()) return true;
        int c = charAt(p);
        if (c == 35) {
            p = eol(p);
            return true;
        }
        if (c == 64 && charAt(p + 1) == 123) {
            begin(T_CONFIG_START, 2, S_CONFIG);
            return true;
        }
        size_t a = p;
        while (a < n && has(a, C_Id)) a++;
        if (a > p) {
            size_t w = skipWs(a);
            if (charAt(w) == 64 && charAt(w + 1) == 123) {
                emit(T_ACTOR, p, a);
                return true;
            }
            if (w > a && ci(w, "as") && isSpaceAt(w + 2)) {
                emit(T_ACTOR, p, a);
                push(S_ALIAS);
                return true;
            }
        }
        size_t b = a;
        while (b < n && has(b, C_IdWide)) b++;
        if (b > p) {
            int end = charAt(b);
            size_t cut = b;
            if (b < n && end != 10 && end != 59) {
                cut--;
                while (cut > p && charAt(cut) != 35) cut--;
            }
            if (cut > p) {
                emitText(T_ACTOR, p, cut, trimStr(src.substr(p, cut - p)));
                pop();
                return true;
            }
        }
        if (charAt(b) == 60) {
            emit(T_INVALID, p, eol(p));
            pop();
            return true;
        }
        if (c != 10) {
            size_t e = eol(p);
            emitText(T_INVALID, p, e, trimStr(src.substr(p, e - p)));
            pop();
            return true;
        }
        return false;
    };

    auto step = [&]() -> bool {
        switch (state) {
            case S_INITIAL:
                initial();
                return true;
            case S_ID:
                return id();
            case S_ALIAS:
                if (skipSameLine()) return true;
                if (charAt(p) == 35) {
                    p = eol(p);
                    return true;
                }
                pop();
                pop();
                if (word("as")) begin(T_AS, 2, S_LINE);
                else emit(T_NEWLINE, p, p);
                return true;
            case S_LINE:
                if (skipSameLine()) return true;
                if (charAt(p) == 35) {
                    p = eol(p);
                    return true;
                }
                emit(T_restOfLine, p, lineRun(p));
                pop();
                return true;
            case S_CONFIG:
                if (charAt(p) != 125) {
                    size_t q = src.find('}', p);
                    if (q == std::string::npos) q = n;
                    emit(T_CONFIG_CONTENT, p, q);
                    return true;
                } else {
                    size_t w = skipWs(p + 1);
                    bool alias = w > p + 1 && ci(w, "as") && isSpaceAt(w + 2);
                    emit(T_CONFIG_END, p, p + 1);
                    pop();
                    if (alias) push(S_ALIAS);
                    else pop();
                    return true;
                }
            case S_acc_title:
            case S_acc_descr:
                emit(state == S_acc_title ? T_acc_title_value : T_acc_descr_value, p, eol(p));
                pop();
                return true;
            default:
                if (charAt(p) == 125) {
                    p++;
                    pop();
                } else {
                    size_t q = src.find('}', p);
                    if (q == std::string::npos) q = n;
                    emit(T_acc_descr_multiline_value, p, q);
                }
                return true;
        }
    };

    while (p < n) {
        if (!step()) {
            types.push_back(T_ERROR);
            texts.push_back("");
            starts.push_back(p);
            return {types, texts, starts};
        }
    }

    if (state == S_INITIAL || state == S_ALIAS) emit(T_NEWLINE, n, n);
    else if (state == S_LINE) emit(T_restOfLine, n, n);
    else if (state == S_acc_title) emit(T_acc_title_value, n, n);
    else if (state == S_acc_descr) emit(T_acc_descr_value, n, n);
    else if (state == S_acc_descr_multiline) emit(T_acc_descr_multiline_value, n, n);
    types.push_back(T_END);
    texts.push_back("");
    starts.push_back(n);
    return {types, texts, starts};
}

} // namespace detail_lexer

// ============================================================================
// Port of src/diagrams/sequence/parser.ts
// ============================================================================

namespace detail_parser {

using namespace detail_lexer;

enum class K : int {
    Top,
    Box,
    Loop,
    Rect,
    Opt,
    Break,
    Alt,
    Par,
    Critical
};

struct FrameSection {
    std::vector<SeqItem> list;
    std::string dividerText;
};

struct ParseFrame {
    K kind = K::Top;
    std::string text;
    int startType = 0;
    std::vector<SeqItem> list;
    std::vector<FrameSection> sections;
};

inline int signalForToken(int tok) noexcept {
    switch (tok) {
        case T_SOLID_OPEN_ARROW: return LINETYPE::SOLID_OPEN;
        case T_DOTTED_OPEN_ARROW: return LINETYPE::DOTTED_OPEN;
        case T_SOLID_ARROW: return LINETYPE::SOLID;
        case T_DOTTED_ARROW: return LINETYPE::DOTTED;
        case T_BIDIRECTIONAL_SOLID_ARROW: return LINETYPE::BIDIRECTIONAL_SOLID;
        case T_BIDIRECTIONAL_DOTTED_ARROW: return LINETYPE::BIDIRECTIONAL_DOTTED;
        case T_SOLID_CROSS: return LINETYPE::SOLID_CROSS;
        case T_DOTTED_CROSS: return LINETYPE::DOTTED_CROSS;
        case T_SOLID_POINT: return LINETYPE::SOLID_POINT;
        case T_DOTTED_POINT: return LINETYPE::DOTTED_POINT;
        case T_SOLID_ARROW_TOP: return LINETYPE::SOLID_TOP;
        case T_SOLID_ARROW_BOTTOM: return LINETYPE::SOLID_BOTTOM;
        case T_STICK_ARROW_TOP: return LINETYPE::STICK_TOP;
        case T_STICK_ARROW_BOTTOM: return LINETYPE::STICK_BOTTOM;
        case T_SOLID_ARROW_TOP_DOTTED: return LINETYPE::SOLID_TOP_DOTTED;
        case T_SOLID_ARROW_BOTTOM_DOTTED: return LINETYPE::SOLID_BOTTOM_DOTTED;
        case T_STICK_ARROW_TOP_DOTTED: return LINETYPE::STICK_TOP_DOTTED;
        case T_STICK_ARROW_BOTTOM_DOTTED: return LINETYPE::STICK_BOTTOM_DOTTED;
        case T_SOLID_ARROW_TOP_REVERSE: return LINETYPE::SOLID_ARROW_TOP_REVERSE;
        case T_SOLID_ARROW_BOTTOM_REVERSE: return LINETYPE::SOLID_ARROW_BOTTOM_REVERSE;
        case T_STICK_ARROW_TOP_REVERSE: return LINETYPE::STICK_ARROW_TOP_REVERSE;
        case T_STICK_ARROW_BOTTOM_REVERSE: return LINETYPE::STICK_ARROW_BOTTOM_REVERSE;
        case T_SOLID_ARROW_TOP_REVERSE_DOTTED: return LINETYPE::SOLID_ARROW_TOP_REVERSE_DOTTED;
        case T_SOLID_ARROW_BOTTOM_REVERSE_DOTTED: return LINETYPE::SOLID_ARROW_BOTTOM_REVERSE_DOTTED;
        case T_STICK_ARROW_TOP_REVERSE_DOTTED: return LINETYPE::STICK_ARROW_TOP_REVERSE_DOTTED;
        case T_STICK_ARROW_BOTTOM_REVERSE_DOTTED: return LINETYPE::STICK_ARROW_BOTTOM_REVERSE_DOTTED;
        default: return -1;
    }
}

inline K blockForToken(int tok) noexcept {
    switch (tok) {
        case T_loop: return K::Loop;
        case T_rect: return K::Rect;
        case T_opt: return K::Opt;
        case T_break: return K::Break;
        case T_alt: return K::Alt;
        case T_par: return K::Par;
        case T_par_over: return K::Par;
        case T_critical: return K::Critical;
        case T_box: return K::Box;
        default: return K::Top;
    }
}

inline void parseSequence(const std::string& src, SeqDb& db) {
    auto [types, texts, starts] = tokenize(src);
    size_t i = 0;
    const auto& names = tokenNames();
    const std::string SIGNAL_TOKENS =
        "'SOLID_OPEN_ARROW', 'DOTTED_OPEN_ARROW', 'SOLID_ARROW', 'DOTTED_ARROW', 'SOLID_CROSS', 'DOTTED_CROSS', 'SOLID_POINT', 'DOTTED_POINT'";

    auto fail = [&](const std::string& expected) -> void {
        int t = types[i];
        if (t == T_ERROR) throw syntaxError("sequence", src, starts[i], "", true);
        throw syntaxError("sequence",  src,  starts[i], Pele::cat({"Expecting ", expected, ", got '", names[t], "'"}));
    };
    auto expect = [&](int type) -> std::string {
        if (types[i] != type) fail(Pele::cat({"'", names[type], "'"}));
        return texts[i++];
    };
    auto actor = [&]() -> SeqStatement {
        int t = types[i];
        if (t != T_ACTOR && t != T_link) fail("'ACTOR'");
        SeqStatement st;
        st.type = "addParticipant";
        st.actor = texts[i++];
        return st;
    };
    auto text2 = [&]() -> SeqText {
        std::string raw = trimStr(expect(T_TXT));
        return db.parseMessage(raw.empty() ? "" : raw.substr(1));
    };

    auto participant = [&]() -> SeqStatement {
        int t = types[i++];
        if (t == T_destroy) {
            SeqStatement node = actor();
            expect(T_NEWLINE);
            node.type = "destroyParticipant";
            return node;
        }
        SeqStatement node = actor();
        if (types[i] == T_CONFIG_START && types[i - 1] == T_ACTOR) {
            i++;
            std::string content = expect(T_CONFIG_CONTENT);
            expect(T_CONFIG_END);
            node.config = trimStr(content);
            node.hasConfig = true;
        }
        node.draw = (t == T_participant) ? "participant" : "actor";
        node.hasDraw = true;
        if (types[i] == T_AS) {
            i++;
            std::string alias = expect(T_restOfLine);
            expect(T_NEWLINE);
            node.description = db.parseMessage(alias);
            node.hasDescription = true;
        } else if (types[i] == T_NEWLINE) {
            i++;
        } else {
            fail("'AS', 'NEWLINE'");
        }
        return node;
    };

    auto signal = [&](const SeqStatement& from) -> SeqItem {
        bool centralFrom = false;
        if (types[i] == T_CENTRAL) {
            centralFrom = true;
            i++;
        }
        int signalType = signalForToken(types[i]);
        if (signalType < 0) fail(SIGNAL_TOKENS);
        i++;
        int mark = types[i];
        if (mark == T_CENTRAL || (!centralFrom && (mark == T_PLUS || mark == T_MINUS))) i++;
        SeqStatement to = actor();
        SeqText msg = text2();
        SeqStatement message;
        message.type = "addMessage";
        message.from = from.actor;
        message.hasFrom = true;
        message.to = to.actor;
        message.hasTo = true;
        message.signalType = signalType;
        message.hasSignalType = true;
        message.msg = msg;
        message.hasMsg = true;

        std::vector<SeqItem> out;
        out.push_back(SeqItem::makeStmt(from));
        out.push_back(SeqItem::makeStmt(to));

        if (centralFrom) {
            bool dual = (mark == T_CENTRAL);
            message.activate = dual;
            message.centralConnection = dual ? LINETYPE::CENTRAL_CONNECTION_DUAL : LINETYPE::CENTRAL_CONNECTION_REVERSE;
            message.hasCentralConnection = true;
            out.push_back(SeqItem::makeStmt(message));
            if (dual) {
                SeqStatement cc;
                cc.type = "centralConnection";
                cc.signalType = LINETYPE::CENTRAL_CONNECTION;
                cc.hasSignalType = true;
                cc.actor = to.actor;
                out.push_back(SeqItem::makeStmt(cc));
            }
            SeqStatement ccr;
            ccr.type = "centralConnectionReverse";
            ccr.signalType = LINETYPE::CENTRAL_CONNECTION_REVERSE;
            ccr.hasSignalType = true;
            ccr.actor = from.actor;
            out.push_back(SeqItem::makeStmt(ccr));
        } else if (mark == T_PLUS) {
            message.activate = true;
            out.push_back(SeqItem::makeStmt(message));
            SeqStatement act;
            act.type = "activeStart";
            act.signalType = LINETYPE::ACTIVE_START;
            act.hasSignalType = true;
            act.actor = to.actor;
            out.push_back(SeqItem::makeStmt(act));
        } else if (mark == T_MINUS) {
            out.push_back(SeqItem::makeStmt(message));
            SeqStatement deact;
            deact.type = "activeEnd";
            deact.signalType = LINETYPE::ACTIVE_END;
            deact.hasSignalType = true;
            deact.actor = from.actor;
            out.push_back(SeqItem::makeStmt(deact));
        } else if (mark == T_CENTRAL) {
            message.activate = true;
            message.centralConnection = LINETYPE::CENTRAL_CONNECTION;
            message.hasCentralConnection = true;
            out.push_back(SeqItem::makeStmt(message));
            SeqStatement cc;
            cc.type = "centralConnection";
            cc.signalType = LINETYPE::CENTRAL_CONNECTION;
            cc.hasSignalType = true;
            cc.actor = to.actor;
            out.push_back(SeqItem::makeStmt(cc));
        } else {
            out.push_back(SeqItem::makeStmt(message));
        }
        return SeqItem::makeList(std::move(out));
    };

    auto menu = [&](const std::string& type) -> SeqItem {
        i++;
        SeqStatement who = actor();
        SeqStatement st;
        st.type = type;
        st.actor = who.actor;
        st.text = text2();
        st.hasText = true;
        return SeqItem::makeList({SeqItem::makeStmt(who), SeqItem::makeStmt(st)});
    };

    auto note = [&]() -> SeqItem {
        i++;
        int t = types[i];
        if (t == T_over) {
            i++;
            SeqStatement first = actor();
            SeqItem who = SeqItem::makeStmt(first);
            std::vector<std::string> ids = {first.actor, first.actor};
            if (types[i] == T_COMMA) {
                i++;
                SeqStatement second = actor();
                who = SeqItem::makeList({SeqItem::makeStmt(first), SeqItem::makeStmt(second)});
                ids = {first.actor, second.actor};
            }
            SeqStatement st;
            st.type = "addNote";
            st.placement = PLACEMENT::OVER;
            st.hasPlacement = true;
            st.actorPair = std::move(ids);
            st.hasActorPair = true;
            st.text = text2();
            st.hasText = true;
            return SeqItem::makeList({std::move(who), SeqItem::makeStmt(st)});
        }
        if (t != T_left_of && t != T_right_of) fail("'over', 'left_of', 'right_of'");
        i++;
        SeqStatement who = actor();
        int placement = (t == T_left_of) ? PLACEMENT::LEFTOF : PLACEMENT::RIGHTOF;
        SeqStatement st;
        st.type = "addNote";
        st.placement = placement;
        st.hasPlacement = true;
        st.actor = who.actor;
        st.text = text2();
        st.hasText = true;
        return SeqItem::makeList({SeqItem::makeStmt(who), SeqItem::makeStmt(st)});
    };

    auto autonumber = [&]() -> SeqStatement {
        i++;
        SeqStatement node;
        node.type = "sequenceIndex";
        node.sequenceVisible = true;
        node.signalType = LINETYPE::AUTONUMBER;
        node.hasSignalType = true;
        if (types[i] == T_NUM) {
            node.sequenceIndex = std::stod(texts[i++]);
            node.hasSequenceIndex = true;
            node.sequenceIndexStep = (types[i] == T_NUM) ? std::stod(texts[i++]) : 1.0;
            node.hasSequenceIndexStep = true;
        } else if (types[i] == T_off) {
            i++;
            node.sequenceVisible = false;
        }
        if (types[i] != T_NEWLINE) fail("'NEWLINE', 'NUM', 'off'");
        i++;
        return node;
    };

    auto fold = [&](ParseFrame& frame, const std::string& divider, int keyKind, int signalType) -> std::vector<SeqItem> {
        std::vector<SeqItem> list = std::move(frame.list);
        for (int k = static_cast<int>(frame.sections.size()) - 1; k >= 0; k--) {
            SeqStatement head;
            head.type = divider;
            head.signalType = signalType;
            head.hasSignalType = true;
            SeqText parsed = db.parseMessage(frame.sections[k].dividerText);
            if (keyKind == 0) { head.altText = parsed; head.hasAltText = true; }
            else if (keyKind == 1) { head.parText = parsed; head.hasParText = true; }
            else { head.optionText = parsed; head.hasOptionText = true; }
            std::vector<SeqItem> combined = std::move(frame.sections[k].list);
            combined.push_back(SeqItem::makeStmt(head));
            combined.push_back(SeqItem::makeList(std::move(list)));
            list = std::move(combined);
        }
        return list;
    };

    auto close = [&](ParseFrame& frame) -> SeqItem {
        const std::string& text = frame.text;
        std::vector<SeqItem> list;
        switch (frame.kind) {
            case K::Box: {
                list = std::move(frame.list);
                SeqStatement s0;
                s0.type = "boxStart";
                s0.boxData = db.parseBoxData(text);
                s0.hasBoxData = true;
                list[0] = SeqItem::makeStmt(s0);
                SeqStatement sEnd;
                sEnd.type = "boxEnd";
                sEnd.boxText = text;
                list.push_back(SeqItem::makeStmt(sEnd));
                break;
            }
            case K::Loop: {
                list = std::move(frame.list);
                SeqStatement s0;
                s0.type = "loopStart";
                s0.loopText = db.parseMessage(text);
                s0.hasLoopText = true;
                s0.signalType = LINETYPE::LOOP_START;
                s0.hasSignalType = true;
                list[0] = SeqItem::makeStmt(s0);
                SeqStatement sEnd;
                sEnd.type = "loopEnd";
                sEnd.signalType = LINETYPE::LOOP_END;
                sEnd.hasSignalType = true;
                list.push_back(SeqItem::makeStmt(sEnd));
                break;
            }
            case K::Rect: {
                list = std::move(frame.list);
                SeqStatement s0;
                s0.type = "rectStart";
                s0.color = db.parseMessage(text);
                s0.hasColor = true;
                s0.signalType = LINETYPE::RECT_START;
                s0.hasSignalType = true;
                list[0] = SeqItem::makeStmt(s0);
                SeqStatement sEnd;
                sEnd.type = "rectEnd";
                sEnd.color = db.parseMessage(text);
                sEnd.hasColor = true;
                sEnd.signalType = LINETYPE::RECT_END;
                sEnd.hasSignalType = true;
                list.push_back(SeqItem::makeStmt(sEnd));
                break;
            }
            case K::Opt: {
                list = std::move(frame.list);
                SeqStatement s0;
                s0.type = "optStart";
                s0.optText = db.parseMessage(text);
                s0.hasOptText = true;
                s0.signalType = LINETYPE::OPT_START;
                s0.hasSignalType = true;
                list[0] = SeqItem::makeStmt(s0);
                SeqStatement sEnd;
                sEnd.type = "optEnd";
                sEnd.optText = db.parseMessage(text);
                sEnd.hasOptText = true;
                sEnd.signalType = LINETYPE::OPT_END;
                sEnd.hasSignalType = true;
                list.push_back(SeqItem::makeStmt(sEnd));
                break;
            }
            case K::Break: {
                list = std::move(frame.list);
                SeqStatement s0;
                s0.type = "breakStart";
                s0.breakText = db.parseMessage(text);
                s0.hasBreakText = true;
                s0.signalType = LINETYPE::BREAK_START;
                s0.hasSignalType = true;
                list[0] = SeqItem::makeStmt(s0);
                SeqStatement sEnd;
                sEnd.type = "breakEnd";
                sEnd.optText = db.parseMessage(text);
                sEnd.hasOptText = true;
                sEnd.signalType = LINETYPE::BREAK_END;
                sEnd.hasSignalType = true;
                list.push_back(SeqItem::makeStmt(sEnd));
                break;
            }
            case K::Alt: {
                list = fold(frame, "else", 0, LINETYPE::ALT_ELSE);
                SeqStatement s0;
                s0.type = "altStart";
                s0.altText = db.parseMessage(text);
                s0.hasAltText = true;
                s0.signalType = LINETYPE::ALT_START;
                s0.hasSignalType = true;
                list[0] = SeqItem::makeStmt(s0);
                SeqStatement sEnd;
                sEnd.type = "altEnd";
                sEnd.signalType = LINETYPE::ALT_END;
                sEnd.hasSignalType = true;
                list.push_back(SeqItem::makeStmt(sEnd));
                break;
            }
            case K::Par: {
                list = fold(frame, "and", 1, LINETYPE::PAR_AND);
                SeqStatement s0;
                s0.type = "parStart";
                s0.parText = db.parseMessage(text);
                s0.hasParText = true;
                s0.signalType = frame.startType;
                s0.hasSignalType = true;
                list[0] = SeqItem::makeStmt(s0);
                SeqStatement sEnd;
                sEnd.type = "parEnd";
                sEnd.signalType = LINETYPE::PAR_END;
                sEnd.hasSignalType = true;
                list.push_back(SeqItem::makeStmt(sEnd));
                break;
            }
            default: {
                list = fold(frame, "option", 2, LINETYPE::CRITICAL_OPTION);
                SeqStatement s0;
                s0.type = "criticalStart";
                s0.criticalText = db.parseMessage(text);
                s0.hasCriticalText = true;
                s0.signalType = LINETYPE::CRITICAL_START;
                s0.hasSignalType = true;
                list[0] = SeqItem::makeStmt(s0);
                SeqStatement sEnd;
                sEnd.type = "criticalEnd";
                sEnd.signalType = LINETYPE::CRITICAL_END;
                sEnd.hasSignalType = true;
                list.push_back(SeqItem::makeStmt(sEnd));
                break;
            }
        }
        return SeqItem::makeList(std::move(list));
    };

    while (types[i] == T_NEWLINE) i++;
    expect(T_SD);

    std::vector<ParseFrame> stack;
    ParseFrame frame;
    frame.kind = K::Top;

    while (true) {
        int t = types[i];
        if (frame.kind == K::Box) {
            if (t == T_participant || t == T_participant_actor || t == T_destroy) {
                frame.list.push_back(SeqItem::makeStmt(participant()));
            } else if (t == T_NEWLINE) {
                frame.list.push_back(SeqItem::makeEmpty());
                i++;
            } else if (t == T_end) {
                i++;
                SeqItem done = close(frame);
                frame = std::move(stack.back());
                stack.pop_back();
                frame.list.push_back(std::move(done));
            } else {
                fail("'end', 'NEWLINE', 'participant', 'participant_actor', 'destroy'");
            }
            continue;
        }
        switch (t) {
            case T_NEWLINE:
            case T_INVALID:
                frame.list.push_back(SeqItem::makeEmpty());
                i++;
                break;
            case T_participant:
            case T_participant_actor:
            case T_destroy:
                frame.list.push_back(SeqItem::makeStmt(participant()));
                break;
            case T_create: {
                i++;
                int next = types[i];
                if (next != T_participant && next != T_participant_actor && next != T_destroy) {
                    fail("'participant', 'participant_actor', 'destroy'");
                }
                SeqStatement node = participant();
                node.type = "createParticipant";
                frame.list.push_back(SeqItem::makeStmt(node));
                break;
            }
            case T_ACTOR:
                frame.list.push_back(signal(actor()));
                expect(T_NEWLINE);
                break;
            case T_link: {
                int next = types[i + 1];
                frame.list.push_back(next == T_ACTOR || next == T_link ? menu("addALink") : signal(actor()));
                expect(T_NEWLINE);
                break;
            }
            case T_links:
                frame.list.push_back(menu("addLinks"));
                expect(T_NEWLINE);
                break;
            case T_properties:
                frame.list.push_back(menu("addProperties"));
                expect(T_NEWLINE);
                break;
            case T_details:
                frame.list.push_back(menu("addDetails"));
                expect(T_NEWLINE);
                break;
            case T_note:
                frame.list.push_back(note());
                expect(T_NEWLINE);
                break;
            case T_autonumber:
                frame.list.push_back(SeqItem::makeStmt(autonumber()));
                break;
            case T_activate:
            case T_deactivate: {
                i++;
                SeqStatement who = actor();
                expect(T_NEWLINE);
                SeqStatement st;
                st.type = (t == T_activate) ? "activeStart" : "activeEnd";
                st.signalType = (t == T_activate) ? LINETYPE::ACTIVE_START : LINETYPE::ACTIVE_END;
                st.hasSignalType = true;
                st.actor = who.actor;
                frame.list.push_back(SeqItem::makeStmt(st));
                break;
            }
            case T_title:
            case T_legacy_title: {
                std::string titleStr = texts[i++].substr(t == T_title ? 6 : 7);
                db.setDiagramTitle(titleStr);
                frame.list.push_back(SeqItem::makeStr(titleStr));
                break;
            }
            case T_acc_title: {
                i++;
                std::string value = trimStr(expect(T_acc_title_value));
                db.setAccTitle(value);
                frame.list.push_back(SeqItem::makeStr(value));
                break;
            }
            case T_acc_descr: {
                i++;
                std::string value = trimStr(expect(T_acc_descr_value));
                db.setAccDescription(value);
                frame.list.push_back(SeqItem::makeStr(value));
                break;
            }
            case T_acc_descr_multiline_value: {
                std::string value = trimStr(texts[i++]);
                db.setAccDescription(value);
                frame.list.push_back(SeqItem::makeStr(value));
                break;
            }
            case T_loop:
            case T_rect:
            case T_opt:
            case T_break:
            case T_alt:
            case T_par:
            case T_par_over:
            case T_critical:
            case T_box: {
                i++;
                std::string text = expect(T_restOfLine);
                stack.push_back(std::move(frame));
                frame = ParseFrame();
                frame.kind = blockForToken(t);
                frame.text = text;
                frame.startType = (t == T_par_over) ? LINETYPE::PAR_OVER_START : LINETYPE::PAR_START;
                frame.list.push_back(SeqItem::makeEmpty());
                break;
            }
            case T_else:
            case T_and:
            case T_option: {
                K expectedKind = (t == T_else) ? K::Alt : (t == T_and ? K::Par : K::Critical);
                if (frame.kind != expectedKind) fail("'end'");
                i++;
                std::string text = expect(T_restOfLine);
                frame.sections.push_back({std::move(frame.list), text});
                frame.list.clear();
                break;
            }
            case T_end: {
                if (frame.kind == K::Top) fail("'NEWLINE', 'EOF'");
                i++;
                SeqItem done = close(frame);
                frame = std::move(stack.back());
                stack.pop_back();
                frame.list.push_back(std::move(done));
                break;
            }
            case T_END:
                if (frame.kind != K::Top) fail("'end'");
                db.apply(SeqItem::makeList(std::move(frame.list)));
                return;
            default:
                fail("'NEWLINE', 'ACTOR', 'participant', 'note', 'end'");
        }
    }
}

} // namespace detail_parser

// ============================================================================
// Port of src/diagrams/sequence/actors.ts
// ============================================================================

struct ActorSize {
    double w = 0.0;
    double h = 0.0;
};

namespace detail_actors {

constexpr double PAD_X = 16.0;
constexpr double PAD_Y = 10.0;
constexpr double GLYPH = 34.0;
constexpr double GLYPH_GAP = 4.0;
constexpr double STACK = 5.0;
constexpr double CAP = 6.0;
inline const std::string SHAPE = " fill=\"var(--_s)\" stroke=\"var(--_b)\"";
inline const std::string LINE = " fill=\"none\" stroke=\"var(--_b)\"";

inline bool isKnownKind(const std::string& type) noexcept {
    return type == "participant" || type == "actor" || type == "boundary" || type == "control" ||
           type == "entity" || type == "database" || type == "collections" || type == "queue";
}

inline std::string actorKind(const std::string& type) noexcept {
    return isKnownKind(type) ? type : "participant";
}

inline bool isFigure(const std::string& kind) noexcept {
    return kind == "actor" || kind == "boundary" || kind == "control" || kind == "entity" || kind == "database";
}

inline ActorSize actorSize(const std::string& kind, const Label& label, double minW, double minH, double paddingX = PAD_X) noexcept {
    if (isFigure(kind)) {
        return {std::max(40.0, label.width + 8.0), GLYPH + (label.height > 0.0 ? GLYPH_GAP + label.height : 0.0)};
    }
    double extra = kind == "collections" ? STACK : (kind == "queue" ? 2.0 * CAP : 0.0);
    return {
        std::max(minW, label.width + 2.0 * paddingX) + extra,
        std::max(minH, label.height + 2.0 * PAD_Y) + (kind == "collections" ? STACK : 0.0)
    };
}

inline std::string figure(const std::string& kind, double cx, double top) noexcept {
    std::string x = num(cx);
    auto y = [&](double dy) -> std::string { return num(top + dy); };
    if (kind == "actor") {
        return Pele::cat({"<circle cx=\"", x, "\" cy=\"", y(7.0), "\" r=\"6\"", SHAPE, "/>", "<path d=\"M", x, ",", y(13.0), "V", y(24.0), "M", num(cx - 10.0), ",", y(17.0), "H", num(cx + 10.0), "M", num(cx - 8.0), ",", y(34.0), "L", x, ",", y(24.0), "L", num(cx + 8.0), ",", y(34.0), "\"", LINE, "/>"});
    }
    if (kind == "boundary") {
        return Pele::cat({"<path d=\"M", num(cx - 19.0), ",", y(6.0), "V", y(28.0), "M", num(cx - 19.0), ",", y(17.0), "H", num(cx - 8.0), "\"", LINE, "/>", "<circle cx=\"", num(cx + 3.0), "\" cy=\"", y(17.0), "\" r=\"11\"", SHAPE, "/>"});
    }
    if (kind == "control") {
        return Pele::cat({"<circle cx=\"", x, "\" cy=\"", y(21.0), "\" r=\"11\"", SHAPE, "/>", "<path d=\"M", num(cx + 5.0), ",", y(5.0), "L", num(cx - 1.0), ",", y(10.0), "L", num(cx + 5.0), ",", y(15.0), "\"", LINE, "/>"});
    }
    if (kind == "entity") {
        return Pele::cat({"<circle cx=\"", x, "\" cy=\"", y(15.0), "\" r=\"11\"", SHAPE, "/>", "<path d=\"M", num(cx - 11.0), ",", y(30.0), "H", num(cx + 11.0), "\"", LINE, "/>"});
    }
    std::string l = num(cx - 14.0);
    std::string r = num(cx + 14.0);
    return Pele::cat({"<path d=\"M", l, ",", y(7.0), "V", y(27.0), "A14,5 0 0 0 ", r, ",", y(27.0), "V", y(7.0), "A14,5 0 0 0 ", l, ",", y(7.0), "Z\"", SHAPE, "/>", "<path d=\"M", l, ",", y(7.0), "A14,5 0 0 0 ", r, ",", y(7.0), "\"", LINE, "/>"});
}

inline std::string drawActor(
    const std::string& kind,
    double cx,
    double top,
    const ActorSize& size,
    const Label& label,
    const IconResolver& icons,
    const std::string& inkAttr = ""
) {
    double w = size.w;
    double h = size.h;
    const std::string textAttr = " class=\"pele-label\"";
    if (isFigure(kind)) {
        return figure(kind, cx, top) + labelSvg(label, cx, top + GLYPH + GLYPH_GAP + label.height / 2.0, textAttr + inkAttr, icons);
    }
    double left = cx - w / 2.0;
    if (kind == "collections") {
        double bw = w - STACK;
        double bh = h - STACK;
        std::string back = Pele::cat({"<rect x=\"", num(left + STACK), "\" y=\"", num(top), "\" width=\"", num(bw), "\" height=\"", num(bh), "\"", SHAPE, ROUNDED, "/>"});
        std::string front = Pele::cat({"<rect x=\"", num(left), "\" y=\"", num(top + STACK), "\" width=\"", num(bw), "\" height=\"", num(bh), "\"", SHAPE, ROUNDED, "/>"});
        return back + front + labelSvg(label, left + bw / 2.0, top + STACK + bh / 2.0, textAttr, icons);
    }
    if (kind == "queue") {
        std::string l = num(left + CAP);
        std::string r = num(left + w - CAP);
        std::string b = num(top + h);
        std::string ry = num(h / 2.0);
        return Pele::cat({"<path d=\"M", l, ",", num(top), "H", r, "A6,", ry, " 0 0 1 ", r, ",", b, "H", l, "A6,", ry, " 0 0 1 ", l, ",", num(top), "Z\"", SHAPE, "/>", "<path d=\"M", r, ",", num(top), "A6,", ry, " 0 0 0 ", r, ",", b, "\"", LINE, "/>", labelSvg(label, cx - CAP / 2.0, top + h / 2.0, textAttr, icons)});
    }
    return Pele::cat({"<rect x=\"", num(left), "\" y=\"", num(top), "\" width=\"", num(w), "\" height=\"", num(h), "\"", SHAPE, ROUNDED, "/>", labelSvg(label, cx, top + h / 2.0, textAttr, icons)});
}

} // namespace detail_actors

// ============================================================================
// Port of src/diagrams/sequence/render.ts
// ============================================================================

namespace detail_render {

using namespace detail_actors;

enum class Head {
    None,
    Arrow,
    Cross,
    Open,
    HalfSolid,
    HalfStick
};

struct LineStyle {
    bool valid = false;
    bool dotted = false;
    Head head = Head::None;
    bool atStart = false;
    bool atEnd = false;
    bool bottom = false;
};

inline const std::array<LineStyle, 64>& lineStyles() noexcept {
    static const std::array<LineStyle, 64> styles = []() {
        std::array<LineStyle, 64> s{};
        auto style = [&](int type, bool dotted, Head head, int ends = 1, bool bottom = false) {
            s[type] = {true, dotted, head, ends != 1, ends != 0, bottom};
        };
        style(LINETYPE::SOLID, false, Head::Arrow);
        style(LINETYPE::DOTTED, true, Head::Arrow);
        style(LINETYPE::SOLID_CROSS, false, Head::Cross);
        style(LINETYPE::DOTTED_CROSS, true, Head::Cross);
        style(LINETYPE::SOLID_OPEN, false, Head::None);
        style(LINETYPE::DOTTED_OPEN, true, Head::None);
        style(LINETYPE::SOLID_POINT, false, Head::Open);
        style(LINETYPE::DOTTED_POINT, true, Head::Open);
        style(LINETYPE::BIDIRECTIONAL_SOLID, false, Head::Arrow, 2);
        style(LINETYPE::BIDIRECTIONAL_DOTTED, true, Head::Arrow, 2);
        for (bool dotted : {false, true}) {
            int d = dotted ? 10 : 0;
            style(LINETYPE::SOLID_TOP + d, dotted, Head::HalfSolid);
            style(LINETYPE::SOLID_BOTTOM + d, dotted, Head::HalfSolid, 1, true);
            style(LINETYPE::STICK_TOP + d, dotted, Head::HalfStick);
            style(LINETYPE::STICK_BOTTOM + d, dotted, Head::HalfStick, 1, true);
            style(LINETYPE::SOLID_ARROW_TOP_REVERSE + d, dotted, Head::HalfSolid, 0);
            style(LINETYPE::SOLID_ARROW_BOTTOM_REVERSE + d, dotted, Head::HalfSolid, 0, true);
            style(LINETYPE::STICK_ARROW_TOP_REVERSE + d, dotted, Head::HalfStick, 0);
            style(LINETYPE::STICK_ARROW_BOTTOM_REVERSE + d, dotted, Head::HalfStick, 0, true);
        }
        return s;
    }();
    return styles;
}

inline std::string frameKindFor(int type) noexcept {
    switch (type) {
        case LINETYPE::LOOP_START: return "loop";
        case LINETYPE::ALT_START: return "alt";
        case LINETYPE::OPT_START: return "opt";
        case LINETYPE::PAR_START: return "par";
        case LINETYPE::PAR_OVER_START: return "par";
        case LINETYPE::CRITICAL_START: return "critical";
        case LINETYPE::BREAK_START: return "break";
        case LINETYPE::RECT_START: return "rect";
        default: return "";
    }
}

constexpr double MSG_PAD = 12.0;
constexpr double SELF_W = 28.0;
constexpr double SELF_H = 22.0;
constexpr double ACT_W = 10.0;
constexpr double ACT_STEP = 4.0;
constexpr double ACT_MIN = 14.0;
constexpr double NOTE_GAP = 10.0;
constexpr double NOTE_OVER = 16.0;
constexpr double FRAME_PAD = 20.0;
constexpr double FRAME_NEST = 10.0;
constexpr double TAB_H = 20.0;
constexpr double FRAME_TEXT = 240.0;
constexpr double BADGE_H = 16.0;
constexpr double CENTRAL_R = 4.0;
inline const std::string DASH = " stroke-dasharray=\"4 4\"";

struct Numbering {
    std::vector<double> index;
    std::vector<bool> shown;
    bool visible = false;
};

inline bool isLineMessage(int type, bool hasType) noexcept {
    if (!hasType || type < 0 || type >= 64) return false;
    return lineStyles()[type].valid;
}

inline Numbering numberMessages(const std::vector<SeqMessage>& messages, bool always) noexcept {
    Numbering out;
    out.index.assign(messages.size(), std::numeric_limits<double>::quiet_NaN());
    out.shown.assign(messages.size(), false);
    double current = 1.0;
    double step = 1.0;
    bool visible = false;
    for (size_t i = 0; i < messages.size(); i++) {
        const auto& m = messages[i];
        if (m.hasType && m.type == LINETYPE::AUTONUMBER) {
            if (m.numbering.hasStart && m.numbering.start != 0.0) current = m.numbering.start;
            if (m.numbering.hasStep && m.numbering.step != 0.0) step = m.numbering.step;
            visible = m.numbering.visible;
        } else if (isLineMessage(m.type, m.hasType)) {
            out.index[i] = current;
            out.shown[i] = visible || always;
            current = std::floor((current + step) * 100.0 + 0.5) / 100.0;
        }
    }
    out.visible = visible;
    return out;
}

struct Column {
    std::string id;
    SeqActor actor;
    std::string kind;
    Label label;
    ActorSize size;
    double x = 0.0;
    int createAt = -1;
    int destroyAt = -1;
    double top = 0.0;
    double lifeStart = 0.0;
    double lifeEnd = -1.0;
    std::vector<double> activations;
};

enum class V {
    Line,
    Self,
    Note
};

struct View {
    V kind = V::Line;
    int a = 0;
    int b = 0;
    Label label;
    double x = 0.0;
    double w = 0.0;
};

struct Frame {
    std::string kind;
    double x0 = std::numeric_limits<double>::infinity();
    double x1 = -std::numeric_limits<double>::infinity();
    double top = 0.0;
    Label title;
    double tabW = 0.0;
    std::string fill;
    std::vector<size_t> dividers;
    std::vector<double> dividerY;
    std::vector<Label> dividerLabels;
};

struct BoxRun {
    std::shared_ptr<SeqBox> box;
    int first = 0;
    int last = 0;
    Label title;
    double grow = 0.0;
};

inline double option(const Config& conf, const std::string& key, double fallback) noexcept {
    if (const auto* v = conf.get(key); v && v->isNumber() && v->nVal >= 0.0 && v->nVal <= 2000.0) {
        return v->nVal;
    }
    return fallback;
}

inline std::string half(double x, double y, double dir, double up, bool stick, const std::string& color) noexcept {
    std::string bx = num(x - dir * ARROW);
    if (stick) return Pele::cat({"<path class=\"pele-marker\" d=\"M", num(x), ",", num(y), "L", bx, ",", num(y + up * 5.0), "\"/>"});
    return Pele::cat({"<path class=\"pele-marker\" d=\"M", num(x), ",", num(y), "L", bx, ",", num(y + up * 4.5), "V", num(y), "Z\" fill=\"", color, "\" stroke=\"none\"/>"});
}

inline std::string head(Head type, double x, double y, double dir, double up, const std::string& color) noexcept {
    switch (type) {
        case Head::Arrow:
            return marker("arrow_point", x, y, dir, 0.0, color);
        case Head::Cross:
            return marker("arrow_cross", x, y, dir, 0.0, color);
        case Head::Open:
            return Pele::cat({"<path class=\"pele-marker\" d=\"M", num(x - dir * ARROW), ",", num(y - 4.5), "L", num(x), ",", num(y), "L", num(x - dir * ARROW), ",", num(y + 4.5), "\" stroke-linejoin=\"round\"/>"});
        case Head::HalfSolid:
            return half(x, y, dir, up, false, color);
        case Head::HalfStick:
            return half(x, y, dir, up, true, color);
        default:
            return "";
    }
}

inline double trim(Head type) noexcept {
    return type == Head::Arrow ? ARROW - 1.0 : (type == Head::Cross ? 4.0 : 0.0);
}

inline Rendered draw(const SeqDb& db, const Config& config, const RenderOptions& options, bool tight, double hold = 0.0, bool byNeed = true) noexcept {
    const double INF = std::numeric_limits<double>::infinity();
    Config conf = YamlValue::makeMap();
    if (const auto* sc = config.get("sequence"); sc && sc->isMap()) conf = *sc;

    double size = options.fontSize > 0.0 ? options.fontSize : 16.0;
    double small = std::round(size * 0.875);
    double tiny = std::round(size * 0.75);
    const TextMeasurer& measurer = options.measurer ? *options.measurer : defaultMeasurer();
    const auto& icons = options.icons;
    double pad = options.hasPadding ? options.padding : 8.0;

    bool mirror = true;
    if (const auto* ma = conf.get("mirrorActors"); ma && ma->isBool() && !ma->bVal) mirror = false;
    bool rightAngles = false;
    if (const auto* ra = conf.get("rightAngles"); ra && ra->isBool() && ra->bVal) rightAngles = true;

    double actorMargin = std::min(option(conf, "actorMargin", 40.0), tight ? 8.0 : INF);
    double minW = std::min(option(conf, "width", 64.0), tight ? 48.0 : INF);
    double minH = option(conf, "height", std::round(size * 1.5) + 20.0);
    double maxWOpt = options.maxWidth > 0.0 ? options.maxWidth : 320.0;
    double wrapW = std::min(
        option(conf, "width", 160.0),
        tight ? std::max(32.0, (maxWOpt - 2.0 * pad) / std::max<size_t>(1, db.actors.size()) - actorMargin - 24.0) : INF
    );
    double msgGap = std::max(4.0, 14.0 + option(conf, "messageMargin", 35.0) - 35.0);
    double notePad = std::min(option(conf, "noteMargin", 10.0), tight ? 6.0 : INF);
    double boxPad = std::min(option(conf, "boxMargin", 10.0), tight ? 6.0 : INF);
    int align = 0;
    if (const auto* al = conf.get("messageAlign"); al && al->isString()) {
        if (al->sVal == "left") align = -1;
        else if (al->sVal == "right") align = 1;
    }

    double msgPad = tight ? 6.0 : MSG_PAD;
    double noteOver = tight ? 8.0 : NOTE_OVER;
    double framePad = tight ? 12.0 : FRAME_PAD;
    double frameNest = tight ? 6.0 : FRAME_NEST;
    const auto& messages = db.messages;
    size_t count = messages.size();
    bool showSeqNums = false;
    if (const auto* ssn = conf.get("showSequenceNumbers"); ssn && ssn->isBool() && ssn->bVal) showSeqNums = true;
    Numbering numbering = numberMessages(messages, showSeqNums);

    auto text = [&](const std::string* raw, double fontSize, bool wrap, double width = -1.0) -> Label {
        double w = width < 0.0 ? wrapW : width;
        Label label = layoutLabel(raw ? *raw : "", false, measurer, fontSize, (tight || wrap) ? w : INF);
        return tight ? breakLabel(label, measurer, std::max(120.0, w)) : label;
    };
    auto messageText = [&](const std::string& raw, bool wrap, double width = -1.0) -> Label {
        return text(&raw, small, wrap, width);
    };

    bool hideUnused = false;
    if (const auto* hu = conf.get("hideUnusedParticipants"); hu && hu->isBool() && hu->bVal) hideUnused = true;
    Pele::FlatSet<std::string> used;
    if (hideUnused) {
        for (const auto& m : messages) {
            if (m.hasFrom) used.insert(m.from);
            if (m.hasTo) used.insert(m.to);
        }
    }

    std::vector<int> nextLine(count + 1, -1);
    for (int i = static_cast<int>(count) - 1; i >= 0; i--) {
        nextLine[i] = isLineMessage(messages[i].type, messages[i].hasType) ? i : nextLine[i + 1];
    }

    std::vector<Column> cols;
    Pele::FlatMap<std::string, int> colOf;
    for (const auto& id : db.actorOrder) {
        if (hideUnused && used.count(id) == 0) continue;
        const auto& actor = db.actors.at(id);
        std::string kind = actorKind(actor.type);
        Label label = text(&actor.description, size, actor.wrap);
        int createAt = -1;
        if (auto it = db.createdActors.find(id); it != db.createdActors.end()) {
            createAt = nextLine[std::min(it->second, static_cast<int>(count))];
            if (createAt >= 0 && (!messages[createAt].hasTo || messages[createAt].to != id)) createAt = -1;
        }
        int destroyAt = -1;
        if (auto it = db.destroyedActors.find(id); it != db.destroyedActors.end()) {
            destroyAt = nextLine[std::min(it->second, static_cast<int>(count))];
            if (destroyAt >= 0 && (!messages[destroyAt].hasTo || messages[destroyAt].to != id) &&
                (!messages[destroyAt].hasFrom || messages[destroyAt].from != id)) {
                destroyAt = -1;
            }
        }
        colOf[id] = static_cast<int>(cols.size());
        Column col;
        col.id = id;
        col.actor = actor;
        col.kind = kind;
        col.label = label;
        col.size = actorSize(kind, label, minW, minH, tight ? 8.0 : PAD_X);
        col.createAt = createAt;
        col.destroyAt = destroyAt;
        cols.push_back(std::move(col));
    }
    int last = static_cast<int>(cols.size()) - 1;

    Pele::FlatMap<int, double> needs;
    auto need = [&](int lo, int hi, double width) {
        if (lo < 0 || hi > last) return;
        int key = lo * static_cast<int>(cols.size()) + hi;
        if (needs[key] < width) needs[key] = width;
    };
    auto badgeWidth = [&](size_t i) -> double {
        return numbering.shown[i] ? std::max(BADGE_H, measurer.width(num(numbering.index[i]), tiny, 0) + 8.0) : 0.0;
    };

    std::vector<std::optional<View>> views(count);
    std::vector<std::unordered_map<double, Label>> sized(count);
    std::vector<bool> hasSized(count, false);
    auto sizedText = [&](size_t i, double room) -> Label {
        if (!hasSized[i]) {
            if (room == wrapW) return views[i]->label;
            hasSized[i] = true;
            sized[i][wrapW] = views[i]->label;
        }
        auto it = sized[i].find(room);
        if (it == sized[i].end()) {
            Label lbl = messageText(messages[i].message, true, room);
            sized[i][room] = lbl;
            return lbl;
        }
        return it->second;
    };

    for (size_t i = 0; i < count; i++) {
        const auto& m = messages[i];
        if (!m.hasFrom || !m.hasTo || m.isNumbering) continue;
        auto aIt = colOf.find(m.from);
        auto bIt = colOf.find(m.to);
        if (aIt == colOf.end() || bIt == colOf.end()) continue;
        int a = aIt->second;
        int b = bIt->second;
        if (m.hasType && m.type == LINETYPE::NOTE) {
            Label label = text(&m.message, small, m.wrap);
            double w = label.width + 2.0 * notePad;
            if (m.placement == PLACEMENT::LEFTOF) need(a - 1, a, w + 2.0 * NOTE_GAP + ACT_W);
            else if (m.placement == PLACEMENT::RIGHTOF) need(a, a + 1, w + 2.0 * NOTE_GAP + ACT_W);
            else if (a == b) {
                need(a - 1, a, w / 2.0 + NOTE_GAP);
                need(a, a + 1, w / 2.0 + NOTE_GAP);
            } else need(std::min(a, b), std::max(a, b), w - 2.0 * noteOver);
            views[i] = View{V::Note, a, b, label, 0.0, w};
        } else if (isLineMessage(m.type, m.hasType)) {
            Label label = messageText(m.message, m.wrap);
            double badge = badgeWidth(i);
            if (a == b) {
                need(a, a + 1, SELF_W + 8.0 + label.width + 16.0 + ACT_W + badge);
                views[i] = View{V::Self, a, b, label, 0.0, 0.0};
            } else {
                double created = (cols[b].createAt == static_cast<int>(i)) ? cols[b].size.w / 2.0 : 0.0;
                double central = m.centralConnection != 0 ? 4.0 * CENTRAL_R : 0.0;
                need(
                    std::min(a, b),
                    std::max(a, b),
                    (tight ? std::min(label.width, std::max(16.0, wrapW - 16.0)) : label.width) + 2.0 * msgPad + ACT_W + badge + created + central
                );
                views[i] = View{V::Line, a, b, label, 0.0, 0.0};
            }
        }
    }

    std::vector<BoxRun> runs;
    for (int i = 0; i <= last; i++) {
        const auto& box = cols[i].actor.box;
        if (box && !runs.empty() && runs.back().box == box && runs.back().last == i - 1) {
            runs.back().last = i;
        } else if (box) {
            runs.push_back({box, i, i, text(box->hasName ? &box->name : nullptr, small, box->wrap), 0.0});
        }
    }

    std::vector<double> gaps(std::max(last, 0), 0.0);
    for (int i = 0; i < last; i++) {
        const auto& left = cols[i];
        const auto& right = cols[i + 1];
        double boxed = left.actor.box != right.actor.box
            ? (left.actor.box ? boxPad : 0.0) + (right.actor.box ? boxPad : 0.0)
            : 0.0;
        int key = i * static_cast<int>(cols.size()) + i + 1;
        double needW = needs.count(key) ? needs[key] : 0.0;
        gaps[i] = std::max(left.size.w / 2.0 + right.size.w / 2.0 + actorMargin + boxed, needW);
    }

    struct Span { int lo; int hi; double width; };
    std::vector<Span> spans;
    for (const auto& [key, width] : needs) {
        int lo = key / static_cast<int>(cols.size());
        int hi = key % static_cast<int>(cols.size());
        if (hi - lo > 1) spans.push_back({lo, hi, width});
    }
    std::sort(spans.begin(), spans.end(), [](const Span& p, const Span& q) {
        int dp = p.hi - p.lo;
        int dq = q.hi - q.lo;
        return dp != dq ? dp < dq : p.lo < q.lo;
    });
    for (const auto& [lo, hi, width] : spans) {
        double total = 0.0;
        for (int i = lo; i < hi; i++) total += gaps[i];
        if (total >= width) continue;
        double share = (width - total) / (hi - lo);
        for (int i = lo; i < hi; i++) gaps[i] += share;
    }

    auto place = [&]() {
        if (!cols.empty()) cols[0].x = 0.0;
        for (int i = 1; i <= last; i++) cols[i].x = cols[i - 1].x + gaps[i - 1];
    };
    place();
    for (auto& run : runs) {
        double inner = cols[run.last].x + cols[run.last].size.w / 2.0 - (cols[run.first].x - cols[run.first].size.w / 2.0);
        run.grow = std::max(0.0, (run.title.width - inner) / 2.0);
        if (run.grow == 0.0) continue;
        if (run.first > 0) gaps[run.first - 1] += run.grow;
        if (run.last < last) gaps[run.last] += run.grow;
    }

    double minX = INF;
    double maxX = -INF;
    auto extend = [&](double x0, double x1) {
        if (x0 < minX) minX = x0;
        if (x1 > maxX) maxX = x1;
    };
    auto boxEdges = [&](const BoxRun& run) -> std::pair<double, double> {
        return {
            cols[run.first].x - cols[run.first].size.w / 2.0 - boxPad - run.grow,
            cols[run.last].x + cols[run.last].size.w / 2.0 + boxPad + run.grow
        };
    };

    std::vector<std::shared_ptr<Frame>> frames(count);
    auto layout = [&]() {
        place();
        minX = INF;
        maxX = -INF;
        frames.assign(count, nullptr);
        for (const auto& col : cols) extend(col.x - col.size.w / 2.0, col.x + col.size.w / 2.0);
        for (const auto& run : runs) {
            auto [bx0, bx1] = boxEdges(run);
            extend(bx0, bx1);
        }

        std::vector<std::shared_ptr<Frame>> open;
        auto spanAll = [&]() -> std::pair<double, double> {
            return last < 0 ? std::make_pair(0.0, 120.0) : std::make_pair(cols[0].x - framePad, cols[last].x + framePad);
        };
        for (size_t i = 0; i < count; i++) {
            const auto& m = messages[i];
            int type = m.type;
            std::string fKind = m.hasType ? frameKindFor(type) : "";
            if (!fKind.empty()) {
                auto frame = std::make_shared<Frame>();
                frame->kind = fKind;
                frame->x0 = INF;
                frame->x1 = -INF;
                frame->fill = (fKind == "rect" && !m.isNumbering) ? m.message : "";
                frame->dividers.push_back(i);
                frames[i] = frame;
                open.push_back(frame);
            } else if (m.hasType && (type == LINETYPE::ALT_ELSE || type == LINETYPE::PAR_AND || type == LINETYPE::CRITICAL_OPTION)) {
                if (!open.empty()) open.back()->dividers.push_back(i);
            } else if (
                m.hasType && (
                    type == LINETYPE::LOOP_END || type == LINETYPE::ALT_END || type == LINETYPE::OPT_END ||
                    type == LINETYPE::PAR_END || type == LINETYPE::CRITICAL_END || type == LINETYPE::BREAK_END ||
                    type == LINETYPE::RECT_END
                )
            ) {
                if (open.empty()) continue;
                auto frame = open.back();
                open.pop_back();
                frames[i] = frame;
                if (frame->x0 > frame->x1) {
                    auto [sx0, sx1] = spanAll();
                    frame->x0 = sx0;
                    frame->x1 = sx1;
                }
                if (frame->kind != "rect") {
                    frame->tabW = measurer.width(frame->kind, tiny, Style::Bold) + 14.0;
                    for (size_t k = 0; k < frame->dividers.size(); k++) {
                        const auto& raw = messages[frame->dividers[k]].message;
                        double tab = (k == 0) ? frame->tabW + 8.0 : 0.0;
                        double room = frame->x1 - frame->x0 - 16.0 - tab;
                        Label label;
                        if (!raw.empty()) {
                            std::string bracketed = Pele::cat({"[", raw, "]"});
                            label = text(&bracketed, small, true, std::max(room, tight ? std::min(FRAME_TEXT, wrapW) : FRAME_TEXT));
                        }
                        if (label.width > room) frame->x1 += label.width - room;
                        if (k == 0) frame->title = label;
                        else frame->dividerLabels.push_back(label);
                    }
                }
                extend(frame->x0, frame->x1);
                if (!open.empty()) {
                    auto& parent = open.back();
                    parent->x0 = std::min(parent->x0, frame->x0 - frameNest);
                    parent->x1 = std::max(parent->x1, frame->x1 + frameNest);
                }
            } else if (views[i].has_value()) {
                auto& view = *views[i];
                const auto& a = cols[view.a];
                const auto& b = cols[view.b];
                double x0 = 0.0;
                double x1 = 0.0;
                if (view.kind == V::Note) {
                    if (m.placement == PLACEMENT::LEFTOF) view.x = a.x - NOTE_GAP - view.w;
                    else if (m.placement == PLACEMENT::RIGHTOF) view.x = a.x + NOTE_GAP;
                    else if (view.a == view.b) view.x = a.x - view.w / 2.0;
                    else {
                        double lo = std::min(a.x, b.x) - noteOver;
                        double hi = std::max(a.x, b.x) + noteOver;
                        if (tight || m.wrap) view.label = text(&m.message, small, true, std::max(wrapW, hi - lo - 2.0 * notePad));
                        view.w = std::max(hi - lo, view.label.width + 2.0 * notePad);
                        view.x = (lo + hi - view.w) / 2.0;
                    }
                    x0 = view.x;
                    x1 = view.x + view.w;
                } else if (view.kind == V::Self) {
                    x0 = a.x;
                    x1 = a.x + ACT_W + SELF_W + 8.0 + view.label.width + badgeWidth(i);
                } else {
                    x0 = std::min(a.x, b.x);
                    x1 = std::max(a.x, b.x);
                    if (tight || m.wrap) view.label = sizedText(i, std::max(wrapW, x1 - x0 - 2.0 * msgPad - ACT_W));
                    if (b.createAt == static_cast<int>(i)) {
                        if (b.x > a.x) x1 += b.size.w / 2.0;
                        else x0 -= b.size.w / 2.0;
                    }
                }
                extend(x0, x1);
                if (!open.empty()) {
                    auto& frame = open.back();
                    frame->x0 = std::min(frame->x0, x0 - framePad);
                    frame->x1 = std::max(frame->x1, x1 + framePad);
                }
            }
        }
    };
    layout();

    bool grown = false;
    bool uneven = false;
    if (tight && options.maxWidth > 0.0) {
        double spare = std::floor(options.maxWidth) - std::ceil(maxX - minX + 2.0 * pad) - hold;
        std::vector<int> stretch(cols.size(), 0);
        std::vector<double> want(gaps.size(), 0.0);
        std::vector<size_t> wrapped;
        int lines = 0;
        for (size_t i = 0; spare > 0.0 && i < views.size(); i++) {
            if (!views[i].has_value() || views[i]->kind != V::Line) continue;
            const auto& view = *views[i];
            Label natural = messageText(messages[i].message, false, INF);
            if (view.label.lines.size() <= natural.lines.size()) continue;
            wrapped.push_back(i);
            lines += static_cast<int>(view.label.lines.size());
            int lo = std::min(view.a, view.b);
            int hi = std::max(view.a, view.b);
            stretch[lo]++;
            stretch[hi]--;
            double shortW = (natural.width - (cols[hi].x - cols[lo].x - 2.0 * msgPad - ACT_W)) / (hi - lo);
            for (int g = lo; g < hi; g++) want[g] = std::max(want[g], shortW);
        }
        int active = 0;
        int crossed = 0;
        for (size_t i = 0; i < gaps.size(); i++) {
            active += stretch[i];
            stretch[i] = active > 0 ? 1 : 0;
            crossed += stretch[i];
        }
        if (crossed > 0) {
            std::vector<double> before = gaps;
            auto spread = [&](auto&& shareFn) -> std::vector<int> {
                for (size_t g = 0; g < gaps.size(); g++) gaps[g] = before[g] + shareFn(g);
                layout();
                std::vector<int> out;
                for (size_t idx : wrapped) out.push_back(static_cast<int>(views[idx]->label.lines.size()));
                return out;
            };
            auto sumInt = [](const std::vector<int>& counts) -> int {
                int s = 0;
                for (int c : counts) s += c;
                return s;
            };
            auto sumDbl = [](const std::vector<double>& vals) -> double {
                double s = 0.0;
                for (double v : vals) s += v;
                return s;
            };
            auto even = [&](size_t g) -> double { return (stretch[g] * spare) / crossed; };
            std::vector<int> counts = spread(even);
            double missing = sumDbl(want);
            auto needsShare = [&](size_t g) -> double { return (want[g] * (spare - 0.01)) / missing; };
            bool someWider = false;
            if (missing > 0.0) {
                for (size_t g = 0; g < gaps.size(); g++) {
                    if (needsShare(g) > even(g)) { someWider = true; break; }
                }
            }
            if (byNeed && crossed > 1 && missing > 0.0 && someWider) {
                std::vector<int> first = counts;
                counts = spread(needsShare);
                bool allLe = true;
                for (size_t k = 0; k < counts.size(); k++) {
                    if (counts[k] > first[k]) { allLe = false; break; }
                }
                uneven = (sumInt(counts) < sumInt(first)) && allLe;
                if (!uneven) counts = spread(even);
            }
            grown = sumInt(counts) < lines;
            if (!grown) {
                gaps = before;
                layout();
            }
        }
    }

    double boxTitleH = 0.0;
    for (const auto& run : runs) boxTitleH = std::max(boxTitleH, run.title.height);
    double boxTop = !runs.empty() ? boxPad + (boxTitleH > 0.0 ? boxTitleH + 4.0 : 0.0) : 0.0;
    double rowH = 0.0;
    for (const auto& col : cols) if (col.createAt < 0) rowH = std::max(rowH, col.size.h);
    double headBottom = boxTop + rowH;
    for (auto& col : cols) {
        col.top = headBottom - col.size.h;
        col.lifeStart = headBottom;
    }

    std::vector<double> xs;
    xs.reserve(cols.size());
    for (const auto& col : cols) xs.push_back(col.x);
    auto crosses = [&](double x0, double x1) -> bool {
        size_t lo = 0;
        size_t hi = xs.size();
        while (lo < hi) {
            size_t mid = (lo + hi) >> 1;
            if (xs[mid] <= x0) lo = mid + 1;
            else hi = mid;
        }
        return lo < xs.size() && xs[lo] < x1;
    };

    std::vector<std::shared_ptr<Frame>> stack;
    Pele::FlatMap<std::string, std::string> inks;
    auto inkOf = [&](const std::string& fill) -> std::string {
        if (fill.empty()) return "";
        auto it = inks.find(fill);
        if (it == inks.end()) {
            std::string val = ink(resolveStyle({Pele::cat({"fill:", fill})}).shape);
            inks[fill] = val;
            return val;
        }
        return it->second;
    };
    auto inkOn = [&](const std::string& fill) -> std::string {
        std::string color = inkOf(fill);
        return !color.empty() ? Pele::cat({" fill=\"", color, "\""}) : "";
    };
    auto fillUnder = [&](double l, double r) -> std::string {
        for (int k = static_cast<int>(stack.size()) - 1; k >= 0; k--) {
            if (stack[k]->kind == "rect") return stack[k]->fill;
        }
        for (const auto& run : runs) {
            auto [x0, x1] = boxEdges(run);
            if (l >= x0 && r <= x1) return run.box->fill;
        }
        return "";
    };
    auto under = [&](double l, double r) -> std::string {
        return inkOn(fillUnder(l, r));
    };
    auto backed = [&](const Label& label, double cx, double cy, bool frame = false) -> std::string {
        if (label.width == 0.0) return "";
        double l = cx - label.width / 2.0;
        bool plate = crosses(l, l + label.width);
        std::string back = plate
            ? Pele::cat({"<rect x=\"", num(l - 3.0), "\" y=\"", num(cy - label.height / 2.0 + 2.0), "\" width=\"", num(label.width + 6.0), "\" height=\"", num(label.height - 4.0), "\" rx=\"3\" fill=\"var(--_bg)\" stroke=\"none\"/>"})
            : "";
        std::string fill = plate ? "" : under(l, l + label.width);
        return back + labelSvg(
            label,
            cx,
            cy,
            frame ? Pele::cat({" class=\"pele-frame-label\"", (!fill.empty() ? fill : std::string(" fill=\"var(--_m)\"")), " stroke=\"none\""}) : fill,
            icons
        );
    };
    auto leftEdge = [&](const Column& col, int extra = 0) -> double {
        return (static_cast<int>(col.activations.size()) + extra > 0) ? col.x - ACT_W / 2.0 : col.x;
    };
    auto rightEdge = [&](const Column& col, int extra = 0) -> double {
        int depth = static_cast<int>(col.activations.size()) + extra;
        return depth > 0 ? col.x + ACT_W / 2.0 + (depth - 1) * ACT_STEP : col.x;
    };

    std::vector<std::string> rectsOut;
    std::vector<std::string> activationsOut;
    std::string framesOut;
    std::string edgesOut;
    std::string labelsOut;
    std::string notesOut;
    std::string actorsOut;
    std::string crossesOut;

    auto endActivation = [&](Column& col, double end) -> double {
        double start = col.activations.back();
        col.activations.pop_back();
        double bottom = std::max(end, start + ACT_MIN);
        double x = col.x - ACT_W / 2.0 + col.activations.size() * ACT_STEP;
        extend(x, x + ACT_W);
        activationsOut.push_back(Pele::cat({"<rect class=\"pele-activation\" data-id=\"", escText(col.id), "\" x=\"", num(x), "\" y=\"", num(start), "\" width=\"10\" height=\"", num(bottom - start), "\" rx=\"2\"/>"}));
        return bottom;
    };
    auto destroy = [&](Column& col, double lineY) -> double {
        double cx = col.x;
        double cy = lineY + 16.0;
        col.lifeEnd = cy;
        Pele::appendCat(crossesOut, {"<path class=\"pele-destroy\" data-id=\"", escText(col.id), "\" d=\"M", num(cx - 6.0), ",", num(cy - 6.0), "L", num(cx + 6.0), ",", num(cy + 6.0), "M", num(cx + 6.0), ",", num(cy - 6.0), "L", num(cx - 6.0), ",", num(cy + 6.0), "\"/>"});
        return cy + 6.0;
    };
    auto actorSvg = [&](const Column& col, double top, bool footer) -> std::string {
        std::string cls;
        if (auto it = col.actor.properties.find("class"); it != col.actor.properties.end()) {
            cls = classNames(it->second);
        }
        return Pele::cat({"<g class=\"pele-node pele-actor pele-actor-", col.kind, (footer ? " pele-actor-footer" : ""), cls, "\" data-id=\"", escText(col.id), "\">", drawActor(col.kind, col.x, top, col.size, col.label, icons, isFigure(col.kind) ? inkOn(col.actor.box ? col.actor.box->fill : "") : ""), "</g>"});
    };
    auto badge = [&](size_t i, double cx, double cy, double w) -> std::string {
        return Pele::cat({"<g class=\"pele-sequence-number\"><rect x=\"", num(cx - w / 2.0), "\" y=\"", num(cy - BADGE_H / 2.0), "\" width=\"", num(w), "\" height=\"16\" rx=\"8\" fill=\"var(--_l)\" stroke=\"none\"/><text x=\"", num(cx), "\" y=\"", num(cy + tiny * 0.35), "\" text-anchor=\"middle\" font-size=\"", num(tiny), "\" fill=\"var(--_bg)\" stroke=\"none\">", esc(num(numbering.index[i])), "</text></g>"});
    };

    const auto& styles = lineStyles();
    double y = headBottom + 6.0;
    for (size_t i = 0; i < count; i++) {
        const auto& m = messages[i];
        int type = m.type;
        const auto& frame = frames[i];

        if (m.hasType && type == LINETYPE::ACTIVE_START) {
            if (m.hasFrom) {
                if (auto it = colOf.find(m.from); it != colOf.end()) {
                    cols[it->second].activations.push_back(y);
                }
            }
        } else if (m.hasType && type == LINETYPE::ACTIVE_END) {
            if (m.hasFrom) {
                if (auto it = colOf.find(m.from); it != colOf.end() && !cols[it->second].activations.empty()) {
                    y = std::max(y, endActivation(cols[it->second], y));
                }
            }
        } else if (frame && frame->dividers[0] == i) {
            stack.push_back(frame);
            if (frame->kind == "rect") {
                frame->top = y + 8.0;
                y = frame->top;
            } else {
                frame->top = y + 12.0;
                y = frame->top + std::max(TAB_H, frame->title.height + 2.0);
            }
        } else if (frame) {
            stack.pop_back();
            double bottom = y + 12.0;
            std::string x = num(frame->x0);
            std::string w = num(frame->x1 - frame->x0);
            if (frame->kind == "rect") {
                std::string fill = !frame->fill.empty() ? resolveStyle({"fill:" + frame->fill}).shape : "";
                rectsOut.push_back(Pele::cat({"<rect class=\"pele-rect\" x=\"", x, "\" y=\"", num(frame->top), "\" width=\"", w, "\" height=\"", num(bottom - frame->top), "\"", rounded((!fill.empty() ? "" : " fill-opacity=\"0.5\"") + fill), "/>"}));
            } else {
                std::string inner = Pele::cat({"<rect x=\"", x, "\" y=\"", num(frame->top), "\" width=\"", w, "\" height=\"", num(bottom - frame->top), "\" fill=\"none\"", ROUNDED, "/>"});
                for (size_t k = 0; k < frame->dividerY.size(); k++) {
                    double dy = frame->dividerY[k];
                    const auto& label = frame->dividerLabels[k];
                    Pele::appendCat(inner, {"<path d=\"M", x, ",", num(dy), "H", num(frame->x1), "\"", DASH, "/>", backed(label, frame->x0 + 8.0 + label.width / 2.0, dy + 2.0 + label.height / 2.0, true)});
                }
                inner += "<rect x=\"" + x + "\" y=\"" + num(frame->top) + "\" width=\"" + num(frame->tabW) + "\" height=\"20\" fill=\"var(--_s)\"" + ROUNDED + "/>" +
                         "<text class=\"pele-frame-kind\" x=\"" + num(frame->x0 + frame->tabW / 2.0) + "\" y=\"" +
                         num(frame->top + TAB_H / 2.0 + tiny * 0.35) + "\" text-anchor=\"middle\" font-size=\"" + num(tiny) +
                         "\" font-weight=\"var(--_hw)\" fill=\"var(--_m)\" stroke=\"none\">" + frame->kind + "</text>" +
                         backed(
                             frame->title,
                             frame->x0 + frame->tabW + 8.0 + frame->title.width / 2.0,
                             frame->top + std::max(TAB_H, frame->title.height) / 2.0,
                             true
                         );
                framesOut += "<g class=\"pele-frame pele-frame-" + frame->kind + "\">" + inner + "</g>";
            }
            y = bottom;
        } else if (m.hasType && (type == LINETYPE::ALT_ELSE || type == LINETYPE::PAR_AND || type == LINETYPE::CRITICAL_OPTION)) {
            if (!stack.empty()) {
                auto& current = stack.back();
                const auto& label = current->dividerLabels[current->dividerY.size()];
                current->dividerY.push_back(y + 12.0);
                y += 12.0 + (label.height > 0.0 ? label.height : 0.0);
            }
        } else if (views[i].has_value() && views[i]->kind == V::Note) {
            const auto& view = *views[i];
            const auto& a = cols[view.a];
            double h = view.label.height + 2.0 * std::max(4.0, notePad - 4.0);
            double top = y + 10.0;
            double x = view.x;
            if (m.placement == PLACEMENT::RIGHTOF) x += rightEdge(a) - a.x;
            else if (m.placement == PLACEMENT::LEFTOF) x += leftEdge(a) - a.x;
            extend(x, x + view.w);
            Pele::appendCat(notesOut, {"<g class=\"pele-note\" data-id=\"i", escText(m.id), "\">", "<rect x=\"", num(x), "\" y=\"", num(top), "\" width=\"", num(view.w), "\" height=\"", num(h), "\"", ROUNDED, "/>", labelSvg(view.label, x + view.w / 2.0, top + h / 2.0, " class=\"pele-label\" stroke=\"none\" fill=\"var(--_fg)\"", icons), "</g>"});
            y = top + h;
        } else if (views[i].has_value()) {
            const auto& view = *views[i];
            const auto& st = styles[type];
            auto& a = cols[view.a];
            auto& b = cols[view.b];
            const auto& label = view.label;
            double bw = badgeWidth(i);
            int starting = 0;
            for (size_t k = i + 1; k < count; k++) {
                if (!messages[k].hasType) break;
                int t = messages[k].type;
                if (t == LINETYPE::ACTIVE_START) {
                    if (messages[k].hasFrom && messages[k].from == m.to) starting++;
                } else if (t != LINETYPE::ACTIVE_END && t != LINETYPE::CENTRAL_CONNECTION && t != LINETYPE::CENTRAL_CONNECTION_REVERSE) {
                    break;
                }
            }
            double up = st.bottom ? 1.0 : -1.0;
            std::string attrs = st.dotted ? DASH : "";
            int cc = m.centralConnection;
            bool circleTo = (cc == LINETYPE::CENTRAL_CONNECTION || cc == LINETYPE::CENTRAL_CONNECTION_DUAL);
            bool circleFrom = (cc == LINETYPE::CENTRAL_CONNECTION_REVERSE || cc == LINETYPE::CENTRAL_CONNECTION_DUAL);
            std::string line;
            std::string color = "var(--_l)";
            auto circle = [&](double cx, double cy) -> std::string {
                return Pele::cat({"<circle class=\"pele-central\" cx=\"", num(cx), "\" cy=\"", num(cy), "\" r=\"4\" fill=\"", color, "\" stroke=\"none\"/>"});
            };
            bool tailAtEnd = st.atStart && !st.atEnd;
            double headTrim = trim(st.head);
            std::string inner;
            double lineY = 0.0;

            if (view.kind == V::Self) {
                double top = y + msgGap + std::max(0.0, (label.height - SELF_H) / 2.0);
                double bottom = top + SELF_H;
                double x0 = circleFrom ? a.x + CENTRAL_R : rightEdge(a);
                double x1 = circleTo ? a.x + CENTRAL_R : rightEdge(a, starting);
                double out = std::max(x0, x1) + SELF_W + bw / 2.0;
                line = inkOf(fillUnder(a.x, out));
                if (!line.empty()) color = line;
                if (circleFrom) inner += circle(a.x, top);
                if (circleTo) inner += circle(a.x, bottom);
                if (bw > 0.0) {
                    inner += badge(i, tailAtEnd ? x1 : x0, tailAtEnd ? bottom : top, bw);
                    if (tailAtEnd) x1 += bw / 2.0;
                    else x0 += bw / 2.0;
                }
                std::string heads = (st.atStart ? head(st.head, x0, top, -1.0, up, color) : "") +
                                    (st.atEnd ? head(st.head, x1, bottom, -1.0, -up, color) : "");
                std::string from = num(x0 + (st.atStart ? headTrim : 0.0));
                std::string to = num(x1 + (st.atEnd ? headTrim : 0.0));
                std::string d = rightAngles
                    ? Pele::cat({"M", from, ",", num(top), "H", num(out), "V", num(bottom), "H", to})
                    : Pele::cat({"M", from, ",", num(top), "H", num(out - 6.0), "A6,6 0 0 1 ", num(out), ",", num(top + 6.0), "V", num(bottom - 6.0), "A6,6 0 0 1 ", num(out - 6.0), ",", num(bottom), "H", to});
                inner = Pele::cat({"<path d=\"", d, "\"", attrs, "/>", heads, inner});
                extend(a.x, out + 8.0 + label.width);
                if (label.width > 0.0) {
                    Pele::appendCat(labelsOut, {"<g class=\"pele-edge-label\" data-id=\"i", escText(m.id), "\">", labelSvg(label, out + 8.0 + label.width / 2.0, (top + bottom) / 2.0, under(out + 8.0, out + 8.0 + label.width), icons), "</g>"});
                }
                lineY = std::max(bottom, (top + bottom) / 2.0 + label.height / 2.0);
            } else {
                double dir = b.x > a.x ? 1.0 : -1.0;
                bool creating = (b.createAt == static_cast<int>(i));
                line = inkOf(fillUnder(std::min(a.x, b.x), std::max(a.x, b.x)));
                if (!line.empty()) color = line;
                lineY = y + msgGap + std::max(label.height > 0.0 ? label.height + 2.0 : 6.0, creating ? b.size.h / 2.0 - msgGap + 6.0 : 0.0);
                double x0 = dir > 0.0 ? rightEdge(a) : leftEdge(a);
                double x1 = dir > 0.0 ? leftEdge(b, starting) : rightEdge(b, starting);
                if (creating) {
                    b.top = lineY - b.size.h / 2.0;
                    b.lifeStart = b.top + b.size.h;
                    x1 = b.x - dir * (isFigure(b.kind) ? 18.0 : b.size.w / 2.0 + 1.0);
                }
                if (circleFrom) {
                    inner += circle(a.x, lineY);
                    x0 = a.x + dir * CENTRAL_R;
                }
                if (circleTo) {
                    inner += circle(b.x, lineY);
                    x1 = b.x - dir * CENTRAL_R;
                }
                if (bw > 0.0) {
                    if (tailAtEnd) {
                        double cx = circleTo ? x1 - dir * (bw / 2.0 + 2.0) : x1;
                        inner += badge(i, cx, lineY, bw);
                        x1 = cx - (dir * bw) / 2.0;
                    } else {
                        double cx = circleFrom ? x0 + dir * (bw / 2.0 + 2.0) : x0;
                        inner += badge(i, cx, lineY, bw);
                        x0 = cx + (dir * bw) / 2.0;
                    }
                }
                std::string heads = (st.atStart ? head(st.head, x0, lineY, -dir, up * dir, color) : "") +
                                    (st.atEnd ? head(st.head, x1, lineY, dir, up * dir, color) : "");
                double from = x0 + (st.atStart ? dir * headTrim : 0.0);
                double to = x1 - (st.atEnd ? dir * headTrim : 0.0);
                inner = Pele::cat({"<path d=\"M", num(from), ",", num(lineY), "H", num(to), "\"", attrs, "/>", heads, inner});
                if (label.width > 0.0) {
                    double l = std::min(x0, x1);
                    double r = std::max(x0, x1);
                    double cx = align < 0 ? l + msgPad + label.width / 2.0 : (align > 0 ? r - msgPad - label.width / 2.0 : (l + r) / 2.0);
                    extend(cx - label.width / 2.0, cx + label.width / 2.0);
                    Pele::appendCat(labelsOut, {"<g class=\"pele-edge-label\" data-id=\"i", escText(m.id), "\">", backed(label, cx, lineY - 4.0 - label.height / 2.0), "</g>"});
                }
                if (creating) y = lineY + b.size.h / 2.0 - 6.0;
            }
            Pele::appendCat(edgesOut, {"<g class=\"pele-edge pele-message\" data-id=\"i", escText(m.id), "\"", (!line.empty() ? Pele::cat({" stroke=\"", line, "\""}) : ""), ">", inner, "</g>"});
            y = std::max(y, lineY);
            if (a.destroyAt == static_cast<int>(i)) y = std::max(y, destroy(a, lineY));
            if (b.destroyAt == static_cast<int>(i) && view.b != view.a) y = std::max(y, destroy(b, lineY));
        }
    }

    y += 20.0;
    for (auto& col : cols) while (!col.activations.empty()) endActivation(col, y);
    double bottom = y;
    if (mirror) {
        double footH = 0.0;
        for (const auto& col : cols) {
            if (col.lifeEnd >= 0.0) continue;
            footH = std::max(footH, col.size.h);
            actorsOut += actorSvg(col, y, true);
        }
        bottom = y + footH;
    }
    std::string lifelines;
    for (const auto& col : cols) {
        actorsOut += actorSvg(col, col.top, false);
        double end = col.lifeEnd >= 0.0 ? col.lifeEnd : y;
        if (end > col.lifeStart) {
            Pele::appendCat(lifelines, {"<path class=\"pele-lifeline\" data-id=\"", escText(col.id), "\" d=\"M", num(col.x), ",", num(col.lifeStart), "V", num(end), "\"/>"});
        }
    }

    std::string boxesOut;
    for (const auto& run : runs) {
        auto [x0, x1] = boxEdges(run);
        std::string fill = run.box->fill == "transparent" ? "" : resolveStyle({"fill:" + run.box->fill}).shape;
        Pele::appendCat(boxesOut, {"<g class=\"pele-cluster pele-box\"", (run.box->hasName ? Pele::cat({" data-id=\"", escText(run.box->name), "\""}) : ""), ">", "<rect x=\"", num(x0), "\" y=\"0\" width=\"", num(x1 - x0), "\" height=\"", num(bottom + boxPad), "\" fill=\"var(--_a)\" stroke=\"var(--_b)\"", rounded((!fill.empty() ? "" : " fill-opacity=\"0.5\"") + fill), "/>", labelSvg(
                        run.title, 
                        (x0 + x1) / 2.0, 
                        boxPad / 2.0 + 2.0 + run.title.height / 2.0, Pele::cat({" class=\"pele-cluster-label\"", (!inkOn(run.box->fill).empty() ? inkOn(run.box->fill) : std::string(" fill=\"var(--_m)\""))}), 
                        icons
                    ), "</g>"});
    }
    if (!runs.empty()) bottom += boxPad;

    double minY = 0.0;
    std::string titleOut;
    double room = titleRoom(options);
    Label plainTitle = titleLabel(db.title, measurer, size, tight ? room : INF);
    Label title = tight ? breakLabel(plainTitle, measurer, std::max(120.0, room)) : plainTitle;
    if (minX > maxX) extend(0.0, std::max(title.width, 40.0));
    if (title.width > 0.0) {
        double cx = (minX + maxX) / 2.0;
        extend(cx - title.width / 2.0, cx + title.width / 2.0);
        minY = -title.height - 12.0;
        titleOut = labelSvg(title, cx, minY + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"", icons);
    }

    double width = std::ceil(maxX - minX + 2.0 * pad);
    if (grown && width > options.maxWidth) {
        return draw(db, config, options, tight, uneven ? hold : (hold > 0.0 ? INF : width - std::floor(options.maxWidth)), false);
    }
    double height = std::ceil(bottom - minY + 2.0 * pad);

    std::string rectsJoined;
    for (auto it = rectsOut.rbegin(); it != rectsOut.rend(); ++it) rectsJoined += *it;
    std::string actsJoined;
    for (auto it = activationsOut.rbegin(); it != activationsOut.rend(); ++it) actsJoined += *it;

    std::string svg = svgDocument(
        "sequence",
        width,
        height,
        size,
        options,
        db.asMeta(),
        "<g transform=\"translate(" + num(pad - minX) + "," + num(pad - minY) + ")\">" +
            titleOut +
            (!boxesOut.empty() ? Pele::cat({"<g class=\"pele-boxes\" font-size=\"", num(small), "\">", boxesOut, "</g>"}) : "") +
            (!rectsOut.empty() ? Pele::cat({"<g class=\"pele-rects\" fill=\"var(--_a)\">", rectsJoined, "</g>"}) : "") +
            (!lifelines.empty() ? Pele::cat({"<g class=\"pele-lifelines\" fill=\"none\" stroke=\"var(--_b)\">", lifelines, "</g>"}) : "") +
            (!activationsOut.empty() ? Pele::cat({"<g class=\"pele-activations\" fill=\"var(--_s)\" stroke=\"var(--_b)\">", actsJoined, "</g>"}) : "") +
            (!framesOut.empty() ? Pele::cat({"<g class=\"pele-frames\" font-size=\"", num(small), "\" stroke=\"var(--_b)\">", framesOut, "</g>"}) : "") +
            (!edgesOut.empty() ? Pele::cat({"<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">", edgesOut, "</g>"}) : "") +
            (!crossesOut.empty() ? Pele::cat({"<g class=\"pele-destroys\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"1.5\" stroke-linecap=\"round\">", crossesOut, "</g>"}) : "") +
            (!labelsOut.empty() ? Pele::cat({"<g class=\"pele-edge-labels\" font-size=\"", num(small), "\">", labelsOut, "</g>"}) : "") +
            (!notesOut.empty() ? Pele::cat({"<g class=\"pele-notes\" font-size=\"", num(small), "\" fill=\"var(--_a)\" stroke=\"var(--_b)\">", notesOut, "</g>"}) : "") +
            (!actorsOut.empty() ? Pele::cat({"<g class=\"pele-nodes\">", actorsOut, "</g>"}) : "") +
        "</g>"
    );
    return {svg, width, height, {}};
}

} // namespace detail_render

inline SeqDb parse(const std::string& source, const Config& config, const std::string& title = "") {
    SeqDbOptions opts;
    if (const auto* w = config.get("wrap"); w && w->isBool()) {
        opts.wrap = w->bVal;
    }
    if (const auto* sc = config.get("sequence"); sc && sc->isMap()) {
        if (const auto* sw = sc->get("wrap"); sw && sw->isBool() && sw->bVal) {
            opts.sequenceWrap = true;
        }
    }
    SeqDb db(opts);
    if (!title.empty()) db.setDiagramTitle(title);
    detail_parser::parseSequence(source, db);
    return db;
}

inline Rendered renderSequence(const SeqDb& db, const Config& config, const RenderOptions& options) noexcept {
    return Pele::tighten(options, [&](bool tight) {
        return detail_render::draw(db, config, options, tight);
    });
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    SeqDb db = parse(source, config, title);
    return renderSequence(db, config, options);
}

inline Rendered render(const std::string& source, const Config& config, const RenderOptions& options = {}, const std::string& title = "") {
    return render(source, config, title, options);
}

} // namespace Pele::SequenceDiagram

namespace Pele {

inline Rendered renderSequence(const std::string& source, const Config& config = Config::makeMap(), const RenderOptions& options = {}, const std::string& title = "") {
    return SequenceDiagram::render(source, config, title, options);
}

inline Rendered renderSequence(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return SequenceDiagram::render(source, config, title, options);
}

} // namespace Pele
