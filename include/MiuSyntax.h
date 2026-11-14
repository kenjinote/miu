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
#include "MiuCore.h"

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
    void insert(const std::string& s){
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
    UrlMarkdown,
    Email,
    CppInclude,
    CppDirective,
    XmlTag,
    XmlAttrName,
    XmlEntity,
    CssPropName,
    CssHexColor,
    CssUnit,
    CssSelector,
    NixAttrName,
    LineKeyBeforeEq,
    LineKeyBeforeEqNoDot,
    MakefileTarget,
    AsmLabel
};
struct SyntaxRuleMeta {
    SyntaxRuleKind kind = SyntaxRuleKind::GenericRegex;
    char reqChar1 = 0;
    char reqChar2 = 0;
};
struct SyntaxDef {
    bool active = false;
    std::string lang;
    SyntaxWordSet keywords;
    SyntaxWordSet literals;
    std::string commentLine;
    std::string commentBlockStart;
    std::string commentBlockEnd;
    std::string lineComment;
    std::string blockCommentStart;
    std::string blockCommentEnd;
    bool disableStrings = false;
    bool disableSingleQuoteStrings = false;
    bool lispWords = false;
    std::vector<std::pair<std::regex, SyntaxColor>> regexes;
    std::vector<SyntaxRuleMeta> ruleMetas;
};
static inline wchar_t toLowerAsciiW(wchar_t ch) noexcept {
    return (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch + (L'a' - L'A')) : ch;
}
static inline char toLowerAsciiC(char ch) noexcept {
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch + ('a' - 'A')) : ch;
}
static inline bool hasOddPrecedingBackslashes(std::string_view sv, size_t pos) noexcept {
    size_t bs = 0;
    while (pos > bs && sv[pos - 1 - bs] == '\\') ++bs;
    return (bs & 1u) != 0;
}
static inline SyntaxRuleMeta classifySyntaxRegex(const std::string& pat) {
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
    if (pat == R"((https?|ftp)://[^\s<>")\]]+)") { m.kind = SyntaxRuleKind::UrlMarkdown; m.reqChar1 = ':'; m.reqChar2 = '/'; return m; }
    if (pat.find("(https?|ftp)://") == 0) { m.kind = SyntaxRuleKind::Url; m.reqChar1 = ':'; m.reqChar2 = '/'; return m; }
    if (pat.find("@[a-zA-Z0-9.-]+") != std::string::npos) { m.kind = SyntaxRuleKind::Email; m.reqChar1 = '@'; return m; }
    if (pat == R"(</?[a-zA-Z0-9:-]+>?)") { m.kind = SyntaxRuleKind::XmlTag; m.reqChar1 = '<'; return m; }
    if (pat == R"(\b[a-zA-Z0-9:-]+(?=\s*=))") { m.kind = SyntaxRuleKind::XmlAttrName; m.reqChar1 = '='; return m; }
    if (pat == R"(&[#a-zA-Z0-9]+;)") { m.kind = SyntaxRuleKind::XmlEntity; m.reqChar1 = '&'; m.reqChar2 = ';'; return m; }
    if (pat == R"(\b[a-zA-Z-]+(?=\s*:))") { m.kind = SyntaxRuleKind::CssPropName; m.reqChar1 = ':'; return m; }
    if (pat == R"(\B#[0-9a-fA-F]{3,6}\b)") { m.kind = SyntaxRuleKind::CssHexColor; m.reqChar1 = '#'; return m; }
    if (pat == R"(\b(px|em|rem|vh|vw|%)\b)") { m.kind = SyntaxRuleKind::CssUnit; return m; }
    if (pat == R"((\.|#|:)[a-zA-Z_-]+)") { m.kind = SyntaxRuleKind::CssSelector; return m; }
    if (pat == R"([a-zA-Z0-9_.-]+[ \t]*(?==[^=]))") { m.kind = SyntaxRuleKind::NixAttrName; m.reqChar1 = '='; return m; }
    if (pat == R"((?:^|[\r\n])[ \t]*[a-zA-Z0-9_.-]+(?=[ \t]*=))") { m.kind = SyntaxRuleKind::LineKeyBeforeEq; m.reqChar1 = '='; return m; }
    if (pat == R"((?:^|[\r\n])[ \t]*[a-zA-Z0-9_-]+(?=[ \t]*=))") { m.kind = SyntaxRuleKind::LineKeyBeforeEqNoDot; m.reqChar1 = '='; return m; }
    if (pat == R"((?:^|[\r\n])[a-zA-Z0-9_./-]+(?=[ \t]*:(?!=)))") { m.kind = SyntaxRuleKind::MakefileTarget; m.reqChar1 = ':'; return m; }
    if (pat == R"((?:^|[\r\n])[ \t]*[a-zA-Z_.][a-zA-Z0-9_.$@]*(?=[ \t]*:))") { m.kind = SyntaxRuleKind::AsmLabel; m.reqChar1 = ':'; return m; }
    if (!pat.empty()) {
        bool hasTopAlt = false;
        {
            int pDepth = 0;
            bool inBrk = false;
            for (size_t k = 0; k < pat.size(); ++k) {
                char ch = pat[k];
                if (ch == '\\') { k++; continue; }
                if (inBrk) { if (ch == ']') inBrk = false; continue; }
                if (ch == '[') { inBrk = true; continue; }
                if (ch == '(') pDepth++;
                else if (ch == ')' && pDepth > 0) pDepth--;
                else if (ch == '|' && pDepth == 0) { hasTopAlt = true; break; }
            }
        }
        if (!hasTopAlt) {
            if (pat[0] == '@' || pat[0] == '$' || pat[0] == '%' || pat[0] == '#' || pat[0] == ':' || pat[0] == '`' || pat[0] == '"' || pat[0] == '=' || pat[0] == '\'' || pat[0] == '&' || pat[0] == '<' || pat[0] == '-') m.reqChar1 = pat[0];
            else if (pat.find("(?:#:|:)") == 0) m.reqChar1 = ':';
            else if (pat.find("(?:#'|')") == 0) m.reqChar1 = '\'';
            else if (pat.find(R"(\*[)") == 0) m.reqChar1 = '*';
            else if (pat.find(R"(\+[)") == 0) m.reqChar1 = '+';
            else if (pat.find(R"(\?)") == 0) m.reqChar1 = '?';
            else if (pat.find(R"(\.)") == 0) m.reqChar1 = '.';
            else if (pat.find(R"(\([*])") == 0) { m.reqChar1 = '('; m.reqChar2 = '*'; }
            else if (pat.find("Windows ") == 0) m.reqChar1 = 'W';
            else if (pat.find("</?") == 0) m.reqChar1 = '<';
            else if (pat.find("&[#") == 0) m.reqChar1 = '&';
            else if (pat.find(R"(\B#)") == 0) m.reqChar1 = '#';
            else if (pat.find(R"(\\[a-zA-Z])") == 0) m.reqChar1 = '\\';
            else if (pat.find(R"(\{)") == 0) m.reqChar1 = '{';
            else if (pat.find(R"(\$)") == 0) m.reqChar1 = '$';
            else if (pat.find(R"(\[<)") == 0) { m.reqChar1 = '['; m.reqChar2 = '<'; }
            else if (pat.find("--[") == 0) m.reqChar1 = '-';
            else if (pat.find("(?:^|[\\r\\n])[ \\t]*\\[") == 0) { m.reqChar1 = '['; m.reqChar2 = ']'; }
            else if (pat.find("(?:^|[\\r\\n])\\+") == 0) m.reqChar1 = '+';
            else if (pat.find("(?:^|[\\r\\n])-") == 0) m.reqChar1 = '-';
            else if (pat.find("(?:^|[\\r\\n])@@") == 0) m.reqChar1 = '@';
            else if (pat.find("(?=[ \\t]*=)") != std::string::npos || pat.find("(?==[^=])") != std::string::npos) m.reqChar1 = '=';
            else if (pat.find("(?=[ \\t]*:") != std::string::npos || pat.find(":[a-zA-Z0-9_]+") != std::string::npos) m.reqChar1 = ':';
        }
    }
    return m;
}
using MiuCore::isSafeUserRegex;


inline std::wstring extractSyntaxExtensionFromPath(const std::wstring& targetPath) {
    std::wstring ext = L"";
    size_t slashPos = targetPath.find_last_of(L"\\/");
    std::wstring baseName = (slashPos == std::wstring::npos) ? targetPath : targetPath.substr(slashPos + 1);
    std::wstring lowerBase = baseName;
    for (auto& c : lowerBase) c = toLowerAsciiW(c);
    if (lowerBase == L"dockerfile" || lowerBase == L"containerfile" || lowerBase.rfind(L"dockerfile.", 0) == 0 || lowerBase.rfind(L"containerfile.", 0) == 0) ext = L"dockerfile";
    else if (lowerBase == L"makefile" || lowerBase == L"gnumakefile" || lowerBase == L"bsdmakefile" || lowerBase == L"justfile") ext = L"makefile";
    else if (lowerBase == L"cmakelists.txt") ext = L"cmake";
    else if (lowerBase == L".env" || lowerBase.rfind(L".env.", 0) == 0) ext = L"env";
    else if (lowerBase == L".bashrc" || lowerBase == L".bash_profile" || lowerBase == L".zshrc" || lowerBase == L".zprofile" || lowerBase == L".zshenv" || lowerBase == L".zlogin" || lowerBase == L".zlogout" || lowerBase == L".profile" || lowerBase == L".cshrc" || lowerBase == L".tcshrc" || lowerBase == L".kshrc" || lowerBase == L"pkgbuild") ext = L"sh";
    else if (lowerBase == L".emacs" || lowerBase == L".sbclrc" || lowerBase == L".clisprc" || lowerBase == L".guile") ext = L"lisp";
    else if (lowerBase == L"gemfile" || lowerBase == L"rakefile" || lowerBase == L"podfile" || lowerBase == L"fastfile" || lowerBase == L"appfile" || lowerBase == L"matchfile" || lowerBase == L"deliverfile" || lowerBase == L"gymfile" || lowerBase == L"scanfile" || lowerBase == L"snapfile" || lowerBase == L"brewfile" || lowerBase == L"vagrantfile" || lowerBase == L"berksfile" || lowerBase == L"guardfile" || lowerBase == L"capfile" || lowerBase == L"thorfile" || lowerBase == L"dangerfile") ext = L"rb";
    else if (lowerBase == L"jenkinsfile") ext = L"groovy";
    else if (lowerBase == L"build" || lowerBase == L"build.bazel" || lowerBase == L"workspace" || lowerBase == L"workspace.bazel" || lowerBase == L"sconstruct" || lowerBase == L"sconscript" || lowerBase == L"tiltfile") ext = L"py";
    else if (lowerBase == L".vimrc" || lowerBase == L"_vimrc" || lowerBase == L".gvimrc" || lowerBase == L"_gvimrc" || lowerBase == L"vimrc" || lowerBase == L"gvimrc" || lowerBase == L".exrc") ext = L"vim";
    else if (lowerBase == L".gitignore" || lowerBase == L".gitattributes" || lowerBase == L".dockerignore" || lowerBase == L".npmignore" || lowerBase == L".eslintignore" || lowerBase == L".prettierignore" || lowerBase == L".clang-format" || lowerBase == L".clang-tidy" || lowerBase == L".yamllint" || lowerBase == L".htaccess") ext = L"conf";
    else if (lowerBase == L".editorconfig" || lowerBase == L".gitconfig" || lowerBase == L".gitmodules" || lowerBase == L".npmrc" || lowerBase == L".yarnrc" || lowerBase == L".flake8" || lowerBase == L".pylintrc" || lowerBase == L".coveragerc") ext = L"ini";
    else if (lowerBase == L".prettierrc" || lowerBase == L".babelrc" || lowerBase == L".eslintrc" || lowerBase == L".stylelintrc" || lowerBase == L".hintrc" || lowerBase == L".swcrc") ext = L"json";
    else {
        size_t dotPos = baseName.find_last_of(L".");
        if (dotPos != std::wstring::npos) ext = baseName.substr(dotPos + 1);
    }
    return ext;
}

inline std::string extractSyntaxExtensionFromPath(const std::string& utf8Path) {
    size_t slashPos = utf8Path.find_last_of("\\/");
    std::string baseName = (slashPos == std::string::npos) ? utf8Path : utf8Path.substr(slashPos + 1);
    std::string lowerBase = baseName;
    for (char& c : lowerBase) {
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
    }
    std::string ext;
    if (lowerBase == "dockerfile" || lowerBase == "containerfile" || lowerBase.rfind("dockerfile.", 0) == 0 || lowerBase.rfind("containerfile.", 0) == 0) ext = "dockerfile";
    else if (lowerBase == "makefile" || lowerBase == "gnumakefile" || lowerBase == "bsdmakefile" || lowerBase == "justfile") ext = "makefile";
    else if (lowerBase == "cmakelists.txt") ext = "cmake";
    else if (lowerBase == ".env" || lowerBase.rfind(".env.", 0) == 0) ext = "env";
    else if (lowerBase == ".bashrc" || lowerBase == ".bash_profile" || lowerBase == ".zshrc" || lowerBase == ".zprofile" || lowerBase == ".zshenv" || lowerBase == ".zlogin" || lowerBase == ".zlogout" || lowerBase == ".profile" || lowerBase == ".cshrc" || lowerBase == ".tcshrc" || lowerBase == ".kshrc" || lowerBase == "pkgbuild") ext = "sh";
    else if (lowerBase == ".emacs" || lowerBase == ".sbclrc" || lowerBase == ".clisprc" || lowerBase == ".guile") ext = "lisp";
    else if (lowerBase == "gemfile" || lowerBase == "rakefile" || lowerBase == "podfile" || lowerBase == "fastfile" || lowerBase == "appfile" || lowerBase == "matchfile" || lowerBase == "deliverfile" || lowerBase == "gymfile" || lowerBase == "scanfile" || lowerBase == "snapfile" || lowerBase == "brewfile" || lowerBase == "vagrantfile" || lowerBase == "berksfile" || lowerBase == "guardfile" || lowerBase == "capfile" || lowerBase == "thorfile" || lowerBase == "dangerfile") ext = "rb";
    else if (lowerBase == "jenkinsfile") ext = "groovy";
    else if (lowerBase == "build" || lowerBase == "build.bazel" || lowerBase == "workspace" || lowerBase == "workspace.bazel" || lowerBase == "sconstruct" || lowerBase == "sconscript" || lowerBase == "tiltfile") ext = "py";
    else if (lowerBase == ".vimrc" || lowerBase == "_vimrc" || lowerBase == ".gvimrc" || lowerBase == "_gvimrc" || lowerBase == "vimrc" || lowerBase == "gvimrc" || lowerBase == ".exrc") ext = "vim";
    else if (lowerBase == ".gitignore" || lowerBase == ".gitattributes" || lowerBase == ".dockerignore" || lowerBase == ".npmignore" || lowerBase == ".eslintignore" || lowerBase == ".prettierignore" || lowerBase == ".clang-format" || lowerBase == ".clang-tidy" || lowerBase == ".yamllint" || lowerBase == ".htaccess") ext = "conf";
    else if (lowerBase == ".editorconfig" || lowerBase == ".gitconfig" || lowerBase == ".gitmodules" || lowerBase == ".npmrc" || lowerBase == ".yarnrc" || lowerBase == ".flake8" || lowerBase == ".pylintrc" || lowerBase == ".coveragerc") ext = "ini";
    else if (lowerBase == ".prettierrc" || lowerBase == ".babelrc" || lowerBase == ".eslintrc" || lowerBase == ".stylelintrc" || lowerBase == ".hintrc" || lowerBase == ".swcrc") ext = "json";
    else {
        size_t dotPos = lowerBase.find_last_of('.');
        if (dotPos != std::string::npos) ext = lowerBase.substr(dotPos + 1);
    }
    if (ext.empty()) return "";
    return (ext[0] == '.') ? ext : ("." + ext);
}

inline std::wstring extractSyntaxExtensionFromUtf8Path(const std::string& utf8Path) {
    std::string ext = extractSyntaxExtensionFromPath(utf8Path);
    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
    std::wstring wext;
    wext.reserve(ext.size());
    for (unsigned char c : ext) wext.push_back((wchar_t)c);
    return wext;
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
regex: </?[a-zA-Z0-9:-]+>?
regex_color: 0.3 0.6 0.8 1.0
regex: \b[a-zA-Z0-9:-]+(?=\s*=)
regex_color: 0.8 0.5 0.3 1.0
regex: &[#a-zA-Z0-9]+;
regex_color: 0.6 0.4 0.7 1.0
)";
        static constexpr const char* kSyntax6 = R"([css]
comment_block: /* */
regex: \b[a-zA-Z-]+(?=\s*:)
regex_color: 0.3 0.6 0.8 1.0
regex: \B#[0-9a-fA-F]{3,6}\b
regex_color: 0.6 0.4 0.7 1.0
regex: \b(px|em|rem|vh|vw|%)\b
regex_color: 0.8 0.5 0.3 1.0
regex: (\.|#|:)[a-zA-Z_-]+
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax7 = R"([markdown]
no_strings: 1
comment_block: <!-- -->
regex: (?:^|[\r\n])[ \t]*#{1,6}[ \t]+[^\r\n]*
regex_color: 0.3 0.7 0.9 1.0
regex: (\*\*\*|___)[^*_\r\n\\]+(\*\*\*|___)|(\*\*|__)(?:[^*_\r\n\\]|\\.|[*][^*\r\n]|_[^_\r\n])+(\*\*|__)
regex_color: 0.9 0.5 0.3 1.0
regex: (?:^|[\r\n])[ \t]*`{3,}[^\r\n]*(?:\r\n|\r(?!\n)|\n)(?:[^\r\n]*(?:\r\n|\r(?!\n)|\n)){0,120}?[ \t]*`{3,}[ \t]*(?=\r|\n|$)|(?:^|[\r\n])[ \t]*~{3,}[^\r\n]*(?:\r\n|\r(?!\n)|\n)(?:[^\r\n]*(?:\r\n|\r(?!\n)|\n)){0,120}?[ \t]*~{3,}[ \t]*(?=\r|\n|$)|(?:^|[\r\n])[ \t]*(?:`{3,}|~{3,})[^\r\n]*|``+(?:[^`\r\n]|`[^`\r\n])+``+|`[^`\r\n\\]*(?:\\.[^`\r\n\\]*)*`
regex_color: 0.5 0.7 0.5 1.0
regex: (https?|ftp)://[^\s<>")\]]+
regex_color: 0.2 0.6 0.8 1.0
)";
        static constexpr const char* kSyntax8 = R"([rust]
no_single_quote: 1
keyword: as break const continue crate else enum extern fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait type unsafe use where while async await dyn abstract become box do final macro override priv typeof unsized virtual yield
literal: true false Option Result Some None Ok Err
comment_line: //
comment_block: /* */
regex: '(?:\\(?:x[0-9a-fA-F]{2}|u\{[0-9a-fA-F_]{1,6}\}|.)|[^'\\\r\n])'
regex_color: 0.64 0.08 0.08 1.0
regex: '[a-zA-Z_][a-zA-Z0-9_]*\b
regex_color: 0.85 0.65 0.35 1.0
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
regex: (?:^|[\r\n])[ \t]*([rR][eE][mM]\b|::)[^\r\n]*
regex_color: 0.38 0.62 0.38 1.0
regex: %[a-zA-Z0-9_]+%
regex_color: 0.8 0.5 0.3 1.0
regex: (?:^|[\r\n])[ \t]*:[a-zA-Z0-9_]+
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
regex: \$\{[^}]+\}
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
regex: \$[a-zA-Z_][a-zA-Z0-9_]*
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
regex: (?:^|[\r\n])[ \t]*\[[^\r\n\]]+\]
regex_color: 0.3 0.65 0.9 1.0
regex: (?:^|[\r\n])[ \t]*[a-zA-Z0-9_.-]+(?=[ \t]*=)
regex_color: 0.8 0.5 0.3 1.0
)";
        static constexpr const char* kSyntax21 = R"([yaml]
comment_line: #
regex: (?:^|[\r\n])[ \t]*[a-zA-Z0-9_.-]+:
regex_color: 0.3 0.6 0.8 1.0
regex: \b(true|false|null|yes|no|on|off)\b
regex_color: 0.8 0.4 0.2 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax22 = R"([latex]
no_strings: 1
comment_line: %
comment_block: \begin{comment} \end{comment}
regex: \\[a-zA-Z]+
regex_color: 0.3 0.6 0.8 1.0
regex: \{[^}\r\n]*\}
regex_color: 0.8 0.5 0.3 1.0
regex: \$(?:\\.|[^$\\\r\n])*\$
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
no_single_quote: 1
keyword: abstract and as assert base begin class default delegate do done downcast downto elif else end exception extern false finally fixed for fun function global if in inherit inline interface internal lazy let match member module mutable namespace new not null of open or override private public rec return select static struct then to true try type upcast use val void when while with yield async seq task int float bool string unit list array option result
literal: true false null None Some Ok Error
comment_line: //
comment_block: (* *)
regex: '(?:\\.|[^'\\\r\n])'
regex_color: 0.64 0.08 0.08 1.0
regex: '[a-zA-Z_][a-zA-Z0-9_]*\b
regex_color: 0.85 0.65 0.35 1.0
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
no_single_quote: 1
keyword: as case class data default deriving do else family forall foreign hiding if import in infix infixl infixr instance let mdo module newtype of proc qualified rec then type where Int Integer Float Double Bool Char String Maybe Either IO Monad Functor Applicative Foldable Traversable Eq Ord Show Read Num Enum Bounded Monoid Semigroup
literal: True False Nothing Just Left Right EQ LT GT otherwise
comment_line: --
comment_block: {- -}
regex: '(?:\\[^\r\n']+|[^'\\\r\n])'
regex_color: 0.64 0.08 0.08 1.0
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
regex: --[a-zA-Z0-9_-]+(?:=[^\s]+)?
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
regex: (?:^|[\r\n])[ \t]*[a-zA-Z0-9_-]+(?=[ \t]*=)
regex_color: 0.3 0.65 0.9 1.0
regex: \$\{[^}]+\}
regex_color: 0.8 0.5 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax37 = R"([toml]
literal: true false TRUE FALSE inf nan
comment_line: #
regex: (?:^|[\r\n])[ \t]*\[\[(?:[^\r\n\]]+)\]\]|(?:^|[\r\n])[ \t]*\[(?:[^\r\n\]]+)\]
regex_color: 0.3 0.65 0.9 1.0
regex: (?:^|[\r\n])[ \t]*[a-zA-Z0-9_.-]+(?=[ \t]*=)
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
regex: \$\{[^}]+\}
regex_color: 0.8 0.5 0.3 1.0
regex: \$<[^>]+>
regex_color: 0.6 0.4 0.7 1.0
regex: \b([0-9]+(\.[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax41 = R"([makefile]
keyword: include sinclude ifeq ifneq ifdef ifndef else endif define endef export unexport override private vpath subst patsubst strip findstring filter filter-out sort word wordlist words firstword lastword dir notdir suffix basename addprefix addsuffix join wildcard realpath abspath error warning info shell origin flavor foreach if or and call eval file value PHONY DEFAULT PRECIOUS INTERMEDIATE SECONDARY SECONDEXPANSION DELETE_ON_ERROR IGNORE LOW_RESOLUTION_TIME SILENT EXPORT_ALL_VARIABLES NOTPARALLEL ONESHELL POSIX
comment_line: #
regex: (?:^|[\r\n])[a-zA-Z0-9_./-]+(?=[ \t]*:(?!=))
regex_color: 0.3 0.65 0.9 1.0
regex: \$(?:\([^)]+\)|\{[^}]+\}|[@%<?^+|*])
regex_color: 0.8 0.5 0.3 1.0
)";
        static constexpr const char* kSyntax42 = R"([reg]
keyword: REGEDIT4
comment_line: ;
regex: Windows Registry Editor Version [0-9.]+
regex_color: 0.6 0.4 0.7 1.0
regex: (?:^|[\r\n])[ \t]*\[-?HKEY_[A-Z_]+[^\r\n\]]*\]
regex_color: 0.3 0.65 0.9 1.0
regex: =(?:dword|qword|hex(?:\([0-9a-fA-F]+\))?):[0-9a-fA-F,\\\t ]*
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax43 = R"([shader]
keyword: cbuffer tbuffer struct class interface technique technique10 technique11 pass compile return if else for while do switch case default break continue discard in out inout uniform attribute varying buffer shared coherent volatile restrict readonly writeonly layout flat smooth noperspective centroid sample patch precise invariant highp mediump lowp const static extern inline register packoffset fn var let override workgroup storage private function void bool int uint dword half float double min16float min16int min16uint float2 float3 float4 int2 int3 int4 uint2 uint3 uint4 bool2 bool3 bool4 half2 half3 half4 double2 double3 double4 float2x2 float3x3 float4x4 matrix vector vec2 vec3 vec4 ivec2 ivec3 ivec4 uvec2 uvec3 uvec4 bvec2 bvec3 bvec4 dvec2 dvec3 dvec4 mat2 mat3 mat4 vec2f vec3f vec4f vec2i vec3i vec4i vec2u vec3u vec4u mat2x2f mat3x3f mat4x4f f32 i32 u32 Texture1D Texture2D Texture3D TextureCube Texture2DArray RWTexture1D RWTexture2D RWTexture3D StructuredBuffer RWStructuredBuffer ByteAddressBuffer RWByteAddressBuffer SamplerState SamplerComparisonState sampler1D sampler2D sampler3D samplerCube image2D kernel vertex fragment device constant thread threadgroup threadgroup_imageblock ray_data object_data mesh object uchar ushort ulong texture1d texture2d texture3d texturecube depth2d sampler metal namespace using
literal: true false SV_Position SV_Target SV_Target0 SV_Target1 SV_Target2 SV_Target3 SV_Depth SV_VertexID SV_InstanceID SV_DispatchThreadID SV_GroupID SV_GroupThreadID SV_GroupIndex POSITION NORMAL TEXCOORD0 TEXCOORD1 TEXCOORD2 COLOR0 gl_Position gl_FragCoord gl_FragDepth gl_VertexID gl_InstanceID gl_GlobalInvocationID gl_LocalInvocationID gl_WorkGroupID stage_in position thread_position_in_grid thread_position_in_threadgroup threadgroup_position_in_grid
comment_line: //
comment_block: /* */
regex: #[ \t]*[a-zA-Z0-9_]+
regex_color: 0.6 0.4 0.7 1.0
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.6 0.4 0.7 1.0
regex: \[\[[a-zA-Z_][a-zA-Z0-9_(), \t]*\]\]
regex_color: 0.6 0.4 0.7 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLuU]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax44 = R"([asm]
keyword: mov movb movw movl movq movabs movzx movzbl movzwl movsx movsbl movswl movslq lea leal leaq push pushq pop popq call callq ret retq lret iret iretq jmp ljmp je jz jne jnz jg jge jl jle ja jae jb jbe jc jnc jo jno js jns jp jpe jnp jpo jecxz jrcxz loop loope loopne cmove cmovz cmovne cmovnz cmovg cmovge cmovl cmovle cmova cmovae cmovb cmovbe seta sete setne setg setge setl setle add addb addw addl addq adc adcx adox sub subb subw subl subq sbb mul imul div idiv inc dec neg and or xor not shl shr sal sar rol ror rcl rcr shld shrd bsf bsr bswap bt btc btr bts popcnt lzcnt tzcnt cmp cmpb cmpw cmpl cmpq test testb testw testl testq nop ud2 int into syscall sysret sysenter sysexit cpuid rdtsc rdtscp cld std cli sti clc stc cmc rep repe repz repne repnz cmpsb cmpsw cmpsd cmpsq movsb movsw movsd movsq stosb stosw stosd stosq lodsb lodsw lodsd lodsq scasb scasw scasd scasq xchg xadd cmpxchg cmpxchg8b cmpxchg16b lock pause wait hlt leave enter cbw cwde cdqe cwd cdq cqo lfence mfence sfence prefetchnta prefetcht0 prefetcht1 prefetcht2 movaps movups movapd movupd movdqa movdqu movd movq movss movsd movhlps movlhps shufps shufpd unpcklps unpckhps addps addpd addss addsd subps subpd subss subsd mulps mulpd mulss mulsd divps divpd divss divsd sqrtps sqrtpd sqrtss sqrtsd maxps maxpd minps minpd andps andpd andnps andnpd orps orpd xorps xorpd pxor por pand pandn paddb paddw paddd paddq psubb psubw psubd psubq pmullw pmulld pcmpeqb pcmpeqw pcmpeqd pcmpgtb pcmpgtw pcmpgtd pshufb pshufd psllw pslld psllq psrlw psrld psrlq psraw psrad vmovaps vmovups vmovapd vmovupd vmovdqa vmovdqu vaddps vaddpd vsubps vsubpd vmulps vmulpd vdivps vdivpd vxorps vxorpd vpxor vpor vpand vfmadd132ps vfmadd213ps vfmadd231ps vfmadd231pd vbroadcastss vbroadcastsd vzeroupper vzeroall ldr ldrb ldrh ldrsb ldrsh ldrsw ldp ldnp ldxr ldaxr str strb strh stp stnp stxr stlxr bl blr br b beq bne blt bgt ble bge bcs bcc bmi bpl bvs bvc bhi bls cbz cbnz tbz tbnz adr adrp madd msub mneg smull umull sdiv udiv csel cset csetm cinc cinv cneg ubfm sbfm bfm ubfx sbfx bfi lsl lsr asr bic orn eon tst cmn mvn rev rev16 rev32 clz rbit mrs msr svc hvc smc isb dsb dmb fmov fadd fsub fmul fdiv fsqrt fcmp fcvt scvtf ucvtf fcvtzs fcvtzu lui auipc jal jalr lb lh lw ld lbu lhu lwu sb sh sw sd addi slti sltiu xori ori andi slli srli srai addw subw sllw srlw sraw addiw slliw srliw sraiw mulh mulhsu mulhu divu rem remu mulw divw divuw remw remuw ecall ebreak mret sret uret wfi fence csrr csrw csrs csrc csrrw csrrs csrrc li la mv j jr beqz bnez blez bgez bltz bgtz seqz snez sltz sgtz db dw dd dq dt do dy dz resb resw resd resq rest reso resy resz incbin equ times section segment global extern common weak local type size align balign p2align org bits use16 use32 use64 default rel abs struc endstruc istruc iend at macro endm %macro %endmacro %define %assign %ifdef %ifndef %else %endif %include %rep %endrep PUBLIC EXTRN EXTERNDEF PROTO PROC ENDP SEGMENT ENDS ASSUME MACRO ENDM ALIGN ORG EVEN RECORD STRUCT UNION TYPEDEF END BYTE SBYTE WORD SWORD DWORD SDWORD FWORD QWORD SQWORD TBYTE OWORD XMMWORD YMMWORD ZMMWORD PTR OFFSET FLAT SHORT NEAR FAR PROC module func param result local global table memory type import export start elem data offset mut i32 i64 f32 f64 v128 funcref externref
literal: rax rbx rcx rdx rsi rdi rbp rsp r8 r9 r10 r11 r12 r13 r14 r15 rip eflags rflags eax ebx ecx edx esi edi ebp esp r8d r9d r10d r11d r12d r13d r14d r15d eip ax bx cx dx si di bp sp r8w r9w r10w r11w r12w r13w r14w r15w al bl cl dl ah bh ch dh sil dil bpl spl r8b r9b r10b r11b r12b r13b r14b r15b mm0 mm1 mm2 mm3 mm4 mm5 mm6 mm7 xmm0 xmm1 xmm2 xmm3 xmm4 xmm5 xmm6 xmm7 xmm8 xmm9 xmm10 xmm11 xmm12 xmm13 xmm14 xmm15 xmm16 xmm17 xmm18 xmm19 xmm20 xmm21 xmm22 xmm23 xmm24 xmm25 xmm26 xmm27 xmm28 xmm29 xmm30 xmm31 ymm0 ymm1 ymm2 ymm3 ymm4 ymm5 ymm6 ymm7 ymm8 ymm9 ymm10 ymm11 ymm12 ymm13 ymm14 ymm15 zmm0 zmm1 zmm2 zmm3 zmm4 zmm5 zmm6 zmm7 k0 k1 k2 k3 k4 k5 k6 k7 cr0 cr2 cr3 cr4 dr0 dr1 dr2 dr3 dr6 dr7 cs ds es fs gs ss st0 st1 st2 st3 st4 st5 st6 st7 x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 x10 x11 x12 x13 x14 x15 x16 x17 x18 x19 x20 x21 x22 x23 x24 x25 x26 x27 x28 x29 x30 lr pc xzr wzr w0 w1 w2 w3 w4 w5 w6 w7 w8 w9 w10 w11 w12 w13 w14 w15 w16 w17 w18 w19 w20 w21 w22 w23 w24 w25 w26 w27 w28 w29 w30 v0 v1 v2 v3 v4 v5 v6 v7 v8 v9 v10 v11 v12 v13 v14 v15 v16 v17 v18 v19 v20 v21 v22 v23 v24 v25 v26 v27 v28 v29 v30 v31 q0 q1 q2 q3 q4 q5 q6 q7 d0 d1 d2 d3 d4 d5 d6 d7 s0 s1 s2 s3 s4 s5 s6 s7 s8 s9 s10 s11 cpsr spsr fpsr fpcr zero ra gp tp t0 t1 t2 t3 t4 t5 t6 fp a0 a1 a2 a3 a4 a5 a6 a7 ft0 ft1 ft2 ft3 ft4 ft5 ft6 ft7 fs0 fs1 fa0 fa1 fa2 fa3 fa4 fa5 fa6 fa7
comment_line: ;
comment_block: /* */
regex: (?:^|[\r\n])[ \t]*[a-zA-Z_.][a-zA-Z0-9_.$@]*(?=[ \t]*:)
regex_color: 0.6 0.4 0.7 1.0
regex: %[a-zA-Z0-9]+
regex_color: 0.35 0.75 0.85 1.0
regex: \.[a-zA-Z_][a-zA-Z0-9_.]*
regex_color: 0.3 0.65 0.9 1.0
regex: \b[0-9][0-9a-fA-F]*[hH]\b
regex_color: 0.4 0.7 0.6 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax45 = R"([verilog]
no_single_quote: 1
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
comment_block: /* */
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax47 = R"([diff]
no_strings: 1
regex: (?:^|[\r\n])@@[^\r\n]*@@[^\r\n]*
regex_color: 0.3 0.7 0.9 1.0
regex: (?:^|[\r\n])(?:diff --git|index [0-9a-fA-F]+|\+\+\+ |--- )[^\r\n]*
regex_color: 0.7 0.5 0.85 1.0
regex: (?:^|[\r\n])\+(?!\+\+)[^\r\n]*
regex_color: 0.35 0.75 0.45 1.0
regex: (?:^|[\r\n])-(?!--)[^\r\n]*
regex_color: 0.9 0.4 0.4 1.0
)";
        static constexpr const char* kSyntax48 = R"([log]
no_strings: 1
keyword: INFO INFORMATION NOTICE DEBUG TRACE VERBOSE info information notice debug trace verbose Info Information Notice Debug Trace Verbose
literal: WARN WARNING DEPRECATED CAUTION warn warning deprecated caution Warn Warning Deprecated Caution
regex: "[^"\r\n]*"
regex_color: 0.8 0.55 0.35 1.0
regex: :[0-9]{2}:[0-9]{2}(?:[.,][0-9]+)?(?:Z|[+-][0-9]{2}:?[0-9]{2})?\b
regex_color: 0.4 0.7 0.65 1.0
regex: \b[0-9]{4}[-/][0-9]{2}[-/][0-9]{2}\b
regex_color: 0.4 0.7 0.65 1.0
regex: \b(?:[Ee][Rr][Rr][Oo][Rr]|[Ff][Aa][Tt][Aa][Ll]|[Cc][Rr][Ii][Tt][Ii][Cc][Aa][Ll]|[Ss][Ee][Vv][Ee][Rr][Ee]|[Pp][Aa][Nn][Ii][Cc]|[Ff][Aa][Ii][Ll][Ee][Dd]|[Ff][Aa][Ii][Ll]|[Ee][Xx][Cc][Ee][Pp][Tt][Ii][Oo][Nn]|[Ee][Rr][Rr])\b
regex_color: 0.95 0.35 0.35 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b([0-9]+(\.[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static const char* kSyntax49 = R"([lisp]
no_single_quote: 1
lisp_words: 1
keyword: defun defmacro defvar defparameter defconstant defstruct defclass defgeneric defmethod deftype defpackage in-package defsetf define-modify-macro define-compiler-macro define-symbol-macro define-condition define-method-combination defalias defsubst defcustom defgroup defface defconst defadvice cl-defun cl-defmacro cl-defstruct cl-defmethod cl-defgeneric cl-loop cl-letf cl-letf* cl-flet cl-labels cl-macrolet cl-symbol-macrolet cl-destructuring-bind cl-multiple-value-bind cl-case cl-ecase cl-typecase cl-etypecase cl-return cl-return-from cl-block define define-syntax define-record-type define-values define-library define-struct define-type define-syntax-rule define/contract syntax-rules syntax-case syntax-parse with-syntax datum->syntax syntax->datum defn defn- def defonce defmulti defprotocol defrecord definterface ns fn lambda let let* letrec letrec* let-values let*-values letrec-values let-syntax letrec-syntax letfn fluid-let flet labels macrolet symbol-macrolet destructuring-bind multiple-value-bind multiple-value-call multiple-value-list multiple-value-prog1 multiple-value-setq with-open-file with-open-stream with-output-to-string with-input-from-string with-slots with-accessors with-gensyms with-standard-io-syntax with-package-iterator with-hash-table-iterator with-simple-restart with-compilation-unit with-current-buffer with-temp-buffer with-temp-file with-output-to-temp-buffer with-selected-window with-demoted-errors with-eval-after-load with-open with-redefs with-out-str with-in-str save-excursion save-restriction save-current-buffer save-match-data unwind-protect condition-case handler-case handler-bind restart-case restart-bind ignore-errors if when unless cond condp case typecase etypecase ctypecase ecase ccase pcase pcase-let pcase-let* pcase-lambda pcase-dolist match match* match-let match-lambda do do* dolist dotimes doseq do-symbols do-external-symbols do-all-symbols loop recur repeat while until for for* for/list for*/list for/vector for/hash for/sum for/fold for/first for/and for/or foreach for-each begin begin0 progn prog1 prog2 prog prog* progv block return return-from go tagbody catch throw try finally monitor-enter monitor-exit eval-when locally the declare proclaim declaim provide require use-package import export shadow shadowing-import unexport unintern make-package find-package delete-package module module+ module* struct include load load-file compile compile-file autoload interactive quote function var deref atom swap! reset! compare-and-set! ref dosync alter commute ensure agent send send-off await future promise deliver locking binding setq setq-local setq-default defvar-local setf psetq psetf incf decf push pushnew pop rotatef shiftf remf set! set-car! set-cdr! vector-set! string-set! box unbox set-box! delay force parameterize make-parameter guard dynamic-wind call-with-current-continuation call/cc call-with-values call-with-input-file call-with-output-file with-input-from-file with-output-to-file values values-list eval apply funcall trampoline macroexpand macroexpand-1 complement constantly identity partial comp juxt memoize fnil every-pred some-fn mapcar mapc mapcan maplist mapcon map map-into mapv mapcat map-indexed filter filterv filter-map keep keep-indexed reduce reduce-kv foldl foldr remove remove-if remove-if-not delete delete-if delete-if-not remove-duplicates delete-duplicates substitute find find-if position count sort stable-sort merge every some notany notevery andmap ormap cons car cdr caar cadr cdar cddr caaar caadr cadar caddr cdaar cdadr cddar cdddr first second third fourth fifth sixth seventh eighth ninth tenth rest last butlast nth nthcdr append nconc reverse nreverse list list* make-list copy-list copy-alist copy-tree assoc rassoc member adjoin union intersection set-difference subsetp acons pairlis vector make-array adjust-array aref svref elt length subseq copy-seq fill replace concatenate coerce into conj disj assoc-in dissoc update update-in get-in select-keys keys vals merge-with zipmap vec set hash-map array-map sorted-map sorted-set subvec peek empty seq lazy-seq lazy-cat repeatedly iterate cycle range take take-while take-nth drop drop-while drop-last split-at split-with partition partition-all partition-by group-by frequencies interleave interpose distinct dedupe format prin1 princ print pprint println pr prn pr-str prn-str print-str println-str terpri fresh-line display displayln newline printf fprintf sprintf eprintf read read-line read-char unread-char peek-char read-from-string read-string slurp spit write write-line write-string write-char message user-error error warn signal cerror invoke-restart find-restart compute-restarts abort continue muffle-warning store-value use-value break assert check-type typep subtypep type-of class-of instance? isa? make-instance initialize-instance reinitialize-instance shared-initialize slot-value slot-boundp slot-makunbound slot-exists-p call-next-method next-method-p change-class eq eql equal equalp eq? eqv? equal? identical? distinct? null not and or xor atom consp listp symbolp keywordp numberp integerp rationalp floatp realp complexp characterp stringp vectorp bit-vector-p arrayp packagep functionp compiled-function-p streamp hash-table-p pathnamep zerop plusp minusp oddp evenp null? pair? list? symbol? keyword? number? integer? real? rational? ratio? decimal? float? complex? exact? inexact? zero? positive? negative? pos? neg? pos-int? neg-int? nat-int? odd? even? string? char? vector? procedure? fn? ifn? boolean? port? input-port? output-port? eof-object? nil? some? true? false? empty? not-empty seq? coll? map? set? associative? sequential? counted? reversible? sorted? contains? var? delay? future? realized? string=? string<? string>? string<=? string>=? char=? char<? char>? char<=? char>=? string= string/= string< string> string<= string>= string-equal char= char/= char< char> char<= char>= char-equal gensym gentemp intern make-symbol find-symbol symbol-name symbol-value symbol-function symbol-plist symbol-package boundp fboundp makunbound fmakunbound get getf put get-properties make-hash-table gethash remhash maphash clrhash hash-table-count sxhash -> ->> as-> cond-> cond->> some-> some->> doto if-let when-let if-some when-some if-not when-not when-first + - * / = /= < > <= >= 1+ 1- inc dec max min abs mod rem quot quotient remainder modulo floor ceiling truncate round sin cos tan asin acos atan sqrt isqrt exp expt log ash logand logior logxor lognot bit-and bit-or bit-xor bit-not bit-shift-left bit-shift-right str subs concat substring string-append string-length string-ref vector-length vector-ref make-vector make-string #lang
literal: t nil true false #t #f #true #false #nil &optional &rest &key &body &aux &allow-other-keys &whole &environment &
comment_line: ;
comment_block: #| |#
regex: (?:#:|:)[a-zA-Z0-9_+\-*/?!<>=&$%^~.@]+
regex_color: 0.75 0.5 0.85 1.0
regex: (?:#'|')[a-zA-Z_+\-*/?!<>=&$%^~][a-zA-Z0-9_+\-*/?!<>=&$%^~.@]*
regex_color: 0.85 0.65 0.35 1.0
regex: \*[a-zA-Z0-9_+\-*/?!<>=&$%^~]+\*
regex_color: 0.35 0.75 0.85 1.0
regex: \+[a-zA-Z0-9_+\-*/?!<>=&$%^~]+\+
regex_color: 0.35 0.75 0.85 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax50 = R"([objc]
keyword: alignas alignof and and_eq asm auto bitand bitor bool break case catch char char16_t char32_t class compl const constexpr const_cast continue decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq id Class SEL IMP BOOL instancetype super self in out inout bycopy byref oneway strong weak unsafe_unretained assign retain copy readonly readwrite nonatomic atomic nullable nonnull null_unspecified null_resettable __weak __strong __unsafe_unretained __autoreleasing __bridge __bridge_transfer __bridge_retained __block NSObject NSString NSMutableString NSArray NSMutableArray NSDictionary NSMutableDictionary NSSet NSMutableSet NSNumber NSData NSMutableData NSError NSURL NSURLRequest NSURLSession NSBundle NSNotificationCenter NSUserDefaults NSFileManager NSThread NSOperationQueue dispatch_queue_t NSInteger NSUInteger CGFloat CGRect CGPoint CGSize NSRange UIView UIViewController UIWindow UIApplication NSView NSViewController NSWindow NSApplication
literal: YES NO true false NULL nil Nil nullptr
comment_line: //
comment_block: /* */
regex: #[ \t]*(?:include|import)[ \t]*[<"][^>"]+[>"]
regex_color: 0.8 0.4 0.2 1.0
regex: #[ \t]*[a-zA-Z0-9_]+
regex_color: 0.6 0.4 0.7 1.0
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.75 0.45 0.85 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLuU]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax51 = R"([groovy]
keyword: as assert break case catch class const continue def default do else enum extends finally for goto if implements import in instanceof interface new package return super switch this throw throws trait try var while public protected private static final abstract synchronized native transient volatile strictfp void boolean byte char short int long float double String Object Closure List Map Set Range BigDecimal BigInteger File plugins buildscript repositories dependencies android defaultConfig buildTypes signingConfigs productFlavors compileOptions kotlinOptions sourceSets allprojects subprojects ext tasks register named implementation api compileOnly runtimeOnly testImplementation androidTestImplementation debugImplementation releaseImplementation annotationProcessor kapt ksp classpath google mavenCentral gradlePluginPortal maven apply plugin id version group description namespace compileSdk minSdk targetSdk versionCode versionName pipeline agent stages stage steps post environment options parameters triggers tools when parallel script sh bat echo checkout always success failure
literal: true false null it
comment_line: //
comment_block: /* */
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.75 0.5 0.85 1.0
regex: \$\{[^}\r\n]+\}|\$[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.35 0.75 0.85 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLdD]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax52 = R"([ocaml]
no_single_quote: 1
keyword: and as assert asr begin class constraint do done downto else end exception external for fun function functor if in include inherit initializer land lazy let lor lsl lsr lxor match method mod module mutable new nonrec object of open or private rec sig struct then to try type val virtual when while with abstype datatype eqtype fn handle infix infixr local op orelse andalso raise sharing structure where withtype switch pri pub external int float bool char string bytes unit list array option result ref exn seq Hashtbl Map Set Array List String Char Int Float Printf Format Lwt Async
literal: true false None Some Ok Error
comment_block: (* *)
regex: '(?:\\.|[^'\\\r\n])'
regex_color: 0.64 0.08 0.08 1.0
regex: '[a-zA-Z_][a-zA-Z0-9_]*\b
regex_color: 0.85 0.65 0.35 1.0
regex: \b[A-Z][a-zA-Z0-9_]*\b
regex_color: 0.3 0.65 0.85 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax53 = R"([erlang]
keyword: after and andalso band begin bnot bor bsl bsr bxor case catch cond div end fun if let not of or orelse receive rem try when xor module export import compile record define undef ifdef ifndef else endif include include_lib behaviour behavior callback spec type opaque export_type spawn spawn_link spawn_monitor self register unregister whereis send exit error throw apply is_atom is_binary is_bitstring is_boolean is_float is_function is_integer is_list is_map is_number is_pid is_port is_record is_reference is_tuple hd tl length size byte_size bit_size tuple_size map_size element setelement make_ref node nodes monitor demonitor link unlink trap_exit process_flag erlang lists maps io file ets dets mnesia gen_server gen_statem gen_event supervisor application sys timer proplists binary re string
literal: true false ok undefined infinity
comment_line: %
regex: -(?:module|export|import|compile|record|define|undef|ifdef|ifndef|else|endif|include|include_lib|behaviour|behavior|callback|spec|type|opaque|export_type)\b
regex_color: 0.75 0.5 0.85 1.0
regex: \?[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.85 0.6 0.3 1.0
regex: \b[A-Z_][a-zA-Z0-9_@]*\b
regex_color: 0.35 0.7 0.85 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax54 = R"([fortran]
keyword: program end subroutine function module submodule contains use only implicit none integer real double precision complex logical character type class dimension allocatable pointer target intent in out inout optional parameter save public private protected intrinsic external interface abstract procedure generic operator assignment allocate deallocate nullify if then else elseif select case default do while concurrent cycle exit continue goto return stop error call read write print open close inquire format rewind backspace flush where elsewhere forall pure elemental recursive result bind common equivalence data namelist sequence extends pass nopass deferred non_overridable associate block critical sync all images memory codimension PROGRAM END SUBROUTINE FUNCTION MODULE SUBMODULE CONTAINS USE ONLY IMPLICIT NONE INTEGER REAL DOUBLE PRECISION COMPLEX LOGICAL CHARACTER TYPE CLASS DIMENSION ALLOCATABLE POINTER TARGET INTENT IN OUT INOUT OPTIONAL PARAMETER SAVE PUBLIC PRIVATE PROTECTED INTRINSIC EXTERNAL INTERFACE ABSTRACT PROCEDURE GENERIC OPERATOR ASSIGNMENT ALLOCATE DEALLOCATE NULLIFY IF THEN ELSE ELSEIF SELECT CASE DEFAULT DO WHILE CONCURRENT CYCLE EXIT CONTINUE GOTO RETURN STOP ERROR CALL READ WRITE PRINT OPEN CLOSE INQUIRE FORMAT REWIND BACKSPACE FLUSH WHERE ELSEWHERE FORALL PURE ELEMENTAL RECURSIVE RESULT BIND COMMON EQUIVALENCE DATA NAMELIST SEQUENCE
literal: true false TRUE FALSE
comment_line: !
regex: \.(?:true|false|and|or|not|eqv|neqv|eq|ne|lt|le|gt|ge|TRUE|FALSE|AND|OR|NOT|EQV|NEQV|EQ|NE|LT|LE|GT|GE)\.
regex_color: 0.75 0.5 0.85 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax55 = R"([cobol]
lisp_words: 1
keyword: IDENTIFICATION ID DIVISION PROGRAM-ID ENVIRONMENT CONFIGURATION SOURCE-COMPUTER OBJECT-COMPUTER SPECIAL-NAMES INPUT-OUTPUT FILE-CONTROL I-O-CONTROL DATA FILE WORKING-STORAGE LOCAL-STORAGE LINKAGE REPORT SCREEN SECTION FD SD PIC PICTURE VALUE VALUES FILLER REDEFINES OCCURS TIMES DEPENDING ON ASCENDING DESCENDING KEY INDEXED BY USAGE DISPLAY COMP COMP-1 COMP-2 COMP-3 COMP-4 COMP-5 COMPUTATIONAL BINARY PACKED-DECIMAL INDEX POINTER JUSTIFIED BLANK WHEN SIGN LEADING TRAILING SEPARATE SYNCHRONIZED SYNC EXTERNAL GLOBAL PROCEDURE DECLARATIVES END-DECLARATIVES USING RETURNING GIVING ACCEPT ADD TO GIVING ON-SIZE-ERROR NOT-ON-SIZE-ERROR END-ADD ALTER CALL CANCEL CLOSE COMPUTE END-COMPUTE CONTINUE COPY DELETE END-DELETE DIVIDE INTO REMAINDER END-DIVIDE ENTRY EVALUATE ALSO THRU THROUGH OTHER END-EVALUATE EXIT GOBACK GO TO IF THEN ELSE NEXT SENTENCE END-IF INITIALIZE INSPECT TALLYING REPLACING CONVERTING LEADING FIRST ALL INITIAL BEFORE AFTER MERGE MOVE CORRESPONDING CORR MULTIPLY END-MULTIPLY OPEN INPUT OUTPUT I-O EXTEND PERFORM VARYING FROM UNTIL WITH TEST END-PERFORM READ AT END NOT-AT-END INVALID-KEY NOT-INVALID-KEY END-READ RELEASE RETURN REWRITE END-REWRITE SEARCH END-SEARCH SET SORT START END-START STOP RUN STRING DELIMITED SIZE END-STRING SUBTRACT END-SUBTRACT UNSTRING END-UNSTRING WRITE ADVANCING PAGE LINES END-WRITE XML PARSE GENERATE JSON identification division program-id environment configuration input-output file-control data file working-storage local-storage linkage procedure section pic picture value filler redefines occurs usage comp comp-3 perform varying until if then else end-if evaluate when other end-evaluate move to compute add subtract multiply divide display accept open close read write rewrite delete call using goback stop run exit
literal: ZERO ZEROS ZEROES SPACE SPACES HIGH-VALUE HIGH-VALUES LOW-VALUE LOW-VALUES QUOTE QUOTES NULL NULLS TRUE FALSE zero zeros zeroes space spaces true false null
comment_line: *>
regex: (?:^|[\r\n])[0-9 ]{0,6}\*[^\r\n]*
regex_color: 0.42 0.6 0.33 1.0
regex: \b([0-9]+(\.[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax56 = R"([prisma]
keyword: datasource generator model enum type view provider url directUrl shadowDatabaseUrl previewFeatures output binaryTargets engineType relationMode extensions schemas String Boolean Int BigInt Float Decimal DateTime Json Bytes Unsupported
literal: true false null now autoincrement cuid uuid ulid dbgenerated env Cascade Restrict NoAction SetNull SetDefault Asc Desc
comment_line: //
regex: @@?[a-zA-Z_][a-zA-Z0-9_.]*
regex_color: 0.75 0.5 0.85 1.0
regex: \b([0-9]+(\.[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax57 = R"([solidity]
keyword: pragma solidity abicoder import as from contract interface library abstract is using for struct enum event error modifier function constructor fallback receive external public internal private pure view payable constant immutable anonymous indexed virtual override returns memory storage calldata transient if else while do break continue return try catch revert require assert emit new delete unchecked assembly let switch case default leave mapping bool string bytes byte address int int8 int16 int24 int32 int64 int128 int256 uint uint8 uint16 uint24 uint32 uint64 uint128 uint256 bytes1 bytes2 bytes4 bytes8 bytes16 bytes20 bytes32 ufixed fixed wei gwei ether seconds minutes hours days weeks keccak256 sha256 ripemd160 ecrecover addmod mulmod selfdestruct
literal: true false this super msg block tx abi type
comment_line: //
comment_block: /* */
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax58 = R"([nim]
keyword: addr and as asm bind block break case cast concept const continue converter defer discard distinct div do elif else end enum except export finally for from func if import in include interface iterator let macro method mixin mod nil not notin object of or out proc ptr raise ref return shl shr static template try tuple type using var when while xor yield int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 float float32 float64 bool char string cstring pointer ByteAddress Natural Positive seq array openArray varargs set HashSet Table OrderedTable Option Future void auto echo assert doAssert
literal: true false nil result
comment_line: #
comment_block: #[ ]#
regex: \{\.[a-zA-Z0-9_:, \t]+\.\}
regex_color: 0.75 0.5 0.85 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([a-zA-Z0-9_]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax59 = R"([d]
keyword: abstract alias align asm assert auto body bool break byte case cast catch cdouble cent cfloat char class const continue creal dchar debug default delegate delete deprecated do double else enum export extern final finally float for foreach foreach_reverse function goto idouble if ifloat immutable import in inout int interface invariant ireal is lazy long macro mixin module new nothrow out override package pragma private protected public pure real ref return scope shared short static struct super switch synchronized template this throw try typedef typeid typeof ubyte ucent uint ulong union unittest ushort version void wchar while with __gshared __traits __vector __parameters string wstring dstring size_t ptrdiff_t Object Throwable Exception Error
literal: true false null __FILE__ __LINE__ __MODULE__ __FUNCTION__ __PRETTY_FUNCTION__
comment_line: //
comment_block: /* */
regex: @[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.75 0.5 0.85 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?([fFlLuU]*))\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax60 = R"([pascal]
keyword: and array as asm begin case class const constructor destructor dispinterface div do downto else end except exports file finalization finally for function goto if implementation in inherited initialization inline interface is label library mod not object of on operator or out packed procedure program property raise record repeat resourcestring set shl shr string then threadvar to try type unit until uses var while with xor private protected public published virtual override overload reintroduce abstract dynamic message stdcall cdecl pascal register safecall forward external read write default nodefault stored index dispid Integer Cardinal ShortInt SmallInt LongInt Int64 UInt64 NativeInt NativeUInt Byte Word LongWord DWORD QWord Single Double Extended Comp Currency Real Boolean ByteBool WordBool LongBool Char AnsiChar WideChar ShortString AnsiString WideString UnicodeString String Variant OleVariant Pointer PChar TObject TClass TInterfacedObject TPersistent TComponent TStrings TStringList TList TStream TMemoryStream TFileStream Exception SysUtils Classes Forms Controls Graphics Dialogs Windows Messages
literal: true false nil True False Nil Self Result TRUE FALSE NIL
comment_line: //
comment_block: { }
regex: \$[0-9a-fA-F]+
regex_color: 0.4 0.7 0.6 1.0
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax61 = R"([ada]
no_single_quote: 1
keyword: abort abs abstract accept access aliased all and array at begin body case constant declare delay delta digits do else elsif end entry exception exit for function generic goto if in interface is limited loop mod new not null of or others out overriding package pragma private procedure protected raise range record rem renames requeue return reverse select separate some subtype synchronized tagged task terminate then type until use when while with xor Integer Natural Positive Float Long_Float Short_Float Boolean Character Wide_Character String Wide_String Duration System Ada Text_IO Interfaces
literal: True False null true false NULL TRUE FALSE
comment_line: --
regex: '[^'\r\n]'
regex_color: 0.64 0.08 0.08 1.0
regex: '[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.75 0.5 0.85 1.0
regex: \b([0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax62 = R"([prolog]
keyword: module use_module ensure_loaded dynamic multifile discontiguous volatile meta_predicate op initialization is mod rem div rdiv abs sign max min sqrt sin cos tan exp log not once ignore call apply catch throw findall bagof setof forall aggregate aggregate_all member memberchk append prefix suffix select nth0 nth1 last length reverse permutation flatten sort msort keysort numlist between succ plus functor arg univ copy_term term_variables subsumes_term unify_with_occurs_check var nonvar atom integer float rational number atomic compound callable ground cyclic_term acyclic_term is_list assert asserta assertz retract retractall abolish clause current_predicate current_op consult reconsult listing open close read write writeln writeq write_canonical print format nl get_char get_code put_char put_code halt abort trace notrace spy nospy
literal: true fail false !
comment_line: %
comment_block: /* */
regex: :-|\?-->|-->
regex_color: 0.75 0.5 0.85 1.0
regex: \b[A-Z_][a-zA-Z0-9_]*\b
regex_color: 0.35 0.7 0.85 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax63 = R"([vim]
keyword: let unlet const set setlocal setglobal setfiletype if elseif else endif while endwhile for in endfor try catch finally endtry throw break continue return finish function endfunction endfunc func def enddef call execute eval echo echon echomsg echoerr echohl silent unsilent verbose normal augroup autocmd au doautocmd command com delcommand map nmap vmap xmap smap omap imap cmap tmap lmap noremap nnoremap vnoremap xnoremap snoremap onoremap inoremap cnoremap tnoremap lnoremap unmap nunmap vunmap xunmap sunmap ounmap iunmap cunmap tunmap mapclear abbreviate ab iabbrev cabbrev unabbreviate syntax syn highlight hi clear filetype plugin indent on off source runtime packadd packloadall colorscheme colo sign match highlight normal! set! let! function! command! Plug vim9script export import var final static abstract class endclass interface endinterface enum endenum type
literal: true false null none v:true v:false v:null v:none v:val v:key v:count v:version v:shell_error
comment_line: "
regex: \b(?:g|b|w|t|s|l|a|v):[a-zA-Z0-9_]+
regex_color: 0.35 0.75 0.85 1.0
regex: &[a-zA-Z_][a-zA-Z0-9_]*
regex_color: 0.75 0.5 0.85 1.0
regex: <[a-zA-Z0-9_-]+>
regex_color: 0.85 0.6 0.3 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        static constexpr const char* kSyntax64 = R"([nix]
keyword: let in if then else assert with rec inherit or import builtins derivation mkDerivation stdenv lib pkgs config options modulesPath inputs outputs description inputsFrom buildInputs nativeBuildInputs propagatedBuildInputs checkInputs nativeCheckInputs patches postPatch preConfigure configureFlags buildPhase checkPhase installPhase postInstall fixupPhase doCheck src pname name version url sha256 hash rev ref owner repo fetchurl fetchzip fetchgit fetchTarball fetchFromGitHub fetchFromGitLab callPackage override overrideAttrs overrideDerivation mkShell mkIf mkMerge mkOption mkEnableOption mkPackageOption mkForce mkDefault mkBefore mkAfter types writeText writeScript writeShellScript writeShellScriptBin writeShellApplication runCommand symlinkJoin buildEnv flake-utils nixpkgs home-manager darwin
literal: true false null
comment_line: #
comment_block: /* */
regex: \$\{[^}\r\n]+\}
regex_color: 0.85 0.6 0.3 1.0
regex: [a-zA-Z0-9_.-]+[ \t]*(?==[^=])
regex_color: 0.35 0.7 0.85 1.0
regex: (https?|ftp)://[^\s/$.?#].[^\s]*
regex_color: 0.2 0.6 0.8 1.0
regex: \b(0x[0-9a-fA-F]+|[0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?)\b
regex_color: 0.4 0.7 0.6 1.0
)";
        struct SyntaxEntry { const wchar_t* ext; const char* syntax; };
        static const SyntaxEntry syntaxTable[] = {
            { L"mmd", kSyntax24 }, { L"mermaid", kSyntax24 },
            { L"cpp", kSyntax0 }, { L"c", kSyntax0 }, { L"cc", kSyntax0 }, { L"cxx", kSyntax0 }, { L"c++", kSyntax0 }, { L"cp", kSyntax0 }, { L"h", kSyntax0 }, { L"hpp", kSyntax0 }, { L"hxx", kSyntax0 }, { L"hh", kSyntax0 }, { L"inl", kSyntax0 }, { L"ipp", kSyntax0 }, { L"tpp", kSyntax0 }, { L"rc", kSyntax0 }, { L"ino", kSyntax0 }, { L"cu", kSyntax0 }, { L"cuh", kSyntax0 },
            { L"cs", kSyntax1 }, { L"csx", kSyntax1 },
            { L"py", kSyntax2 }, { L"pyw", kSyntax2 }, { L"pyi", kSyntax2 }, { L"pyx", kSyntax2 }, { L"pxd", kSyntax2 }, { L"pxi", kSyntax2 }, { L"bzl", kSyntax2 }, { L"bazel", kSyntax2 }, { L"star", kSyntax2 }, { L"gyp", kSyntax2 }, { L"gypi", kSyntax2 }, { L"wsgi", kSyntax2 }, { L"build", kSyntax2 }, { L"workspace", kSyntax2 }, { L"sconstruct", kSyntax2 }, { L"sconscript", kSyntax2 }, { L"tiltfile", kSyntax2 },
            { L"js", kSyntax3 }, { L"ts", kSyntax3 }, { L"jsx", kSyntax3 }, { L"tsx", kSyntax3 }, { L"mjs", kSyntax3 }, { L"cjs", kSyntax3 }, { L"mts", kSyntax3 }, { L"cts", kSyntax3 }, { L"es6", kSyntax3 }, { L"pac", kSyntax3 },
            { L"json", kSyntax4 }, { L"jsonc", kSyntax4 }, { L"jsonl", kSyntax4 }, { L"ndjson", kSyntax4 }, { L"geojson", kSyntax4 }, { L"webmanifest", kSyntax4 }, { L"har", kSyntax4 }, { L"prettierrc", kSyntax4 }, { L"babelrc", kSyntax4 }, { L"eslintrc", kSyntax4 }, { L"stylelintrc", kSyntax4 }, { L"hintrc", kSyntax4 }, { L"swcrc", kSyntax4 },
            { L"html", kSyntax5 }, { L"htm", kSyntax5 }, { L"xhtml", kSyntax5 }, { L"shtml", kSyntax5 }, { L"xml", kSyntax5 }, { L"svg", kSyntax5 }, { L"xsl", kSyntax5 }, { L"xslt", kSyntax5 }, { L"xsd", kSyntax5 }, { L"wsdl", kSyntax5 }, { L"rng", kSyntax5 }, { L"dtd", kSyntax5 }, { L"rss", kSyntax5 }, { L"atom", kSyntax5 }, { L"mathml", kSyntax5 }, { L"vue", kSyntax5 }, { L"svelte", kSyntax5 }, { L"astro", kSyntax5 }, { L"vcxproj", kSyntax5 }, { L"csproj", kSyntax5 }, { L"fsproj", kSyntax5 }, { L"vbproj", kSyntax5 }, { L"props", kSyntax5 }, { L"targets", kSyntax5 }, { L"wapproj", kSyntax5 }, { L"appxmanifest", kSyntax5 }, { L"resw", kSyntax5 }, { L"resx", kSyntax5 }, { L"plist", kSyntax5 }, { L"storyboard", kSyntax5 }, { L"xib", kSyntax5 }, { L"xaml", kSyntax5 }, { L"fxml", kSyntax5 }, { L"glade", kSyntax5 }, { L"ui", kSyntax5 }, { L"qrc", kSyntax5 }, { L"nuspec", kSyntax5 }, { L"config", kSyntax5 }, { L"manifest", kSyntax5 },
            { L"css", kSyntax6 }, { L"scss", kSyntax6 }, { L"sass", kSyntax6 }, { L"less", kSyntax6 }, { L"styl", kSyntax6 }, { L"pcss", kSyntax6 }, { L"postcss", kSyntax6 }, { L"wxss", kSyntax6 },
            { L"md", kSyntax7 }, { L"markdown", kSyntax7 }, { L"mdx", kSyntax7 }, { L"mkd", kSyntax7 }, { L"mdown", kSyntax7 }, { L"rmd", kSyntax7 }, { L"qmd", kSyntax7 },
            { L"rs", kSyntax8 }, { L"go", kSyntax9 }, { L"java", kSyntax10 },
            { L"ps1", kSyntax11 }, { L"psm1", kSyntax11 }, { L"psd1", kSyntax11 },
            { L"bat", kSyntax12 }, { L"cmd", kSyntax12 },
            { L"sh", kSyntax13 }, { L"bash", kSyntax13 }, { L"zsh", kSyntax13 }, { L"fish", kSyntax13 }, { L"ksh", kSyntax13 }, { L"csh", kSyntax13 }, { L"tcsh", kSyntax13 }, { L"ebuild", kSyntax13 }, { L"apkbuild", kSyntax13 }, { L"bashrc", kSyntax13 }, { L"bash_profile", kSyntax13 }, { L"zshrc", kSyntax13 }, { L"zprofile", kSyntax13 }, { L"zshenv", kSyntax13 }, { L"zlogin", kSyntax13 }, { L"zlogout", kSyntax13 }, { L"profile", kSyntax13 }, { L"cshrc", kSyntax13 }, { L"tcshrc", kSyntax13 }, { L"kshrc", kSyntax13 }, { L"pkgbuild", kSyntax13 },
            { L"sql", kSyntax14 }, { L"php", kSyntax15 }, { L"phtml", kSyntax15 },
            { L"rb", kSyntax16 }, { L"ruby", kSyntax16 }, { L"gemspec", kSyntax16 }, { L"ru", kSyntax16 }, { L"rake", kSyntax16 }, { L"thor", kSyntax16 }, { L"jbuilder", kSyntax16 }, { L"podspec", kSyntax16 }, { L"gemfile", kSyntax16 }, { L"rakefile", kSyntax16 }, { L"podfile", kSyntax16 }, { L"fastfile", kSyntax16 }, { L"appfile", kSyntax16 }, { L"matchfile", kSyntax16 }, { L"deliverfile", kSyntax16 }, { L"gymfile", kSyntax16 }, { L"scanfile", kSyntax16 }, { L"snapfile", kSyntax16 }, { L"brewfile", kSyntax16 }, { L"vagrantfile", kSyntax16 }, { L"berksfile", kSyntax16 }, { L"guardfile", kSyntax16 }, { L"capfile", kSyntax16 }, { L"thorfile", kSyntax16 }, { L"dangerfile", kSyntax16 },
            { L"swift", kSyntax17 },
            { L"kt", kSyntax18 }, { L"kts", kSyntax18 }, { L"kotlin", kSyntax18 },
            { L"lua", kSyntax19 },
            { L"ini", kSyntax20 }, { L"inf", kSyntax20 }, { L"cfg", kSyntax20 }, { L"editorconfig", kSyntax20 }, { L"gitconfig", kSyntax20 }, { L"gitmodules", kSyntax20 }, { L"npmrc", kSyntax20 }, { L"yarnrc", kSyntax20 }, { L"flake8", kSyntax20 }, { L"pylintrc", kSyntax20 }, { L"coveragerc", kSyntax20 }, { L"service", kSyntax20 }, { L"socket", kSyntax20 }, { L"timer", kSyntax20 },
            { L"conf", kSyntax21 }, { L"yml", kSyntax21 }, { L"yaml", kSyntax21 }, { L"gitignore", kSyntax21 }, { L"gitattributes", kSyntax21 }, { L"dockerignore", kSyntax21 }, { L"npmignore", kSyntax21 }, { L"eslintignore", kSyntax21 }, { L"prettierignore", kSyntax21 }, { L"clang-format", kSyntax21 }, { L"clang-tidy", kSyntax21 }, { L"yamllint", kSyntax21 }, { L"htaccess", kSyntax21 },
            { L"tex", kSyntax22 }, { L"latex", kSyntax22 }, { L"bib", kSyntax22 }, { L"sty", kSyntax22 }, { L"cls", kSyntax22 },
            { L"dart", kSyntax25 },
            { L"r", kSyntax26 },
            { L"zig", kSyntax27 },
            { L"scala", kSyntax28 }, { L"sc", kSyntax28 }, { L"sbt", kSyntax28 },
            { L"pl", kSyntax29 }, { L"pm", kSyntax29 }, { L"t", kSyntax29 },
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
            { L"mk", kSyntax41 }, { L"mak", kSyntax41 }, { L"makefile", kSyntax41 }, { L"gnumakefile", kSyntax41 }, { L"bsdmakefile", kSyntax41 }, { L"justfile", kSyntax41 },
            { L"reg", kSyntax42 },
            { L"hlsl", kSyntax43 }, { L"fx", kSyntax43 }, { L"glsl", kSyntax43 }, { L"vert", kSyntax43 }, { L"frag", kSyntax43 }, { L"comp", kSyntax43 }, { L"geom", kSyntax43 }, { L"tesc", kSyntax43 }, { L"tese", kSyntax43 }, { L"wgsl", kSyntax43 }, { L"metal", kSyntax43 }, { L"vsh", kSyntax43 }, { L"fsh", kSyntax43 }, { L"glslv", kSyntax43 }, { L"glslf", kSyntax43 },
            { L"asm", kSyntax44 }, { L"s", kSyntax44 }, { L"inc", kSyntax44 }, { L"nasm", kSyntax44 }, { L"yasm", kSyntax44 }, { L"masm", kSyntax44 }, { L"s51", kSyntax44 }, { L"a51", kSyntax44 }, { L"avr", kSyntax44 }, { L"mips", kSyntax44 }, { L"riscv", kSyntax44 }, { L"wat", kSyntax44 }, { L"wast", kSyntax44 },
            { L"v", kSyntax45 }, { L"sv", kSyntax45 }, { L"svh", kSyntax45 }, { L"vh", kSyntax45 },
            { L"vhd", kSyntax46 }, { L"vhdl", kSyntax46 },
            { L"diff", kSyntax47 }, { L"patch", kSyntax47 },
            { L"log", kSyntax48 },
            { L"lisp", kSyntax49 }, { L"lsp", kSyntax49 }, { L"cl", kSyntax49 }, { L"asd", kSyntax49 }, { L"ros", kSyntax49 },
            { L"el", kSyntax49 }, { L"emacs", kSyntax49 }, { L"sbclrc", kSyntax49 }, { L"clisprc", kSyntax49 }, { L"guile", kSyntax49 },
            { L"scm", kSyntax49 }, { L"ss", kSyntax49 }, { L"sld", kSyntax49 },
            { L"rkt", kSyntax49 },
            { L"clj", kSyntax49 }, { L"cljs", kSyntax49 }, { L"cljc", kSyntax49 }, { L"edn", kSyntax49 },
            { L"hy", kSyntax49 }, { L"fnl", kSyntax49 }, { L"janet", kSyntax49 },
            { L"m", kSyntax50 }, { L"mm", kSyntax50 },
            { L"groovy", kSyntax51 }, { L"gvy", kSyntax51 }, { L"gy", kSyntax51 }, { L"gsh", kSyntax51 }, { L"gradle", kSyntax51 }, { L"jenkinsfile", kSyntax51 },
            { L"ml", kSyntax52 }, { L"mli", kSyntax52 }, { L"sml", kSyntax52 }, { L"sig", kSyntax52 }, { L"fun", kSyntax52 }, { L"re", kSyntax52 }, { L"rei", kSyntax52 }, { L"res", kSyntax52 }, { L"resi", kSyntax52 },
            { L"erl", kSyntax53 }, { L"hrl", kSyntax53 }, { L"escript", kSyntax53 },
            { L"f", kSyntax54 }, { L"for", kSyntax54 }, { L"f77", kSyntax54 }, { L"f90", kSyntax54 }, { L"f95", kSyntax54 }, { L"f03", kSyntax54 }, { L"f08", kSyntax54 }, { L"f18", kSyntax54 }, { L"fpp", kSyntax54 },
            { L"cob", kSyntax55 }, { L"cbl", kSyntax55 }, { L"cpy", kSyntax55 }, { L"cobol", kSyntax55 },
            { L"prisma", kSyntax56 },
            { L"sol", kSyntax57 },
            { L"nim", kSyntax58 }, { L"nims", kSyntax58 }, { L"nimble", kSyntax58 },
            { L"d", kSyntax59 }, { L"di", kSyntax59 },
            { L"pas", kSyntax60 }, { L"dpr", kSyntax60 }, { L"dpk", kSyntax60 }, { L"pp", kSyntax60 }, { L"lpr", kSyntax60 },
            { L"adb", kSyntax61 }, { L"ads", kSyntax61 }, { L"ada", kSyntax61 },
            { L"pro", kSyntax62 }, { L"prolog", kSyntax62 }, { L"yap", kSyntax62 },
            { L"vim", kSyntax63 }, { L"vimrc", kSyntax63 }, { L"_vimrc", kSyntax63 }, { L"gvimrc", kSyntax63 }, { L"_gvimrc", kSyntax63 }, { L"exrc", kSyntax63 },
            { L"nix", kSyntax64 }
        };
        std::wstring lowerExt = ext;
        for (auto& c : lowerExt) c = toLowerAsciiW(c);
        if (!lowerExt.empty() && lowerExt[0] == L'.') lowerExt.erase(0, 1);
        const char* targetSyntaxPtr = kSyntax23;
        for (const auto& entry : syntaxTable) {
            if (lowerExt == entry.ext) {
                targetSyntaxPtr = entry.syntax;
                break;
            }
        }
        thread_local std::vector<std::pair<const char*, SyntaxDef>> compiledSyntaxCache;
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
            if (line.size() >= 3 && line.front() == '[' && line.back() == ']' && parsed.lang.empty()) {
                parsed.lang = line.substr(1, line.size() - 2);
                continue;
            }
            if (line.find("keyword: ") == 0) {
                size_t p = 9; std::string w;
                while (nextWord(line, p, w)) parsed.keywords.insert(w);
            }
            if (line.find("literal: ") == 0) {
                size_t p = 9; std::string w;
                while (nextWord(line, p, w)) parsed.literals.insert(w);
            }
            if (line == "no_strings: 1") parsed.disableStrings = true;
            if (line == "no_single_quote: 1") parsed.disableSingleQuoteStrings = true;
            if (line == "lisp_words: 1") parsed.lispWords = true;
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
                    try {
                        std::string cleanPat = lastRegex;
                        std::regex_constants::syntax_option_type rxFlags = std::regex_constants::optimize;
                        for (;;) {
                            size_t icPos = cleanPat.find("(?i)");
                            if (icPos == std::string::npos) break;
                            cleanPat.erase(icPos, 4);
                            rxFlags |= std::regex_constants::icase;
                        }
                        parsed.regexes.push_back({ std::regex(cleanPat, rxFlags), SyntaxColor{r, g, b, a} });
                        parsed.ruleMetas.push_back(classifySyntaxRegex(cleanPat));
                    } catch (const std::regex_error&) {}
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
    int initialBlockState,
    ApplySpanFn&& applySpan)
{
    if (visibleText.empty()) return;
    const size_t vLen = visibleText.size();
    const char* vData = visibleText.data();
    const bool isLispSyntax = (currentSyntax.lang == "lisp" || (currentSyntax.commentBlockStart == "#|" && currentSyntax.commentBlockEnd == "|#"));
    auto isStdWordChar = [](char c) noexcept -> bool { return std::isalnum((unsigned char)c) || c == '_'; };
    auto isLispWordChar = [](char c) noexcept -> bool {
        return std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == '*' || c == '+' ||
               c == '!' || c == '?' || c == '/' || c == '<' || c == '>' || c == '=' || c == '&' || c == '#';
    };
    auto isWChar = [&](char c) noexcept -> bool { return currentSyntax.lispWords ? isLispWordChar(c) : isStdWordChar(c); };
    auto isWordChar = [&](char c) noexcept -> bool { return currentSyntax.lispWords ? isLispWordChar(c) : isStdWordChar(c); };
    for (size_t i = 0; i < currentSyntax.regexes.size(); ++i) {
        SyntaxRuleMeta meta = (i < currentSyntax.ruleMetas.size()) ? currentSyntax.ruleMetas[i] : SyntaxRuleMeta{};
        if (meta.reqChar1 != 0 && visibleText.find(meta.reqChar1) == std::string::npos) continue;
        if (meta.reqChar2 != 0 && visibleText.find(meta.reqChar2) == std::string::npos) continue;

        auto setEffect = [&](size_t start, size_t len) {
            if (len > 0) applySpan(start, len, (int)i);
        };

        if (meta.kind >= SyntaxRuleKind::NumberCpp && meta.kind <= SyntaxRuleKind::NumberSimple) {
            const bool allowCppSep = (meta.kind == SyntaxRuleKind::NumberCpp);
            size_t p = 0;
            while (p < vLen) {
                unsigned char ch = (unsigned char)vData[p];
                bool isLispSignedOrRadix = false;
                if (isLispSyntax && (p == 0 || (!isWChar(vData[p - 1]) && !hasOddPrecedingBackslashes(visibleText, p) && vData[p - 1] != ':'))) {
                    if ((ch == '+' || ch == '-') && p + 1 < vLen && std::isdigit((unsigned char)vData[p + 1])) {
                        isLispSignedOrRadix = true;
                    } else if (ch == '#' && p + 2 < vLen) {
                        char r = toLowerAsciiC(vData[p + 1]);
                        if ((r == 'x' && std::isxdigit((unsigned char)vData[p + 2])) ||
                            (r == 'b' && (vData[p + 2] == '0' || vData[p + 2] == '1')) ||
                            (r == 'o' && (vData[p + 2] >= '0' && vData[p + 2] <= '7'))) {
                            isLispSignedOrRadix = true;
                        }
                    }
                }
                if (isLispSignedOrRadix || (ch >= '0' && ch <= '9' && (p == 0 || (!isWChar(vData[p - 1]) && !hasOddPrecedingBackslashes(visibleText, p) && (!isLispSyntax || vData[p - 1] != ':'))))) {
                    size_t start = p;
                    bool matchedBase = false;
                    if (isLispSyntax && vData[p] == '#' && p + 2 < vLen) {
                        char r = toLowerAsciiC(vData[p + 1]);
                        p += 2;
                        if (r == 'x') { while (p < vLen && std::isxdigit((unsigned char)vData[p])) p++; matchedBase = true; }
                        else if (r == 'b') { while (p < vLen && (vData[p] == '0' || vData[p] == '1')) p++; matchedBase = true; }
                        else if (r == 'o') { while (p < vLen && (vData[p] >= '0' && vData[p] <= '7')) p++; matchedBase = true; }
                    } else {
                        if (isLispSyntax && (vData[p] == '+' || vData[p] == '-')) p++;
                        if (vData[p] == '0' && p + 2 <= vLen) {
                            char n1 = vData[p + 1];
                            if (meta.kind != SyntaxRuleKind::NumberSimple && meta.kind != SyntaxRuleKind::NumberRust && (n1 == 'x' || n1 == 'X') && p + 2 < vLen && std::isxdigit((unsigned char)vData[p + 2])) {
                                p += 2;
                                while (p < vLen) {
                                    if (std::isxdigit((unsigned char)vData[p])) p++;
                                    else if (allowCppSep && vData[p] == '\'' && p + 1 < vLen && std::isxdigit((unsigned char)vData[p + 1])) p += 2;
                                    else break;
                                }
                                if (meta.kind == SyntaxRuleKind::NumberCpp) {
                                    while (p < vLen && (vData[p]=='l'||vData[p]=='L'||vData[p]=='u'||vData[p]=='U')) p++;
                                }
                                matchedBase = true;
                            } else if ((meta.kind == SyntaxRuleKind::NumberPy || meta.kind == SyntaxRuleKind::NumberCpp) && (n1 == 'b' || (allowCppSep && n1 == 'B')) && p + 2 < vLen && (vData[p + 2] == '0' || vData[p + 2] == '1')) {
                                p += 2;
                                while (p < vLen) {
                                    if (vData[p] == '0' || vData[p] == '1') p++;
                                    else if (allowCppSep && vData[p] == '\'' && p + 1 < vLen && (vData[p + 1] == '0' || vData[p + 1] == '1')) p += 2;
                                    else break;
                                }
                                if (meta.kind == SyntaxRuleKind::NumberCpp) {
                                    while (p < vLen && (vData[p]=='l'||vData[p]=='L'||vData[p]=='u'||vData[p]=='U')) p++;
                                }
                                matchedBase = true;
                            } else if (meta.kind == SyntaxRuleKind::NumberPy && n1 == 'o' && p + 2 < vLen && (vData[p + 2] >= '0' && vData[p + 2] <= '7')) {
                                p += 2;
                                while (p < vLen && (vData[p] >= '0' && vData[p] <= '7')) p++;
                                matchedBase = true;
                            }
                        }
                    }
                    size_t dotPos = std::string::npos;
                    if (!matchedBase) {
                        while (p < vLen) {
                            if (std::isdigit((unsigned char)vData[p])) p++;
                            else if (allowCppSep && vData[p] == '\'' && p + 1 < vLen && std::isdigit((unsigned char)vData[p + 1])) p += 2;
                            else break;
                        }
                        if (currentSyntax.lang == "verilog" && p + 2 < vLen && vData[p] == '\'') {
                            size_t vp = p + 1;
                            if (vp < vLen && (vData[vp] == 's' || vData[vp] == 'S')) vp++;
                            if (vp < vLen) {
                                char vb = toLowerAsciiC(vData[vp]);
                                if (vb == 'b' || vb == 'o' || vb == 'd' || vb == 'h') {
                                    vp++;
                                    size_t vdStart = vp;
                                    while (vp < vLen) {
                                        char vc = vData[vp];
                                        if (std::isxdigit((unsigned char)vc) || vc == '_' || vc == 'x' || vc == 'X' || vc == 'z' || vc == 'Z' || vc == '?') vp++;
                                        else break;
                                    }
                                    if (vp > vdStart) p = vp;
                                }
                            }
                        } else if (isLispSyntax && p + 1 < vLen && vData[p] == '/' && std::isdigit((unsigned char)vData[p + 1])) {
                            p += 2;
                            while (p < vLen && std::isdigit((unsigned char)vData[p])) p++;
                        } else {
                            if (p + 1 < vLen && vData[p] == '.' && std::isdigit((unsigned char)vData[p + 1])) {
                                dotPos = p;
                                p += 2;
                                while (p < vLen) {
                                    if (std::isdigit((unsigned char)vData[p])) p++;
                                    else if (allowCppSep && vData[p] == '\'' && p + 1 < vLen && std::isdigit((unsigned char)vData[p + 1])) p += 2;
                                    else break;
                                }
                            }
                            if (meta.kind != SyntaxRuleKind::NumberSimple && p + 1 < vLen) {
                                char expCh = vData[p];
                                bool isExp = (expCh == 'e' || expCh == 'E') ||
                                             (isLispSyntax && (expCh == 'd' || expCh == 'D' || expCh == 'f' || expCh == 'F' || expCh == 's' || expCh == 'S' || expCh == 'l' || expCh == 'L'));
                                if (isExp) {
                                    size_t ep = p + 1;
                                    if (ep < vLen && (vData[ep] == '+' || vData[ep] == '-')) ep++;
                                    if (ep < vLen && std::isdigit((unsigned char)vData[ep])) {
                                        p = ep + 1;
                                        while (p < vLen) {
                                            if (std::isdigit((unsigned char)vData[p])) p++;
                                            else if (allowCppSep && vData[p] == '\'' && p + 1 < vLen && std::isdigit((unsigned char)vData[p + 1])) p += 2;
                                            else break;
                                        }
                                    }
                                }
                            }
                        }
                        if (isLispSyntax && p < vLen && (vData[p] == 'N' || vData[p] == 'M')) {
                            p++;
                        } else if (meta.kind == SyntaxRuleKind::NumberCpp) {
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
                    } else if (dotPos != std::string::npos && !isLispSyntax) {
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
                if (hasOddPrecedingBackslashes(visibleText, p)) { p++; continue; }
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
                if (hasOddPrecedingBackslashes(visibleText, p)) { p++; continue; }
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
        } else if (meta.kind == SyntaxRuleKind::Url || meta.kind == SyntaxRuleKind::UrlMarkdown) {
            auto isRxSpace = [](char c) noexcept -> bool {
                return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
            };
            size_t searchPos = 0;
            size_t colonPos = 0;
            while ((colonPos = visibleText.find("://", searchPos)) != std::string::npos) {
                size_t schemeStart = std::string::npos;
                if (colonPos >= 5 && std::memcmp(vData + colonPos - 5, "https", 5) == 0) schemeStart = colonPos - 5;
                else if (colonPos >= 4 && std::memcmp(vData + colonPos - 4, "http", 4) == 0) schemeStart = colonPos - 4;
                else if (colonPos >= 3 && std::memcmp(vData + colonPos - 3, "ftp", 3) == 0) schemeStart = colonPos - 3;
                size_t after = colonPos + 3;
                if (schemeStart != std::string::npos && !hasOddPrecedingBackslashes(visibleText, schemeStart)) {
                    if (meta.kind == SyntaxRuleKind::UrlMarkdown) {
                        auto isMdStop = [&](char c) noexcept -> bool {
                            return isRxSpace(c) || c == '<' || c == '>' || c == '"' || c == ')' || c == ']';
                        };
                        if (after < vLen && !isMdStop(vData[after])) {
                            size_t end = after + 1;
                            while (end < vLen && !isMdStop(vData[end])) end++;
                            setEffect(schemeStart, end - schemeStart);
                            searchPos = end;
                            continue;
                        }
                    } else {
                        if (after + 1 < vLen) {
                            char c0 = vData[after];
                            char c1 = vData[after + 1];
                            if (!isRxSpace(c0) && c0 != '/' && c0 != '$' && c0 != '.' && c0 != '?' && c0 != '#' && c1 != '\r' && c1 != '\n') {
                                size_t end = after + 2;
                                while (end < vLen && !isRxSpace(vData[end])) end++;
                                setEffect(schemeStart, end - schemeStart);
                                searchPos = end;
                                continue;
                            }
                        }
                    }
                }
                searchPos = after;
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::Email) {
            auto isEmailLocal = [](char c) noexcept -> bool {
                unsigned char u = (unsigned char)c;
                return std::isalnum(u) || c == '.' || c == '_' || c == '%' || c == '+' || c == '-';
            };
            auto isEmailDom = [](char c) noexcept -> bool {
                unsigned char u = (unsigned char)c;
                return std::isalnum(u) || c == '.' || c == '-';
            };
            size_t searchPos = 0;
            size_t minLocalStart = 0;
            size_t atPos = 0;
            while ((atPos = visibleText.find('@', searchPos)) != std::string::npos) {
                size_t localStart = atPos;
                while (localStart > minLocalStart && isEmailLocal(vData[localStart - 1])) localStart--;
                while (localStart < atPos) {
                    bool prevW = (localStart > 0) && isStdWordChar(vData[localStart - 1]);
                    bool curW = isStdWordChar(vData[localStart]);
                    if (prevW != curW && !hasOddPrecedingBackslashes(visibleText, localStart)) break;
                    localStart++;
                }
                size_t domEnd = atPos + 1;
                size_t lastDot = std::string::npos;
                bool allAlphaSinceDot = false;
                size_t bestValidEnd = 0;
                while (domEnd < vLen && isEmailDom(vData[domEnd])) {
                    char dc = vData[domEnd];
                    if (dc == '.') {
                        if (lastDot != std::string::npos && lastDot > atPos + 1 && allAlphaSinceDot && (domEnd - lastDot - 1) >= 2) {
                            bestValidEnd = domEnd;
                        }
                        lastDot = domEnd;
                        allAlphaSinceDot = true;
                    } else if (!std::isalpha((unsigned char)dc)) {
                        allAlphaSinceDot = false;
                    }
                    domEnd++;
                }
                if (lastDot != std::string::npos && lastDot > atPos + 1 && allAlphaSinceDot &&
                    (domEnd - lastDot - 1) >= 2 && (domEnd == vLen || !isStdWordChar(vData[domEnd]))) {
                    bestValidEnd = domEnd;
                }
                if (localStart < atPos && bestValidEnd > atPos + 1) {
                    setEffect(localStart, bestValidEnd - localStart);
                    minLocalStart = bestValidEnd;
                    searchPos = (domEnd > bestValidEnd) ? domEnd : bestValidEnd;
                } else {
                    minLocalStart = atPos + 1;
                    searchPos = (domEnd > atPos + 1) ? domEnd : (atPos + 1);
                }
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::XmlTag) {
            auto isTagCh = [](char c) noexcept -> bool {
                return std::isalnum((unsigned char)c) || c == ':' || c == '-';
            };
            size_t p = 0;
            while ((p = visibleText.find('<', p)) != std::string::npos) {
                if (hasOddPrecedingBackslashes(visibleText, p)) { p++; continue; }
                size_t q = p + 1;
                if (q < vLen && vData[q] == '/') q++;
                size_t nameStart = q;
                while (q < vLen && isTagCh(vData[q])) q++;
                if (q > nameStart) {
                    if (q < vLen && vData[q] == '>') q++;
                    setEffect(p, q - p);
                    p = q;
                } else {
                    p++;
                }
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::XmlAttrName || meta.kind == SyntaxRuleKind::CssPropName) {
            const char targetDelim = (meta.kind == SyntaxRuleKind::XmlAttrName) ? '=' : ':';
            auto isNameCh = [&](char c) noexcept -> bool {
                unsigned char u = (unsigned char)c;
                if (meta.kind == SyntaxRuleKind::XmlAttrName) return std::isalnum(u) || c == ':' || c == '-';
                return std::isalpha(u) || c == '-';
            };
            auto isRxSpace = [](char c) noexcept -> bool {
                return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
            };
            size_t p = 0;
            while (p < vLen) {
                if (!isNameCh(vData[p])) { p++; continue; }
                size_t runStart = p;
                while (p < vLen && isNameCh(vData[p])) p++;
                size_t eqPos = p;
                while (eqPos < vLen && isRxSpace(vData[eqPos])) eqPos++;
                if (eqPos < vLen && vData[eqPos] == targetDelim) {
                    size_t s = runStart;
                    while (s < p) {
                        bool prevW = (s > 0) && isStdWordChar(vData[s - 1]);
                        bool curW = isStdWordChar(vData[s]);
                        if (prevW != curW && !hasOddPrecedingBackslashes(visibleText, s)) break;
                        s++;
                    }
                    if (s < p) setEffect(s, p - s);
                }
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::XmlEntity) {
            size_t p = 0;
            while ((p = visibleText.find('&', p)) != std::string::npos) {
                if (hasOddPrecedingBackslashes(visibleText, p)) { p++; continue; }
                size_t q = p + 1;
                while (q < vLen && (std::isalnum((unsigned char)vData[q]) || vData[q] == '#')) q++;
                if (q > p + 1 && q < vLen && vData[q] == ';') {
                    setEffect(p, q + 1 - p);
                    p = q + 1;
                } else {
                    p = (q > p + 1) ? q : (p + 1);
                }
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::NixAttrName) {
            auto isNixCh = [](char c) noexcept -> bool {
                return std::isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-';
            };
            size_t p = 0;
            while (p < vLen) {
                if (!isNixCh(vData[p])) { p++; continue; }
                size_t s = p;
                while (p < vLen && isNixCh(vData[p])) p++;
                size_t eqPos = p;
                while (eqPos < vLen && (vData[eqPos] == ' ' || vData[eqPos] == '\t')) eqPos++;
                if (eqPos + 1 < vLen && vData[eqPos] == '=' && vData[eqPos + 1] != '=') {
                    while (s < p && hasOddPrecedingBackslashes(visibleText, s)) s++;
                    if (s < p) setEffect(s, eqPos - s);
                }
                p = (eqPos > p) ? eqPos : p;
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::LineKeyBeforeEq || meta.kind == SyntaxRuleKind::LineKeyBeforeEqNoDot ||
                   meta.kind == SyntaxRuleKind::MakefileTarget || meta.kind == SyntaxRuleKind::AsmLabel) {
            const bool allowDot = (meta.kind != SyntaxRuleKind::LineKeyBeforeEqNoDot);
            size_t ls = 0;
            while (ls < vLen) {
                size_t matchStart = ls;
                size_t q = ls;
                if (ls > 0) {
                    if (vData[ls] != '\r' && vData[ls] != '\n') {
                        size_t nl = visibleText.find_first_of("\r\n", ls);
                        if (nl == std::string::npos) break;
                        ls = nl;
                        continue;
                    }
                    q = ls + 1;
                }
                if (meta.kind == SyntaxRuleKind::MakefileTarget) {
                    size_t ks = q;
                    while (q < vLen && (std::isalnum((unsigned char)vData[q]) || vData[q] == '_' || vData[q] == '.' || vData[q] == '/' || vData[q] == '-')) q++;
                    if (q > ks) {
                        size_t cPos = q;
                        while (cPos < vLen && (vData[cPos] == ' ' || vData[cPos] == '\t')) cPos++;
                        if (cPos < vLen && vData[cPos] == ':' && (cPos + 1 >= vLen || vData[cPos + 1] != '=')) {
                            if (!hasOddPrecedingBackslashes(visibleText, matchStart)) setEffect(matchStart, q - matchStart);
                        }
                    }
                } else if (meta.kind == SyntaxRuleKind::AsmLabel) {
                    while (q < vLen && (vData[q] == ' ' || vData[q] == '\t')) q++;
                    if (q < vLen && (std::isalpha((unsigned char)vData[q]) || vData[q] == '_' || vData[q] == '.')) {
                        q++;
                        while (q < vLen && (std::isalnum((unsigned char)vData[q]) || vData[q] == '_' || vData[q] == '.' || vData[q] == '$' || vData[q] == '@')) q++;
                        size_t cPos = q;
                        while (cPos < vLen && (vData[cPos] == ' ' || vData[cPos] == '\t')) cPos++;
                        if (cPos < vLen && vData[cPos] == ':') {
                            if (!hasOddPrecedingBackslashes(visibleText, matchStart)) setEffect(matchStart, q - matchStart);
                        }
                    }
                } else {
                    while (q < vLen && (vData[q] == ' ' || vData[q] == '\t')) q++;
                    size_t ks = q;
                    while (q < vLen && (std::isalnum((unsigned char)vData[q]) || vData[q] == '_' || vData[q] == '-' || (allowDot && vData[q] == '.'))) q++;
                    if (q > ks) {
                        size_t eqPos = q;
                        while (eqPos < vLen && (vData[eqPos] == ' ' || vData[eqPos] == '\t')) eqPos++;
                        if (eqPos < vLen && vData[eqPos] == '=') {
                            if (!hasOddPrecedingBackslashes(visibleText, matchStart)) setEffect(matchStart, q - matchStart);
                        }
                    }
                }
                size_t nextNl = visibleText.find_first_of("\r\n", (ls == 0) ? 0 : (ls + 1));
                if (nextNl == std::string::npos) break;
                ls = nextNl;
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::CssHexColor) {
            size_t p = 0;
            while ((p = visibleText.find('#', p)) != std::string::npos) {
                if (!hasOddPrecedingBackslashes(visibleText, p) && (p == 0 || !isStdWordChar(vData[p - 1]))) {
                    size_t q = p + 1;
                    while (q < vLen && std::isxdigit((unsigned char)vData[q])) q++;
                    size_t hexLen = q - (p + 1);
                    if (hexLen >= 3 && hexLen <= 6 && (q == vLen || !isStdWordChar(vData[q]))) {
                        setEffect(p, q - p);
                        p = q;
                        continue;
                    }
                }
                p++;
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::CssUnit) {
            size_t p = 0;
            while (p < vLen) {
                char c = vData[p];
                if (c == '%') {
                    if (p > 0 && p + 1 < vLen && isStdWordChar(vData[p - 1]) && isStdWordChar(vData[p + 1]) &&
                        !hasOddPrecedingBackslashes(visibleText, p)) {
                        setEffect(p, 1);
                    }
                    p++;
                } else if ((c == 'p' || c == 'e' || c == 'r' || c == 'v') &&
                           (p == 0 || !isStdWordChar(vData[p - 1])) &&
                           !hasOddPrecedingBackslashes(visibleText, p)) {
                    size_t uLen = 0;
                    if (c == 'p' && p + 2 <= vLen && vData[p + 1] == 'x') uLen = 2;
                    else if (c == 'e' && p + 2 <= vLen && vData[p + 1] == 'm') uLen = 2;
                    else if (c == 'r' && p + 3 <= vLen && vData[p + 1] == 'e' && vData[p + 2] == 'm') uLen = 3;
                    else if (c == 'v' && p + 2 <= vLen && (vData[p + 1] == 'h' || vData[p + 1] == 'w')) uLen = 2;
                    if (uLen > 0 && (p + uLen == vLen || !isStdWordChar(vData[p + uLen]))) {
                        setEffect(p, uLen);
                        p += uLen;
                        continue;
                    }
                    p++;
                } else {
                    p++;
                }
            }
            continue;
        } else if (meta.kind == SyntaxRuleKind::CssSelector) {
            size_t p = 0;
            while (p < vLen) {
                char c = vData[p];
                if ((c == '.' || c == '#' || c == ':') && !hasOddPrecedingBackslashes(visibleText, p)) {
                    size_t q = p + 1;
                    while (q < vLen) {
                        char d = vData[q];
                        if ((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || d == '_' || d == '-') q++;
                        else break;
                    }
                    if (q > p + 1) {
                        setEffect(p, q - p);
                        p = q;
                        continue;
                    }
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
                if (hasOddPrecedingBackslashes(visibleText, mPos) && visibleText[mPos] != '\r' && visibleText[mPos] != '\n') continue;
                setEffect(mPos, mLen);
            }
        } catch (...) {}
    }
    size_t i = 0;
    size_t len = visibleText.length();
    const bool isPythonLike = (currentSyntax.lang == "python" || (currentSyntax.commentBlockStart == "\"\"\"" && currentSyntax.commentBlockEnd == "\"\"\""));
    const bool isPascalLike = (currentSyntax.lang == "pascal" || (currentSyntax.commentBlockStart == "{" && currentSyntax.commentBlockEnd == "}"));
    const bool isLatexLike = (currentSyntax.lang == "latex" || (currentSyntax.commentLine == "%" && currentSyntax.disableStrings));
    const bool isLispLike = isLispSyntax;
    const bool isCobolLike = (currentSyntax.lang == "cobol");
    const bool isBatchLike = (currentSyntax.lang == "batch" || currentSyntax.commentLine == "REM" || currentSyntax.commentLine == "rem");
    const bool isNestableBlock = (
        currentSyntax.lang == "julia" || currentSyntax.lang == "lisp" ||
        currentSyntax.lang == "haskell" || currentSyntax.lang == "nim" ||
        currentSyntax.lang == "ocaml" || currentSyntax.lang == "fsharp" ||
        currentSyntax.lang == "rust" || currentSyntax.lang == "swift" ||
        (currentSyntax.commentBlockStart == "#=" && currentSyntax.commentBlockEnd == "=#") ||
        (currentSyntax.commentBlockStart == "#|" && currentSyntax.commentBlockEnd == "|#") ||
        (currentSyntax.commentBlockStart == "{-" && currentSyntax.commentBlockEnd == "-}") ||
        (currentSyntax.commentBlockStart == "#[" && currentSyntax.commentBlockEnd == "]#") ||
        (currentSyntax.commentBlockStart == "(*" && currentSyntax.commentBlockEnd == "*)")
    );
    const bool symmetricBlockDelim = (!currentSyntax.commentBlockStart.empty() && currentSyntax.commentBlockStart == currentSyntax.commentBlockEnd);
    auto skipSingleQuoteNonString = [&](size_t pos) noexcept -> bool {
        if (currentSyntax.lang == "verilog") return true;
        if (currentSyntax.lang == "vhdl") {
            if (pos > 0) {
                unsigned char prev = (unsigned char)visibleText[pos - 1];
                if (std::isalnum(prev) || prev == '_' || prev == ')' || prev == ']') return true;
            }
            if (!(pos + 2 < len && visibleText[pos + 2] == '\'' && visibleText[pos + 1] != '\r' && visibleText[pos + 1] != '\n')) return true;
            return false;
        }
        if (currentSyntax.lang == "julia" || (currentSyntax.commentBlockStart == "#=" && currentSyntax.commentBlockEnd == "=#")) {
            if (pos > 0) {
                unsigned char prev = (unsigned char)visibleText[pos - 1];
                if (std::isalnum(prev) || prev == '_' || prev == '!' || prev >= 0x80 || prev == ')' || prev == ']' || prev == '}' || prev == '\'') return true;
            }
        }
        if (pos > 0 && std::isalnum((unsigned char)visibleText[pos - 1])) {
            size_t k = pos - 1;
            while (k > 0 && (std::isalnum((unsigned char)visibleText[k - 1]) || visibleText[k - 1] == '_' || visibleText[k - 1] == '.' || visibleText[k - 1] == '\'')) {
                --k;
            }
            if (std::isdigit((unsigned char)visibleText[k]) ||
                (visibleText[k] == '.' && k + 1 < pos && std::isdigit((unsigned char)visibleText[k + 1]))) {
                return true;
            }
        }
        return false;
    };
    int blockDepth = (initialBlockState > 0) ? initialBlockState : 0;
    char pyBlockQuote = (isPythonLike && initialBlockState == 1) ? '"' : ((isPythonLike && initialBlockState == 2) ? '\'' : 0);
    int pascalBlockKind = (isPascalLike && (initialBlockState == 1 || initialBlockState == 2)) ? initialBlockState : 0;
    while (i < len) {
        if (blockDepth > 0) {
            size_t startStr = i;
            if (isPythonLike) {
                size_t endPos = std::string::npos;
                size_t k = i;
                while (k + 3 <= len) {
                    if (visibleText[k] == '\\') { k += 2; continue; }
                    if ((pyBlockQuote == 0 || pyBlockQuote == '"') && visibleText.compare(k, 3, "\"\"\"") == 0) {
                        endPos = k + 3;
                        break;
                    }
                    if ((pyBlockQuote == 0 || pyBlockQuote == '\'') && visibleText.compare(k, 3, "'''") == 0) {
                        endPos = k + 3;
                        break;
                    }
                    ++k;
                }
                if (endPos != std::string::npos) {
                    i = endPos;
                    blockDepth = 0;
                    pyBlockQuote = 0;
                } else {
                    i = len;
                }
                applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Comment);
                continue;
            }
            if (isPascalLike) {
                size_t endPos = std::string::npos;
                for (size_t k = i; k < len; ++k) {
                    if ((pascalBlockKind == 0 || pascalBlockKind == 1) && visibleText[k] == '}') {
                        endPos = k + 1;
                        break;
                    }
                    if ((pascalBlockKind == 0 || pascalBlockKind == 2) && k + 1 < len && visibleText[k] == '*' && visibleText[k + 1] == ')') {
                        endPos = k + 2;
                        break;
                    }
                }
                if (endPos != std::string::npos) {
                    i = endPos;
                    blockDepth = 0;
                    pascalBlockKind = 0;
                } else {
                    i = len;
                }
                applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Comment);
                continue;
            }
            if (isNestableBlock && !symmetricBlockDelim) {
                const size_t cbsLen = currentSyntax.commentBlockStart.length();
                const size_t cbeLen = currentSyntax.commentBlockEnd.length();
                size_t k = i;
                while (k < len && blockDepth > 0) {
                    if (k + cbsLen <= len && visibleText.compare(k, cbsLen, currentSyntax.commentBlockStart) == 0) {
                        blockDepth++;
                        k += cbsLen;
                    } else if (k + cbeLen <= len && visibleText.compare(k, cbeLen, currentSyntax.commentBlockEnd) == 0) {
                        blockDepth--;
                        k += cbeLen;
                    } else {
                        k++;
                    }
                }
                i = k;
                applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Comment);
                continue;
            }
            size_t endComment = std::string::npos;
            size_t searchPos = i;
            while (searchPos <= len) {
                size_t cand = visibleText.find(currentSyntax.commentBlockEnd, searchPos);
                if (cand == std::string::npos) break;
                if (symmetricBlockDelim && hasOddPrecedingBackslashes(visibleText, cand)) {
                    searchPos = cand + 1;
                    continue;
                }
                endComment = cand;
                break;
            }
            if (endComment != std::string::npos) {
                i = endComment + currentSyntax.commentBlockEnd.length();
                blockDepth = 0;
            } else {
                i = len;
            }
            applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Comment);
        } else {
            if (isPythonLike && i + 3 <= len && (visibleText.compare(i, 3, "\"\"\"") == 0 || visibleText.compare(i, 3, "'''") == 0)) {
                if (!hasOddPrecedingBackslashes(visibleText, i)) {
                    pyBlockQuote = visibleText[i];
                    applySpan(i, 3, (int)SyntaxTokenCategory::Comment);
                    i += 3;
                    blockDepth = (pyBlockQuote == '"') ? 1 : 2;
                    continue;
                }
            }
            if (isPascalLike && (visibleText[i] == '{' || (i + 1 < len && visibleText[i] == '(' && visibleText[i + 1] == '*'))) {
                size_t cbsLen = (visibleText[i] == '{') ? 1 : 2;
                pascalBlockKind = (visibleText[i] == '{') ? 1 : 2;
                applySpan(i, cbsLen, (int)SyntaxTokenCategory::Comment);
                i += cbsLen;
                blockDepth = pascalBlockKind;
                continue;
            }
            if (!isPythonLike && !isPascalLike && !currentSyntax.commentBlockStart.empty() &&
                visibleText.compare(i, currentSyntax.commentBlockStart.length(), currentSyntax.commentBlockStart) == 0) {
                if (symmetricBlockDelim && hasOddPrecedingBackslashes(visibleText, i)) {
                    i++;
                    continue;
                }
                size_t cbsLen = currentSyntax.commentBlockStart.length();
                applySpan(i, cbsLen, (int)SyntaxTokenCategory::Comment);
                i += cbsLen;
                blockDepth = 1;
                continue;
            }
            if (isCobolLike && (visibleText[i] == '*' || visibleText[i] == '/') && !(i + 1 < len && visibleText[i] == '*' && visibleText[i + 1] == '>')) {
                size_t lineStart = visibleText.find_last_of("\r\n", i);
                lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                if (i - lineStart <= 6) {
                    bool seqAreaOk = true;
                    for (size_t k = lineStart; k < i; ++k) {
                        if (visibleText[k] != ' ' && !std::isdigit((unsigned char)visibleText[k])) {
                            seqAreaOk = false;
                            break;
                        }
                    }
                    if (seqAreaOk) {
                        size_t eol = visibleText.find_first_of("\r\n", i);
                        if (eol == std::string::npos) eol = len;
                        applySpan(lineStart, eol - lineStart, (int)SyntaxTokenCategory::Comment);
                        i = eol;
                        continue;
                    }
                }
            }
            if (isBatchLike && i + 1 < len && visibleText[i] == ':' && visibleText[i + 1] == ':') {
                size_t lineStart = visibleText.find_last_of("\r\n", i);
                lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                bool onlySpaceBefore = true;
                for (size_t k = lineStart; k < i; ++k) {
                    if (visibleText[k] != ' ' && visibleText[k] != '\t') {
                        onlySpaceBefore = false;
                        break;
                    }
                }
                if (onlySpaceBefore) {
                    size_t eol = visibleText.find_first_of("\r\n", i);
                    if (eol == std::string::npos) eol = len;
                    applySpan(i, eol - i, (int)SyntaxTokenCategory::Comment);
                    i = eol;
                    continue;
                }
            }
            if (!currentSyntax.commentLine.empty() && visibleText.compare(i, currentSyntax.commentLine.length(), currentSyntax.commentLine) == 0) {
                bool isLineComment = true;
                const size_t clLen = currentSyntax.commentLine.length();
                if (isLatexLike && hasOddPrecedingBackslashes(visibleText, i)) {
                    isLineComment = false;
                } else if (std::isalpha((unsigned char)currentSyntax.commentLine.back())) {
                    bool prevBoundary = (i == 0 || !isStdWordChar(visibleText[i - 1]));
                    bool nextBoundary = (i + clLen >= len || !isStdWordChar(visibleText[i + clLen]));
                    size_t lineStart = visibleText.find_last_of("\r\n", i);
                    lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                    bool atCommandStart = true;
                    for (size_t k = lineStart; k < i; ++k) {
                        char pc = visibleText[k];
                        if (pc != ' ' && pc != '\t' && pc != '@') {
                            if (pc == '&' || pc == '|' || pc == '(') atCommandStart = true;
                            else atCommandStart = false;
                        }
                    }
                    isLineComment = prevBoundary && nextBoundary && atCommandStart;
                } else if (currentSyntax.commentLine == "\"") {
                    size_t lineStart = visibleText.find_last_of("\r\n", i);
                    lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                    bool onlyWhitespaceBefore = true;
                    for (size_t k = lineStart; k < i; ++k) {
                        if (visibleText[k] != ' ' && visibleText[k] != '\t') {
                            onlyWhitespaceBefore = false;
                            break;
                        }
                    }
                    if (!onlyWhitespaceBefore) {
                        size_t eolCheck = visibleText.find_first_of("\r\n", i + 1);
                        if (eolCheck == std::string::npos) eolCheck = len;
                        bool hasClosingQuote = false;
                        for (size_t k = i + 1; k < eolCheck; ++k) {
                            if (visibleText[k] == '\\' && k + 1 < eolCheck) {
                                ++k;
                            } else if (visibleText[k] == '"') {
                                hasClosingQuote = true;
                                break;
                            }
                        }
                        bool precededBySpace = (i > lineStart && (visibleText[i - 1] == ' ' || visibleText[i - 1] == '\t'));
                        isLineComment = (precededBySpace && !hasClosingQuote);
                    }
                }
                if (isLineComment) {
                    size_t startStr = i;
                    size_t eol = visibleText.find_first_of("\r\n", i);
                    if (eol == std::string::npos) eol = len;
                    i = eol;
                    applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::Comment);
                    continue;
                }
            }
            if (currentSyntax.disableSingleQuoteStrings && isLispLike) {
                if (i + 2 < len && visibleText[i] == '#' && visibleText[i + 1] == '\\') {
                    size_t startStr = i;
                    i += 3;
                    if (std::isalnum((unsigned char)visibleText[i - 1])) {
                        while (i < len && (std::isalnum((unsigned char)visibleText[i]) || visibleText[i] == '-' || visibleText[i] == '_')) i++;
                    }
                    applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::String);
                    continue;
                }
                if (i + 1 < len && visibleText[i] == '\\') {
                    size_t startStr = i;
                    i += 2;
                    if (std::isalnum((unsigned char)visibleText[i - 1])) {
                        while (i < len && std::isalnum((unsigned char)visibleText[i])) i++;
                    }
                    applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::String);
                    continue;
                }
                if (i + 2 < len && visibleText[i] == '?' && visibleText[i + 1] == '\\' && (i == 0 || !isWordChar(visibleText[i - 1]))) {
                    size_t startStr = i;
                    i += 3;
                    applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::String);
                    continue;
                }
                if (i + 1 < len && visibleText[i] == '?' && (visibleText[i + 1] == '"' || visibleText[i + 1] == ';' || visibleText[i + 1] == '|') && (i == 0 || !isWordChar(visibleText[i - 1]))) {
                    size_t startStr = i;
                    i += 2;
                    applySpan(startStr, i - startStr, (int)SyntaxTokenCategory::String);
                    continue;
                }
            }
            if (currentSyntax.disableSingleQuoteStrings && !currentSyntax.disableStrings && !isLispLike &&
                currentSyntax.lang != "verilog" && visibleText[i] == '\'') {
                bool canOpenChar = (i == 0 || !isStdWordChar(visibleText[i - 1])) ||
                                   (currentSyntax.lang == "rust" && visibleText[i - 1] == 'b' && (i < 2 || !isStdWordChar(visibleText[i - 2])));
                if (canOpenChar) {
                    if (i + 2 < len && visibleText[i + 1] != '\'' && visibleText[i + 1] != '\\' &&
                        visibleText[i + 1] != '\r' && visibleText[i + 1] != '\n') {
                        size_t uLen = MiuCore::utf8CharLenAt(visibleText, i + 1);
                        if (i + 1 + uLen < len && visibleText[i + 1 + uLen] == '\'') {
                            applySpan(i, uLen + 2, (int)SyntaxTokenCategory::String);
                            i += uLen + 2;
                            continue;
                        }
                    }
                    if (i + 3 < len && visibleText[i + 1] == '\\') {
                        size_t lim = std::min(len, i + 14);
                        size_t closePos = std::string::npos;
                        for (size_t k = i + 3; k < lim; ++k) {
                            if (visibleText[k] == '\r' || visibleText[k] == '\n') break;
                            if (visibleText[k] == '\'') { closePos = k; break; }
                        }
                        if (closePos != std::string::npos) {
                            applySpan(i, closePos + 1 - i, (int)SyntaxTokenCategory::String);
                            i = closePos + 1;
                            continue;
                        }
                    }
                }
            }
            if (!currentSyntax.disableStrings && (visibleText[i] == '"' || (!currentSyntax.disableSingleQuoteStrings && visibleText[i] == '\''))) {
                if (visibleText[i] == '\'' && skipSingleQuoteNonString(i)) {
                    i++;
                    continue;
                }
                if (isPascalLike && visibleText[i] == '"') {
                    i++;
                    continue;
                }
                char quote = visibleText[i];
                size_t startStr = i;
                i++;
                while (i < len) {
                    if (isPascalLike && quote == '\'') {
                        if (visibleText[i] == '\'') {
                            if (i + 1 < len && visibleText[i + 1] == '\'') { i += 2; continue; }
                            i++;
                            break;
                        }
                        if (visibleText[i] == '\n' || visibleText[i] == '\r') break;
                        i++;
                        continue;
                    }
                    if (visibleText[i] == '\\' && i + 1 < len) {
                        i += 2;
                    } else if (visibleText[i] == quote) {
                        i++;
                        break;
                    } else if (!isLispLike && (visibleText[i] == '\n' || visibleText[i] == '\r')) {
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
                if (startStr > 0 && ((currentSyntax.lispWords && visibleText[startStr - 1] == ':') || (currentSyntax.disableSingleQuoteStrings && visibleText[startStr - 1] == '\''))) {
                    continue;
                }
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
    int initialBlockState,
    std::vector<SyntaxColor>& outColors,
    bool isDarkMode = false)
{
    outColors.assign(visibleText.size(), SyntaxColor{0.0f, 0.0f, 0.0f, 0.0f});
    if (visibleText.empty() || !currentSyntax.active) return;
    SyntaxColor colComment = isDarkMode ? SyntaxColor{0.42f, 0.60f, 0.33f, 1.0f} : SyntaxColor{0.0f, 0.50f, 0.0f, 1.0f};
    SyntaxColor colString  = isDarkMode ? SyntaxColor{0.81f, 0.57f, 0.47f, 1.0f} : SyntaxColor{0.64f, 0.08f, 0.08f, 1.0f};
    SyntaxColor colKeyword = isDarkMode ? SyntaxColor{0.34f, 0.61f, 0.84f, 1.0f} : SyntaxColor{0.0f, 0.0f, 0.0f, 1.0f};
    colKeyword = isDarkMode ? SyntaxColor{0.34f, 0.61f, 0.84f, 1.0f} : SyntaxColor{0.0f, 0.0f, 1.0f, 1.0f};
    SyntaxColor colLiteral = isDarkMode ? SyntaxColor{0.34f, 0.61f, 0.84f, 1.0f} : SyntaxColor{0.0f, 0.35f, 0.70f, 1.0f};

    scanSyntaxHighlighting(currentSyntax, visibleText, initialBlockState, [&](size_t start, size_t len, int cat) {
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

inline int scanBlockCommentState(std::string_view preText, const SyntaxDef& currentSyntax, int initialBlockState = 0) noexcept {
    if (!currentSyntax.active || currentSyntax.commentBlockStart.empty() || currentSyntax.commentBlockEnd.empty()) {
        return 0;
    }
    if (preText.empty()) return (initialBlockState > 0) ? initialBlockState : 0;
    const bool isPythonLike = (currentSyntax.lang == "python" || (currentSyntax.commentBlockStart == "\"\"\"" && currentSyntax.commentBlockEnd == "\"\"\""));
    const bool isPascalLike = (currentSyntax.lang == "pascal" || (currentSyntax.commentBlockStart == "{" && currentSyntax.commentBlockEnd == "}"));
    const bool isLatexLike = (currentSyntax.lang == "latex" || (currentSyntax.commentLine == "%" && currentSyntax.disableStrings));
    const bool isLispLike = (currentSyntax.lang == "lisp" || (currentSyntax.commentBlockStart == "#|" && currentSyntax.commentBlockEnd == "|#"));
    const bool isCobolLike = (currentSyntax.lang == "cobol");
    const bool isBatchLike = (currentSyntax.lang == "batch" || currentSyntax.commentLine == "REM" || currentSyntax.commentLine == "rem");
    const bool isNestableBlock = (
        currentSyntax.lang == "julia" || currentSyntax.lang == "lisp" ||
        currentSyntax.lang == "haskell" || currentSyntax.lang == "nim" ||
        currentSyntax.lang == "ocaml" || currentSyntax.lang == "fsharp" ||
        currentSyntax.lang == "rust" || currentSyntax.lang == "swift" ||
        (currentSyntax.commentBlockStart == "#=" && currentSyntax.commentBlockEnd == "=#") ||
        (currentSyntax.commentBlockStart == "#|" && currentSyntax.commentBlockEnd == "|#") ||
        (currentSyntax.commentBlockStart == "{-" && currentSyntax.commentBlockEnd == "-}") ||
        (currentSyntax.commentBlockStart == "#[" && currentSyntax.commentBlockEnd == "]#") ||
        (currentSyntax.commentBlockStart == "(*" && currentSyntax.commentBlockEnd == "*)")
    );
    if (initialBlockState <= 0 &&
        preText.find(currentSyntax.commentBlockStart) == std::string_view::npos &&
        !(isPythonLike && preText.find("'''") != std::string_view::npos) &&
        !(isPascalLike && preText.find("(*") != std::string_view::npos)) {
        return 0;
    }
    const bool symmetricDelim = (currentSyntax.commentBlockStart == currentSyntax.commentBlockEnd);
    const size_t len = preText.size();
    const size_t cbsLen = currentSyntax.commentBlockStart.size();
    const size_t cbeLen = currentSyntax.commentBlockEnd.size();
    const size_t clLen = currentSyntax.commentLine.size();
    auto isStdW = [](char c) noexcept -> bool {
        return std::isalnum((unsigned char)c) || c == '_';
    };
    auto isLispW = [](char c) noexcept -> bool {
        return std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == '*' || c == '+' ||
               c == '!' || c == '?' || c == '/' || c == '<' || c == '>' || c == '=' || c == '&' || c == '#';
    };
    auto skipSingleQuoteNonString = [&](size_t pos) noexcept -> bool {
        if (currentSyntax.lang == "verilog") return true;
        if (currentSyntax.lang == "vhdl") {
            if (pos > 0) {
                unsigned char prev = (unsigned char)preText[pos - 1];
                if (std::isalnum(prev) || prev == '_' || prev == ')' || prev == ']') return true;
            }
            if (!(pos + 2 < len && preText[pos + 2] == '\'' && preText[pos + 1] != '\r' && preText[pos + 1] != '\n')) return true;
            return false;
        }
        if (currentSyntax.lang == "julia" || (currentSyntax.commentBlockStart == "#=" && currentSyntax.commentBlockEnd == "=#")) {
            if (pos > 0) {
                unsigned char prev = (unsigned char)preText[pos - 1];
                if (std::isalnum(prev) || prev == '_' || prev == '!' || prev >= 0x80 || prev == ')' || prev == ']' || prev == '}' || prev == '\'') return true;
            }
        }
        if (pos > 0 && std::isalnum((unsigned char)preText[pos - 1])) {
            size_t k = pos - 1;
            while (k > 0 && (std::isalnum((unsigned char)preText[k - 1]) || preText[k - 1] == '_' || preText[k - 1] == '.' || preText[k - 1] == '\'')) {
                --k;
            }
            if (std::isdigit((unsigned char)preText[k]) ||
                (preText[k] == '.' && k + 1 < pos && std::isdigit((unsigned char)preText[k + 1]))) {
                return true;
            }
        }
        return false;
    };
    int blockDepth = (initialBlockState > 0) ? initialBlockState : 0;
    char pyBlockQuote = (isPythonLike && initialBlockState == 1) ? '"' : ((isPythonLike && initialBlockState == 2) ? '\'' : 0);
    int pascalBlockKind = (isPascalLike && (initialBlockState == 1 || initialBlockState == 2)) ? initialBlockState : 0;
    size_t i = 0;
    while (i < len) {
        if (blockDepth > 0) {
            if (isPythonLike) {
                size_t endPos = std::string_view::npos;
                size_t k = i;
                while (k + 3 <= len) {
                    if (preText[k] == '\\') { k += 2; continue; }
                    if ((pyBlockQuote == 0 || pyBlockQuote == '"') && preText.compare(k, 3, "\"\"\"") == 0) {
                        endPos = k + 3;
                        break;
                    }
                    if ((pyBlockQuote == 0 || pyBlockQuote == '\'') && preText.compare(k, 3, "'''") == 0) {
                        endPos = k + 3;
                        break;
                    }
                    ++k;
                }
                if (endPos != std::string_view::npos) {
                    i = endPos;
                    blockDepth = 0;
                    pyBlockQuote = 0;
                    continue;
                }
                return (pyBlockQuote == '\'') ? 2 : 1;
            }
            if (isPascalLike) {
                size_t endPos = std::string_view::npos;
                for (size_t k = i; k < len; ++k) {
                    if ((pascalBlockKind == 0 || pascalBlockKind == 1) && preText[k] == '}') {
                        endPos = k + 1;
                        break;
                    }
                    if ((pascalBlockKind == 0 || pascalBlockKind == 2) && k + 1 < len && preText[k] == '*' && preText[k + 1] == ')') {
                        endPos = k + 2;
                        break;
                    }
                }
                if (endPos != std::string_view::npos) {
                    i = endPos;
                    blockDepth = 0;
                    pascalBlockKind = 0;
                    continue;
                }
                return (pascalBlockKind == 2) ? 2 : 1;
            }
            if (isNestableBlock && !symmetricDelim) {
                size_t k = i;
                while (k < len && blockDepth > 0) {
                    if (k + cbsLen <= len && preText.compare(k, cbsLen, currentSyntax.commentBlockStart) == 0) {
                        blockDepth++;
                        k += cbsLen;
                    } else if (k + cbeLen <= len && preText.compare(k, cbeLen, currentSyntax.commentBlockEnd) == 0) {
                        blockDepth--;
                        k += cbeLen;
                    } else {
                        k++;
                    }
                }
                i = k;
                if (blockDepth > 0) return blockDepth;
                continue;
            }
            size_t endComment = std::string_view::npos;
            size_t searchPos = i;
            while (searchPos <= len) {
                size_t cand = preText.find(currentSyntax.commentBlockEnd, searchPos);
                if (cand == std::string_view::npos) break;
                if (symmetricDelim && hasOddPrecedingBackslashes(preText, cand)) {
                    searchPos = cand + 1;
                    continue;
                }
                endComment = cand;
                break;
            }
            if (endComment != std::string_view::npos) {
                i = endComment + cbeLen;
                blockDepth = 0;
            } else {
                return 1;
            }
        } else {
            if (isPythonLike && i + 3 <= len && (preText.compare(i, 3, "\"\"\"") == 0 || preText.compare(i, 3, "'''") == 0)) {
                if (!hasOddPrecedingBackslashes(preText, i)) {
                    pyBlockQuote = preText[i];
                    i += 3;
                    blockDepth = (pyBlockQuote == '"') ? 1 : 2;
                    continue;
                }
            }
            if (isPascalLike && (preText[i] == '{' || (i + 1 < len && preText[i] == '(' && preText[i + 1] == '*'))) {
                pascalBlockKind = (preText[i] == '{') ? 1 : 2;
                i += (preText[i] == '{') ? 1 : 2;
                blockDepth = pascalBlockKind;
                continue;
            }
            if (!isPythonLike && !isPascalLike && i + cbsLen <= len && preText.compare(i, cbsLen, currentSyntax.commentBlockStart) == 0) {
                if (symmetricDelim && hasOddPrecedingBackslashes(preText, i)) {
                    i++;
                    continue;
                }
                i += cbsLen;
                blockDepth = 1;
                continue;
            }
            if (isCobolLike && (preText[i] == '*' || preText[i] == '/') && !(i + 1 < len && preText[i] == '*' && preText[i + 1] == '>')) {
                size_t lineStart = preText.find_last_of("\r\n", i);
                lineStart = (lineStart == std::string_view::npos) ? 0 : lineStart + 1;
                if (i - lineStart <= 6) {
                    bool seqAreaOk = true;
                    for (size_t k = lineStart; k < i; ++k) {
                        if (preText[k] != ' ' && !std::isdigit((unsigned char)preText[k])) {
                            seqAreaOk = false;
                            break;
                        }
                    }
                    if (seqAreaOk) {
                        size_t eol = preText.find_first_of("\r\n", i);
                        i = (eol == std::string_view::npos) ? len : eol;
                        continue;
                    }
                }
            }
            if (isBatchLike && i + 1 < len && preText[i] == ':' && preText[i + 1] == ':') {
                size_t lineStart = preText.find_last_of("\r\n", i);
                lineStart = (lineStart == std::string_view::npos) ? 0 : lineStart + 1;
                bool onlySpaceBefore = true;
                for (size_t k = lineStart; k < i; ++k) {
                    if (preText[k] != ' ' && preText[k] != '\t') {
                        onlySpaceBefore = false;
                        break;
                    }
                }
                if (onlySpaceBefore) {
                    size_t eol = preText.find_first_of("\r\n", i);
                    i = (eol == std::string_view::npos) ? len : eol;
                    continue;
                }
            }
            if (clLen > 0 && i + clLen <= len && preText.compare(i, clLen, currentSyntax.commentLine) == 0) {
                bool isLineComment = true;
                if (isLatexLike && hasOddPrecedingBackslashes(preText, i)) {
                    isLineComment = false;
                } else if (std::isalpha((unsigned char)currentSyntax.commentLine.back())) {
                    bool prevBoundary = (i == 0 || !isStdW(preText[i - 1]));
                    bool nextBoundary = (i + clLen >= len || !isStdW(preText[i + clLen]));
                    size_t lineStart = preText.find_last_of("\r\n", i);
                    lineStart = (lineStart == std::string_view::npos) ? 0 : lineStart + 1;
                    bool atCommandStart = true;
                    for (size_t k = lineStart; k < i; ++k) {
                        char pc = preText[k];
                        if (pc != ' ' && pc != '\t' && pc != '@') {
                            if (pc == '&' || pc == '|' || pc == '(') atCommandStart = true;
                            else atCommandStart = false;
                        }
                    }
                    isLineComment = prevBoundary && nextBoundary && atCommandStart;
                } else if (currentSyntax.commentLine == "\"") {
                    size_t lineStart = preText.find_last_of("\r\n", i);
                    lineStart = (lineStart == std::string_view::npos) ? 0 : lineStart + 1;
                    bool onlyWhitespaceBefore = true;
                    for (size_t k = lineStart; k < i; ++k) {
                        if (preText[k] != ' ' && preText[k] != '\t') {
                            onlyWhitespaceBefore = false;
                            break;
                        }
                    }
                    if (!onlyWhitespaceBefore) {
                        size_t eolCheck = preText.find_first_of("\r\n", i + 1);
                        if (eolCheck == std::string_view::npos) eolCheck = len;
                        bool hasClosingQuote = false;
                        for (size_t k = i + 1; k < eolCheck; ++k) {
                            if (preText[k] == '\\' && k + 1 < eolCheck) ++k;
                            else if (preText[k] == '"') { hasClosingQuote = true; break; }
                        }
                        bool precededBySpace = (i > lineStart && (preText[i - 1] == ' ' || preText[i - 1] == '\t'));
                        isLineComment = (precededBySpace && !hasClosingQuote);
                    }
                }
                if (isLineComment) {
                    size_t eol = preText.find_first_of("\r\n", i);
                    i = (eol == std::string_view::npos) ? len : eol;
                    continue;
                }
            }
            if (currentSyntax.disableSingleQuoteStrings && isLispLike) {
                if (i + 2 < len && preText[i] == '#' && preText[i + 1] == '\\') { i += 3; continue; }
                if (i + 1 < len && preText[i] == '\\') { i += 2; continue; }
                if (i + 2 < len && preText[i] == '?' && preText[i + 1] == '\\' && (i == 0 || !isLispW(preText[i - 1]))) { i += 3; continue; }
                if (i + 1 < len && preText[i] == '?' && (preText[i + 1] == '"' || preText[i + 1] == ';' || preText[i + 1] == '|') && (i == 0 || !isLispW(preText[i - 1]))) { i += 2; continue; }
            }
            if (currentSyntax.disableSingleQuoteStrings && !currentSyntax.disableStrings && !isLispLike &&
                currentSyntax.lang != "verilog" && preText[i] == '\'') {
                bool canOpenChar = (i == 0 || !isStdW(preText[i - 1])) ||
                                   (currentSyntax.lang == "rust" && preText[i - 1] == 'b' && (i < 2 || !isStdW(preText[i - 2])));
                if (canOpenChar) {
                    if (i + 2 < len && preText[i + 1] != '\'' && preText[i + 1] != '\\' &&
                        preText[i + 1] != '\r' && preText[i + 1] != '\n') {
                        size_t uLen = MiuCore::utf8CharLenAt(preText, i + 1);
                        if (i + 1 + uLen < len && preText[i + 1 + uLen] == '\'') {
                            i += uLen + 2;
                            continue;
                        }
                    }
                    if (i + 3 < len && preText[i + 1] == '\\') {
                        size_t lim = std::min(len, i + 14);
                        size_t closePos = std::string_view::npos;
                        for (size_t k = i + 3; k < lim; ++k) {
                            if (preText[k] == '\r' || preText[k] == '\n') break;
                            if (preText[k] == '\'') { closePos = k; break; }
                        }
                        if (closePos != std::string_view::npos) {
                            i = closePos + 1;
                            continue;
                        }
                    }
                }
            }
            if (!currentSyntax.disableStrings && (preText[i] == '"' || (!currentSyntax.disableSingleQuoteStrings && preText[i] == '\''))) {
                if (preText[i] == '\'' && skipSingleQuoteNonString(i)) {
                    i++;
                    continue;
                }
                if (isPascalLike && preText[i] == '"') {
                    i++;
                    continue;
                }
                char quote = preText[i++];
                while (i < len) {
                    if (isPascalLike && quote == '\'') {
                        if (preText[i] == '\'') {
                            if (i + 1 < len && preText[i + 1] == '\'') { i += 2; continue; }
                            i++;
                            break;
                        }
                        if (preText[i] == '\n' || preText[i] == '\r') break;
                        i++;
                        continue;
                    }
                    if (preText[i] == '\\' && i + 1 < len) i += 2;
                    else if (preText[i] == quote) { i++; break; }
                    else if (!isLispLike && (preText[i] == '\n' || preText[i] == '\r')) break;
                    else i++;
                }
                continue;
            }
            i++;
        }
    }
    return blockDepth;
}
