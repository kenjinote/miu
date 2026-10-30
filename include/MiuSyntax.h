#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <unordered_set>
#include <regex>
#include <cctype>
#include <cwctype>
#include <cstdint>
#include <cstring>
#include <algorithm>

struct SyntaxColor {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    bool operator==(const SyntaxColor& o) const noexcept {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const SyntaxColor& o) const noexcept {
        return !(*this == o);
    }
};

struct SyntaxWordSet {
    std::deque<std::string> storage;
    std::unordered_set<std::string_view> lookup;
    SyntaxWordSet() = default;
    SyntaxWordSet(const SyntaxWordSet& o) {
        for (const auto& s : o.storage) insert(s);
    }
    SyntaxWordSet& operator=(const SyntaxWordSet& o) {
        if (this != &o) {
            lookup.clear();
            storage.clear();
            for (const auto& s : o.storage) insert(s);
        }
        return *this;
    }
    SyntaxWordSet(SyntaxWordSet&& o) noexcept {
        for (const auto& s : o.storage) insert(s);
    }
    SyntaxWordSet& operator=(SyntaxWordSet&& o) noexcept {
        if (this != &o) {
            lookup.clear();
            storage.clear();
            for (const auto& s : o.storage) insert(s);
        }
        return *this;
    }
    void insert(const std::string& s)  noexcept{
        storage.push_back(s);
        lookup.insert(std::string_view(storage.back()));
    }
    bool contains(std::string_view sv) const noexcept {
        return lookup.find(sv) != lookup.end();
    }
};
enum class SyntaxRuleKind : uint8_t {
    GenericRegex = 0,
    NumberCpp,
    NumberCs,
    NumberPy,
    NumberBasic,
    NumberRust,
    NumberJava,
    NumberKotlin,
    NumberSimple,
    Url,
    Email,
    CppInclude,
    CppDirective
};
struct SyntaxRuleMeta {
    SyntaxRuleKind kind = SyntaxRuleKind::GenericRegex;
    char reqChar1 = 0;
    char reqChar2 = 0;
};
struct SyntaxDef {
    bool active = false;
    SyntaxWordSet keywords;
    SyntaxWordSet literals;
    std::string commentLine;
    std::string commentBlockStart;
    std::string commentBlockEnd;
    std::string lineComment;
    std::string blockCommentStart;
    std::string blockCommentEnd;
    bool disableStrings = false;
    std::vector<std::pair<std::regex, SyntaxColor>> regexes;
    std::vector<SyntaxRuleMeta> ruleMetas;
};
static inline SyntaxRuleMeta classifySyntaxRegex(const std::string& pat) noexcept {
    SyntaxRuleMeta m;
    if (pat == R"(#[ \t]*include[ \t]*[<"][^>"]+[>"])") { m.kind = SyntaxRuleKind::CppInclude; m.reqChar1 = '#'; return m; }
    if (pat == R"(#[ \t]*[a-zA-Z0-9_]+)") { m.kind = SyntaxRuleKind::CppDirective; m.reqChar1 = '#'; return m; }
    if (pat == R"(\b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLuU]*))\b)") { m.kind = SyntaxRuleKind::NumberCpp; return m; }
    if (pat == R"(\b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLmUdD]*))\b)") { m.kind = SyntaxRuleKind::NumberCs; return m; }
    if (pat == R"(\b(0x[0-9a-fA-F]+|0b[01]+|0o[0-7]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?(j)?)\b)") { m.kind = SyntaxRuleKind::NumberPy; return m; }
    if (pat == R"(\b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b)" || pat == R"(\b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b)") { m.kind = SyntaxRuleKind::NumberBasic; return m; }
    if (pat == R"(\b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b)") { m.kind = SyntaxRuleKind::NumberRust; return m; }
    if (pat == R"(\b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLdD]*))\b)") { m.kind = SyntaxRuleKind::NumberJava; return m; }
    if (pat == R"(\b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlL]*))\b)") { m.kind = SyntaxRuleKind::NumberKotlin; return m; }
    if (pat == R"(\b([0-9]+(\.[0-9]+)?)\b)") { m.kind = SyntaxRuleKind::NumberSimple; return m; }
    if (pat.find("(https?|ftp)://") == 0) { m.kind = SyntaxRuleKind::Url; m.reqChar1 = ':'; m.reqChar2 = '/'; return m; }
    if (pat.find("@[a-zA-Z0-9.-]+") != std::string::npos) { m.kind = SyntaxRuleKind::Email; m.reqChar1 = '@'; return m; }
    if (!pat.empty()) {
        if (pat[0] == '@' || pat[0] == '$' || pat[0] == '%' || pat[0] == '#' || pat[0] == ':' || pat[0] == '`' || pat[0] == '"' || pat[0] == '=') m.reqChar1 = pat[0];
        else if (pat.find("Windows ") == 0) m.reqChar1 = 'W';
        else if (pat.find("</?") == 0) m.reqChar1 = '<';
        else if (pat.find("&[#") == 0) m.reqChar1 = '&';
        else if (pat.find(R"(\B#)") == 0) m.reqChar1 = '#';
        else if (pat.find(R"(\\[a-zA-Z])") == 0) m.reqChar1 = '\\';
        else if (pat.find(R"(\{)") == 0) m.reqChar1 = '{';
        else if (pat.find(R"(\$)") == 0) m.reqChar1 = '$';
        else if (pat.find(R"(\[<)") == 0) { m.reqChar1 = '['; m.reqChar2 = '<'; }
        else if (pat.find(R"(\.[a-zA-Z_])") == 0) m.reqChar1 = '.';
        else if (pat.find("--[") == 0) m.reqChar1 = '-';
        else if (pat.find("(?:^|\\n)[ \\t]*\\[") == 0) { m.reqChar1 = '['; m.reqChar2 = ']'; }
        else if (pat.find("(?:^|\\n)\\+") == 0) m.reqChar1 = '+';
        else if (pat.find("(?:^|\\n)-") == 0) m.reqChar1 = '-';
        else if (pat.find("(?:^|\\n)@@") == 0) m.reqChar1 = '@';
        else if (pat.find("(?=[ \\t]*=)") != std::string::npos) m.reqChar1 = '=';
        else if (pat.find("(?=[ \\t]*:") != std::string::npos) m.reqChar1 = ':';
    }
    return m;
}
static inline bool isSafeUserRegex(const std::string& q) noexcept {
    if (q.empty() || q.size() > 1024) return false;
    int parenDepth = 0;
    bool groupHasQuantifier[64] = {};
    bool inBracket = false;
    for (size_t i = 0; i < q.size(); ++i) {
        char c = q[i];
        if (c == '\\') { i++; continue; }
        if (inBracket) {
            if (c == ']') inBracket = false;
            continue;
        }
        if (c == '[') { inBracket = true; continue; }
        if (c == '(') {
            parenDepth++;
            if (parenDepth >= 32) return false;
            groupHasQuantifier[parenDepth] = false;
        } else if (c == ')') {
            if (parenDepth <= 0) return false;
            bool hadQ = groupHasQuantifier[parenDepth];
            parenDepth--;
            if (parenDepth > 0 && hadQ) groupHasQuantifier[parenDepth] = true;
            if (i + 1 < q.size()) {
                char next = q[i + 1];
                if ((next == '+' || next == '*' || next == '{') && hadQ) {
                    return false; // Reject nested repeating quantifier ReDoS (e.g. (a+)+, (.*)*)
                }
            }
        } else if (c == '+' || c == '*' || c == '{') {
            if (parenDepth > 0) groupHasQuantifier[parenDepth] = true;
        }
    }
    return parenDepth == 0 && !inBracket;
}


inline std::wstring extractSyntaxExtensionFromPath(const std::wstring& targetPath) {
    std::wstring ext = L"";
    size_t slashPos = targetPath.find_last_of(L"\\/");
    std::wstring baseName = (slashPos == std::wstring::npos) ? targetPath : targetPath.substr(slashPos + 1);
    std::wstring lowerBase = baseName;
    for (auto& c : lowerBase) c = std::towlower(c);
    if (lowerBase == L"dockerfile" || lowerBase == L"containerfile" || lowerBase.rfind(L"dockerfile.", 0) == 0 || lowerBase.rfind(L"containerfile.", 0) == 0) ext = L"dockerfile";
    else if (lowerBase == L"makefile" || lowerBase == L"gnumakefile") ext = L"makefile";
    else if (lowerBase == L"cmakelists.txt") ext = L"cmake";
    else if (lowerBase == L".env" || lowerBase.rfind(L".env.", 0) == 0) ext = L"env";
    else if (lowerBase == L".bashrc" || lowerBase == L".bash_profile" || lowerBase == L".zshrc" || lowerBase == L".profile") ext = L"sh";
    else {
        size_t dotPos = baseName.find_last_of(L".");
        if (dotPos != std::wstring::npos) ext = baseName.substr(dotPos + 1);
    }
    return ext;
}

inline std::wstring extractSyntaxExtensionFromUtf8Path(const std::string& utf8Path) {
    std::wstring wpath;
    wpath.reserve(utf8Path.size());
    for (unsigned char c : utf8Path) wpath.push_back((wchar_t)c);
    return extractSyntaxExtensionFromPath(wpath);
}

inline std::string extractSyntaxExtensionFromPath(const std::string& utf8Path) {
    std::wstring wext = extractSyntaxExtensionFromUtf8Path(utf8Path);
    std::string ext;
    ext.reserve(wext.size() + 1);
    if (!wext.empty() && wext[0] != L'.') ext.push_back('.');
    for (wchar_t wc : wext) ext.push_back((char)std::towlower(wc));
    return ext;
}

inline void loadSyntaxDefForExtension(const std::wstring& ext, SyntaxDef& outSyntax, std::wstring& outLowerExt) {
        static constexpr const char* kSyntax0 = R"([cpp]
keyword: int float double return if else for while break class struct static virtual void bool const auto namespace using template typename public private protected new delete goto unsigned char short long typedef sizeof switch case default constexpr noexcept inline explicit friend mutable catch try throw operator decltype alignas alignof static_cast dynamic_cast reinterpret_cast const_cast typeid union enum final override thread_local DWORD HWND HRESULT HANDLE HINSTANCE HICON HCURSOR HBRUSH HKEY HDC HGLRC LRESULT WPARAM LPARAM WNDCLASS WNDCLASSEX MSG PAINTSTRUCT RECT FILETIME SYSTEMTIME WORD BYTE UINT ULONG LONG INT BOOL BOOLEAN TCHAR WCHAR LPSTR LPCSTR LPWSTR LPCWSTR LPTSTR LPCTSTR LPVOID PVOID std vector string wstring map set unordered_map unordered_set list deque stringstream wstringstream regex sregex_iterator
literal: true false nullptr TRUE FALSE NULL INFINITE INVALID_HANDLE_VALUE
comment_line: //
comment_block: /* */
regex: #[ \t]*include[ \t]*[<"][^>"]+[>"]
regex_color: 0.6 0.4 0.6 1.0
regex: #[ \t]*[a-zA-Z0-9_]+
regex_color: 0.6 0.4 0.6 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLuU]*))\b
regex_color: 0.4 0.7 0.6 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}\b
regex_color: 0.8 0.4 0.2 1.0
)";
        static constexpr const char* kSyntax1 = R"([cs]
keyword: abstract as base bool break byte case catch char checked class const continue decimal default delegate do double else enum event explicit extern false finally fixed float for foreach goto if implicit in int interface internal is lock long namespace new null object operator out override params private protected public readonly ref return sbyte sealed short sizeof stackalloc static string struct switch this throw true try typeof uint ulong unchecked unsafe ushort using virtual void volatile while async await var yield nameof record init get set
literal: true false null
comment_line: //
comment_block: /* */
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLmUdD]*))\b
regex_color: 0.4 0.7 0.6 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}\b
regex_color: 0.8 0.4 0.2 1.0
)";
        static constexpr const char* kSyntax2 = R"([python]
keyword: and as assert break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass print raise return try while with yield async await match case type
literal: True False None
comment_line: #
comment_block: """ """
regex: \b(0x[0-9a-fA-F]+|0b[01]+|0o[0-7]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?(j)?)\b
regex_color: 0.4 0.7 0.6 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}\b
regex_color: 0.8 0.4 0.2 1.0
)";
        static constexpr const char* kSyntax3 = R"([js]
keyword: break case catch class const continue debugger default delete do else export extends finally for function if import in instanceof new return super switch this throw try typeof var void while with yield let static enum implements package protected interface private public async await type namespace declare module as any unknown never boolean number string symbol get set require exports
literal: true false null undefined NaN Infinity
comment_line: //
comment_block: /* */
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}\b
regex_color: 0.8 0.4 0.2 1.0
)";
        static constexpr const char* kSyntax4 = R"([json]
literal: true false null
comment_line: //
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}\b
regex_color: 0.8 0.4 0.2 1.0
)";
        static constexpr const char* kSyntax5 = R"([html]
comment_block: <!-- -->
regex: </?[a-zA-Z0-9\-:]+>?
regex_color: 0.3 0.6 0.8 1.0
regex: \b[a-zA-Z0-9\-:]+(?=\s*=)
regex_color: 0.8 0.5 0.3 1.0
regex: &[#a-zA-Z0-9]+;
regex_color: 0.6 0.4 0.7 1.0
)";
        static constexpr const char* kSyntax6 = R"([css]
comment_block: /* */
regex: \b[a-zA-Z\-]+(?=\s*:)
regex_color: 0.3 0.6 0.8 1.0
regex: \B#[0-9a-fA-F]{3,6}\b
regex_color: 0.6 0.4 0.7 1.0
regex: \b(px|em|rem|vh|vw|%)\b
regex_color: 0.8 0.5 0.3 1.0
regex: (\.|#|:)[a-zA-Z_\-]+
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax7 = R"([markdown]
comment_block: <!-- -->
regex: (?:^|\n)[ \t]*#{1,6}[ \t]+[^\r\n]*
regex_color: 0.3 0.7 0.9 1.0
regex: (\*\*\*|___)[^\*\_\r\n\\]+(\*\*\*|___)|(\*\*|__)(?:[^\*\_\r\n\\]|\\.|[*][^*\r\n]|_[^_\r\n])+(\*\*|__)
regex_color: 0.9 0.5 0.3 1.0
regex: (?:^|\n)[ \t]*`{3,}[^\r\n]*\r?\n(?:[^\r\n]*\r?\n){0,120}?[ \t]*`{3,}[ \t]*(?=\r|\n|$)|(?:^|\n)[ \t]*~{3,}[^\r\n]*\r?\n(?:[^\r\n]*\r?\n){0,120}?[ \t]*~{3,}[ \t]*(?=\r|\n|$)|(?:^|\n)[ \t]*(?:`{3,}|~{3,})[^\r\n]*|``+(?:[^`\r\n]|`[^`\r\n])+``+|`[^`\r\n\\]*(?:\\.[^`\r\n\\]*)*`
regex_color: 0.5 0.7 0.5 1.0
regex: (https?|ftp)://[^\s<>")\]]+
regex_color: 0.2 0.6 0.8 1.0
)";
        static constexpr const char* kSyntax8 = R"([rust]
keyword: as break const continue crate else enum extern fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait type unsafe use where while async await dyn abstract become box do final macro override priv typeof unsized virtual yield
literal: true false Option Result Some None Ok Err
comment_line: //
comment_block: /* */
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
regex: #[ \t]*\[.*?\]
regex_color: 0.6 0.4 0.7 1.0
regex: \b[a-zA-Z_][a-zA-Z0-9_]*!
regex_color: 0.3 0.6 0.8 1.0
)";
        static constexpr const char* kSyntax9 = R"([go]
keyword: break default func interface select case defer go map struct chan else goto package switch const fallthrough if range type continue for import return var
literal: true false iota nil make new len cap append close delete panic recover
comment_line: //
comment_block: /* */
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax10 = R"([java]
keyword: abstract assert boolean break byte case catch char class const continue default do double else enum extends final finally float for goto if implements import instanceof int interface long native new package private protected public return short static strictfp super switch synchronized this throw throws transient try void volatile while var yield record sealed permits non-sealed
literal: true false null
comment_line: //
comment_block: /* */
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLdD]*))\b
regex_color: 0.4 0.7 0.6 1.0
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
)";
        static constexpr const char* kSyntax11 = R"([powershell]
keyword: begin break catch continue data do dynamicparam else elseif end exit filter finally for foreach from function if in inlineScript parallel param process return sequence switch throw trap try until while workflow
literal: $true $false $null
comment_line: #
comment_block: <# #>
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
regex: \$[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.8 0.5 0.3 1.0
regex: -[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.4 0.7 0.9 1.0
)";
        static constexpr const char* kSyntax12 = R"([batch]
comment_line: REM
keyword: call cd chdir choice cls color copy date del dir echo endlocal erase exit find findstr for goto if md mkdir move path pause popd prompt pushd rd rem ren rename rmdir set setlocal shift start time title type ver verify vol CALL CD CHDIR CHOICE CLS COLOR COPY DATE DEL DIR ECHO ENDLOCAL ERASE EXIT FIND FINDSTR FOR GOTO IF MD MKDIR MOVE PATH PAUSE POPD PROMPT PUSHD RD REM REN RENAME RMDIR SET SETLOCAL SHIFT START TIME TITLE TYPE VER VERIFY VOL
regex: (?:^|\n)[ \t]*([rR][eE][mM]\b|::)[^\r\n]*
regex_color: 0.38 0.62 0.38 1.0
regex: %[a-zA-Z0-9_]+%
regex_color: 0.8 0.5 0.3 1.0
regex: (?:^|\n)[ \t]*:[a-zA-Z0-9_]+
regex_color: 0.6 0.4 0.7 1.0
)";
        static constexpr const char* kSyntax13 = R"([bash]
keyword: if then else elif fi case esac for select while until do done in function time coproc source alias export read local
literal: true false
comment_line: #
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
regex: \$[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.8 0.5 0.3 1.0
regex: \$\{[^\}]+\}
regex_color: 0.8 0.5 0.3 1.0
)";
        static constexpr const char* kSyntax14 = R"([sql]
comment_line: --
comment_block: /* */
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
regex: \b(SELECT|INSERT|UPDATE|DELETE|FROM|WHERE|JOIN|LEFT|RIGHT|INNER|OUTER|ON|GROUP|BY|ORDER|HAVING|LIMIT|OFFSET|AS|IN|AND|OR|NOT|IS|NULL|CREATE|ALTER|DROP|TABLE|INDEX|VIEW|PRIMARY|KEY|FOREIGN|REFERENCES|DEFAULT|UNIQUE)\b
regex_color: 0.3 0.6 0.8 1.0
regex: \b(select|insert|update|delete|from|where|join|left|right|inner|outer|on|group|by|order|having|limit|offset|as|in|and|or|not|is|null|create|alter|drop|table|index|view|primary|key|foreign|references|default|unique)\b
regex_color: 0.3 0.6 0.8 1.0
)";
        static constexpr const char* kSyntax15 = R"([php]
keyword: echo print if else elseif for foreach while do switch case break continue return include require function class public private protected static try catch throw new extends implements trait namespace use yield global
literal: true false null TRUE FALSE NULL
comment_line: //
comment_block: /* */
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
regex: \$[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*
regex_color: 0.8 0.5 0.3 1.0
)";
        static constexpr const char* kSyntax16 = R"([ruby]
keyword: def end class module if unless else elsif case when while until for do yield return break next redo retry rescue ensure super alias undef in include require
literal: true false nil self
comment_line: #
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax17 = R"([swift]
keyword: func var let class struct enum protocol extension init deinit return if else switch case for in while do catch throw try await async guard defer fileprivate private open public internal
literal: true false nil
comment_line: //
comment_block: /* */
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax18 = R"([kotlin]
keyword: fun val var class interface object package import return if else when for while do break continue try catch finally throw in is as typealias
literal: true false null
comment_line: //
comment_block: /* */
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlL]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax19 = R"([lua]
keyword: and break do else elseif end false for function if in local nil not or repeat return then true until while
literal: true false nil
comment_line: --
comment_block: --[[ ]]
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax20 = R"([ini]
comment_line: ;
regex: (?:^|\n)[ \t]*\[[^\r\n\]]+\]
regex_color: 0.3 0.65 0.9 1.0
regex: (?:^|\n)[ \t]*[a-zA-Z0-9_.\-]+(?=[ \t]*=)
regex_color: 0.8 0.5 0.3 1.0
)";
        static constexpr const char* kSyntax21 = R"([yaml]
comment_line: #
regex: (?:^|\n)[ \t]*[a-zA-Z0-9_.\-]+:
regex_color: 0.3 0.6 0.8 1.0
regex: \b(true|false|null|yes|no|on|off)\b
regex_color: 0.8 0.4 0.2 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax22 = R"([latex]
comment_line: %
regex: \\[a-zA-Z]+
regex_color: 0.3 0.6 0.8 1.0
regex: \{.*?\}
regex_color: 0.8 0.5 0.3 1.0
regex: \$.*?\$
regex_color: 0.6 0.4 0.7 1.0
)";
        static constexpr const char* kSyntax23 = R"([default]
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}\b
regex_color: 0.8 0.4 0.2 1.0
)";
        static constexpr const char* kSyntax24 = R"([mermaid]
keyword: flowchart flowchart-elk graph sequenceDiagram classDiagram stateDiagram stateDiagram-v2 erDiagram gantt pie gitGraph mindmap timeline journey xychart-beta xychart quadrantChart sankey-beta sankey block-beta block packet-beta packet kanban architecture-beta architecture C4Context C4Container C4Component C4Dynamic C4Deployment radar-beta radar treemap-beta treemap venn-beta venn ishikawa-beta ishikawa wardley-beta wardley cynefin-beta cynefin railroad-beta railroad swimlane-beta swimlane usecase-beta usecase agentflow-beta agentflow eventmodeling treeView-beta treeView requirementDiagram requirement subgraph end participant actor loop alt else opt par and critical break rect note over left right of activate deactivate autonumber title section dateFormat axisFormat click style classDef class linkStyle direction TB TD BT RL LR showData
comment_line: %%
regex: (?:<-->|<-\.->|<==>|-->|---|-\.->|-\.-|==>|===|--x|--o|o--o|x--x|->>|-->>|-x|-\)|--\)|:::)
regex_color: 0.9 0.5 0.3 1.0
regex: \b([0-9]+(\.[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax25 = R"([dart]
keyword: abstract as assert async await base break case catch class const continue covariant default deferred do dynamic else enum export extends extension external factory final finally for Function get hide if implements import in interface is late library mixin new on operator part required rethrow return sealed set show static super switch sync this throw try typedef var void when while with yield int double num bool String List Map Set Future Stream Iterable Object Never Widget BuildContext StatelessWidget StatefulWidget State Key
literal: true false null
comment_line: //
comment_block: /* */
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax26 = R"([r]
keyword: if else repeat while function for in next break return library require source setwd getwd c list matrix array factor vector character numeric integer logical complex raw summary plot print cat paste paste0 ifelse switch try tryCatch stop warning message invisible lapply sapply vapply tapply mapply apply subset merge aggregate reshape
literal: TRUE FALSE NULL NA NA_integer_ NA_real_ NA_complex_ NA_character_ Inf NaN T F
comment_line: #
regex: (?:<-|<<-|->|->>|%>%|\|>)
regex_color: 0.8 0.5 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax27 = R"([zig]
keyword: addrspace align allowzero and anyframe anytype asm async await break callconv catch comptime const continue defer else enum errdefer error export extern fn for if inline noalias noinline nosuspend opaque or orelse packed pub resume return linksection struct suspend switch test threadlocal try union unreachable usingnamespace var volatile while u8 u16 u32 u64 u128 usize i8 i16 i32 i64 i128 isize f16 f32 f64 f80 f128 bool anyerror void noreturn type comptime_int comptime_float c_short c_ushort c_int c_uint c_long c_ulong c_longlong c_ulonglong c_longdouble
literal: true false null undefined
comment_line: //
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax28 = R"([scala]
keyword: abstract case catch class def do else enum export extends final finally for forSome given if implicit import lazy match new object override package private protected return sealed super then throw trait try type val var while with yield Any AnyRef AnyVal Nothing Null Unit Boolean Byte Short Int Long Float Double Char String Seq List Vector Map Set Option Either Future Try
literal: true false null None Some Left Right Success Failure Nil
comment_line: //
comment_block: /* */
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLdD]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax29 = R"([perl]
keyword: my our local state sub use require package no if elsif else unless while until for foreach do next last redo goto return eval BEGIN END CHECK INIT UNITCHECK given when default say print printf chomp chop split join map grep sort reverse keys values each exists delete defined undef blessed ref die warn open close read write seek tell binmode opendir readdir closedir mkdir rmdir unlink rename chmod chown system exec fork wait waitpid shift unshift push pop splice bless tie untie tied
literal: undef __FILE__ __LINE__ __PACKAGE__ __SUB__ __END__ __DATA__
comment_line: #
regex: \$[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.8 0.5 0.3 1.0
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.8 0.5 0.3 1.0
regex: %[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.8 0.5 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|0b[01]+|0o[0-7]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?(j)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax30 = R"([vb]
keyword: AddHandler AddressOf Alias And AndAlso As Boolean ByRef Byte ByVal Call Case Catch CBool CByte CChar CDate CDbl CDec Char CInt Class CLng CObj Const Continue CSByte CShort CSng CStr CType CUInt CULng CUShort Date Decimal Declare Default Delegate Dim DirectCast Do Double Each Else ElseIf End EndIf Enum Erase Error Event Exit Explicit False Finally For Friend Function Get GetType Global GoSub GoTo Handles If Implements Imports In Inherits Integer Interface Is IsNot Let Lib Like Long Loop Me Mod Module MustInherit MustOverride MyBase MyClass Namespace Narrowing New Next Not Nothing NotInheritable NotOverridable Object Of On Operator Option Optional Or OrElse Overloads Overridable Overrides ParamArray Partial Private Property Protected Public RaiseEvent ReadOnly ReDim Preserve Rem RemoveHandler Resume Return SByte Select Set Shadows Shared Short Single Static Step Stop String Structure Sub SyncLock Then Throw To True Try TryCast TypeOf UInteger ULong UShort Using Variant Wend When While Widening With WithEvents WriteOnly Xor Async Await Iterator Yield dim sub function end if then else elseif for each in next while wend do loop select case set let nothing true false call option explicit on error resume goto
literal: True False Nothing Null Empty Me MyBase MyClass true false nothing null empty
comment_line: '
regex: #[ \t]*[a-zA-Z0-9_]+
regex_color: 0.6 0.4 0.7 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax31 = R"([fsharp]
keyword: abstract and as assert base begin class default delegate do done downcast downto elif else end exception extern false finally fixed for fun function global if in inherit inline interface internal lazy let match member module mutable namespace new not null of open or override private public rec return select static struct then to true try type upcast use val void when while with yield async seq task int float bool string unit list array option result
literal: true false null None Some Ok Error
comment_line: //
comment_block: (* *)
regex: \[<[^>]+>\]
regex_color: 0.6 0.4 0.7 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLmUdD]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax32 = R"([julia]
keyword: abstract baremodule begin break catch const continue do else elseif end export false finally for function global if import in isa let local macro module mutable outer primitive quote return struct true try type using where while Int Int8 Int16 Int32 Int64 Int128 UInt UInt8 UInt16 UInt32 UInt64 UInt128 Float16 Float32 Float64 Bool Char String Array Vector Matrix Dict Set Tuple NamedTuple Symbol Any Nothing Missing Number Real Integer AbstractFloat Function Type
literal: true false nothing missing undef Inf Inf16 Inf32 Inf64 NaN NaN16 NaN32 NaN64 pi im
comment_line: #
comment_block: #= =#
regex: @[a-zA-Z_][a-zA-Z0-9_!]*
regex_color: 0.6 0.4 0.7 1.0
regex: :[a-zA-Z_][a-zA-Z0-9_!]*
regex_color: 0.8 0.5 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax33 = R"([haskell]
keyword: as case class data default deriving do else family forall foreign hiding if import in infix infixl infixr instance let mdo module newtype of proc qualified rec then type where Int Integer Float Double Bool Char String Maybe Either IO Monad Functor Applicative Foldable Traversable Eq Ord Show Read Num Enum Bounded Monoid Semigroup
literal: True False Nothing Just Left Right EQ LT GT otherwise
comment_line: --
comment_block: {- -}
regex: \{-#.*?#-\}
regex_color: 0.6 0.4 0.7 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax34 = R"([elixir]
keyword: def defp defmodule defprotocol defimpl defmacro defmacrop defstruct defdelegate defexception defguard defguardp defoverridable do end fn if else unless case cond with for in receive after try catch rescue raise throw import require use alias quote unquote super when and or not
literal: true false nil
comment_line: #
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
regex: :[a-zA-Z_][a-zA-Z0-9_?!]*
regex_color: 0.8 0.5 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax35 = R"([dockerfile]
keyword: FROM AS RUN CMD LABEL MAINTAINER EXPOSE ENV ADD COPY ENTRYPOINT VOLUME USER WORKDIR ARG ONBUILD STOPSIGNAL HEALTHCHECK SHELL from as run cmd label maintainer expose env add copy entrypoint volume user workdir arg onbuild stopsignal healthcheck shell
literal: true false NONE
comment_line: #
regex: --[a-zA-Z0-9_\-]+(?:=[^\s]+)?
regex_color: 0.6 0.4 0.7 1.0
regex: \$\{?[a-zA-Z_][a-zA-Z0-9_]*\}?
regex_color: 0.8 0.5 0.3 1.0
regex: \b([0-9]+(\.[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax36 = R"([terraform]
keyword: resource data variable output locals module provider terraform moved import check for in if dynamic content each self var local path count string number bool list set map object tuple any
literal: true false null
comment_line: #
comment_block: /* */
regex: (?:^|\n)[ \t]*[a-zA-Z0-9_\-]+(?=[ \t]*=)
regex_color: 0.3 0.65 0.9 1.0
regex: \$\{[^\}]+\}
regex_color: 0.8 0.5 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax37 = R"([toml]
literal: true false TRUE FALSE inf nan
comment_line: #
regex: (?:^|\n)[ \t]*\[\[(?:[^\r\n\]]+)\]\]|(?:^|\n)[ \t]*\[(?:[^\r\n\]]+)\]
regex_color: 0.3 0.65 0.9 1.0
regex: (?:^|\n)[ \t]*[a-zA-Z0-9_.\-]+(?=[ \t]*=)
regex_color: 0.8 0.5 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax38 = R"([graphql]
keyword: query mutation subscription fragment on type interface union enum input extend schema scalar directive implements repeatable Int Float String Boolean ID QUERY MUTATION SUBSCRIPTION FIELD FRAGMENT_DEFINITION FRAGMENT_SPREAD INLINE_FRAGMENT VARIABLE_DEFINITION SCHEMA SCALAR OBJECT FIELD_DEFINITION ARGUMENT_DEFINITION INTERFACE UNION ENUM ENUM_VALUE INPUT_OBJECT INPUT_FIELD_DEFINITION
literal: true false null
comment_line: #
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
regex: \$[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.8 0.5 0.3 1.0
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax39 = R"([proto]
keyword: syntax edition import weak public package option inf message enum service rpc returns stream oneof map reserved extensions to max repeated optional required group double float int32 int64 uint32 uint64 sint32 sint64 fixed32 fixed64 sfixed32 sfixed64 bool string bytes
literal: true false
comment_line: //
comment_block: /* */
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax40 = R"([cmake]
keyword: cmake_minimum_required project add_executable add_library target_link_libraries target_include_directories target_compile_definitions target_compile_options target_sources set unset option if elseif else endif foreach endforeach while endwhile function endfunction macro endmacro return break continue include find_package find_library find_path find_program find_file file message install configure_file add_custom_command add_custom_target add_subdirectory add_dependencies add_definitions add_compile_definitions add_compile_options enable_testing add_test set_target_properties set_property get_target_property list string math execute_process FetchContent_Declare FetchContent_MakeAvailable PUBLIC PRIVATE INTERFACE REQUIRED QUIET COMPONENTS CONFIG MODULE SHARED STATIC OBJECT IMPORTED ALIAS WIN32 MACOSX_BUNDLE EXCLUDE_FROM_ALL DESTINATION FILES TARGETS DIRECTORY PROGRAMS ARCHIVE LIBRARY RUNTIME
literal: TRUE FALSE ON OFF YES NO NOT AND OR EQUAL LESS GREATER STRLESS STRGREATER STREQUAL MATCHES EXISTS IS_DIRECTORY DEFINED TARGET COMMAND POLICY VERSION_LESS VERSION_GREATER VERSION_EQUAL
comment_line: #
comment_block: #[[ ]]
regex: \$\{[^\}]+\}
regex_color: 0.8 0.5 0.3 1.0
regex: \$<[^>]+>
regex_color: 0.6 0.4 0.7 1.0
regex: \b([0-9]+(\.[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax41 = R"([makefile]
keyword: include sinclude ifeq ifneq ifdef ifndef else endif define endef export unexport override private vpath subst patsubst strip findstring filter filter-out sort word wordlist words firstword lastword dir notdir suffix basename addprefix addsuffix join wildcard realpath abspath error warning info shell origin flavor foreach if or and call eval file value PHONY DEFAULT PRECIOUS INTERMEDIATE SECONDARY SECONDEXPANSION DELETE_ON_ERROR IGNORE LOW_RESOLUTION_TIME SILENT EXPORT_ALL_VARIABLES NOTPARALLEL ONESHELL POSIX
comment_line: #
regex: (?:^|\n)[a-zA-Z0-9_.\-/]+(?=[ \t]*:(?!=))
regex_color: 0.3 0.65 0.9 1.0
regex: \$(?:\([^\)]+\)|\{[^\}]+\}|[@%<?^+|*])
regex_color: 0.8 0.5 0.3 1.0
)";
        static constexpr const char* kSyntax42 = R"([reg]
keyword: REGEDIT4
comment_line: ;
regex: Windows Registry Editor Version [0-9.]+
regex_color: 0.6 0.4 0.7 1.0
regex: (?:^|\n)[ \t]*\[-?HKEY_[A-Z_]+[^\r\n\]]*\]
regex_color: 0.3 0.65 0.9 1.0
regex: =(?:dword|qword|hex(?:\([0-9a-fA-F]+\))?):[0-9a-fA-F,\\\t ]*
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax43 = R"([shader]
keyword: cbuffer tbuffer struct class interface technique technique10 technique11 pass compile return if else for while do switch case default break continue discard in out inout uniform attribute varying buffer shared coherent volatile restrict readonly writeonly layout flat smooth noperspective centroid sample patch precise invariant highp mediump lowp const static extern inline register packoffset fn var let override workgroup storage private function void bool int uint dword half float double min16float min16int min16uint float2 float3 float4 int2 int3 int4 uint2 uint3 uint4 bool2 bool3 bool4 half2 half3 half4 double2 double3 double4 float2x2 float3x3 float4x4 matrix vector vec2 vec3 vec4 ivec2 ivec3 ivec4 uvec2 uvec3 uvec4 bvec2 bvec3 bvec4 dvec2 dvec3 dvec4 mat2 mat3 mat4 vec2f vec3f vec4f vec2i vec3i vec4i vec2u vec3u vec4u mat2x2f mat3x3f mat4x4f f32 i32 u32 Texture1D Texture2D Texture3D TextureCube Texture2DArray RWTexture1D RWTexture2D RWTexture3D StructuredBuffer RWStructuredBuffer ByteAddressBuffer RWByteAddressBuffer SamplerState SamplerComparisonState sampler1D sampler2D sampler3D samplerCube image2D
literal: true false SV_Position SV_Target SV_Target0 SV_Target1 SV_Target2 SV_Target3 SV_Depth SV_VertexID SV_InstanceID SV_DispatchThreadID SV_GroupID SV_GroupThreadID SV_GroupIndex POSITION NORMAL TEXCOORD0 TEXCOORD1 TEXCOORD2 COLOR0 gl_Position gl_FragCoord gl_FragDepth gl_VertexID gl_InstanceID gl_GlobalInvocationID gl_LocalInvocationID gl_WorkGroupID
comment_line: //
comment_block: /* */
regex: #[ \t]*[a-zA-Z0-9_]+
regex_color: 0.6 0.4 0.7 1.0
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLuU]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax44 = R"([asm]
keyword: mov movzx movsx lea push pop call ret jmp je jz jne jnz jg jge jl jle ja jae jb jbe jc jnc jo jno js jns add sub mul imul div idiv inc dec neg and or xor not shl shr sal sar rol ror cmp test nop int syscall sysenter sysexit cpuid rdtsc cld std cli sti rep repe repz repne repnz cmpsb movsb stosb lodsb scasb xchg cmpxchg lock wait hlt leave enter cbw cwd cdq cqo movaps movups movdqa movdqu movss movsd addps addpd subps subpd mulps mulpd divps divpd xorps xorpd pxor por pand paddd psubd vmovaps vmovups vaddps vmulps vfma231ps ldr str ldp stp bl br b cbz cbnz adr adrp mrs msr svc db dw dd dq dt resb resw resd resq equ section segment global extern PUBLIC EXTRN PROC ENDP SEGMENT ENDS ASSUME MACRO ENDM ALIGN ORG BITS USE16 USE32 USE64 BYTE WORD DWORD QWORD XMMWORD YMMWORD ZMMWORD PTR OFFSET FLAT SHORT NEAR FAR
literal: rax rbx rcx rdx rsi rdi rbp rsp r8 r9 r10 r11 r12 r13 r14 r15 rip eax ebx ecx edx esi edi ebp esp r8d r9d r10d r11d r12d r13d r14d r15d ax bx cx dx si di bp sp al bl cl dl ah bh ch dh sil dil bpl spl xmm0 xmm1 xmm2 xmm3 xmm4 xmm5 xmm6 xmm7 xmm8 xmm9 xmm10 xmm11 xmm12 xmm13 xmm14 xmm15 ymm0 ymm1 ymm2 ymm3 ymm4 ymm5 ymm6 ymm7 zmm0 zmm1 cs ds es fs gs ss x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 x10 x11 x12 x13 x14 x15 x16 x17 x18 x19 x20 x21 x22 x23 x24 x25 x26 x27 x28 x29 x30 lr pc w0 w1 w2 w3 w4 w5 w6 w7
comment_line: ;
regex: (?:^|\n)[ \t]*[a-zA-Z_.][a-zA-Z0-9_.$@]*(?=[ \t]*:)
regex_color: 0.6 0.4 0.7 1.0
regex: \.[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.3 0.65 0.9 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax45 = R"([verilog]
keyword: module endmodule input output inout wire reg logic bit byte shortint int longint integer time real shortreal realtime string event chandle enum struct union typedef packed signed unsigned always always_comb always_ff always_latch initial final begin end if else case casex casez default endcase for while repeat forever do break continue return function endfunction task endtask assign deassign force release posedge negedge edge or and not nand nor xor xnor buf notif0 notif1 bufif0 bufif1 parameter localparam specparam defparam generate endgenerate genvar interface endinterface modport clocking endclocking class endclass package endpackage import export virtual extends implements super this new null local protected rand randc constraint solve before property endproperty sequence endsequence assert assume cover fork join join_any join_none disable wait
literal: highz0 highz1 strong0 strong1 pull0 pull1 weak0 weak1 supply0 supply1
comment_line: //
comment_block: /* */
regex: `[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
regex: \$[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.8 0.5 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax46 = R"([vhdl]
keyword: abs access after alias all and architecture array assert attribute begin block body buffer bus case component configuration constant disconnect downto else elsif end entity exit file for function generate generic group guarded if impure in inertial inout is label library linkage literal loop map mod nand new next nor not null of on open or others out package port postponed procedure process pure range record register reject rem report return rol ror select severity signal shared sla sll sra srl subtype then to transport type unaffected units until use variable wait when while with xnor xor std_logic std_logic_vector unsigned signed integer natural positive boolean bit bit_vector real time string std ieee work
literal: true false TRUE FALSE rising_edge falling_edge
comment_line: --
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax47 = R"([diff]
no_strings: 1
regex: (?:^|\n)@@[^\r\n]*@@[^\r\n]*
regex_color: 0.3 0.7 0.9 1.0
regex: (?:^|\n)(?:diff --git|index [0-9a-fA-F]+|\+\+\+ |--- )[^\r\n]*
regex_color: 0.7 0.5 0.85 1.0
regex: (?:^|\n)\+(?!\+\+)[^\r\n]*
regex_color: 0.35 0.75 0.45 1.0
regex: (?:^|\n)-(?!--)[^\r\n]*
regex_color: 0.9 0.4 0.4 1.0
)";
        static constexpr const char* kSyntax48 = R"([log]
no_strings: 1
keyword: INFO INFORMATION NOTICE DEBUG TRACE VERBOSE
literal: WARN WARNING DEPRECATED CAUTION
regex: "[^"\r\n]*"
regex_color: 0.8 0.55 0.35 1.0
regex: :[0-9]{2}:[0-9]{2}(?:[.,][0-9]+)?(?:Z|[+-][0-9]{2}:?[0-9]{2})?\b
regex_color: 0.4 0.7 0.65 1.0
regex: \b[0-9]{4}[-/][0-9]{2}[-/][0-9]{2}\b
regex_color: 0.4 0.7 0.65 1.0
regex: \b(?:ERROR|FATAL|CRITICAL|SEVERE|PANIC|FAIL|FAILED|EXCEPTION|ERR)\b
regex_color: 0.95 0.35 0.35 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b([0-9]+(\.[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        struct SyntaxEntry { const wchar_t* ext; const char* syntax; };
        static const SyntaxEntry syntaxTable[] = {
            { L"mmd", kSyntax24 }, { L"mermaid", kSyntax24 },
            { L"cpp", kSyntax0 }, { L"c", kSyntax0 }, { L"cc", kSyntax0 }, { L"cxx", kSyntax0 }, { L"h", kSyntax0 }, { L"hpp", kSyntax0 }, { L"hxx", kSyntax0 }, { L"hh", kSyntax0 }, { L"inl", kSyntax0 }, { L"ipp", kSyntax0 }, { L"tpp", kSyntax0 }, { L"rc", kSyntax0 }, { L"ino", kSyntax0 }, { L"cu", kSyntax0 }, { L"cuh", kSyntax0 },
            { L"cs", kSyntax1 },
            { L"py", kSyntax2 }, { L"pyw", kSyntax2 }, { L"pyi", kSyntax2 },
            { L"js", kSyntax3 }, { L"ts", kSyntax3 }, { L"jsx", kSyntax3 }, { L"tsx", kSyntax3 }, { L"mjs", kSyntax3 }, { L"cjs", kSyntax3 },
            { L"json", kSyntax4 }, { L"jsonc", kSyntax4 },
            { L"html", kSyntax5 }, { L"htm", kSyntax5 }, { L"xhtml", kSyntax5 }, { L"shtml", kSyntax5 }, { L"xml", kSyntax5 }, { L"svg", kSyntax5 }, { L"xsl", kSyntax5 }, { L"xslt", kSyntax5 }, { L"xsd", kSyntax5 }, { L"vue", kSyntax5 }, { L"svelte", kSyntax5 }, { L"astro", kSyntax5 }, { L"vcxproj", kSyntax5 }, { L"csproj", kSyntax5 }, { L"fsproj", kSyntax5 }, { L"vbproj", kSyntax5 }, { L"props", kSyntax5 }, { L"targets", kSyntax5 }, { L"wapproj", kSyntax5 }, { L"appxmanifest", kSyntax5 }, { L"resw", kSyntax5 }, { L"resx", kSyntax5 }, { L"plist", kSyntax5 }, { L"xaml", kSyntax5 }, { L"nuspec", kSyntax5 }, { L"config", kSyntax5 }, { L"manifest", kSyntax5 },
            { L"css", kSyntax6 }, { L"scss", kSyntax6 }, { L"sass", kSyntax6 }, { L"less", kSyntax6 }, { L"styl", kSyntax6 },
            { L"md", kSyntax7 }, { L"markdown", kSyntax7 }, { L"mdx", kSyntax7 },
            { L"rs", kSyntax8 }, { L"go", kSyntax9 }, { L"java", kSyntax10 },
            { L"ps1", kSyntax11 }, { L"psm1", kSyntax11 }, { L"psd1", kSyntax11 },
            { L"bat", kSyntax12 }, { L"cmd", kSyntax12 },
            { L"sh", kSyntax13 }, { L"bash", kSyntax13 }, { L"zsh", kSyntax13 },
            { L"sql", kSyntax14 }, { L"php", kSyntax15 }, { L"rb", kSyntax16 }, { L"ruby", kSyntax16 }, { L"swift", kSyntax17 },
            { L"kt", kSyntax18 }, { L"kts", kSyntax18 }, { L"kotlin", kSyntax18 },
            { L"lua", kSyntax19 },
            { L"ini", kSyntax20 }, { L"inf", kSyntax20 }, { L"cfg", kSyntax20 },
            { L"conf", kSyntax21 }, { L"yml", kSyntax21 }, { L"yaml", kSyntax21 },
            { L"tex", kSyntax22 }, { L"latex", kSyntax22 },
            { L"dart", kSyntax25 },
            { L"r", kSyntax26 },
            { L"zig", kSyntax27 },
            { L"scala", kSyntax28 }, { L"sc", kSyntax28 },
            { L"pl", kSyntax29 }, { L"pm", kSyntax29 },
            { L"vb", kSyntax30 }, { L"vbs", kSyntax30 }, { L"bas", kSyntax30 }, { L"vba", kSyntax30 },
            { L"fs", kSyntax31 }, { L"fsi", kSyntax31 }, { L"fsx", kSyntax31 },
            { L"jl", kSyntax32 },
            { L"hs", kSyntax33 }, { L"lhs", kSyntax33 },
            { L"ex", kSyntax34 }, { L"exs", kSyntax34 },
            { L"dockerfile", kSyntax35 }, { L"containerfile", kSyntax35 },
            { L"tf", kSyntax36 }, { L"tfvars", kSyntax36 }, { L"hcl", kSyntax36 },
            { L"toml", kSyntax37 }, { L"env", kSyntax37 },
            { L"graphql", kSyntax38 }, { L"gql", kSyntax38 },
            { L"proto", kSyntax39 },
            { L"cmake", kSyntax40 }, { L"cmakelists.txt", kSyntax40 },
            { L"mk", kSyntax41 }, { L"mak", kSyntax41 }, { L"makefile", kSyntax41 }, { L"gnumakefile", kSyntax41 },
            { L"reg", kSyntax42 },
            { L"hlsl", kSyntax43 }, { L"fx", kSyntax43 }, { L"glsl", kSyntax43 }, { L"vert", kSyntax43 }, { L"frag", kSyntax43 }, { L"comp", kSyntax43 }, { L"geom", kSyntax43 }, { L"tesc", kSyntax43 }, { L"tese", kSyntax43 }, { L"wgsl", kSyntax43 },
            { L"asm", kSyntax44 }, { L"s", kSyntax44 }, { L"inc", kSyntax44 },
            { L"v", kSyntax45 }, { L"sv", kSyntax45 }, { L"svh", kSyntax45 }, { L"vh", kSyntax45 },
            { L"vhd", kSyntax46 }, { L"vhdl", kSyntax46 },
            { L"diff", kSyntax47 }, { L"patch", kSyntax47 },
            { L"log", kSyntax48 }
        };
        std::wstring lowerExt = ext;
        for (auto& c : lowerExt) c = std::towlower(c);
        if (!lowerExt.empty() && lowerExt[0] == L'.') lowerExt.erase(0, 1);
        const char* targetSyntaxPtr = kSyntax23;
        for (const auto& entry : syntaxTable) {
            if (lowerExt == entry.ext) {
                targetSyntaxPtr = entry.syntax;
                break;
            }
        }
        static std::vector<std::pair<const char*, SyntaxDef>> compiledSyntaxCache;
        for (const auto& cached : compiledSyntaxCache) {
            if (cached.first == targetSyntaxPtr) {
                outSyntax = cached.second;
                outLowerExt = lowerExt;
                return;
            }
        }
        std::string targetSyntax = targetSyntaxPtr;
        SyntaxDef parsed;
        parsed.active = true;
        std::string lastRegex;
        auto nextWord = [](const std::string& s, size_t& pos, std::string& out) -> bool {
            while (pos < s.size() && (unsigned char)s[pos] <= ' ') pos++;
            if (pos >= s.size()) return false;
            size_t start = pos;
            while (pos < s.size() && (unsigned char)s[pos] > ' ') pos++;
            out.assign(s, start, pos - start);
            return true;
        };
        auto nextFloat = [](const std::string& s, size_t& pos, float& out) -> bool {
            while (pos < s.size() && (unsigned char)s[pos] <= ' ') pos++;
            if (pos >= s.size()) return false;
            bool neg = false;
            if (s[pos] == '-' || s[pos] == '+') { neg = (s[pos] == '-'); pos++; }
            double val = 0.0;
            bool hasDigit = false;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
                val = val * 10.0 + (double)(s[pos] - '0');
                pos++;
                hasDigit = true;
            }
            if (pos < s.size() && s[pos] == '.') {
                pos++;
                double div = 10.0;
                while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
                    val += (double)(s[pos] - '0') / div;
                    div *= 10.0;
                    pos++;
                    hasDigit = true;
                }
            }
            if (!hasDigit) return false;
            out = (float)(neg ? -val : val);
            return true;
        };
        for (size_t linePos = 0; linePos < targetSyntax.size(); ) {
            size_t nl = targetSyntax.find('\n', linePos);
            std::string line = (nl == std::string::npos) ? targetSyntax.substr(linePos) : targetSyntax.substr(linePos, nl - linePos);
            linePos = (nl == std::string::npos) ? targetSyntax.size() : nl + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.find("keyword: ") == 0) {
                size_t p = 9; std::string w;
                while (nextWord(line, p, w)) parsed.keywords.insert(w);
            }
            if (line.find("literal: ") == 0) {
                size_t p = 9; std::string w;
                while (nextWord(line, p, w)) parsed.literals.insert(w);
            }
            if (line == "no_strings: 1") parsed.disableStrings = true;
            if (line.find("comment_line: ") == 0) parsed.commentLine = line.substr(14);
            if (line.find("comment_block: ") == 0) {
                size_t p = 15;
                nextWord(line, p, parsed.commentBlockStart);
                nextWord(line, p, parsed.commentBlockEnd);
            }
            if (line.find("regex: ") == 0) lastRegex = line.substr(7);
            if (line.find("regex_color: ") == 0 && !lastRegex.empty()) {
                size_t p = 13;
                float r, g, b, a;
                if (nextFloat(line, p, r) && nextFloat(line, p, g) && nextFloat(line, p, b) && nextFloat(line, p, a)) {
                    parsed.regexes.push_back({ std::regex(lastRegex, std::regex_constants::optimize), SyntaxColor{r, g, b, a} });
                    parsed.ruleMetas.push_back(classifySyntaxRegex(lastRegex));
                }
                lastRegex = "";
            }
        }
        parsed.lineComment = parsed.commentLine;
        parsed.blockCommentStart = parsed.commentBlockStart;
        parsed.blockCommentEnd = parsed.commentBlockEnd;
        compiledSyntaxCache.push_back({ targetSyntaxPtr, parsed });
        outSyntax = std::move(parsed);

    outLowerExt = lowerExt;
}

inline void loadSyntaxDefForExtension(const std::string& extUtf8, bool /*isDarkMode*/, SyntaxDef& outSyntax) {
    std::wstring wext;
    wext.reserve(extUtf8.size());
    for (unsigned char c : extUtf8) wext.push_back((wchar_t)c);
    std::wstring dummyLower;
    loadSyntaxDefForExtension(wext, outSyntax, dummyLower);
}

enum class SyntaxTokenCategory : int {
    RegexRule = 0, // index >= 0 is ruleIdx
    Comment = -1,
    String = -2,
    Keyword = -3,
    Literal = -4
};

template <typename ApplySpanFn>
inline void scanSyntaxHighlighting(
    const SyntaxDef& currentSyntax,
    const std::string& visibleText,
    bool inBlockComment,
    ApplySpanFn&& applySpan)
{
    if (visibleText.empty()) return;
    const size_t vLen = visibleText.size();
    const char* vData = visibleText.data();
    auto isWChar = [](char c) noexcept -> bool { return std::isalnum((unsigned char)c) || c == '_'; };
    auto isWordChar = [](char c) noexcept -> bool { return std::isalnum((unsigned char)c) || c == '_'; };
    for (size_t i = 0; i < currentSyntax.regexes.size(); ++i) {
        SyntaxRuleMeta meta = (i < currentSyntax.ruleMetas.size()) ? currentSyntax.ruleMetas[i] : SyntaxRuleMeta{};
        if (meta.reqChar1 != 0 && visibleText.find(meta.reqChar1) == std::string::npos) continue;
        if (meta.reqChar2 != 0 && visibleText.find(meta.reqChar2) == std::string::npos) continue;

        auto setEffect = [&](size_t start, size_t len) {
            if (len > 0) applySpan(start, len, (int)i);
        };

        if (meta.kind >= SyntaxRuleKind::NumberCpp && meta.kind <= SyntaxRuleKind::NumberSimple) {
            size_t p = 0;
            while (p < vLen) {
                unsigned char ch = (unsigned char)vData[p];
                if (ch >= '0' && ch <= '9' && (p == 0 || (!isWChar(vData[p - 1]) && vData[p - 1] != '\\'))) {
                    size_t start = p;
                    bool matchedBase = false;
                    if (vData[p] == '0' && p + 2 <= vLen) {
                        char n1 = vData[p + 1];
                        if (meta.kind != SyntaxRuleKind::NumberSimple && meta.kind != SyntaxRuleKind::NumberRust && n1 == 'x' && p + 2 < vLen && std::isxdigit((unsigned char)vData[p + 2])) {
                            p += 2;
                            while (p < vLen && std::isxdigit((unsigned char)vData[p])) p++;
                            matchedBase = true;
                        } else if (meta.kind == SyntaxRuleKind::NumberPy && n1 == 'b' && p + 2 < vLen && (vData[p + 2] == '0' || vData[p + 2] == '1')) {
                            p += 2;
                            while (p < vLen && (vData[p] == '0' || vData[p] == '1')) p++;
                            matchedBase = true;
                        } else if (meta.kind == SyntaxRuleKind::NumberPy && n1 == 'o' && p + 2 < vLen && (vData[p + 2] >= '0' && vData[p + 2] <= '7')) {
                            p += 2;
                            while (p < vLen && (vData[p] >= '0' && vData[p] <= '7')) p++;
                            matchedBase = true;
                        }
                    }
                    size_t dotPos = std::string::npos;
                    if (!matchedBase) {
                        while (p < vLen && std::isdigit((unsigned char)vData[p])) p++;
                        if (p + 1 < vLen && vData[p] == '.' && std::isdigit((unsigned char)vData[p + 1])) {
                            dotPos = p;
                            p += 2;
                            while (p < vLen && std::isdigit((unsigned char)vData[p])) p++;
                        }
                        if (meta.kind != SyntaxRuleKind::NumberSimple && p + 1 < vLen && (vData[p] == 'e' || vData[p] == 'E')) {
                            size_t ep = p + 1;
                            if (ep < vLen && (vData[ep] == '+' || vData[ep] == '-')) ep++;
                            if (ep < vLen && std::isdigit((unsigned char)vData[ep])) {
                                p = ep + 1;
                                while (p < vLen && std::isdigit((unsigned char)vData[p])) p++;
                            }
                        }
                        if (meta.kind == SyntaxRuleKind::NumberCpp) {
                            while (p < vLen && (vData[p]=='f'||vData[p]=='F'||vData[p]=='l'||vData[p]=='L'||vData[p]=='u'||vData[p]=='U')) p++;
                        } else if (meta.kind == SyntaxRuleKind::NumberCs) {
                            while (p < vLen && (vData[p]=='f'||vData[p]=='F'||vData[p]=='l'||vData[p]=='L'||vData[p]=='m'||vData[p]=='M'||vData[p]=='u'||vData[p]=='U'||vData[p]=='d'||vData[p]=='D')) p++;
                        } else if (meta.kind == SyntaxRuleKind::NumberPy) {
                            if (p < vLen && vData[p] == 'j') p++;
                        } else if (meta.kind == SyntaxRuleKind::NumberJava) {
                            while (p < vLen && (vData[p]=='f'||vData[p]=='F'||vData[p]=='l'||vData[p]=='L'||vData[p]=='d'||vData[p]=='D')) p++;
                        } else if (meta.kind == SyntaxRuleKind::NumberKotlin) {
                            while (p < vLen && (vData[p]=='f'||vData[p]=='F'||vData[p]=='l'||vData[p]=='L')) p++;
                        } else if (meta.kind == SyntaxRuleKind::NumberRust) {
                            while (p < vLen && isWChar(vData[p])) p++;
                        }
                    }
                    if (p == vLen || !isWChar(vData[p])) {
                        setEffect(start, p - start);
                    } else if (dotPos != std::string::npos) {
                        setEffect(start, dotPos - start);
                        p = dotPos + 1;
                    } else {
                        while (p < vLen && isWChar(vData[p])) p++;
                    }
                } else if (isWChar(vData[p])) {
                    while (p < vLen && isWChar(vData[p])) p++;
                } else {
                    p++;
                }
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::CppInclude) {
            size_t p = 0;
            while ((p = visibleText.find('#', p)) != std::string::npos) {
                if (p > 0 && vData[p - 1] == '\\') { p++; continue; }
                size_t q = p + 1;
                while (q < vLen && (vData[q] == ' ' || vData[q] == '\t')) q++;
                if (q + 7 <= vLen && std::memcmp(vData + q, "include", 7) == 0) {
                    q += 7;
                    while (q < vLen && (vData[q] == ' ' || vData[q] == '\t')) q++;
                    if (q < vLen && (vData[q] == '<' || vData[q] == '"')) {
                        q++;
                        size_t contentStart = q;
                        while (q < vLen && vData[q] != '>' && vData[q] != '"' && vData[q] != '\r' && vData[q] != '\n') q++;
                        if (q > contentStart && q < vLen && (vData[q] == '>' || vData[q] == '"')) {
                            q++;
                            setEffect(p, q - p);
                            p = q;
                            continue;
                        }
                    }
                }
                p++;
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::CppDirective) {
            size_t p = 0;
            while ((p = visibleText.find('#', p)) != std::string::npos) {
                if (p > 0 && vData[p - 1] == '\\') { p++; continue; }
                size_t q = p + 1;
                while (q < vLen && (vData[q] == ' ' || vData[q] == '\t')) q++;
                size_t idStart = q;
                while (q < vLen && isWChar(vData[q])) q++;
                if (q > idStart) {
                    setEffect(p, q - p);
                    p = q;
                    continue;
                }
                p++;
            }
            continue;
        }

        try {
            std::sregex_iterator words_begin(visibleText.begin(), visibleText.end(), currentSyntax.regexes[i].first);
            std::sregex_iterator words_end;
            for (auto it = words_begin; it != words_end; ++it) {
                size_t mPos = (size_t)it->position();
                size_t mLen = (size_t)it->length();
                if (mPos > 0 && visibleText[mPos - 1] == '\\') continue;
                setEffect(mPos, mLen);
            }
        } catch (...) {}
    }
    size_t i = 0;
    size_t len = visibleText.length();
    while (i < len) {
        if (inBlockComment) {
            size_t startStr = i;
            size_t endComment = visibleText.find(currentSyntax.commentBlockEnd, i);
            if (endComment != std::string::npos) {
                i = endComment + currentSyntax.commentBlockEnd.length();
                inBlockComment = false;
            } else {
                i = len;
            }
            applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Comment);
        } else {
            if (!currentSyntax.commentBlockStart.empty() && visibleText.compare(i, currentSyntax.commentBlockStart.length(), currentSyntax.commentBlockStart) == 0) {
                inBlockComment = true;
                continue;
            }
            if (!currentSyntax.commentLine.empty() && visibleText.compare(i, currentSyntax.commentLine.length(), currentSyntax.commentLine) == 0) {
                size_t startStr = i;
                size_t eol = visibleText.find('\n', i);
                if (eol == std::string::npos) eol = len;
                i = eol;
                applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Comment);
                continue;
            }
            if (!currentSyntax.disableStrings && (visibleText[i] == '"' || visibleText[i] == '\'')) {
                char quote = visibleText[i];
                size_t startStr = i;
                i++;
                while (i < len) {
                    if (visibleText[i] == '\\' && i + 1 < len) {
                        i += 2;
                    } else if (visibleText[i] == quote) {
                        i++;
                        break;
                    } else if (visibleText[i] == '\n') {
                        break;
                    } else {
                        i++;
                    }
                }
                applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::String);
                continue;
            }
            if (isWordChar(visibleText[i])) {
                size_t startStr = i;
                while (i < len && isWordChar(visibleText[i])) i++;
                std::string_view word(visibleText.data() + startStr, i - startStr);
                if (currentSyntax.keywords.contains(word)) applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Keyword);
                else if (currentSyntax.literals.contains(word)) applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Literal);
                continue;
            }
            i++;
        }
    }
}


inline void scanSyntaxHighlighting(
    const std::string& visibleText,
    const SyntaxDef& currentSyntax,
    bool inBlockComment,
    std::vector<SyntaxColor>& outColors,
    bool isDarkMode = false)
{
    outColors.assign(visibleText.size(), SyntaxColor{0.0f, 0.0f, 0.0f, 0.0f});
    if (visibleText.empty() || !currentSyntax.active) return;
    SyntaxColor colComment = isDarkMode ? SyntaxColor{0.42f, 0.60f, 0.33f, 1.0f} : SyntaxColor{0.0f, 0.50f, 0.0f, 1.0f};
    SyntaxColor colString  = isDarkMode ? SyntaxColor{0.81f, 0.57f, 0.47f, 1.0f} : SyntaxColor{0.64f, 0.08f, 0.08f, 1.0f};
    SyntaxColor colKeyword = isDarkMode ? SyntaxColor{0.34f, 0.61f, 0.84f, 1.0f} : SyntaxColor{0.0f, 0.0f, 1.0f, 1.0f};
    SyntaxColor colLiteral = isDarkMode ? SyntaxColor{0.34f, 0.61f, 0.84f, 1.0f} : SyntaxColor{0.0f, 0.35f, 0.70f, 1.0f};

    scanSyntaxHighlighting(currentSyntax, visibleText, inBlockComment, [&](size_t start, size_t len, int cat) {
        SyntaxColor sc{0.0f, 0.0f, 0.0f, 0.0f};
        if (cat >= 0 && (size_t)cat < currentSyntax.regexes.size()) {
            sc = currentSyntax.regexes[cat].second;
        } else if (cat == (int)SyntaxTokenCategory::Comment) {
            sc = colComment;
        } else if (cat == (int)SyntaxTokenCategory::String) {
            sc = colString;
        } else if (cat == (int)SyntaxTokenCategory::Keyword) {
            sc = colKeyword;
        } else if (cat == (int)SyntaxTokenCategory::Literal) {
            sc = colLiteral;
        }
        size_t end = std::min(visibleText.size(), start + len);
        for (size_t k = start; k < end; ++k) {
            outColors[k] = sc;
        }
    });
}
