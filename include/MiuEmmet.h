#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>

namespace Emmet {

enum class Mode {
    Auto,
    Html,
    Jsx,
    Xml,
    Css,
    CssInline,
    CodeStrict
};

struct Tabstop {
    size_t startOffset;
    size_t endOffset;
};

struct TabstopGroup {
    int id = 0;
    std::vector<std::pair<size_t, size_t>> ranges;
};

struct ExpandResult {
    bool matched = false;
    bool success = false;
    size_t abbrStartCol = 0;
    size_t abbrLen = 0;
    size_t cursorOffset = 0;
    std::string expanded;
    std::string text;
    std::vector<Tabstop> tabstops;
    std::vector<TabstopGroup> groups;
};

static inline char toLowerAscii(char c)  noexcept{
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static inline std::string toLowerStr(const std::string& s)  noexcept{
    std::string r = s;
    for (char& c : r) c = toLowerAscii(c);
    return r;
}

static inline bool isAsciiAlpha(char c)  noexcept{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool isAsciiDigit(char c)  noexcept{
    return (c >= '0' && c <= '9');
}

static inline bool isAsciiAlnum(char c)  noexcept{
    return isAsciiAlpha(c) || isAsciiDigit(c);
}

static inline bool isAsciiXdigit(char c)  noexcept{
    return isAsciiDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline bool isAsciiUpper(char c)  noexcept{
    return (c >= 'A' && c <= 'Z');
}

static inline bool isIdentChar(char c)  noexcept{
    return isAsciiAlnum(c) || c == '_' || c == '-';
}

static inline bool isTagChar(char c)  noexcept{
    return isIdentChar(c) || c == ':' || c == '$' || c == '@' || c == '!';
}

static bool isInPipeList(const char* list, const std::string& s)  noexcept{
    if (s.empty()) return false;
    size_t len = s.size();
    const char* p = list;
    while ((p = std::strstr(p, s.c_str())) != nullptr) {
        if (p > list && p[-1] == '|' && p[len] == '|') return true;
        p += len;
    }
    return false;
}

static inline bool isVoidTag(const std::string& lowerTag)  noexcept{
    static constexpr const char kVoidList[] =
        "|area|base|basefont|br|col|command|embed|frame|hr|img|input|isindex|keygen|link|meta|param|source|track|wbr|";
    return isInPipeList(kVoidList, lowerTag);
}

static inline bool isInlineTag(const std::string& lowerTag)  noexcept{
    static constexpr const char kInlineList[] =
        "|a|abbr|acronym|b|basefont|bdi|bdo|big|br|button|cite|code|data|del|dfn|em|font|i|img|input|ins|kbd|label|mark|meter|output|progress|q|rp|rt|rtc|s|samp|slot|small|span|strike|strong|sub|sup|textarea|time|tt|u|var|wbr|";
    return isInPipeList(kInlineList, lowerTag);
}

static inline bool isStructuralContainerTag(const std::string& lowerTag)  noexcept{
    static constexpr const char kStructList[] =
        "|html|head|body|div|ul|ol|dl|table|thead|tbody|tfoot|tr|section|article|header|footer|nav|main|aside|form|fieldset|select|optgroup|figure|details|dialog|blockquote|hgroup|menu|colgroup|picture|video|audio|canvas|map|datalist|template|svg|";
    return isInPipeList(kStructList, lowerTag);
}

static inline bool isBooleanAttr(const std::string& lowerAttr)  noexcept{
    static constexpr const char kBoolList[] =
        "|allowfullscreen|async|autofocus|autoplay|checked|compact|controls|declare|default|defaultchecked|defaultmuted|defaultselected|defer|disabled|enabled|formnovalidate|hidden|indeterminate|inert|ismap|itemscope|loop|multiple|muted|nohref|noresize|noshade|novalidate|nowrap|open|pauseonexit|readonly|required|reversed|scoped|seamless|selected|sortable|truespeed|typemustmatch|visible|";
    return isInPipeList(kBoolList, lowerAttr);
}

static inline bool isCompoundPlusTag(const std::string& lowerTag)  noexcept{
    static constexpr const char kPlusList[] =
        "|ul|ol|dl|table|tr|thead|tbody|tfoot|select|optg|optgroup|map|colg|colgroup|pic|picture|";
    return isInPipeList(kPlusList, lowerTag);
}

static inline bool isKnownHtmlTagOrSnippet(const std::string& rawName)  noexcept{
    if (rawName.empty()) return false;
    std::string lower = toLowerStr(rawName);
    if (lower == "!" || lower == "!!!" || lower == "!!!4t" || lower == "!!!4s" || lower == "!!!xt" || lower == "!!!xs" || lower == "!!!xxs") return true;
    if (lower.compare(0, 5, "lorem") == 0 || lower.compare(0, 6, "lipsum") == 0) {
        size_t pos = (lower[1] == 'o') ? 5 : 6;
        while (pos < lower.size() && isAsciiDigit(lower[pos])) pos++;
        return pos == lower.size();
    }
    std::string normalized;
    for (size_t i = 0; i < lower.size(); ) {
        if (lower[i] == '$') {
            while (i < lower.size() && lower[i] == '$') i++;
            if (i < lower.size() && lower[i] == '@') {
                i++;
                if (i < lower.size() && lower[i] == '-') i++;
                while (i < lower.size() && isAsciiDigit(lower[i])) i++;
            }
            normalized += '1';
        } else {
            normalized += lower[i++];
        }
    }
    if (!normalized.empty() && normalized.back() == '+') {
        std::string stem = normalized.substr(0, normalized.size() - 1);
        if (!isCompoundPlusTag(stem)) return false;
        normalized = stem;
    }

    static constexpr const char kKnownList[] =
        "|a|a:link|a:mail|a:tel|a:blank|abbr|acr|acronym|adr|address|area|area:d|area:c|area:r|area:p|art|article|aside|audio|"
        "b|base|basefont|bdi|bdo|bdo:r|bdo:l|big|bq|blockquote|body|br|btn|button|btn:s|button:s|button:submit|btn:r|button:r|button:reset|"
        "btn:b|button:b|button:button|btn:d|button:d|button:disabled|c|cc:ie|cc:ie6|cc:noie|canvas|cap|caption|center|cite|code|col|colg|colgroup|"
        "command|content|data|datag|datagrid|datal|datalist|dd|del|det|details|dfn|dialog|dir|div|dl|doc|doc4|dt|em|emb|embed|"
        "fset|fieldset|fset:d|fieldset:d|fieldset:disabled|fig|figure|figc|figcaption|font|ftr|footer|form|form:get|form:post|frame|frameset|"
        "h1|h2|h3|h4|h5|h6|head|hdr|header|hgr|hgroup|hr|html|html:5|html:4t|html:4s|html:xt|html:xs|html:xxs|html:xml|"
        "i|ifr|iframe|img|img:s|img:srcset|img:z|img:sizes|inp|input|input:h|input:hidden|input:t|input:text|input:search|"
        "input:e|input:email|input:url|input:p|input:password|input:datetime|input:d|input:date|input:datetime-local|input:month|input:week|input:time|"
        "input:tel|input:n|input:number|input:color|input:c|input:checkbox|input:r|input:radio|input:range|input:f|input:file|input:s|input:submit|"
        "input:i|input:image|input:b|input:button|input:reset|ins|kbd|keygen|label|leg|legend|li|"
        "link|link:css|link:print|link:favicon|link:touch|link:rss|link:atom|link:im|link:import|link:manifest|"
        "main|map|mark|menu|menu:c|menu:context|menu:t|menu:toolbar|menuitem|meta|meta:utf|meta:win|meta:vp|meta:compat|meta:edge|meta:redirect|"
        "meta:kw|meta:desc|meter|nav|nobr|noframes|noscript|obj|object|ol|optg|optgroup|opt|option|out|output|p|param|pic|picture|"
        "pre|prog|progress|q|rp|rt|rtc|ruby|s|samp|script|script:src|script:module|sect|section|select|select:d|select:disabled|"
        "slot|small|src|source|source:src|source:srcset|source:media|source:type|source:sizes|source:media:type|source:media:sizes|source:sizes:type|"
        "span|strike|str|strong|style|sub|sum|summary|sup|svg|table|tbody|td|tmpl|template|tarea|textarea|tfoot|th|thead|"
        "time|title|tr|track|tt|u|ul|var|video|wbr|ri:d|ri:dpr|ri:v|ri:viewport|ri:a|ri:art|ri:t|ri:type|";
    return isInPipeList(kKnownList, normalized);
}
static std::string resolveNumbering(const std::string& input, int index, int total)  noexcept{
    std::string out;
    out.reserve(input.size() + 8);
    for (size_t i = 0; i < input.size(); ) {
        if (input[i] == '\\' && i + 1 < input.size() && (input[i + 1] == '$' || input[i + 1] == '{' || input[i + 1] == '}')) {
            out += input[i + 1];
            i += 2;
            continue;
        }
        if (input[i] == '$') {
            size_t dollarCount = 0;
            while (i < input.size() && input[i] == '$') {
                dollarCount++;
                i++;
            }
            bool descending = false;
            int base = 1;
            if (i < input.size() && input[i] == '@') {
                size_t atPos = i;
                i++;
                if (i < input.size() && input[i] == '-') {
                    descending = true;
                    i++;
                }
                int parsedBase = 0;
                bool hasDigits = false;
                while (i < input.size() && isAsciiDigit(input[i])) {
                    parsedBase = parsedBase * 10 + (input[i] - '0');
                    hasDigits = true;
                    i++;
                }
                if (hasDigits) {
                    base = parsedBase;
                } else if (!descending) {
                    i = atPos;
                }
            }
            int val = descending ? (total + base - 1 - index) : (base + index);
            if (val < 0) val = 0;
            std::string numStr = std::to_string(val);
            if (numStr.size() < dollarCount) {
                out.append(dollarCount - numStr.size(), '0');
            }
            out += numStr;
        } else {
            out += input[i++];
        }
    }
    return out;
}

static std::string generateLorem(int wordCount, int iterationIndex)  noexcept{
    static constexpr const char* kWords[] = {
        "lorem", "ipsum", "dolor", "sit", "amet", "consectetur", "adipisicing", "elit",
        "sed", "do", "eiusmod", "tempor", "incididunt", "ut", "labore", "et", "dolore",
        "magna", "aliqua", "ut", "enim", "ad", "minim", "veniam", "quis", "nostrud",
        "exercitation", "ullamco", "laboris", "nisi", "ut", "aliquip", "ex", "ea",
        "commodo", "consequat", "duis", "aute", "irure", "dolor", "in", "reprehenderit",
        "in", "voluptate", "velit", "esse", "cillum", "dolore", "eu", "fugiat", "nulla",
        "pariatur", "excepteur", "sint", "occaecat", "cupidatat", "non", "proident",
        "sunt", "in", "culpa", "qui", "officia", "deserunt", "mollit", "anim", "id",
        "est", "laborum"
    };
    constexpr int kNumWords = (int)(sizeof(kWords) / sizeof(kWords[0]));
    if (wordCount <= 0) wordCount = 30;
    if (wordCount > 2000) wordCount = 2000;

    std::string out;
    int offset = (iterationIndex == 0) ? 0 : ((iterationIndex * 17) % kNumWords);
    bool capNext = true;
    for (int i = 0; i < wordCount; ++i) {
        if (i > 0) out += ' ';
        std::string w = kWords[(offset + i) % kNumWords];
        if (capNext && !w.empty()) {
            w[0] = (char)std::toupper((unsigned char)w[0]);
            capNext = false;
        }
        out += w;
        if (iterationIndex == 0 && i == 4 && wordCount >= 8) {
            out += ',';
        } else if (i < wordCount - 1 && ((i + 1) % 12 == 0) && wordCount >= 16) {
            out += '.';
            capNext = true;
        }
    }
    if (wordCount >= 6) out += '.';
    return out;
}

struct Attr {
    std::string name;
    std::string value;
    bool isBoolean = false;
    bool hasTabstop = false;
    bool removeDefault = false;
    bool isExpr = false;
};

struct HtmlNode {
    int parent = -1;
    std::vector<int> children;
    bool isGroup = false;
    bool isTextOnly = false;
    bool isComment = false;
    bool isLorem = false;
    int loremWords = 30;
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::vector<Attr> attrs;
    std::string text;
    int repeat = 1;
    bool hasExplicitRepeat = false;
    bool selfClose = false;
    std::string specialTemplate;
};

static std::string resolveImplicitTag(const std::string& parentLowerTag)  noexcept{
    if (parentLowerTag == "ul" || parentLowerTag == "ol" || parentLowerTag == "menu") return "li";
    if (parentLowerTag == "table" || parentLowerTag == "tbody" || parentLowerTag == "thead" || parentLowerTag == "tfoot") return "tr";
    if (parentLowerTag == "tr") return "td";
    if (parentLowerTag == "select" || parentLowerTag == "optgroup") return "option";
    if (parentLowerTag == "dl") return "dt";
    if (parentLowerTag == "map") return "area";
    if (parentLowerTag == "colgroup") return "col";
    if (parentLowerTag == "audio" || parentLowerTag == "video" || parentLowerTag == "picture") return "source";
    if (isInlineTag(parentLowerTag) || parentLowerTag == "p" ||
        parentLowerTag == "h1" || parentLowerTag == "h2" || parentLowerTag == "h3" ||
        parentLowerTag == "h4" || parentLowerTag == "h5" || parentLowerTag == "h6") {
        return "span";
    }
    return "div";
}

static std::string findNearestElementTag(const std::vector<HtmlNode>& pool, int nodeIdx)  noexcept{
    int p = pool[nodeIdx].parent;
    while (p != -1) {
        if (!pool[p].isGroup && !pool[p].tag.empty()) {
            return resolveNumbering(toLowerStr(pool[p].tag), 0, 1);
        }
        p = pool[p].parent;
    }
    return "";
}

static void applySnippetDefaults(std::vector<HtmlNode>& pool, int nodeIdx)  noexcept{
    std::string rawTag = pool[nodeIdx].tag;
    if (rawTag.empty()) return;
    std::string lower = toLowerStr(rawTag);

    if (lower == "!" || lower == "html:5" || lower == "doc" || lower == "doc4") {
        pool[nodeIdx].specialTemplate = (lower == "doc" || lower == "doc4") ? lower : "html:5";
        pool[nodeIdx].tag = "html";
        return;
    }
    if (lower.compare(0, 3, "!!!") == 0 || lower == "html:4t" || lower == "html:4s" || lower == "html:xt" || lower == "html:xs" || lower == "html:xxs") {
        pool[nodeIdx].specialTemplate = lower;
        pool[nodeIdx].tag = "";
        return;
    }
    if (lower == "cc:ie" || lower == "cc:ie6" || lower == "cc:noie") {
        pool[nodeIdx].specialTemplate = lower;
        pool[nodeIdx].tag = "";
        return;
    }

    bool plusCompound = false;
    if (lower.size() > 1 && lower.back() == '+') {
        plusCompound = true;
        lower.pop_back();
        rawTag.pop_back();
        pool[nodeIdx].tag = rawTag;
    }

    std::vector<Attr> defAttrs;
    auto defA = [&](const char* n, const char* v, bool stop, bool b = false) {
        Attr a; a.name = n; a.value = v; a.hasTabstop = stop; a.isBoolean = b;
        defAttrs.push_back(a);
    };

    std::string canon = rawTag;
    if (lower == "c") {
        pool[nodeIdx].isComment = true;
        canon = "";
    } else {
        static constexpr const char kHtmlSnippetBlob[] =
            "bq\0blockquote\0\0"
            "hdr\0header\0\0"
            "ftr\0footer\0\0"
            "btn\0button\0\0"
            "sect\0section\0\0"
            "art\0article\0\0"
            "str\0strong\0\0"
            "adr\0address\0\0"
            "acr\0acronym\0title=|\0"
            "acronym\0acronym\0title=|\0"
            "abbr\0abbr\0title=|\0"
            "fig\0figure\0\0"
            "figc\0figcaption\0\0"
            "cap\0caption\0\0"
            "colg\0colgroup\0\0"
            "fset\0fieldset\0\0"
            "fset:d\0fieldset\0?disabled=disabled\0"
            "fieldset:d\0fieldset\0?disabled=disabled\0"
            "fieldset:disabled\0fieldset\0?disabled=disabled\0"
            "leg\0legend\0\0"
            "optg\0optgroup\0\0"
            "datal\0datalist\0\0"
            "datag\0datagrid\0\0"
            "det\0details\0\0"
            "sum\0summary\0\0"
            "tmpl\0template\0\0"
            "pic\0picture\0\0"
            "src\0source\0\0"
            "out\0output\0\0"
            "prog\0progress\0\0"
            "hgr\0hgroup\0\0"
            "a\0a\0href=|\0"
            "a:link\0a\0href=http://|\0"
            "a:mail\0a\0href=mailto:|\0"
            "a:tel\0a\0href=tel:+|\0"
            "a:blank\0a\0href=http://|\ttarget=_blank\trel=noopener noreferrer\0"
            "base\0base\0href=|\0"
            "bdo\0bdo\0dir=|\0"
            "bdo:r\0bdo\0dir=rtl\0"
            "bdo:l\0bdo\0dir=ltr\0"
            "link\0link\0rel=stylesheet\thref=|\0"
            "link:css\0link\0rel=stylesheet\thref=style.css\0"
            "link:print\0link\0rel=stylesheet\thref=print.css\tmedia=print\0"
            "link:favicon\0link\0rel=shortcut icon\thref=favicon.ico\ttype=image/x-icon\0"
            "link:touch\0link\0rel=apple-touch-icon\thref=favicon.png\0"
            "link:rss\0link\0rel=alternate\ttype=application/rss+xml\ttitle=RSS\thref=rss.xml\0"
            "link:atom\0link\0rel=alternate\ttype=application/atom+xml\ttitle=Atom\thref=atom.xml\0"
            "link:im\0link\0rel=import\thref=|\0"
            "link:import\0link\0rel=import\thref=|\0"
            "link:manifest\0link\0rel=manifest\thref=manifest.json\0"
            "meta:utf\0meta\0http-equiv=Content-Type\tcontent=text/html;charset=UTF-8\0"
            "meta:win\0meta\0http-equiv=Content-Type\tcontent=text/html;charset=windows-1251\0"
            "meta:vp\0meta\0name=viewport\tcontent=width=device-width, initial-scale=1.0\0"
            "meta:compat\0meta\0http-equiv=X-UA-Compatible\tcontent=IE=7\0"
            "meta:edge\0meta\0http-equiv=X-UA-Compatible\tcontent=ie=edge\0"
            "meta:redirect\0meta\0http-equiv=refresh\tcontent=0; url=http://example.com\0"
            "meta:kw\0meta\0name=keywords\tcontent=|\0"
            "meta:desc\0meta\0name=description\tcontent=|\0"
            "script:src\0script\0src=|\0"
            "script:module\0script\0type=module\tsrc=|\0"
            "img\0img\0src=|\talt=|\0"
            "img:s\0img\0srcset=|\tsrc=|\talt=|\0"
            "img:srcset\0img\0srcset=|\tsrc=|\talt=|\0"
            "img:z\0img\0sizes=|\tsrcset=|\tsrc=|\talt=|\0"
            "img:sizes\0img\0sizes=|\tsrcset=|\tsrc=|\talt=|\0"
            "ifr\0iframe\0src=|\tframeborder=0\0"
            "iframe\0iframe\0src=|\tframeborder=0\0"
            "emb\0embed\0src=|\ttype=|\0"
            "embed\0embed\0src=|\ttype=|\0"
            "obj\0object\0data=|\ttype=|\0"
            "object\0object\0data=|\ttype=|\0"
            "param\0param\0name=|\tvalue=|\0"
            "map\0map\0name=|\0"
            "area\0area\0shape=|\tcoords=|\thref=|\talt=|\0"
            "area:d\0area\0shape=default\thref=|\talt=|\0"
            "area:c\0area\0shape=circle\tcoords=|\thref=|\talt=|\0"
            "area:r\0area\0shape=rect\tcoords=|\thref=|\talt=|\0"
            "area:p\0area\0shape=poly\tcoords=|\thref=|\talt=|\0"
            "form\0form\0action=|\0"
            "form:get\0form\0action=|\tmethod=get\0"
            "form:post\0form\0action=|\tmethod=post\0"
            "label\0label\0for=|\0"
            "input\0input\0type=text\0"
            "input:t\0input\0type=text\0"
            "input:text\0input\0type=text\0"
            "inp\0input\0type=text\tname=|\tid=|\0"
            "input:h\0input\0type=hidden\tname=|\0"
            "input:hidden\0input\0type=hidden\tname=|\0"
            "input:search\0input\0type=search\tname=|\tid=|\0"
            "input:e\0input\0type=email\tname=|\tid=|\0"
            "input:email\0input\0type=email\tname=|\tid=|\0"
            "input:url\0input\0type=url\tname=|\tid=|\0"
            "input:p\0input\0type=password\tname=|\tid=|\0"
            "input:password\0input\0type=password\tname=|\tid=|\0"
            "input:datetime\0input\0type=datetime\tname=|\tid=|\0"
            "input:d\0input\0type=date\tname=|\tid=|\0"
            "input:date\0input\0type=date\tname=|\tid=|\0"
            "input:datetime-local\0input\0type=datetime-local\tname=|\tid=|\0"
            "input:month\0input\0type=month\tname=|\tid=|\0"
            "input:week\0input\0type=week\tname=|\tid=|\0"
            "input:time\0input\0type=time\tname=|\tid=|\0"
            "input:tel\0input\0type=tel\tname=|\tid=|\0"
            "input:n\0input\0type=number\tname=|\tid=|\0"
            "input:number\0input\0type=number\tname=|\tid=|\0"
            "input:color\0input\0type=color\tname=|\tid=|\0"
            "input:c\0input\0type=checkbox\tname=|\tid=|\0"
            "input:checkbox\0input\0type=checkbox\tname=|\tid=|\0"
            "input:r\0input\0type=radio\tname=|\tid=|\0"
            "input:radio\0input\0type=radio\tname=|\tid=|\0"
            "input:range\0input\0type=range\tname=|\tid=|\0"
            "input:f\0input\0type=file\tname=|\tid=|\0"
            "input:file\0input\0type=file\tname=|\tid=|\0"
            "input:s\0input\0type=submit\tvalue=|\0"
            "input:submit\0input\0type=submit\tvalue=|\0"
            "input:i\0input\0type=image\tsrc=|\talt=|\0"
            "input:image\0input\0type=image\tsrc=|\talt=|\0"
            "input:b\0input\0type=button\tvalue=|\0"
            "input:button\0input\0type=button\tvalue=|\0"
            "input:reset\0input\0type=reset\tvalue=|\0"
            "select\0select\0name=|\tid=|\0"
            "select:d\0select\0name=|\tid=|\t?disabled=disabled\0"
            "select:disabled\0select\0name=|\tid=|\t?disabled=disabled\0"
            "opt\0option\0value=|\0"
            "option\0option\0value=|\0"
            "tarea\0textarea\0name=|\tid=|\0"
            "textarea\0textarea\0name=|\tid=|\0"
            "btn:b\0button\0type=button\0"
            "button:b\0button\0type=button\0"
            "button:button\0button\0type=button\0"
            "btn:r\0button\0type=reset\0"
            "button:r\0button\0type=reset\0"
            "button:reset\0button\0type=reset\0"
            "btn:s\0button\0type=submit\0"
            "button:s\0button\0type=submit\0"
            "button:submit\0button\0type=submit\0"
            "btn:d\0button\0?disabled=disabled\0"
            "button:d\0button\0?disabled=disabled\0"
            "button:disabled\0button\0?disabled=disabled\0"
            "video\0video\0src=|\0"
            "audio\0audio\0src=|\0"
            "source:src\0source\0src=|\ttype=|\0"
            "source:srcset\0source\0srcset=|\0"
            "source:media\0source\0media=(min-width: )\tsrcset=|\0"
            "source:type\0source\0srcset=|\ttype=image/\0"
            "source:sizes\0source\0sizes=|\tsrcset=|\0"
            "source:media:type\0source\0media=(min-width: )\tsrcset=|\ttype=image/\0"
            "source:media:sizes\0source\0media=(min-width: )\tsizes=|\tsrcset=|\0"
            "source:sizes:type\0source\0sizes=|\tsrcset=|\ttype=image/\0"
            "ri:d\0img\0srcset=|\tsrc=|\talt=|\0"
            "ri:dpr\0img\0srcset=|\tsrc=|\talt=|\0"
            "ri:v\0img\0sizes=|\tsrcset=|\tsrc=|\talt=|\0"
            "ri:viewport\0img\0sizes=|\tsrcset=|\tsrc=|\talt=|\0"
            "ri:a\0picture\0\0"
            "ri:art\0picture\0\0"
            "ri:t\0picture\0\0"
            "ri:type\0picture\0\0"
            "track\0track\0src=|\tkind=|\tsrclang=|\tlabel=|\0"
            "menu:c\0menu\0type=context\0"
            "menu:context\0menu\0type=context\0"
            "menu:t\0menu\0type=toolbar\0"
            "menu:toolbar\0menu\0type=toolbar\0"
            "html:xml\0html\0xmlns=http://www.w3.org/1999/xhtml\0";

        const char* p = kHtmlSnippetBlob;
        while (*p) {
            const char* k = p;
            p += std::strlen(p) + 1;
            const char* cTag = p;
            p += std::strlen(p) + 1;
            const char* attrSpec = p;
            p += std::strlen(p) + 1;
            if (lower == k) {
                canon = cTag;
                const char* ap = attrSpec;
                while (*ap) {
                    const char* tab = std::strchr(ap, '\t');
                    size_t len = tab ? (size_t)(tab - ap) : std::strlen(ap);
                    std::string item(ap, len);
                    size_t eq = item.find('=');
                    if (eq != std::string::npos) {
                        std::string aname = item.substr(0, eq);
                        std::string aval = item.substr(eq + 1);
                        bool isBool = false;
                        bool hasStop = false;
                        if (!aname.empty() && aname[0] == '?') {
                            isBool = true;
                            aname.erase(0, 1);
                        }
                        if (!aval.empty() && aval.back() == '|') {
                            hasStop = true;
                            aval.pop_back();
                        }
                        defA(aname.c_str(), aval.c_str(), hasStop, isBool);
                    }
                    if (!tab) break;
                    ap = tab + 1;
                }
                break;
            }
        }
    }
    pool[nodeIdx].tag = canon;

    if (!defAttrs.empty()) {
        std::vector<Attr> merged;
        for (auto& da : defAttrs) {
            bool removed = false;
            bool overridden = false;
            for (const auto& ua : pool[nodeIdx].attrs) {
                if (toLowerStr(ua.name) == toLowerStr(da.name)) {
                    if (ua.removeDefault) removed = true;
                    else {
                        merged.push_back(ua);
                        overridden = true;
                    }
                    break;
                }
            }
            if (da.name == "id" && !pool[nodeIdx].id.empty()) continue;
            if (da.name == "class" && !pool[nodeIdx].classes.empty()) continue;
            if (!removed && !overridden) {
                merged.push_back(da);
            }
        }
        for (const auto& ua : pool[nodeIdx].attrs) {
            if (ua.removeDefault) continue;
            bool alreadyIn = false;
            for (const auto& da : defAttrs) {
                if (toLowerStr(ua.name) == toLowerStr(da.name)) {
                    alreadyIn = true;
                    break;
                }
            }
            if (!alreadyIn) merged.push_back(ua);
        }
        pool[nodeIdx].attrs = std::move(merged);
    } else {
        pool[nodeIdx].attrs.erase(
            std::remove_if(pool[nodeIdx].attrs.begin(), pool[nodeIdx].attrs.end(), [](const Attr& a) { return a.removeDefault; }),
            pool[nodeIdx].attrs.end()
        );
    }

    bool riArt = (lower == "ri:a" || lower == "ri:art");
    bool riType = (lower == "ri:t" || lower == "ri:type");
    if ((plusCompound || riArt || riType) && pool[nodeIdx].children.empty()) {
        std::string cLower = toLowerStr(canon);
        auto addChild = [&](int parentIdx, const char* childTag) -> int {
            HtmlNode ch;
            ch.parent = parentIdx;
            ch.tag = childTag;
            int chIdx = (int)pool.size();
            pool.push_back(std::move(ch));
            pool[parentIdx].children.push_back(chIdx);
            applySnippetDefaults(pool, chIdx);
            return chIdx;
        };
        if (riArt) {
            addChild(nodeIdx, "source:media");
            addChild(nodeIdx, "img");
        } else if (riType) {
            addChild(nodeIdx, "source:type");
            addChild(nodeIdx, "img");
        } else if (cLower == "ul" || cLower == "ol") {
            addChild(nodeIdx, "li");
        } else if (cLower == "dl") {
            addChild(nodeIdx, "dt");
            addChild(nodeIdx, "dd");
        } else if (cLower == "table") {
            int trIdx = addChild(nodeIdx, "tr");
            addChild(trIdx, "td");
        } else if (cLower == "tr" || cLower == "thead" || cLower == "tbody" || cLower == "tfoot") {
            if (cLower == "tr") addChild(nodeIdx, "td");
            else {
                int trIdx = addChild(nodeIdx, "tr");
                addChild(trIdx, "td");
            }
        } else if (cLower == "select" || cLower == "optgroup") {
            addChild(nodeIdx, "option");
        } else if (cLower == "map") {
            addChild(nodeIdx, "area");
        } else if (cLower == "colgroup") {
            addChild(nodeIdx, "col");
        } else if (cLower == "picture") {
            addChild(nodeIdx, "source:srcset");
            addChild(nodeIdx, "img");
        }
    }
}

static bool parseHtmlAbbreviation(const std::string& abbr, std::vector<HtmlNode>& pool)  noexcept{
    pool.clear();
    HtmlNode root;
    root.isGroup = true;
    pool.push_back(root);

    size_t i = 0;
    size_t n = abbr.size();

    auto parseSequence = [&](auto& self, int parentIdx, bool insideGroup) -> bool {
        int curParent = parentIdx;
        int prevNode = -1;

        while (i < n) {
            if (insideGroup && abbr[i] == ')') {
                return true;
            }

            int nodeIdx = -1;
            if (abbr[i] == '(') {
                i++;
                HtmlNode grp;
                grp.parent = curParent;
                grp.isGroup = true;
                nodeIdx = (int)pool.size();
                pool.push_back(grp);
                pool[curParent].children.push_back(nodeIdx);

                if (!self(self, nodeIdx, true)) return false;
                if (i >= n || abbr[i] != ')') return false;
                i++;

                if (pool[nodeIdx].children.empty()) return false;

                if (i < n && abbr[i] == '*') {
                    i++;
                    if (i >= n || !isAsciiDigit(abbr[i])) return false;
                    int rep = 0;
                    while (i < n && isAsciiDigit(abbr[i])) {
                        rep = rep * 10 + (abbr[i] - '0');
                        if (rep > 1000) rep = 1000;
                        i++;
                    }
                    if (rep <= 0) return false;
                    pool[nodeIdx].repeat = rep;
                    pool[nodeIdx].hasExplicitRepeat = true;
                }
            } else {
                HtmlNode elem;
                elem.parent = curParent;
                size_t startPos = i;

                if (abbr.compare(i, 3, "!!!") == 0) {
                    size_t s = i;
                    i += 3;
                    while (i < n && isAsciiAlnum(abbr[i])) i++;
                    elem.tag = abbr.substr(s, i - s);
                } else if (abbr[i] == '!' && (i + 1 == n || abbr[i + 1] == '+' || abbr[i + 1] == '>' || abbr[i + 1] == '^' || abbr[i + 1] == ')')) {
                    elem.tag = "!";
                    i++;
                } else if (isAsciiAlpha(abbr[i])) {
                    size_t s = i;
                    while (i < n && isTagChar(abbr[i]) && abbr[i] != '!') {
                        i++;
                    }
                    elem.tag = abbr.substr(s, i - s);
                    if (i < n && abbr[i] == '+' && isCompoundPlusTag(toLowerStr(elem.tag))) {
                        bool isCompound = (i + 1 == n || abbr[i + 1] == ')' || abbr[i + 1] == '^' || abbr[i + 1] == '>' || abbr[i + 1] == '*' ||
                                           abbr[i + 1] == '.' || abbr[i + 1] == '#' || abbr[i + 1] == '[' || abbr[i + 1] == '{');
                        if (isCompound) {
                            elem.tag += '+';
                            i++;
                        }
                    }
                }

                while (i < n) {
                    if (abbr[i] == '#') {
                        i++;
                        size_t s = i;
                        while (i < n && (isIdentChar(abbr[i]) || abbr[i] == '$' || abbr[i] == '@')) i++;
                        if (i == s) return false;
                        elem.id = abbr.substr(s, i - s);
                    } else if (abbr[i] == '.') {
                        i++;
                        size_t s = i;
                        while (i < n && (isIdentChar(abbr[i]) || abbr[i] == '$' || abbr[i] == '@')) i++;
                        if (i == s) return false;
                        elem.classes.push_back(abbr.substr(s, i - s));
                    } else if (abbr[i] == '[') {
                        i++;
                        bool parsedAnyAttr = false;
                        while (i < n && abbr[i] != ']') {
                            while (i < n && (abbr[i] == ' ' || abbr[i] == '\t' || abbr[i] == ',')) i++;
                            if (i >= n || abbr[i] == ']') break;
                            Attr a;
                            bool isQuotedName = false;
                            if (abbr[i] == '\'' || abbr[i] == '"') {
                                char q = abbr[i++];
                                size_t s = i;
                                while (i < n && abbr[i] != q) i++;
                                if (i >= n) return false;
                                a.name = abbr.substr(s, i - s);
                                i++;
                                a.isBoolean = true;
                                isQuotedName = true;
                            } else {
                                if (isAsciiDigit(abbr[i])) return false;
                                size_t s = i;
                                while (i < n && abbr[i] != '=' && abbr[i] != ' ' && abbr[i] != '\t' && abbr[i] != ']' && abbr[i] != '!') {
                                    char ac = abbr[i];
                                    if (!(isAsciiAlnum(ac) || ac == '-' || ac == '_' || ac == ':' || ac == '.' || ac == '@' || ac == '$')) {
                                        return false;
                                    }
                                    i++;
                                }
                                a.name = abbr.substr(s, i - s);
                                if (i < n && abbr[i] == '!') {
                                    a.removeDefault = true;
                                    i++;
                                }
                            }
                            if (a.name.empty()) return false;
                            if (i < n && abbr[i] == '=') {
                                i++;
                                a.isBoolean = false;
                                if (i < n && (abbr[i] == '"' || abbr[i] == '\'')) {
                                    char q = abbr[i++];
                                    size_t s = i;
                                    while (i < n && abbr[i] != q) {
                                        if (abbr[i] == '\\' && i + 1 < n) i++;
                                        i++;
                                    }
                                    if (i >= n) return false;
                                    a.value = abbr.substr(s, i - s);
                                    i++;
                                } else if (i < n && abbr[i] == '{') {
                                    i++;
                                    size_t s = i;
                                    int depth = 1;
                                    while (i < n && depth > 0) {
                                        if (abbr[i] == '{') depth++;
                                        else if (abbr[i] == '}') depth--;
                                        if (depth > 0) i++;
                                    }
                                    if (i >= n) return false;
                                    a.value = abbr.substr(s, i - s);
                                    a.isExpr = true;
                                    i++;
                                } else {
                                    size_t s = i;
                                    while (i < n && abbr[i] != ' ' && abbr[i] != '\t' && abbr[i] != ']') i++;
                                    a.value = abbr.substr(s, i - s);
                                    if (a.value.empty()) a.hasTabstop = true;
                                }
                            } else if (!a.removeDefault && !a.isBoolean) {
                                if (!isQuotedName && a.name.size() == 1) return false;
                                if (isBooleanAttr(toLowerStr(a.name))) {
                                    a.isBoolean = true;
                                } else {
                                    a.hasTabstop = true;
                                }
                            }
                            elem.attrs.push_back(std::move(a));
                            parsedAnyAttr = true;
                        }
                        if (!parsedAnyAttr || i >= n || abbr[i] != ']') return false;
                        i++;
                    } else if (abbr[i] == '{') {
                        i++;
                        std::string txt;
                        int depth = 1;
                        while (i < n && depth > 0) {
                            if (abbr[i] == '\\' && i + 1 < n) {
                                txt += abbr[i];
                                txt += abbr[i + 1];
                                i += 2;
                                continue;
                            }
                            if (abbr[i] == '{') depth++;
                            else if (abbr[i] == '}') {
                                depth--;
                                if (depth == 0) break;
                            }
                            txt += abbr[i++];
                        }
                        if (i >= n || abbr[i] != '}') return false;
                        i++;
                        elem.text += txt;
                        if (elem.tag.empty() && elem.id.empty() && elem.classes.empty() && elem.attrs.empty()) {
                            if (elem.text.empty()) return false;
                            elem.isTextOnly = true;
                        }
                    } else if (abbr[i] == '*' && i > startPos) {
                        i++;
                        if (i >= n || !isAsciiDigit(abbr[i])) return false;
                        int rep = 0;
                        while (i < n && isAsciiDigit(abbr[i])) {
                            rep = rep * 10 + (abbr[i] - '0');
                            if (rep > 1000) rep = 1000;
                            i++;
                        }
                        if (rep <= 0) return false;
                        elem.repeat = rep;
                        elem.hasExplicitRepeat = true;
                    } else if (abbr[i] == '/' && i > startPos) {
                        i++;
                        elem.selfClose = true;
                    } else {
                        break;
                    }
                }

                if (i < n && abbr[i] == '+' && !elem.tag.empty() && elem.tag.back() != '+' && isCompoundPlusTag(toLowerStr(elem.tag))) {
                    if (i + 1 == n || abbr[i + 1] == ')' || abbr[i + 1] == '^' || abbr[i + 1] == '>') {
                        elem.tag += '+';
                        i++;
                    }
                }

                if (i == startPos) return false;

                if (!elem.tag.empty() || !elem.id.empty() || !elem.classes.empty() || !elem.attrs.empty()) {
                    elem.isTextOnly = false;
                }

                std::string lowerTag = toLowerStr(elem.tag);
                if (lowerTag.compare(0, 5, "lorem") == 0 || lowerTag.compare(0, 6, "lipsum") == 0) {
                    size_t numStart = (lowerTag[1] == 'o') ? 5 : 6;
                    bool allDigits = true;
                    int words = 0;
                    for (size_t k = numStart; k < lowerTag.size(); ++k) {
                        if (!isAsciiDigit(lowerTag[k])) { allDigits = false; break; }
                        words = words * 10 + (lowerTag[k] - '0');
                    }
                    if (allDigits) {
                        elem.isLorem = true;
                        elem.loremWords = (numStart < lowerTag.size()) ? words : 30;
                        elem.tag.clear();
                    }
                }

                nodeIdx = (int)pool.size();
                pool.push_back(std::move(elem));
                pool[curParent].children.push_back(nodeIdx);
                if (pool[nodeIdx].tag.empty() && !pool[nodeIdx].isGroup && !pool[nodeIdx].isTextOnly && !pool[nodeIdx].isComment && pool[nodeIdx].specialTemplate.empty()) {
                    if (!pool[nodeIdx].isLorem || !pool[nodeIdx].id.empty() || !pool[nodeIdx].classes.empty() || !pool[nodeIdx].attrs.empty() ||
                        (pool[nodeIdx].hasExplicitRepeat && !findNearestElementTag(pool, nodeIdx).empty())) {
                        pool[nodeIdx].tag = resolveImplicitTag(findNearestElementTag(pool, nodeIdx));
                    }
                }
                applySnippetDefaults(pool, nodeIdx);
            }

            prevNode = nodeIdx;

            if (i >= n) break;
            if (insideGroup && abbr[i] == ')') break;

            if (abbr[i] == '>') {
                i++;
                if (i >= n || (insideGroup && abbr[i] == ')')) return false;
                curParent = prevNode;
            } else if (abbr[i] == '+') {
                i++;
                if (i >= n || (insideGroup && abbr[i] == ')')) return false;
            } else if (abbr[i] == '^') {
                while (i < n && abbr[i] == '^') {
                    if (curParent != parentIdx && pool[curParent].parent != -1) {
                        curParent = pool[curParent].parent;
                    }
                    i++;
                }
                if (i < n && abbr[i] == '+') i++;
                if (i >= n || (insideGroup && abbr[i] == ')')) return false;
            } else {
                return false;
            }
        }
        return true;
    };

    if (!parseSequence(parseSequence, 0, false)) return false;
    if (i != n) return false;
    return !pool[0].children.empty();
}

static std::string getEffectiveTag(const std::vector<HtmlNode>& pool, int nodeIdx)  noexcept{
    const HtmlNode& node = pool[nodeIdx];
    if (node.isGroup || node.isTextOnly || node.isComment || !node.specialTemplate.empty()) return node.tag;
    if (node.isLorem && node.tag.empty()) return "";
    if (!node.tag.empty()) return node.tag;
    return resolveImplicitTag(findNearestElementTag(pool, nodeIdx));
}

static bool nodeHasBlockDescendant(const std::vector<HtmlNode>& pool, int nodeIdx)  noexcept{
    for (int chIdx : pool[nodeIdx].children) {
        const HtmlNode& ch = pool[chIdx];
        if (ch.isGroup) {
            if (nodeHasBlockDescendant(pool, chIdx)) return true;
        } else if (!ch.isTextOnly) {
            std::string eff = toLowerStr(getEffectiveTag(pool, chIdx));
            if (!eff.empty() && !isInlineTag(eff)) return true;
            if (nodeHasBlockDescendant(pool, chIdx)) return true;
        }
    }
    return false;
}

static bool nodeShouldFormatChildrenMultiline(const std::vector<HtmlNode>& pool, int nodeIdx, const std::string& lowerTag)  noexcept{
    const HtmlNode& node = pool[nodeIdx];
    if (node.children.empty()) return false;
    bool hasElementChild = false;
    bool hasDirectTextChild = false;
    for (int chIdx : node.children) {
        const HtmlNode& ch = pool[chIdx];
        if (ch.isGroup) {
            hasElementChild = true;
        } else if (ch.isTextOnly) {
            hasDirectTextChild = true;
        } else {
            std::string eff = getEffectiveTag(pool, chIdx);
            if (!eff.empty() || ch.isComment || !ch.specialTemplate.empty()) {
                hasElementChild = true;
            }
        }
    }
    if (!hasElementChild) return false;
    if (nodeHasBlockDescendant(pool, nodeIdx)) return true;
    if (hasDirectTextChild) return false;
    if (isStructuralContainerTag(lowerTag)) return true;
    return false;
}

static void extractHtmlFilters(std::string& abbr, bool& outBem, bool& outComment, bool& outEscape)  noexcept{
    outBem = false;
    outComment = false;
    outEscape = false;
    while (abbr.size() >= 2) {
        size_t pipePos = abbr.rfind('|');
        if (pipePos == std::string::npos || pipePos == 0 || pipePos + 1 >= abbr.size()) break;
        if (abbr[pipePos - 1] == '|') break;
        std::string flt = toLowerStr(abbr.substr(pipePos + 1));
        if (flt == "bem") {
            outBem = true;
            abbr.resize(pipePos);
        } else if (flt == "c") {
            outComment = true;
            abbr.resize(pipePos);
        } else if (flt == "e") {
            outEscape = true;
            abbr.resize(pipePos);
        } else {
            break;
        }
    }
}

static void applyBemFilter(std::vector<HtmlNode>& pool)  noexcept{
    std::vector<std::string> nodeBlock(pool.size());
    auto findAncestorBlock = [&](int idx, int depthUp) -> std::string {
        int p = pool[idx].parent;
        int foundCount = 0;
        std::string lastFound;
        while (p != -1) {
            if (!nodeBlock[p].empty()) {
                lastFound = nodeBlock[p];
                foundCount++;
                if (foundCount >= depthUp) return lastFound;
            }
            p = pool[p].parent;
        }
        return lastFound;
    };

    auto addUniqueClass = [](std::vector<std::string>& list, const std::string& c) {
        if (c.empty()) return;
        for (const auto& existing : list) {
            if (existing == c) return;
        }
        list.push_back(c);
    };

    for (size_t i = 1; i < pool.size(); ++i) {
        HtmlNode& node = pool[i];
        if (node.isGroup || node.classes.empty()) continue;

        std::vector<std::string> newClasses;
        std::string currentBase;

        for (const std::string& rawCls : node.classes) {
            if (rawCls.empty()) continue;

            if (rawCls.compare(0, 2, "__") == 0 && rawCls.size() > 2) {
                std::string rest = rawCls.substr(2);
                std::string blk = !nodeBlock[i].empty() ? nodeBlock[i] : findAncestorBlock((int)i, 1);
                size_t modPos = rest.find('_');
                if (modPos != std::string::npos && modPos > 0 && modPos + 1 < rest.size()) {
                    std::string elemName = rest.substr(0, modPos);
                    std::string modName = rest.substr(modPos + 1);
                    std::string base = blk.empty() ? elemName : (blk + "__" + elemName);
                    addUniqueClass(newClasses, base);
                    addUniqueClass(newClasses, base + "_" + modName);
                    currentBase = base;
                } else {
                    std::string base = blk.empty() ? rest : (blk + "__" + rest);
                    addUniqueClass(newClasses, base);
                    currentBase = base;
                }
                if (nodeBlock[i].empty()) nodeBlock[i] = blk;
            } else if (rawCls[0] == '_' && rawCls.size() > 1) {
                std::string modPart = rawCls.substr(1);
                if (!modPart.empty() && modPart[0] == '_') modPart.erase(0, 1);
                std::string base = !currentBase.empty() ? currentBase : findAncestorBlock((int)i, 1);
                if (!base.empty()) {
                    addUniqueClass(newClasses, base);
                    addUniqueClass(newClasses, base + "_" + modPart);
                    currentBase = base;
                } else {
                    addUniqueClass(newClasses, rawCls);
                }
            } else if (rawCls[0] == '-' && rawCls.size() > 1) {
                size_t dCount = 0;
                while (dCount < rawCls.size() && rawCls[dCount] == '-') dCount++;
                std::string rest = rawCls.substr(dCount);
                if (rest.empty()) continue;
                if (!currentBase.empty() && dCount == 1) {
                    addUniqueClass(newClasses, currentBase);
                    addUniqueClass(newClasses, currentBase + "_" + rest);
                } else {
                    std::string blk = findAncestorBlock((int)i, (int)dCount);
                    size_t modPos = rest.find('_');
                    if (modPos != std::string::npos && modPos > 0 && modPos + 1 < rest.size()) {
                        std::string elemName = rest.substr(0, modPos);
                        std::string modName = rest.substr(modPos + 1);
                        std::string base = blk.empty() ? elemName : (blk + "__" + elemName);
                        addUniqueClass(newClasses, base);
                        addUniqueClass(newClasses, base + "_" + modName);
                        currentBase = base;
                    } else {
                        std::string base = blk.empty() ? rest : (blk + "__" + rest);
                        addUniqueClass(newClasses, base);
                        currentBase = base;
                    }
                    if (nodeBlock[i].empty()) nodeBlock[i] = blk;
                }
            } else {
                size_t dblPos = rawCls.find("__");
                if (dblPos != std::string::npos && dblPos > 0 && dblPos + 2 < rawCls.size()) {
                    std::string blk = rawCls.substr(0, dblPos);
                    std::string rest = rawCls.substr(dblPos + 2);
                    if (nodeBlock[i].empty()) nodeBlock[i] = blk;
                    size_t modPos = rest.find('_');
                    if (modPos != std::string::npos && modPos > 0 && modPos + 1 < rest.size()) {
                        std::string elemName = rest.substr(0, modPos);
                        std::string base = blk + "__" + elemName;
                        addUniqueClass(newClasses, base);
                        addUniqueClass(newClasses, rawCls);
                        currentBase = base;
                    } else {
                        addUniqueClass(newClasses, rawCls);
                        currentBase = rawCls;
                    }
                } else {
                    size_t modPos = rawCls.find('_');
                    if (modPos != std::string::npos && modPos > 0 && modPos + 1 < rawCls.size()) {
                        std::string blk = rawCls.substr(0, modPos);
                        if (nodeBlock[i].empty()) nodeBlock[i] = blk;
                        addUniqueClass(newClasses, blk);
                        addUniqueClass(newClasses, rawCls);
                        currentBase = blk;
                    } else {
                        if (nodeBlock[i].empty()) nodeBlock[i] = rawCls;
                        addUniqueClass(newClasses, rawCls);
                        currentBase = rawCls;
                    }
                }
            }
        }
        node.classes = std::move(newClasses);
    }
}

static void renderHtmlTree(
    const std::vector<HtmlNode>& pool,
    Mode mode,
    const std::string& baseIndent,
    const std::string& newlineStr,
    std::string& out,
    std::vector<Tabstop>& tabstops,
    bool filterComment = false,
    bool filterEscape = false
)  noexcept{
    auto addStop = [&](const std::string& placeholder = "") {
        size_t s = out.size();
        out += placeholder;
        size_t e = out.size();
        tabstops.push_back({ s, e });
    };

    auto appendNewlineIndent = [&](int depth) {
        out += newlineStr;
        out += baseIndent;
        for (int d = 0; d < depth; ++d) out += '\t';
    };

    auto renderNodeList = [&](auto& self, const std::vector<int>& childIndices, int depth, int inheritedIdx, int inheritedTotal, bool multilineSiblings) -> void {
        bool firstRendered = true;
        bool prevWasInline = false;
        bool prevWasTextOnly = false;

        for (size_t ci = 0; ci < childIndices.size(); ++ci) {
            int nodeIdx = childIndices[ci];
            const HtmlNode& node = pool[nodeIdx];
            int repCount = node.repeat;

            for (int r = 0; r < repCount; ++r) {
                int curIdx = node.hasExplicitRepeat ? r : inheritedIdx;
                int curTotal = node.hasExplicitRepeat ? repCount : inheritedTotal;

                if (node.isGroup) {
                    if (!firstRendered && multilineSiblings) {
                        appendNewlineIndent(depth);
                    }
                    self(self, node.children, depth, curIdx, curTotal, multilineSiblings);
                    firstRendered = false;
                    prevWasInline = false;
                    prevWasTextOnly = false;
                    continue;
                }

                std::string effTag = resolveNumbering(getEffectiveTag(pool, nodeIdx), curIdx, curTotal);
                std::string lowerTag = toLowerStr(effTag);
                bool thisIsInline = node.isTextOnly || (effTag.empty() && node.isLorem && !node.hasExplicitRepeat) || (!lowerTag.empty() && isInlineTag(lowerTag) && !node.hasExplicitRepeat);

                if (!firstRendered && multilineSiblings) {
                    bool bothInlineWithText = prevWasInline && thisIsInline && (node.isTextOnly || prevWasTextOnly);
                    if (!bothInlineWithText) {
                        appendNewlineIndent(depth);
                    }
                }
                firstRendered = false;
                prevWasInline = thisIsInline;
                prevWasTextOnly = node.isTextOnly;

                if (!node.specialTemplate.empty()) {
                    const std::string& st = node.specialTemplate;
                    if (st == "!!!") {
                        out += "<!DOCTYPE html>";
                    } else if (st == "!!!4t" || st == "html:4t") {
                        out += "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">";
                    } else if (st == "!!!4s" || st == "html:4s") {
                        out += "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">";
                    } else if (st == "!!!xt" || st == "html:xt") {
                        out += "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">";
                    } else if (st == "!!!xs" || st == "html:xs") {
                        out += "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">";
                    } else if (st == "!!!xxs" || st == "html:xxs") {
                        out += "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">";
                    } else if (st == "html:5" || st == "doc" || st == "doc4") {
                        if (st == "html:5") {
                            out += "<!DOCTYPE html>";
                            appendNewlineIndent(depth);
                        }
                        out += (st == "doc4") ? "<html>" : "<html lang=\"en\">";
                        appendNewlineIndent(depth);
                        out += "<head>";
                        appendNewlineIndent(depth + 1);
                        if (st == "doc4") {
                            out += "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=UTF-8\">";
                        } else {
                            out += "<meta charset=\"UTF-8\">";
                            appendNewlineIndent(depth + 1);
                            out += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
                        }
                        appendNewlineIndent(depth + 1);
                        out += "<title>Document</title>";
                        appendNewlineIndent(depth);
                        out += "</head>";
                        appendNewlineIndent(depth);
                        out += "<body>";
                        appendNewlineIndent(depth + 1);
                        if (!node.children.empty()) {
                            self(self, node.children, depth + 1, curIdx, curTotal, true);
                        } else {
                            addStop();
                        }
                        appendNewlineIndent(depth);
                        out += "</body>";
                        appendNewlineIndent(depth);
                        out += "</html>";
                    } else if (st == "cc:ie" || st == "cc:ie6") {
                        out += "<!--[if IE]>";
                        addStop();
                        out += "<![endif]-->";
                    } else if (st == "cc:noie") {
                        out += "<!--[if !IE]><!-->";
                        addStop();
                        out += "<!--<![endif]-->";
                    }
                    continue;
                }

                if (node.isComment) {
                    out += "<!-- ";
                    if (!node.text.empty()) {
                        out += resolveNumbering(node.text, curIdx, curTotal);
                    } else if (!node.children.empty()) {
                        self(self, node.children, depth, curIdx, curTotal, false);
                    } else {
                        addStop();
                    }
                    out += " -->";
                    continue;
                }

                if (node.isTextOnly) {
                    out += resolveNumbering(node.text, curIdx, curTotal);
                    continue;
                }
                if (node.isLorem && effTag.empty()) {
                    out += generateLorem(node.loremWords, curIdx);
                    continue;
                }

                out += '<';
                out += effTag;

                if (!node.id.empty()) {
                    out += " id=\"";
                    out += resolveNumbering(node.id, curIdx, curTotal);
                    out += '"';
                }

                if (!node.classes.empty()) {
                    out += (mode == Mode::Jsx) ? " className=\"" : " class=\"";
                    for (size_t k = 0; k < node.classes.size(); ++k) {
                        if (k > 0) out += ' ';
                        out += resolveNumbering(node.classes[k], curIdx, curTotal);
                    }
                    out += '"';
                }

                for (const auto& a : node.attrs) {
                    std::string attrName = resolveNumbering(a.name, curIdx, curTotal);
                    if (mode == Mode::Jsx) {
                        if (attrName == "class") attrName = "className";
                        else if (attrName == "for") attrName = "htmlFor";
                    }
                    out += ' ';
                    out += attrName;
                    if (a.isBoolean) {
                        if (a.value == "disabled" && attrName == "disabled") {
                            out += "=\"disabled\"";
                        }
                    } else if (a.isExpr) {
                        out += "={";
                        out += resolveNumbering(a.value, curIdx, curTotal);
                        out += '}';
                    } else {
                        out += "=\"";
                        out += resolveNumbering(a.value, curIdx, curTotal);
                        if (a.hasTabstop) addStop();
                        out += '"';
                    }
                }

                bool isVoid = isVoidTag(lowerTag);
                bool selfClosing = node.selfClose || (isVoid && (mode == Mode::Jsx || mode == Mode::Xml));

                if (selfClosing) {
                    out += (mode == Mode::Jsx || node.selfClose) ? " />" : "/>";
                    continue;
                }
                if (isVoid) {
                    out += '>';
                    continue;
                }

                out += '>';

                if (!node.text.empty()) {
                    out += resolveNumbering(node.text, curIdx, curTotal);
                }
                if (node.isLorem) {
                    out += generateLorem(node.loremWords, curIdx);
                }

                if (!node.children.empty()) {
                    bool multi = nodeShouldFormatChildrenMultiline(pool, nodeIdx, lowerTag);
                    if (multi) {
                        appendNewlineIndent(depth + 1);
                        self(self, node.children, depth + 1, curIdx, curTotal, true);
                        appendNewlineIndent(depth);
                    } else {
                        self(self, node.children, depth, curIdx, curTotal, false);
                    }
                } else if (node.text.empty() && !node.isLorem) {
                    bool skipBodyStop = (lowerTag == "script" && !node.attrs.empty()) || (lowerTag == "iframe");
                    if (!skipBodyStop) {
                        addStop();
                    }
                }

                out += "</";
                out += effTag;
                out += '>';
                if (filterComment && (!node.id.empty() || !node.classes.empty())) {
                    out += "<!-- /";
                    if (!node.id.empty()) {
                        out += '#';
                        out += resolveNumbering(node.id, curIdx, curTotal);
                    }
                    for (const auto& cls : node.classes) {
                        out += '.';
                        out += resolveNumbering(cls, curIdx, curTotal);
                    }
                    out += " -->";
                }
            }
        }
    };

    bool rootMultiline = false;
    int totalTop = 0;
    for (int chIdx : pool[0].children) {
        const HtmlNode& ch = pool[chIdx];
        totalTop += ch.repeat;
        std::string eff = toLowerStr(getEffectiveTag(pool, chIdx));
        if (ch.isGroup || ch.hasExplicitRepeat || (!ch.isTextOnly && !eff.empty() && !isInlineTag(eff))) {
            rootMultiline = true;
        }
    }
    if (totalTop <= 1 && !pool[0].children.empty() && !pool[pool[0].children[0]].isGroup) {
        rootMultiline = false;
    }

    renderNodeList(renderNodeList, pool[0].children, 0, 0, 1, rootMultiline);

    if (tabstops.empty() || tabstops.back().endOffset != out.size()) {
        tabstops.push_back({ out.size(), out.size() });
    }

    if (filterEscape && !out.empty()) {
        std::vector<size_t> offsetMap(out.size() + 1, 0);
        std::string esc;
        esc.reserve(out.size() + 32);
        for (size_t i = 0; i < out.size(); ++i) {
            offsetMap[i] = esc.size();
            char ch = out[i];
            if (ch == '<') esc += "&lt;";
            else if (ch == '>') esc += "&gt;";
            else if (ch == '&') esc += "&amp;";
            else esc += ch;
        }
        offsetMap[out.size()] = esc.size();
        for (auto& ts : tabstops) {
            if (ts.startOffset <= out.size()) ts.startOffset = offsetMap[ts.startOffset];
            if (ts.endOffset <= out.size()) ts.endOffset = offsetMap[ts.endOffset];
        }
        out = std::move(esc);
    }
}

// ============================================================================
// CSS Emmet Parser & Expander
// ============================================================================

static constexpr const char kCssExactBlob[] =
    "!\0!important\0"
    "d\0display: |;\0"
    "dn\0display: none;\0"
    "d:n\0display: none;\0"
    "db\0display: block;\0"
    "d:b\0display: block;\0"
    "di\0display: inline;\0"
    "d:i\0display: inline;\0"
    "dib\0display: inline-block;\0"
    "d:ib\0display: inline-block;\0"
    "df\0display: flex;\0"
    "d:f\0display: flex;\0"
    "dif\0display: inline-flex;\0"
    "d:if\0display: inline-flex;\0"
    "dg\0display: grid;\0"
    "d:g\0display: grid;\0"
    "dig\0display: inline-grid;\0"
    "d:ig\0display: inline-grid;\0"
    "dt\0display: table;\0"
    "d:t\0display: table;\0"
    "dtc\0display: table-cell;\0"
    "d:tc\0display: table-cell;\0"
    "dtr\0display: table-row;\0"
    "d:tr\0display: table-row;\0"
    "dl\0display: list-item;\0"
    "d:li\0display: list-item;\0"
    "dcp\0display: contents;\0"
    "v\0visibility: hidden;\0"
    "vh\0visibility: hidden;\0"
    "v:h\0visibility: hidden;\0"
    "vv\0visibility: visible;\0"
    "v:v\0visibility: visible;\0"
    "vc\0visibility: collapse;\0"
    "v:c\0visibility: collapse;\0"
    "pos\0position: relative;\0"
    "posr\0position: relative;\0"
    "pos:r\0position: relative;\0"
    "posa\0position: absolute;\0"
    "pos:a\0position: absolute;\0"
    "posf\0position: fixed;\0"
    "pos:f\0position: fixed;\0"
    "poss\0position: static;\0"
    "pos:s\0position: static;\0"
    "post\0position: sticky;\0"
    "pos:st\0position: sticky;\0"
    "t\0top: |;\0"
    "t:a\0top: auto;\0"
    "r\0right: |;\0"
    "r:a\0right: auto;\0"
    "b\0bottom: |;\0"
    "b:a\0bottom: auto;\0"
    "l\0left: |;\0"
    "l:a\0left: auto;\0"
    "ins\0inset: |;\0"
    "z\0z-index: |;\0"
    "z:a\0z-index: auto;\0"
    "fl\0float: left;\0"
    "fll\0float: left;\0"
    "fl:l\0float: left;\0"
    "flr\0float: right;\0"
    "fl:r\0float: right;\0"
    "fln\0float: none;\0"
    "fl:n\0float: none;\0"
    "cl\0clear: both;\0"
    "clb\0clear: both;\0"
    "cl:b\0clear: both;\0"
    "cll\0clear: left;\0"
    "cl:l\0clear: left;\0"
    "clr\0clear: right;\0"
    "cl:r\0clear: right;\0"
    "cln\0clear: none;\0"
    "cl:n\0clear: none;\0"
    "ov\0overflow: hidden;\0"
    "ovh\0overflow: hidden;\0"
    "ov:h\0overflow: hidden;\0"
    "ova\0overflow: auto;\0"
    "ov:a\0overflow: auto;\0"
    "ovs\0overflow: scroll;\0"
    "ov:s\0overflow: scroll;\0"
    "ovv\0overflow: visible;\0"
    "ov:v\0overflow: visible;\0"
    "ovx\0overflow-x: |;\0"
    "ovxh\0overflow-x: hidden;\0"
    "ovx:h\0overflow-x: hidden;\0"
    "ovxa\0overflow-x: auto;\0"
    "ovx:a\0overflow-x: auto;\0"
    "ovxs\0overflow-x: scroll;\0"
    "ovx:s\0overflow-x: scroll;\0"
    "ovy\0overflow-y: |;\0"
    "ovyh\0overflow-y: hidden;\0"
    "ovy:h\0overflow-y: hidden;\0"
    "ovya\0overflow-y: auto;\0"
    "ovy:a\0overflow-y: auto;\0"
    "ovys\0overflow-y: scroll;\0"
    "ovy:s\0overflow-y: scroll;\0"
    "bxz\0box-sizing: border-box;\0"
    "bxzbb\0box-sizing: border-box;\0"
    "bxz:bb\0box-sizing: border-box;\0"
    "bxzcb\0box-sizing: content-box;\0"
    "bxz:cb\0box-sizing: content-box;\0"
    "bxsh\0box-shadow: |;\0"
    "bxshn\0box-shadow: none;\0"
    "bxsh:n\0box-shadow: none;\0"
    "w\0width: |;\0"
    "w:a\0width: auto;\0"
    "h\0height: |;\0"
    "h:a\0height: auto;\0"
    "maw\0max-width: |;\0"
    "maw:n\0max-width: none;\0"
    "mah\0max-height: |;\0"
    "mah:n\0max-height: none;\0"
    "miw\0min-width: |;\0"
    "mih\0min-height: |;\0"
    "m\0margin: |;\0"
    "m:a\0margin: auto;\0"
    "mt\0margin-top: |;\0"
    "mt:a\0margin-top: auto;\0"
    "mr\0margin-right: |;\0"
    "mr:a\0margin-right: auto;\0"
    "mb\0margin-bottom: |;\0"
    "mb:a\0margin-bottom: auto;\0"
    "ml\0margin-left: |;\0"
    "ml:a\0margin-left: auto;\0"
    "mx\0margin-inline: |;\0"
    "my\0margin-block: |;\0"
    "p\0padding: |;\0"
    "pt\0padding-top: |;\0"
    "pr\0padding-right: |;\0"
    "pb\0padding-bottom: |;\0"
    "pl\0padding-left: |;\0"
    "px\0padding-inline: |;\0"
    "py\0padding-block: |;\0"
    "fx\0flex: |;\0"
    "fxg\0flex-grow: |;\0"
    "fxs\0flex-shrink: |;\0"
    "fxb\0flex-basis: |;\0"
    "ord\0order: |;\0"
    "g\0gap: |;\0"
    "gap\0gap: |;\0"
    "rg\0row-gap: |;\0"
    "cg\0column-gap: |;\0"
    "fxd\0flex-direction: |;\0"
    "fxdr\0flex-direction: row;\0"
    "fxd:r\0flex-direction: row;\0"
    "fxdrr\0flex-direction: row-reverse;\0"
    "fxd:rr\0flex-direction: row-reverse;\0"
    "fxdc\0flex-direction: column;\0"
    "fxd:c\0flex-direction: column;\0"
    "fxdcr\0flex-direction: column-reverse;\0"
    "fxd:cr\0flex-direction: column-reverse;\0"
    "fxw\0flex-wrap: nowrap;\0"
    "fxwn\0flex-wrap: nowrap;\0"
    "fxw:n\0flex-wrap: nowrap;\0"
    "fxww\0flex-wrap: wrap;\0"
    "fxw:w\0flex-wrap: wrap;\0"
    "fxwwr\0flex-wrap: wrap-reverse;\0"
    "fxw:wr\0flex-wrap: wrap-reverse;\0"
    "jc\0justify-content: |;\0"
    "jcc\0justify-content: center;\0"
    "jc:c\0justify-content: center;\0"
    "jcfs\0justify-content: flex-start;\0"
    "jc:fs\0justify-content: flex-start;\0"
    "jcs\0justify-content: flex-start;\0"
    "jcfe\0justify-content: flex-end;\0"
    "jc:fe\0justify-content: flex-end;\0"
    "jce\0justify-content: flex-end;\0"
    "jcsb\0justify-content: space-between;\0"
    "jc:sb\0justify-content: space-between;\0"
    "jcsa\0justify-content: space-around;\0"
    "jc:sa\0justify-content: space-around;\0"
    "jcse\0justify-content: space-evenly;\0"
    "jc:se\0justify-content: space-evenly;\0"
    "ai\0align-items: |;\0"
    "aic\0align-items: center;\0"
    "ai:c\0align-items: center;\0"
    "aifs\0align-items: flex-start;\0"
    "ai:fs\0align-items: flex-start;\0"
    "ais\0align-items: stretch;\0"
    "ai:s\0align-items: stretch;\0"
    "aife\0align-items: flex-end;\0"
    "ai:fe\0align-items: flex-end;\0"
    "aie\0align-items: flex-end;\0"
    "aib\0align-items: baseline;\0"
    "ai:b\0align-items: baseline;\0"
    "ac\0align-content: |;\0"
    "acc\0align-content: center;\0"
    "ac:c\0align-content: center;\0"
    "acfs\0align-content: flex-start;\0"
    "ac:fs\0align-content: flex-start;\0"
    "acfe\0align-content: flex-end;\0"
    "ac:fe\0align-content: flex-end;\0"
    "acsb\0align-content: space-between;\0"
    "ac:sb\0align-content: space-between;\0"
    "acsa\0align-content: space-around;\0"
    "ac:sa\0align-content: space-around;\0"
    "acs\0align-content: stretch;\0"
    "ac:s\0align-content: stretch;\0"
    "as\0align-self: |;\0"
    "asa\0align-self: auto;\0"
    "as:a\0align-self: auto;\0"
    "asc\0align-self: center;\0"
    "as:c\0align-self: center;\0"
    "asfs\0align-self: flex-start;\0"
    "as:fs\0align-self: flex-start;\0"
    "asfe\0align-self: flex-end;\0"
    "as:fe\0align-self: flex-end;\0"
    "asb\0align-self: baseline;\0"
    "as:b\0align-self: baseline;\0"
    "ass\0align-self: stretch;\0"
    "as:s\0align-self: stretch;\0"
    "pi\0place-items: |;\0"
    "pic\0place-items: center;\0"
    "pc\0place-content: |;\0"
    "pcc\0place-content: center;\0"
    "gtc\0grid-template-columns: |;\0"
    "gtr\0grid-template-rows: |;\0"
    "gta\0grid-template-areas: |;\0"
    "gc\0grid-column: |;\0"
    "gr\0grid-row: |;\0"
    "f\0font: |;\0"
    "fz\0font-size: |;\0"
    "lh\0line-height: |;\0"
    "ls\0letter-spacing: |;\0"
    "lts\0letter-spacing: |;\0"
    "ti\0text-indent: |;\0"
    "op\0opacity: |;\0"
    "fw\0font-weight: |;\0"
    "fwn\0font-weight: normal;\0"
    "fw:n\0font-weight: normal;\0"
    "fwb\0font-weight: bold;\0"
    "fw:b\0font-weight: bold;\0"
    "fwbr\0font-weight: bolder;\0"
    "fw:br\0font-weight: bolder;\0"
    "fwlr\0font-weight: lighter;\0"
    "fw:lr\0font-weight: lighter;\0"
    "fs\0font-style: italic;\0"
    "fsn\0font-style: normal;\0"
    "fs:n\0font-style: normal;\0"
    "fsi\0font-style: italic;\0"
    "fs:i\0font-style: italic;\0"
    "fso\0font-style: oblique;\0"
    "fs:o\0font-style: oblique;\0"
    "ff\0font-family: |;\0"
    "ffs\0font-family: serif;\0"
    "ff:s\0font-family: serif;\0"
    "ffss\0font-family: sans-serif;\0"
    "ff:ss\0font-family: sans-serif;\0"
    "ffm\0font-family: monospace;\0"
    "ff:m\0font-family: monospace;\0"
    "ffc\0font-family: cursive;\0"
    "ff:c\0font-family: cursive;\0"
    "fff\0font-family: fantasy;\0"
    "ff:f\0font-family: fantasy;\0"
    "ta\0text-align: left;\0"
    "tal\0text-align: left;\0"
    "ta:l\0text-align: left;\0"
    "tac\0text-align: center;\0"
    "ta:c\0text-align: center;\0"
    "tar\0text-align: right;\0"
    "ta:r\0text-align: right;\0"
    "taj\0text-align: justify;\0"
    "ta:j\0text-align: justify;\0"
    "td\0text-decoration: none;\0"
    "tdn\0text-decoration: none;\0"
    "td:n\0text-decoration: none;\0"
    "tdu\0text-decoration: underline;\0"
    "td:u\0text-decoration: underline;\0"
    "tdo\0text-decoration: overline;\0"
    "td:o\0text-decoration: overline;\0"
    "tdl\0text-decoration: line-through;\0"
    "td:l\0text-decoration: line-through;\0"
    "tt\0text-transform: uppercase;\0"
    "ttn\0text-transform: none;\0"
    "tt:n\0text-transform: none;\0"
    "ttc\0text-transform: capitalize;\0"
    "tt:c\0text-transform: capitalize;\0"
    "ttu\0text-transform: uppercase;\0"
    "tt:u\0text-transform: uppercase;\0"
    "ttl\0text-transform: lowercase;\0"
    "tt:l\0text-transform: lowercase;\0"
    "ws\0white-space: |;\0"
    "wsn\0white-space: nowrap;\0"
    "ws:n\0white-space: nowrap;\0"
    "ws:nw\0white-space: nowrap;\0"
    "wsp\0white-space: pre;\0"
    "ws:p\0white-space: pre;\0"
    "wspw\0white-space: pre-wrap;\0"
    "ws:pw\0white-space: pre-wrap;\0"
    "wspl\0white-space: pre-line;\0"
    "ws:pl\0white-space: pre-line;\0"
    "wsno\0white-space: normal;\0"
    "tov\0text-overflow: ellipsis;\0"
    "tove\0text-overflow: ellipsis;\0"
    "tov:e\0text-overflow: ellipsis;\0"
    "tovc\0text-overflow: clip;\0"
    "tov:c\0text-overflow: clip;\0"
    "tsh\0text-shadow: |;\0"
    "tshn\0text-shadow: none;\0"
    "tsh:n\0text-shadow: none;\0"
    "va\0vertical-align: top;\0"
    "vat\0vertical-align: top;\0"
    "va:t\0vertical-align: top;\0"
    "vam\0vertical-align: middle;\0"
    "va:m\0vertical-align: middle;\0"
    "vab\0vertical-align: bottom;\0"
    "va:b\0vertical-align: bottom;\0"
    "vabl\0vertical-align: baseline;\0"
    "va:bl\0vertical-align: baseline;\0"
    "wob\0word-break: |;\0"
    "wobn\0word-break: normal;\0"
    "wob:n\0word-break: normal;\0"
    "wobba\0word-break: break-all;\0"
    "wob:ba\0word-break: break-all;\0"
    "wobka\0word-break: keep-all;\0"
    "wob:ka\0word-break: keep-all;\0"
    "wow\0word-wrap: |;\0"
    "wowbw\0word-wrap: break-word;\0"
    "wow:bw\0word-wrap: break-word;\0"
    "c\0color: #000;\0"
    "c:r\0color: rgb(0, 0, 0);\0"
    "c:ra\0color: rgba(0, 0, 0, .5);\0"
    "bg\0background: #000;\0"
    "bgn\0background: none;\0"
    "bg:n\0background: none;\0"
    "bgc\0background-color: #fff;\0"
    "bgct\0background-color: transparent;\0"
    "bgc:t\0background-color: transparent;\0"
    "bgi\0background-image: url(|);\0"
    "bgin\0background-image: none;\0"
    "bgi:n\0background-image: none;\0"
    "bgr\0background-repeat: no-repeat;\0"
    "bgrn\0background-repeat: no-repeat;\0"
    "bgr:n\0background-repeat: no-repeat;\0"
    "bgrx\0background-repeat: repeat-x;\0"
    "bgr:x\0background-repeat: repeat-x;\0"
    "bgry\0background-repeat: repeat-y;\0"
    "bgr:y\0background-repeat: repeat-y;\0"
    "bgp\0background-position: 0 0;\0"
    "bgz\0background-size: cover;\0"
    "bgzc\0background-size: cover;\0"
    "bgz:c\0background-size: cover;\0"
    "bgzct\0background-size: contain;\0"
    "bgz:ct\0background-size: contain;\0"
    "bga\0background-attachment: fixed;\0"
    "bga:f\0background-attachment: fixed;\0"
    "bga:s\0background-attachment: scroll;\0"
    "bd\0border: 1px solid #000;\0"
    "bdn\0border: none;\0"
    "bd:n\0border: none;\0"
    "bdt\0border-top: 1px solid #000;\0"
    "bdtn\0border-top: none;\0"
    "bdt:n\0border-top: none;\0"
    "bdr\0border-right: 1px solid #000;\0"
    "bdrn\0border-right: none;\0"
    "bdr:n\0border-right: none;\0"
    "bdb\0border-bottom: 1px solid #000;\0"
    "bdbn\0border-bottom: none;\0"
    "bdb:n\0border-bottom: none;\0"
    "bdl\0border-left: 1px solid #000;\0"
    "bdln\0border-left: none;\0"
    "bdl:n\0border-left: none;\0"
    "bdrs\0border-radius: |;\0"
    "bdrz\0border-radius: |;\0"
    "bdw\0border-width: |;\0"
    "bdtw\0border-top-width: |;\0"
    "bdrw\0border-right-width: |;\0"
    "bdbw\0border-bottom-width: |;\0"
    "bdlw\0border-left-width: |;\0"
    "bds\0border-style: |;\0"
    "bdc\0border-color: #000;\0"
    "bdss\0border-style: solid;\0"
    "bds:s\0border-style: solid;\0"
    "bdsd\0border-style: dashed;\0"
    "bds:d\0border-style: dashed;\0"
    "bdsdt\0border-style: dotted;\0"
    "bds:dt\0border-style: dotted;\0"
    "bdct\0border-color: transparent;\0"
    "bdc:t\0border-color: transparent;\0"
    "bdcl\0border-collapse: collapse;\0"
    "bdclc\0border-collapse: collapse;\0"
    "bdcl:c\0border-collapse: collapse;\0"
    "bdcls\0border-collapse: separate;\0"
    "bdcl:s\0border-collapse: separate;\0"
    "ol\0outline: none;\0"
    "oln\0outline: none;\0"
    "ol:n\0outline: none;\0"
    "lis\0list-style: none;\0"
    "lisn\0list-style: none;\0"
    "lis:n\0list-style: none;\0"
    "cur\0cursor: pointer;\0"
    "curp\0cursor: pointer;\0"
    "cur:p\0cursor: pointer;\0"
    "curd\0cursor: default;\0"
    "cur:d\0cursor: default;\0"
    "curt\0cursor: text;\0"
    "cur:t\0cursor: text;\0"
    "curn\0cursor: not-allowed;\0"
    "cur:na\0cursor: not-allowed;\0"
    "curm\0cursor: move;\0"
    "cur:m\0cursor: move;\0"
    "pe\0pointer-events: none;\0"
    "pen\0pointer-events: none;\0"
    "pe:n\0pointer-events: none;\0"
    "pea\0pointer-events: auto;\0"
    "pe:a\0pointer-events: auto;\0"
    "us\0user-select: none;\0"
    "usn\0user-select: none;\0"
    "us:n\0user-select: none;\0"
    "trf\0transform: |;\0"
    "trfn\0transform: none;\0"
    "trf:n\0transform: none;\0"
    "trfr\0transform: rotate(|deg);\0"
    "trf:r\0transform: rotate(|deg);\0"
    "trfs\0transform: scale(|);\0"
    "trf:s\0transform: scale(|);\0"
    "trft\0transform: translate(|);\0"
    "trf:t\0transform: translate(|);\0"
    "trftx\0transform: translateX(|);\0"
    "trf:tx\0transform: translateX(|);\0"
    "trfty\0transform: translateY(|);\0"
    "trf:ty\0transform: translateY(|);\0"
    "trfo\0transform-origin: |;\0"
    "trs\0transition: |;\0"
    "trsn\0transition: none;\0"
    "trs:n\0transition: none;\0"
    "trsdu\0transition-duration: |;\0"
    "trsdl\0transition-delay: |;\0"
    "trsp\0transition-property: |;\0"
    "trstf\0transition-timing-function: |;\0"
    "anim\0animation: |;\0"
    "animdu\0animation-duration: |;\0"
    "animdl\0animation-delay: |;\0"
    "animn\0animation-name: |;\0"
    "animic\0animation-iteration-count: |;\0"
    "animdir\0animation-direction: |;\0"
    "animfm\0animation-fill-mode: |;\0"
    "animps\0animation-play-state: |;\0"
    "animtf\0animation-timing-function: |;\0"
    "cnt\0content: '|';\0"
    "objf\0object-fit: cover;\0"
    "objfc\0object-fit: cover;\0"
    "objf:c\0object-fit: cover;\0"
    "objfcn\0object-fit: contain;\0"
    "objf:ct\0object-fit: contain;\0"
    "asp\0aspect-ratio: |;\0"
    "fil\0filter: |;\0"
    "bfil\0backdrop-filter: blur(|px);\0"
    "@i\0@import url(|);\0"
    "@import\0@import url(|);\0"
    "\0";

struct CssPrefixProp {
    const char* prefix;
    const char* prop;
    bool unitlessInt;
    bool unitlessFloat;
    const char* defaultSubUnit;
};

static constexpr CssPrefixProp kCssPrefixProps[] = {
    { "animdu", "animation-duration", false, false, "s" },
    { "animdl", "animation-delay", false, false, "s" },
    { "trsdu", "transition-duration", false, false, "s" },
    { "trsdl", "transition-delay", false, false, "s" },
    { "bdtw", "border-top-width", false, false, "px" },
    { "bdrw", "border-right-width", false, false, "px" },
    { "bdbw", "border-bottom-width", false, false, "px" },
    { "bdlw", "border-left-width", false, false, "px" },
    { "bdrs", "border-radius", false, false, "px" },
    { "bdrz", "border-radius", false, false, "px" },
    { "maw", "max-width", false, false, "px" },
    { "mah", "max-height", false, false, "px" },
    { "miw", "min-width", false, false, "px" },
    { "mih", "min-height", false, false, "px" },
    { "bdw", "border-width", false, false, "px" },
    { "bdc", "border-color", false, false, "px" },
    { "bdt", "border-top", false, false, "px" },
    { "bdr", "border-right", false, false, "px" },
    { "bdb", "border-bottom", false, false, "px" },
    { "bdl", "border-left", false, false, "px" },
    { "bgc", "background-color", false, false, "px" },
    { "fxg", "flex-grow", true, true, "" },
    { "fxs", "flex-shrink", true, true, "" },
    { "fxb", "flex-basis", false, false, "px" },
    { "lts", "letter-spacing", false, false, "px" },
    { "ord", "order", true, true, "" },
    { "gap", "gap", false, false, "px" },
    { "ls", "letter-spacing", false, false, "px" },
    { "mt", "margin-top", false, false, "px" },
    { "mr", "margin-right", false, false, "px" },
    { "mb", "margin-bottom", false, false, "px" },
    { "ml", "margin-left", false, false, "px" },
    { "mx", "margin-inline", false, false, "px" },
    { "my", "margin-block", false, false, "px" },
    { "pt", "padding-top", false, false, "px" },
    { "pr", "padding-right", false, false, "px" },
    { "pb", "padding-bottom", false, false, "px" },
    { "pl", "padding-left", false, false, "px" },
    { "px", "padding-inline", false, false, "px" },
    { "py", "padding-block", false, false, "px" },
    { "fz", "font-size", false, false, "px" },
    { "fw", "font-weight", true, true, "" },
    { "lh", "line-height", true, true, "" },
    { "ti", "text-indent", false, false, "px" },
    { "op", "opacity", true, true, "" },
    { "bg", "background", false, false, "px" },
    { "bd", "border", false, false, "px" },
    { "rg", "row-gap", false, false, "px" },
    { "cg", "column-gap", false, false, "px" },
    { "fx", "flex", true, true, "" },
    { "ins", "inset", false, false, "px" },
    { "m", "margin", false, false, "px" },
    { "p", "padding", false, false, "px" },
    { "w", "width", false, false, "px" },
    { "h", "height", false, false, "px" },
    { "t", "top", false, false, "px" },
    { "r", "right", false, false, "px" },
    { "b", "bottom", false, false, "px" },
    { "l", "left", false, false, "px" },
    { "z", "z-index", true, true, "" },
    { "c", "color", false, false, "px" },
    { "g", "gap", false, false, "px" }
};

static int hexVal(char c)  noexcept{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static std::string expandHexColor(const std::string& rawHex, const std::string& alphaFrac = "")  noexcept{
    std::string h6;
    if (rawHex.size() == 1) {
        h6.append(6, rawHex[0]);
    } else if (rawHex.size() == 2) {
        h6 = rawHex + rawHex + rawHex;
    } else if (rawHex.size() == 3) {
        h6 = std::string() + rawHex[0] + rawHex[0] + rawHex[1] + rawHex[1] + rawHex[2] + rawHex[2];
    } else {
        h6 = rawHex;
    }
    if (!alphaFrac.empty() && h6.size() == 6) {
        int r = hexVal(h6[0]) * 16 + hexVal(h6[1]);
        int g = hexVal(h6[2]) * 16 + hexVal(h6[3]);
        int b = hexVal(h6[4]) * 16 + hexVal(h6[5]);
        return "rgba(" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ", 0." + alphaFrac + ")";
    }
    if (rawHex.size() == 1) {
        return std::string("#") + rawHex[0] + rawHex[0] + rawHex[0];
    }
    if (rawHex.size() == 2) {
        return std::string("#") + rawHex + rawHex + rawHex;
    }
    return std::string("#") + rawHex;
}

static bool parseCssValueTokens(const std::string& valPart, const CssPrefixProp& propInfo, std::string& outVal)  noexcept{
    if (valPart.empty()) return false;
    size_t i = 0;
    size_t n = valPart.size();
    if (valPart[i] == ':') {
        i++;
        if (i >= n) return false;
    }

    std::vector<std::string> tokens;
    bool isBorderProp = (std::strcmp(propInfo.prop, "border") == 0 ||
                         std::strcmp(propInfo.prop, "border-top") == 0 ||
                         std::strcmp(propInfo.prop, "border-right") == 0 ||
                         std::strcmp(propInfo.prop, "border-bottom") == 0 ||
                         std::strcmp(propInfo.prop, "border-left") == 0);
    bool hasBorderWidth = false;
    bool hasBorderStyle = false;
    bool hasBorderColor = false;

    while (i < n) {
        if (valPart[i] == '-') {
            if (!tokens.empty()) {
                i++;
                if (i >= n) return false;
            }
        }

        if (valPart[i] == '#') {
            i++;
            size_t s = i;
            while (i < n && isAsciiXdigit(valPart[i])) i++;
            if (i == s) return false;
            std::string rawHex = valPart.substr(s, i - s);
            std::string alphaFrac;
            if (i < n && valPart[i] == '.') {
                i++;
                size_t as = i;
                while (i < n && isAsciiDigit(valPart[i])) i++;
                if (i == as) return false;
                alphaFrac = valPart.substr(as, i - as);
            }
            tokens.push_back(expandHexColor(rawHex, alphaFrac));
            hasBorderColor = true;
        } else if (valPart[i] == '-' || valPart[i] == '.' || isAsciiDigit(valPart[i])) {
            bool neg = false;
            if (valPart[i] == '-') {
                neg = true;
                i++;
            }
            size_t numStart = i;
            bool hasDigits = false;
            bool hasDot = false;
            while (i < n && (isAsciiDigit(valPart[i]) || valPart[i] == '.')) {
                if (valPart[i] == '.') {
                    if (hasDot) break;
                    hasDot = true;
                } else {
                    hasDigits = true;
                }
                i++;
            }
            if (!hasDigits) return false;
            std::string rawNum = valPart.substr(numStart, i - numStart);
            if (!rawNum.empty() && rawNum[0] == '.') rawNum = "0" + rawNum;
            std::string numStr = (neg ? "-" : "") + rawNum;

            size_t uStart = i;
            while (i < n && isAsciiAlpha(valPart[i])) i++;
            std::string unit = toLowerStr(valPart.substr(uStart, i - uStart));

            std::string resolvedUnit;
            if (unit.empty()) {
                if (numStr == "0" || numStr == "-0") {
                    resolvedUnit = "";
                } else if (hasDot) {
                    resolvedUnit = propInfo.unitlessFloat ? "" : (std::strcmp(propInfo.defaultSubUnit, "px") == 0 ? "em" : propInfo.defaultSubUnit);
                } else {
                    resolvedUnit = propInfo.unitlessInt ? "" : propInfo.defaultSubUnit;
                }
            } else if (unit == "p") resolvedUnit = "%";
            else if (unit == "e") resolvedUnit = "em";
            else if (unit == "r") resolvedUnit = "rem";
            else if (unit == "x") resolvedUnit = "ex";
            else if (unit == "px" || unit == "em" || unit == "rem" || unit == "vh" || unit == "vw" ||
                     unit == "vmin" || unit == "vmax" || unit == "dvh" || unit == "dvw" || unit == "svh" || unit == "lvh" ||
                     unit == "ch" || unit == "ex" || unit == "cm" || unit == "mm" || unit == "in" ||
                     unit == "pt" || unit == "pc" || unit == "s" || unit == "ms" || unit == "deg" ||
                     unit == "rad" || unit == "grad" || unit == "turn" || unit == "fr") {
                resolvedUnit = unit;
            } else if (isBorderProp && (unit == "s" || unit == "d" || unit == "dt" || unit == "n")) {
                resolvedUnit = (numStr == "0") ? "" : "px";
                tokens.push_back(numStr + resolvedUnit);
                hasBorderWidth = true;
                if (unit == "s") tokens.push_back("solid");
                else if (unit == "d") tokens.push_back("dashed");
                else if (unit == "dt") tokens.push_back("dotted");
                else if (unit == "n") tokens.push_back("none");
                hasBorderStyle = true;
                continue;
            } else {
                return false;
            }

            tokens.push_back(numStr + resolvedUnit);
            hasBorderWidth = true;
        } else if (isAsciiAlpha(valPart[i])) {
            size_t s = i;
            while (i < n && isAsciiAlpha(valPart[i])) i++;
            std::string kw = toLowerStr(valPart.substr(s, i - s));
            if (kw == "a" || kw == "auto") {
                tokens.push_back("auto");
            } else if (kw == "n" || kw == "none") {
                tokens.push_back("none");
                hasBorderStyle = true;
            } else if (isBorderProp && (kw == "s" || kw == "solid")) {
                tokens.push_back("solid");
                hasBorderStyle = true;
            } else if (isBorderProp && (kw == "d" || kw == "dashed")) {
                tokens.push_back("dashed");
                hasBorderStyle = true;
            } else if (isBorderProp && (kw == "dt" || kw == "dotted")) {
                tokens.push_back("dotted");
                hasBorderStyle = true;
            } else if (isBorderProp && (kw == "db" || kw == "double")) {
                tokens.push_back("double");
                hasBorderStyle = true;
            } else if (kw == "t" || kw == "transparent") {
                tokens.push_back("transparent");
            } else if (kw == "i" || kw == "inherit") {
                tokens.push_back("inherit");
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    if (tokens.empty()) return false;

    if (isBorderProp) {
        if (hasBorderWidth && !hasBorderStyle) {
            if (!(tokens.size() == 1 && tokens[0] == "0")) {
                tokens.insert(tokens.begin() + 1, "solid");
            }
        } else if (!hasBorderWidth && !hasBorderStyle && hasBorderColor) {
            tokens.insert(tokens.begin(), "solid");
            tokens.insert(tokens.begin(), "1px");
        }
    }

    outVal.clear();
    for (size_t k = 0; k < tokens.size(); ++k) {
        if (k > 0) outVal += ' ';
        outVal += tokens[k];
    }
    return true;
}

static bool expandSingleCssAbbreviation(
    const std::string& rawPart,
    const std::string& baseIndent,
    const std::string& newlineStr,
    bool inlineMode,
    std::string& out,
    std::vector<Tabstop>& tabstops
)  noexcept{
    if (rawPart.empty()) return false;
    if (rawPart.size() > 1 && rawPart[0] == '-' && isAsciiAlpha(rawPart[1])) {
        std::string subOut;
        std::vector<Tabstop> subStops;
        if (expandSingleCssAbbreviation(rawPart.substr(1), baseIndent, newlineStr, inlineMode, subOut, subStops)) {
            if (!subOut.empty() && subOut[0] >= 'a' && subOut[0] <= 'z' && subOut.find('\n') == std::string::npos && subOut.find('\r') == std::string::npos && subOut.find(": ") != std::string::npos) {
                static constexpr const char* kPrefixes[] = { "-webkit-", "-moz-", "-ms-", "-o-", "" };
                std::string sep = inlineMode ? " " : (newlineStr + baseIndent);
                for (int pi = 0; pi < 5; ++pi) {
                    if (pi > 0) out += sep;
                    out += kPrefixes[pi];
                    size_t lineBase = out.size();
                    out += subOut;
                    for (const auto& st : subStops) {
                        tabstops.push_back({ lineBase + st.startOffset, lineBase + st.endOffset });
                    }
                }
                return true;
            }
        }
        return false;
    }

    std::string part = rawPart;
    bool important = false;
    if (part.size() > 1 && part.back() == '!') {
        important = true;
        part.pop_back();
    }
    std::string lower = toLowerStr(part);

    auto appendWithStop = [&](const std::string& tmpl, bool imp) {
        std::string s = tmpl;
        if (imp && !s.empty() && s.back() == ';') {
            s.pop_back();
            s += " !important;";
        }
        size_t barPos = s.find('|');
        if (barPos != std::string::npos) {
            out += s.substr(0, barPos);
            tabstops.push_back({ out.size(), out.size() });
            out += s.substr(barPos + 1);
        } else {
            out += s;
        }
    };

    if (lower == "@m" || lower == "@media") {
        out += "@media screen and (";
        tabstops.push_back({ out.size(), out.size() });
        out += ") {" + newlineStr + baseIndent + "\t";
        tabstops.push_back({ out.size(), out.size() });
        out += newlineStr + baseIndent + "}";
        return true;
    }
    if (lower == "@kf" || lower == "@keyframes") {
        out += "@keyframes ";
        tabstops.push_back({ out.size(), out.size() });
        out += " {" + newlineStr + baseIndent + "\t";
        tabstops.push_back({ out.size(), out.size() });
        out += newlineStr + baseIndent + "}";
        return true;
    }
    if (lower == "@f" || lower == "@font-face") {
        out += "@font-face {" + newlineStr + baseIndent + "\tfont-family: ";
        tabstops.push_back({ out.size(), out.size() });
        out += ";" + newlineStr + baseIndent + "\tsrc: url(";
        tabstops.push_back({ out.size(), out.size() });
        out += ");" + newlineStr + baseIndent + "}";
        return true;
    }
    if (lower.compare(0, 3, "lg(") == 0 && lower.back() == ')') {
        std::string inner = part.substr(3, part.size() - 4);
        std::string exp = "background-image: linear-gradient(" + inner + ")";
        if (important) exp += " !important";
        exp += ";";
        out += exp;
        return true;
    }

    for (const char* cp = kCssExactBlob; *cp; ) {
        const char* ca = cp;
        cp += std::strlen(cp) + 1;
        const char* ce = cp;
        cp += std::strlen(cp) + 1;
        if (lower == ca) {
            appendWithStop(ce, important);
            return true;
        }
    }

    if (lower.compare(0, 2, "sq") == 0 && lower.size() > 2) {
        CssPrefixProp dummyW = { "w", "width", false, false, "px" };
        std::string val;
        if (parseCssValueTokens(part.substr(2), dummyW, val)) {
            std::string impStr = important ? " !important;" : ";";
            out += "width: " + val + impStr + newlineStr + baseIndent + "height: " + val + impStr;
            return true;
        }
    }

    // Parameterized transform / filter functions: trfr45, trfs1.5, trftx10, trfty20, bfil10
    if (lower.compare(0, 4, "trfr") == 0 && lower.size() > 4) {
        CssPrefixProp dummyDeg = { "trfr", "transform", false, false, "deg" };
        std::string val;
        if (parseCssValueTokens(part.substr(4), dummyDeg, val)) {
            out += "transform: rotate(" + val + ")" + (important ? " !important;" : ";");
            return true;
        }
    }
    if (lower.compare(0, 4, "trfs") == 0 && lower.size() > 4) {
        CssPrefixProp dummyScale = { "trfs", "transform", true, true, "" };
        std::string val;
        if (parseCssValueTokens(part.substr(4), dummyScale, val)) {
            out += "transform: scale(" + val + ")" + (important ? " !important;" : ";");
            return true;
        }
    }
    if (lower.compare(0, 5, "trftx") == 0 && lower.size() > 5) {
        CssPrefixProp dummyPx = { "trftx", "transform", false, false, "px" };
        std::string val;
        if (parseCssValueTokens(part.substr(5), dummyPx, val)) {
            out += "transform: translateX(" + val + ")" + (important ? " !important;" : ";");
            return true;
        }
    }
    if (lower.compare(0, 5, "trfty") == 0 && lower.size() > 5) {
        CssPrefixProp dummyPx = { "trfty", "transform", false, false, "px" };
        std::string val;
        if (parseCssValueTokens(part.substr(5), dummyPx, val)) {
            out += "transform: translateY(" + val + ")" + (important ? " !important;" : ";");
            return true;
        }
    }
    if (lower.compare(0, 4, "bfil") == 0 && lower.size() > 4) {
        CssPrefixProp dummyPx = { "bfil", "backdrop-filter", false, false, "px" };
        std::string val;
        if (parseCssValueTokens(part.substr(4), dummyPx, val)) {
            out += "backdrop-filter: blur(" + val + ")" + (important ? " !important;" : ";");
            return true;
        }
    }

    for (const auto& pp : kCssPrefixProps) {
        size_t pLen = std::strlen(pp.prefix);
        if (lower.compare(0, pLen, pp.prefix) == 0 && part.size() > pLen) {
            std::string valPart = part.substr(pLen);
            std::string outVal;
            if (parseCssValueTokens(valPart, pp, outVal)) {
                out += pp.prop;
                out += ": ";
                out += outVal;
                if (important) out += " !important";
                out += ';';
                return true;
            }
        }
    }

    return false;
}

static bool tryExpandCss(
    const std::string& abbr,
    bool inlineMode,
    const std::string& baseIndent,
    const std::string& newlineStr,
    std::string& out,
    std::vector<Tabstop>& tabstops
)  noexcept{
    out.clear();
    tabstops.clear();
    std::vector<std::string> parts;
    size_t start = 0;
    int parenDepth = 0;
    for (size_t i = 0; i < abbr.size(); ++i) {
        if (abbr[i] == '(') parenDepth++;
        else if (abbr[i] == ')') parenDepth--;
        else if (abbr[i] == '+' && parenDepth == 0) {
            if (i == start) return false;
            parts.push_back(abbr.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start >= abbr.size()) return false;
    parts.push_back(abbr.substr(start));

    for (size_t idx = 0; idx < parts.size(); ++idx) {
        if (idx > 0) {
            if (inlineMode) out += ' ';
            else out += newlineStr + baseIndent;
        }
        if (!expandSingleCssAbbreviation(parts[idx], baseIndent, newlineStr, inlineMode, out, tabstops)) {
            return false;
        }
    }
    if (tabstops.empty() || tabstops.back().endOffset != out.size()) {
        tabstops.push_back({ out.size(), out.size() });
    }
    return true;
}

static bool isValidHtmlPoolForMode(const std::vector<HtmlNode>& pool, const std::string& abbr, Mode mode)  noexcept{
    if (pool.size() <= 1) return false;
    bool hasNonTextElement = false;
    for (size_t i = 1; i < pool.size(); ++i) {
        if (!pool[i].isGroup && !pool[i].isTextOnly) {
            hasNonTextElement = true;
            break;
        }
    }
    if (!hasNonTextElement) return false;
    bool hasOperators = false;
    for (char c : abbr) {
        if (c == '>' || c == '+' || c == '^' || c == '*' || c == '#' || c == '.' || c == '[' || c == '{' || c == ':' || c == '/' || c == '!') {
            hasOperators = true;
            break;
        }
    }

    if (mode == Mode::CodeStrict && !hasOperators) {
        return false;
    }

    for (size_t i = 1; i < pool.size(); ++i) {
        const HtmlNode& node = pool[i];
        if (node.isGroup || node.isTextOnly || node.isComment || node.isLorem || !node.specialTemplate.empty()) {
            continue;
        }
        if (node.tag.empty()) {
            if (node.id.empty() && node.classes.empty() && node.attrs.empty()) return false;
            continue;
        }
        if (mode == Mode::Xml) {
            continue;
        }
        if (mode == Mode::Jsx && isAsciiUpper(node.tag[0])) {
            continue;
        }
        if (!isKnownHtmlTagOrSnippet(node.tag)) {
            size_t dashPos = node.tag.find('-');
            bool isCustomWebComponent = (dashPos != std::string::npos && dashPos > 0 && dashPos + 1 < node.tag.size() && node.tag.back() != '-') && (mode == Mode::Html || hasOperators);
            if (!isCustomWebComponent) {
                return false;
            }
        }
    }
    return true;
}

static inline bool decodeFullWidthAsciiAt(const std::string& s, size_t idx, char& outAscii)  noexcept{
    if (idx + 2 >= s.size()) return false;
    unsigned char b0 = (unsigned char)s[idx];
    unsigned char b1 = (unsigned char)s[idx + 1];
    unsigned char b2 = (unsigned char)s[idx + 2];
    if (b0 == 0xEF) {
        if (b1 == 0xBC && b2 >= 0x81 && b2 <= 0xBF) {
            outAscii = (char)(b2 - 0x60);
            return true;
        }
        if (b1 == 0xBD && b2 >= 0x80 && b2 <= 0x9E) {
            outAscii = (char)(b2 - 0x20);
            return true;
        }
    }
    return false;
}

static std::string normalizeFullWidthAbbreviation(const std::string& rawAbbr)  noexcept{
    bool hasNonAscii = false;
    for (unsigned char ch : rawAbbr) {
        if (ch >= 0x80) { hasNonAscii = true; break; }
    }
    if (!hasNonAscii) return rawAbbr;

    std::string out;
    out.reserve(rawAbbr.size());
    int braceDepth = 0;
    int bracketDepth = 0;
    char quoteChar = 0;

    for (size_t i = 0; i < rawAbbr.size(); ) {
        char c = rawAbbr[i];
        char fwCh = 0;
        bool isFw = decodeFullWidthAsciiAt(rawAbbr, i, fwCh);

        if (braceDepth > 0) {
            if (c == '\\' && i + 1 < rawAbbr.size()) {
                out += rawAbbr[i];
                out += rawAbbr[i + 1];
                i += 2;
                continue;
            }
            char eff = isFw ? fwCh : c;
            if (eff == '{') {
                braceDepth++;
                out += '{';
                i += isFw ? 3 : 1;
                continue;
            }
            if (eff == '}') {
                braceDepth--;
                out += '}';
                i += isFw ? 3 : 1;
                continue;
            }
            out += c;
            i++;
            continue;
        }

        if (quoteChar != 0) {
            if (c == '\\' && i + 1 < rawAbbr.size()) {
                out += rawAbbr[i];
                out += rawAbbr[i + 1];
                i += 2;
                continue;
            }
            char eff = isFw ? fwCh : c;
            if (eff == quoteChar) {
                quoteChar = 0;
                out += eff;
                i += isFw ? 3 : 1;
                continue;
            }
            out += c;
            i++;
            continue;
        }

        char eff = isFw ? fwCh : c;
        size_t step = isFw ? 3 : 1;
        if (eff == '{') {
            braceDepth = 1;
            out += '{';
        } else if (eff == '[') {
            bracketDepth++;
            out += '[';
        } else if (eff == ']') {
            if (bracketDepth > 0) bracketDepth--;
            out += ']';
        } else if (bracketDepth > 0 && (eff == '"' || eff == '\'')) {
            quoteChar = eff;
            out += eff;
        } else if (isFw) {
            out += fwCh;
        } else {
            out += c;
        }
        i += step;
    }
    return out;
}

static bool extractAbbreviationFromLine(
    const std::string& lineText,
    size_t caretCol,
    size_t& outStartCol,
    std::string& outAbbr,
    bool& outInsideTagQuotes,
    bool& outInsideStyleAttr
)  noexcept{
    outInsideTagQuotes = false;
    outInsideStyleAttr = false;
    if (caretCol == 0 || caretCol > lineText.size()) return false;

    if (caretCol < lineText.size()) {
        char nextCh = lineText[caretCol];
        if (isIdentChar(nextCh) || nextCh == '$') return false;
        char fwNext = 0;
        if (decodeFullWidthAsciiAt(lineText, caretCol, fwNext) && (isIdentChar(fwNext) || fwNext == '$')) return false;
    }

    int braceDepth = 0;
    int bracketDepth = 0;
    int parenDepth = 0;
    char inAttrQuote = 0;

    size_t pos = caretCol;
    while (pos > 0) {
        size_t step = 1;
        char c = lineText[pos - 1];
        if ((unsigned char)c >= 0x80) {
            char fwCh = 0;
            if (pos >= 3 && decodeFullWidthAsciiAt(lineText, pos - 3, fwCh)) {
                c = fwCh;
                step = 3;
            } else {
                if (braceDepth > 0 || inAttrQuote != 0 || bracketDepth > 0 || parenDepth > 0) {
                    pos--;
                    continue;
                }
                break;
            }
        }

        if (braceDepth > 0) {
            if (step == 1 && pos >= 2 && lineText[pos - 2] == '\\') {
                pos -= 2;
                continue;
            }
            if (c == '}') braceDepth++;
            else if (c == '{') braceDepth--;
            pos -= step;
            continue;
        }

        if (inAttrQuote != 0) {
            if (c == inAttrQuote && (step > 1 || pos < 2 || lineText[pos - 2] != '\\')) {
                inAttrQuote = 0;
            }
            pos -= step;
            continue;
        }

        if (bracketDepth > 0) {
            if (c == '"' || c == '\'') {
                inAttrQuote = c;
            } else if (c == '}') {
                braceDepth++;
            } else if (c == ']') {
                bracketDepth++;
            } else if (c == '[') {
                bracketDepth--;
            }
            pos -= step;
            continue;
        }

        if (c == '}') { braceDepth = 1; pos -= step; continue; }
        if (c == '{') { break; }
        if (c == ']') { bracketDepth = 1; pos -= step; continue; }
        if (c == '[') { return false; }
        if (c == ')') { parenDepth++; pos -= step; continue; }
        if (c == '(') {
            if (parenDepth > 0) { parenDepth--; pos -= step; continue; }
            return false;
        }

        if (parenDepth == 0) {
            if (c == ' ' || c == '\t' || c == ';' || c == '"' || c == '\'') {
                break;
            }
            if (c == '<') {
                break;
            }
            if (c == '>' && step == 1) {
                bool closesHtmlTag = false;
                int q = 0, br = 0, bk = 0;
                for (size_t k = 0; k < pos - 1; ++k) {
                    char kc = lineText[k];
                    if (kc == '\\' && k + 1 < pos - 1) { k++; continue; }
                    if (q != 0) {
                        if (kc == q) q = 0;
                    } else if (br > 0) {
                        if (kc == '{') br++;
                        else if (kc == '}') br--;
                    } else if (bk > 0) {
                        if (kc == '"' || kc == '\'') q = kc;
                        else if (kc == '[') bk++;
                        else if (kc == ']') bk--;
                    } else {
                        if (kc == '"' || kc == '\'') q = kc;
                        else if (kc == '{') br = 1;
                        else if (kc == '[') bk = 1;
                        else if (kc == '<' && k + 1 < pos - 1 && (isAsciiAlpha(lineText[k + 1]) || lineText[k + 1] == '/' || lineText[k + 1] == '!' || lineText[k + 1] == '?')) {
                            closesHtmlTag = true;
                        } else if (kc == '>') {
                            closesHtmlTag = false;
                        }
                    }
                }
                if (closesHtmlTag) {
                    break;
                }
            }
        }
        pos -= step;
    }

    if (braceDepth != 0 || bracketDepth != 0 || parenDepth != 0 || inAttrQuote != 0) {
        return false;
    }

    if (pos >= caretCol) return false;

    int quoteChar = 0;
    size_t quoteOpenPos = 0;
    int lastUnclosedLt = -1;
    for (size_t k = 0; k < pos; ++k) {
        char c = lineText[k];
        if (quoteChar != 0) {
            if (c == '\\' && k + 1 < pos) { k++; continue; }
            if (c == quoteChar) quoteChar = 0;
        } else {
            if (c == '"' || c == '\'') {
                quoteChar = c;
                quoteOpenPos = k;
            } else if (c == '<' && (k + 1 >= pos || isAsciiAlpha(lineText[k + 1]) || lineText[k + 1] == '/' || lineText[k + 1] == '!' || lineText[k + 1] == '?')) {
                lastUnclosedLt = (int)k;
            } else if (c == '>' && lastUnclosedLt != -1) {
                lastUnclosedLt = -1;
            }
        }
    }

    if (quoteChar != 0 && lastUnclosedLt != -1) {
        std::string beforeQuote = toLowerStr(lineText.substr((size_t)lastUnclosedLt, quoteOpenPos - (size_t)lastUnclosedLt));
        size_t eqPos = beforeQuote.find_last_not_of(" \t");
        if (eqPos != std::string::npos && beforeQuote[eqPos] == '=') {
            size_t nameEnd = (eqPos > 0) ? beforeQuote.find_last_not_of(" \t", eqPos - 1) : std::string::npos;
            if (nameEnd != std::string::npos && nameEnd >= 4 && beforeQuote.substr(nameEnd - 4, 5) == "style") {
                outInsideStyleAttr = true;
            }
        }
        if (!outInsideStyleAttr) {
            outInsideTagQuotes = true;
            return false;
        }
    } else if (lastUnclosedLt != -1) {
        return false;
    }

    outStartCol = pos;
    outAbbr = lineText.substr(pos, caretCol - pos);
    return !outAbbr.empty();
}

inline ExpandResult expandOnLine(
    const std::string& lineText,
    size_t caretCol,
    Mode mode,
    bool insideCssBlock,
    const std::string& newlineStr = "\n"
) {
    ExpandResult res;
    size_t abbrStart = 0;
    std::string rawAbbr;
    bool insideTagQuotes = false;
    bool insideStyleAttr = false;

    if (!extractAbbreviationFromLine(lineText, caretCol, abbrStart, rawAbbr, insideTagQuotes, insideStyleAttr)) {
        return res;
    }

    std::string abbr = normalizeFullWidthAbbreviation(rawAbbr);

    bool isPlainAlphaWord = !abbr.empty();
    for (char c : abbr) {
        if (!isAsciiAlpha(c)) {
            isPlainAlphaWord = false;
            break;
        }
    }
    if (isPlainAlphaWord && !insideStyleAttr) {
        size_t checkPos = abbrStart;
        while (checkPos > 0) {
            if (lineText[checkPos - 1] == ' ' || lineText[checkPos - 1] == '\t') {
                checkPos--;
            } else if (checkPos >= 3 && (unsigned char)lineText[checkPos - 3] == 0xE3 &&
                       (unsigned char)lineText[checkPos - 2] == 0x80 && (unsigned char)lineText[checkPos - 1] == 0x80) {
                checkPos -= 3;
            } else {
                break;
            }
        }
        if (checkPos > 0) {
            unsigned char prevByte = (unsigned char)lineText[checkPos - 1];
            char prevCh = (char)prevByte;
            bool directlyAfterNonAscii = (checkPos == abbrStart && prevByte >= 0x80);
            if (!directlyAfterNonAscii && prevCh != '>' && prevCh != '{' && prevCh != ';') {
                return res;
            }
        }
    }

    if (!insideCssBlock && !insideStyleAttr && (mode == Mode::Auto || mode == Mode::Html)) {
        int lineBraceBalance = 0;
        int q = 0;
        for (size_t k = 0; k < abbrStart; ++k) {
            char ch = lineText[k];
            if (q != 0) {
                if (ch == q) q = 0;
            } else if (ch == '"' || ch == '\'') {
                q = ch;
            } else if (ch == '{') {
                lineBraceBalance++;
            } else if (ch == '}' && lineBraceBalance > 0) {
                lineBraceBalance--;
            }
        }
        if (lineBraceBalance > 0) {
            insideCssBlock = true;
        }
    }

    std::string baseIndent;
    for (size_t i = 0; i < abbrStart && i < lineText.size(); ) {
        if (lineText[i] == ' ' || lineText[i] == '\t') {
            baseIndent += lineText[i];
            i++;
        } else if (i + 2 < abbrStart && (unsigned char)lineText[i] == 0xE3 &&
                   (unsigned char)lineText[i + 1] == 0x80 && (unsigned char)lineText[i + 2] == 0x80) {
            baseIndent.append(lineText, i, 3);
            i += 3;
        } else {
            break;
        }
    }

    if (insideStyleAttr || mode == Mode::CssInline) {
        if (tryExpandCss(abbr, true, baseIndent, newlineStr, res.expanded, res.tabstops)) {
            res.matched = true;
            res.abbrStartCol = abbrStart;
            res.abbrLen = rawAbbr.size();
        }
        return res;
    }

    if (mode == Mode::Css || insideCssBlock) {
        if (tryExpandCss(abbr, false, baseIndent, newlineStr, res.expanded, res.tabstops)) {
            res.matched = true;
            res.abbrStartCol = abbrStart;
            res.abbrLen = rawAbbr.size();
            return res;
        }
        if (mode == Mode::Css) return res;
    }

    std::string htmlAbbr = abbr;
    bool filterBem = false, filterComment = false, filterEscape = false;
    extractHtmlFilters(htmlAbbr, filterBem, filterComment, filterEscape);

    std::vector<HtmlNode> pool;
    if (parseHtmlAbbreviation(htmlAbbr, pool) && isValidHtmlPoolForMode(pool, abbr, mode)) {
        if (filterBem) {
            applyBemFilter(pool);
        }
        renderHtmlTree(pool, mode, baseIndent, newlineStr, res.expanded, res.tabstops, filterComment, filterEscape);
        res.matched = true;
        res.abbrStartCol = abbrStart;
        res.abbrLen = rawAbbr.size();
        return res;
    }

    if (mode == Mode::Auto && abbr.size() >= 2) {
        if (tryExpandCss(abbr, false, baseIndent, newlineStr, res.expanded, res.tabstops)) {
            res.matched = true;
            res.abbrStartCol = abbrStart;
            res.abbrLen = rawAbbr.size();
            return res;
        }
    }

    return res;
}

inline ExpandResult expandAbbreviation(
    const std::string& abbr,
    const std::string& /*indentUnit*/,
    const std::string& newlineStr,
    const std::string& currentIndent,
    const std::string& syntaxMode,
    bool /*withTabstops*/)
{
    Mode mode = Mode::Html;
    if (syntaxMode == "css") mode = Mode::Css;
    else if (syntaxMode == "jsx") mode = Mode::Jsx;
    else if (syntaxMode == "xml" || syntaxMode == "xsl") mode = Mode::Xml;
    std::string lineText = currentIndent + abbr;
    ExpandResult r = expandOnLine(lineText, lineText.size(), mode, (syntaxMode == "css"), newlineStr);
    if (r.matched) {
        r.success = true;
        r.text = r.expanded;
        if (!r.tabstops.empty()) {
            r.cursorOffset = r.tabstops[0].startOffset;
            for (size_t i = 0; i < r.tabstops.size(); ++i) {
                TabstopGroup g;
                g.id = (i + 1 == r.tabstops.size() && r.tabstops.size() == 1) ? 0 : (int)(i + 1);
                g.ranges.push_back({ r.tabstops[i].startOffset, r.tabstops[i].endOffset });
                r.groups.push_back(std::move(g));
            }
        } else {
            r.cursorOffset = r.text.size();
        }
    }
    return r;
}

} // namespace Emmet

