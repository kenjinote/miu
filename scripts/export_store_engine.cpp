#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "MiuCore.h"
#include "MiuEmmet.h"
#include "MiuPreview.h"
#include "MiuSyntax.h"
#include "Pele.h"

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out.push_back((char)c);
                }
        }
    }
    return out;
}

static std::string colorToHex(const SyntaxColor& c, bool isDark) {
    if (c.a < 0.01f) {
        return isDark ? "#e6edf3" : "#1f2328";
    }
    auto clampByte = [](float v) -> int {
        int b = (int)(v * 255.0f + 0.5f);
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        return b;
    };
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", clampByte(c.r), clampByte(c.g), clampByte(c.b));
    return buf;
}

static std::string htmlEscapeText(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out.push_back(c);
    }
    return out;
}

static std::vector<std::string> buildHighlightedLinesHtml(const std::string& code, const std::wstring& ext, bool isDark) {
    SyntaxDef syn;
    std::wstring low;
    loadSyntaxDefForExtension(ext, syn, low);

    std::vector<std::string> linesHtml;
    int blockState = 0;
    size_t pos = 0;
    const size_t n = code.size();
    while (pos <= n) {
        size_t eol = code.find('\n', pos);
        size_t lineEnd = (eol == std::string::npos) ? n : eol;
        std::string line = code.substr(pos, lineEnd - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::vector<SyntaxColor> cols;
        if (syn.active && !line.empty()) {
            scanSyntaxHighlighting(line, syn, blockState, cols, isDark);
            blockState = scanBlockCommentState(line, syn, blockState);
        } else {
            cols.assign(line.size(), SyntaxColor{0, 0, 0, 0});
        }

        std::string html;
        size_t i = 0;
        while (i < line.size()) {
            std::string hex = colorToHex(cols[i], isDark);
            size_t j = i + 1;
            while (j < line.size() && colorToHex(cols[j], isDark) == hex) {
                j++;
            }
            html += "<span style=\"color:" + hex + "\">" + htmlEscapeText(line.substr(i, j - i)) + "</span>";
            i = j;
        }
        linesHtml.push_back(html);

        if (eol == std::string::npos) break;
        pos = eol + 1;
    }
    return linesHtml;
}

struct RenderedSlide {
    std::string key;
    bool isDark = false;
    std::string previewHtml;
    std::vector<std::string> lines;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: export_store_engine <input_records.bin> <output.json>\n";
        return 1;
    }
    const char* inPath = argv[1];
    const char* outPath = argv[2];

    std::ifstream ifs(inPath, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open input: " << inPath << "\n";
        return 1;
    }

    std::string headerLine;
    if (!std::getline(ifs, headerLine)) {
        std::cerr << "Empty input file\n";
        return 1;
    }
    int totalRecords = std::stoi(headerLine);

    std::vector<std::string> langOrder;
    std::map<std::string, std::vector<RenderedSlide>> byLang;

    for (int r = 0; r < totalRecords; ++r) {
        std::string metaLine;
        if (!std::getline(ifs, metaLine)) {
            std::cerr << "Unexpected EOF reading record header " << r << "\n";
            return 1;
        }
        if (!metaLine.empty() && metaLine.back() == '\r') metaLine.pop_back();

        // Format: prefix \t key \t ext \t isDark(0|1) \t hasPreview(0|1) \t byteLen
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t tab = metaLine.find('\t', start);
            if (tab == std::string::npos) {
                parts.push_back(metaLine.substr(start));
                break;
            }
            parts.push_back(metaLine.substr(start, tab - start));
            start = tab + 1;
        }
        if (parts.size() != 6) {
            std::cerr << "Malformed metaLine at record " << r << ": " << metaLine << "\n";
            return 1;
        }

        const std::string& prefix = parts[0];
        const std::string& key = parts[1];
        const std::string& extAscii = parts[2];
        bool isDark = (parts[3] == "1");
        bool hasPreview = (parts[4] == "1");
        size_t byteLen = (size_t)std::stoull(parts[5]);

        std::string src(byteLen, '\0');
        if (byteLen > 0) {
            ifs.read(&src[0], (std::streamsize)byteLen);
        }
        char nl = 0;
        ifs.get(nl); // trailing '\n'

        if (byLang.find(prefix) == byLang.end()) {
            langOrder.push_back(prefix);
        }

        std::wstring wext(extAscii.begin(), extAscii.end());
        RenderedSlide rs;
        rs.key = key;
        rs.isDark = isDark;
        rs.lines = buildHighlightedLinesHtml(src, wext, isDark);
        if (hasPreview) {
            auto pres = MiuPreview::renderPreview(src, false, "", wext, isDark, "", "STORENONCE");
            rs.previewHtml = pres.bodyContent;
        }
        byLang[prefix].push_back(std::move(rs));
    }

    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"mdThemeCss\": \"" << jsonEscape(MiuPreview::getMdThemeStyle()) << "\",\n";
    oss << "  \"languages\": {\n";
    for (size_t li = 0; li < langOrder.size(); ++li) {
        const std::string& lang = langOrder[li];
        const auto& slides = byLang[lang];
        oss << "    \"" << jsonEscape(lang) << "\": {\n";
        for (size_t si = 0; si < slides.size(); ++si) {
            const auto& sl = slides[si];
            oss << "      \"" << jsonEscape(sl.key) << "\": {\n";
            oss << "        \"isDark\": " << (sl.isDark ? "true" : "false") << ",\n";
            oss << "        \"previewHtml\": \"" << jsonEscape(sl.previewHtml) << "\",\n";
            oss << "        \"lines\": [\n";
            for (size_t k = 0; k < sl.lines.size(); ++k) {
                oss << "          \"" << jsonEscape(sl.lines[k]) << "\"" << (k + 1 < sl.lines.size() ? "," : "") << "\n";
            }
            oss << "        ]\n";
            oss << "      }" << (si + 1 < slides.size() ? "," : "") << "\n";
        }
        oss << "    }" << (li + 1 < langOrder.size() ? "," : "") << "\n";
    }
    oss << "  }\n}\n";

    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "Failed to open output: " << outPath << "\n";
        return 1;
    }
    const std::string jsonStr = oss.str();
    ofs.write(jsonStr.data(), (std::streamsize)jsonStr.size());
    ofs.close();
    std::cout << "[OK] Exported " << langOrder.size() << " languages (" << totalRecords << " slides, " << jsonStr.size() << " bytes) -> " << outPath << "\n";
    return 0;
}
