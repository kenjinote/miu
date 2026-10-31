#include "EditorCore.h"
#include <cassert>
#include <iostream>
#include <random>
#include <filesystem>

static int g_passCount = 0;
static int g_failCount = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "[FAIL] " << __func__ << ":" << __LINE__ << " - " << (msg) << std::endl; \
        g_failCount++; \
    } else { \
        g_passCount++; \
    } \
} while (0)

static void verifyLineStartsInvariant(Editor& ed) {
    std::string full = ed.pt.getRange(0, ed.pt.length());
    std::vector<size_t> expected;
    expected.push_back(0);
    for (size_t i = 0; i < full.size(); ++i) {
        if (full[i] == '\n') expected.push_back(i + 1);
    }
    CHECK(ed.lineStarts == expected, "lineStarts invariant matches exact newline positions");
}

static void testPhase1PieceTableAndFastLineStarts() {
    std::cout << "--- Running Phase 1.1: PieceTable & Fast LineStarts ---" << std::endl;
    Editor ed;
    ed.initGraphics();
    ed.newFile();

    ed.insertAtCursors("Hello");
    ed.insertAtCursors(" ");
    ed.insertAtCursors("World");
    CHECK(ed.pt.length() == 11, "PieceTable totalLength == 11");
    CHECK(ed.pt.pieces.size() == 1, "PieceTable coalesceAround merged sequential inserts into 1 piece");
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "Hello World", "PieceTable content matches");
    verifyLineStartsInvariant(ed);

    ed.insertNewlineWithAutoIndent();
    ed.insertAtCursors("Second line with more characters");
    verifyLineStartsInvariant(ed);
    CHECK(ed.maxLineBytesIdx == 1, "maxLineBytesIdx tracks longest line (line 1)");
    CHECK(ed.maxLineBytes == 32, "maxLineBytes == 32");

    // Erase on longest line should recompute maxLineBytes
    ed.selectLineAt(ed.lineStarts[1], false);
    ed.backspaceAtCursors();
    verifyLineStartsInvariant(ed);
    CHECK(ed.maxLineBytesIdx == 0, "maxLineBytesIdx updated to line 0 after shrinking line 1");
}

static void testPhase1FastSearchReplaceAndReDoS() {
    std::cout << "--- Running Phase 1.2: Fast Search, Single-Pass replaceAll, ReDoS Guard ---" << std::endl;
    CHECK(isSafeUserRegex("^[a-zA-Z0-9_]+$") == true, "Normal regex is safe");
    CHECK(isSafeUserRegex("(a+)+") == false, "Nested quantifier (a+)+ blocked by ReDoS guard");
    CHECK(isSafeUserRegex("([a-z]+)*") == false, "Nested quantifier ([a-z]+)* blocked by ReDoS guard");
    CHECK(isSafeUserRegex(std::string(1200, 'a')) == false, "Overlong regex blocked by ReDoS guard");

    Editor ed;
    ed.initGraphics();
    ed.newFile();
    std::string doc;
    for (int i = 0; i < 200; ++i) {
        doc += "alpha beta gamma delta " + std::to_string(i) + "\n";
    }
    ed.insertAtCursors(doc);
    size_t mLen = 0;
    size_t pos = ed.findText(0, "gamma", true, true, true, false, &mLen);
    CHECK(pos == 11 && mLen == 5, "Fast piece-direct literal search found 'gamma' at 11");

    // Unsafe regex should safely return npos without hanging
    size_t badPos = ed.findText(0, "(a+)+b", true, true, false, true, &mLen);
    CHECK(badPos == std::string::npos, "Unsafe regex safely rejected in findText");

    // Single-pass replaceAll + single-step Undo/Redo
    std::string beforeReplace = ed.pt.getRange(0, ed.pt.length());
    ed.searchQuery = "beta";
    ed.replaceQuery = "BETA_REPLACED";
    ed.searchMatchCase = true;
    ed.searchWholeWord = false;
    ed.searchRegex = false;
    int replacedCount = 0;
    ed.cbShowReplaceAlert = [&](int cnt) { replacedCount = cnt; };
    ed.replaceAll();
    CHECK(replacedCount == 200, "replaceAll replaced all 200 occurrences");
    CHECK(ed.pt.getRange(0, 30).find("BETA_REPLACED") != std::string::npos, "Replaced text present");
    verifyLineStartsInvariant(ed);

    ed.performUndo();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == beforeReplace, "Single Undo completely restores pre-replaceAll document");
    ed.performRedo();
    CHECK(ed.pt.getRange(0, 30).find("BETA_REPLACED") != std::string::npos, "Single Redo reapplies replaceAll");
}

static void testPhase1Utf16MapAndScrollBarFix() {
    std::cout << "--- Running Phase 1.3 & 1.4: Visible UTF-16 Map & v1.0.44 ScrollBar Fix ---" << std::endl;
    Editor ed;
    ed.initGraphics();
    ed.newFile();
    float viewW = 800.0f, viewH = 600.0f;
    ed.cbGetViewSize = [&](float& w, float& h) { w = viewW; h = viewH; };

    // Test UTF-16 map with ASCII (1 byte -> 1 u16), Japanese (3 bytes -> 1 u16), Emoji (4 bytes -> 2 u16)
    std::string sample = "Aあ😀B"; // 'A'(1b,1u16), 'あ'(3b,1u16), '😀'(4b,2u16), 'B'(1b,1u16)
    ed.insertAtCursors(sample);
    ed.buildVisibleUtf16Map(0, sample);
    CHECK(ed.utf8OffsetToUtf16Count(0, 1) == 1, "ASCII 'A' -> 1 UTF-16 code unit");
    CHECK(ed.utf8OffsetToUtf16Count(0, 4) == 2, "'Aあ' -> 2 UTF-16 code units");
    CHECK(ed.utf8OffsetToUtf16Count(0, 8) == 4, "'Aあ😀' -> 4 UTF-16 code units (surrogate pair)");
    CHECK(ed.utf8OffsetToUtf16Count(0, 9) == 5, "'Aあ😀B' -> 5 UTF-16 code units");

    // Test v1.0.44 horizontal scrollbar fix: New file + Enter should NOT trigger horizontal scrollbar
    ed.newFile();
    ed.ensureCaretVisible();
    ScrollBarLayout L0 = ed.getScrollBarLayout(viewW, viewH);
    CHECK(!L0.needH && !L0.needV, "Empty new file has neither H nor V scrollbar");

    ed.insertNewlineWithAutoIndent();
    ed.ensureCaretVisible();
    ScrollBarLayout L1 = ed.getScrollBarLayout(viewW, viewH);
    CHECK(!L1.needH, "v1.0.44 fix: Pressing Enter on new file does NOT show horizontal scrollbar");
    CHECK(!L1.needV, "v1.0.44 fix: 2 lines in 600px view does NOT show vertical scrollbar");
    CHECK(ed.hScrollPos == 0, "hScrollPos remains 0");

    // Japanese full-width input on short line should NOT trigger horizontal scrollbar
    ed.insertAtCursors("こんにちは世界");
    ed.ensureCaretVisible();
    ScrollBarLayout L2 = ed.getScrollBarLayout(viewW, viewH);
    CHECK(!L2.needH, "Full-width Japanese input on short line does NOT show horizontal scrollbar");

    // Long line should trigger horizontal scrollbar, and deleting it should hide it immediately
    ed.insertAtCursors(std::string(300, 'x'));
    ed.ensureCaretVisible();
    ScrollBarLayout L3 = ed.getScrollBarLayout(viewW, viewH);
    CHECK(L3.needH, "300-char line triggers horizontal scrollbar");

    ed.selectAll();
    ed.backspaceAtCursors();
    ed.ensureCaretVisible();
    ScrollBarLayout L4 = ed.getScrollBarLayout(viewW, viewH);
    CHECK(!L4.needH && ed.hScrollPos == 0, "Deleting long line immediately hides horizontal scrollbar");
}

static void testPhase1SoftWrapBracketsEmojiAndEditing() {
    std::cout << "--- Running Phase 1.6 - 1.9: Soft Wrap, Bracket Match, Emoji Graphemes, Editing ---" << std::endl;
    Editor ed;
    ed.initGraphics();
    ed.newFile();
    float viewW = 240.0f, viewH = 400.0f;
    ed.cbGetViewSize = [&](float& w, float& h) { w = viewW; h = viewH; };

    ed.insertAtCursors(std::string(80, 'a') + "\nsecond line");
    ed.toggleWordWrap();
    CHECK(ed.wordWrapEnabled == true, "Soft wrap enabled");
    float wrapW = ed.getEffectiveTextWidth(viewW);
    int vCount = ed.getLineVisualCount(0, wrapW);
    CHECK(vCount > 1, "80-char line wraps into multiple visual sub-lines");

    ed.jumpToFileEdge(true, false);
    ed.moveVisualEnd(false);
    CHECK(ed.cursors[0].head > 0 && ed.cursors[0].head < 80, "moveVisualEnd stops at end of first visual sub-line");
    CHECK(ed.cursors[0].affinity == true, "moveVisualEnd sets backward wrap affinity");
    size_t sub1End = ed.cursors[0].head;
    ed.moveVisualHome(false);
    CHECK(ed.cursors[0].head == 0, "moveVisualHome returns to start of first visual sub-line");
    ed.moveVisualUpDown(false, false);
    CHECK(ed.cursors[0].head == sub1End, "moveVisualUpDown(down) moves to start of second visual sub-line");
    ed.toggleWordWrap();

    // Bracket matching & jumpToMatchingBracket (Cmd+B)
    ed.newFile();
    ed.insertAtCursors("func(a, {b: [1, 2, (3 + 4)]})");
    ed.cursors[0].head = ed.cursors[0].anchor = 4; // at '('
    size_t b1 = 0, b2 = 0;
    CHECK(ed.getAdjacentBrackets(4, b1, b2) && b1 == 4 && b2 == 28, "Matched outer '(' at 4 with ')' at 28");
    ed.jumpToMatchingBracket(false);
    CHECK(ed.cursors[0].head == 28, "Cmd+B jumped to matching ')' at 28");
    ed.jumpToMatchingBracket(false);
    CHECK(ed.cursors[0].head == 4, "Cmd+B jumped back to opening '(' at 4");

    // Emoji / ZWJ / Variation Selector / Skin Tone grapheme cluster movement & deletion
    ed.newFile();
    std::string zwjFamily = "👨\xE2\x80\x8D👩\xE2\x80\x8D👧\xE2\x80\x8D👦";
    std::string heartVS = "❤\xEF\xB8\x8F";
    std::string flagJP = "🇯🇵";
    ed.insertAtCursors(zwjFamily + heartVS + flagJP);
    size_t totalLen = ed.pt.length();
    size_t p1 = ed.moveCaretVisual(0, true);
    CHECK(p1 == zwjFamily.size(), "Right arrow steps over entire ZWJ family emoji in 1 step");
    size_t p2 = ed.moveCaretVisual(p1, true);
    CHECK(p2 == zwjFamily.size() + heartVS.size(), "Right arrow steps over heart + variation selector in 1 step");
    size_t p3 = ed.moveCaretVisual(p2, true);
    CHECK(p3 == totalLen, "Right arrow steps over regional indicator flag pair in 1 step");
    ed.backspaceAtCursors();
    CHECK(ed.pt.length() == zwjFamily.size() + heartVS.size(), "Backspace deletes entire flag pair in 1 step");
    ed.backspaceAtCursors();
    CHECK(ed.pt.length() == zwjFamily.size(), "Backspace deletes heart + VS16 in 1 step");
    ed.backspaceAtCursors();
    CHECK(ed.pt.length() == 0, "Backspace deletes entire ZWJ family emoji in 1 step");

    // duplicateLines on last line without trailing newline
    ed.newFile();
    ed.insertAtCursors("line1\nline2_no_nl");
    ed.cursors[0].head = ed.cursors[0].anchor = ed.pt.length();
    ed.duplicateLines(false);
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "line1\nline2_no_nl\nline2_no_nl", "duplicateLines(down) on EOF line without newline produces exact duplicate without extra blank line");
    ed.performUndo();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "line1\nline2_no_nl", "Undo after duplicateLines restores exact original text");

    // Multi-cursor addition (addCursor) & Rectangular selection + Undo
    ed.newFile();
    ed.insertAtCursors("abc\ndef\nghi");
    ed.jumpToFileEdge(true, false);
    ed.addCursor(false);
    ed.addCursor(false);
    CHECK(ed.cursors.size() == 3, "addCursor added 3 vertical cursors");
    ed.insertAtCursors("X_");
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "X_abc\nX_def\nX_ghi", "Multi-cursor insert updated all 3 lines");
    ed.performUndo();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "abc\ndef\nghi", "Undo restored multi-cursor edit");

    // Keyboard rectangular selection past line end (pendingPadding) + insert + Undo
    ed.newFile();
    ed.insertAtCursors("a\nbb\nccc");
    ed.jumpToFileEdge(true, false);
    ed.moveRectSelection(2, 5); // 3 lines, 5 columns right (past end of "a" and "bb")
    CHECK(ed.isRectSelecting && ed.cursors.size() == 3, "Keyboard rectangular selection active across 3 lines");
    ed.insertAtCursors("!");
    verifyLineStartsInvariant(ed);
    ed.performUndo();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "a\nbb\nccc", "Undo after virtual-padded rectangular edit restores exact original lines");

    // Gutter line range selection & Drag-and-drop selection move
    ed.newFile();
    ed.insertAtCursors("first\nsecond\nthird\n");
    ed.selectLinesRange(0, 1, false);
    CHECK(ed.cursors[0].start() == 0 && ed.cursors[0].end() == 13, "Gutter drag selected lines 0..1");
    ed.selectWordAt(0, false); // selects "first"
    CHECK(ed.isPointInSelection(2) == true, "Point 2 is inside selected word 'first'");
    ed.performDragMove(13); // move "first" to start of "third"
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "\nsecond\nfirstthird\n", "performDragMove moved 'first' to targetPos");
    ed.performUndo();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "first\nsecond\nthird\n", "Single Undo restores performDragMove");
}

static void testPhase2SyntaxAll49CategoriesCommentAndEmmet() {
    std::cout << "--- Running Phase 2: All 49 Syntax Categories, Comment Toggle, Emmet ---" << std::endl;
    Editor ed;
    ed.initGraphics();

    const std::vector<std::string> sampleFiles = {
        "main.cpp", "script.py", "app.tsx", "index.html", "styles.scss",
        "config.json", "page.md", "run.sh", "server.rs", "main.go",
        "App.java", "Program.cs", "index.php", "app.rb", "View.swift",
        "Main.kt", "query.sql", "deploy.yaml", "Cargo.toml", "app.lua",
        "script.r", "analysis.jl", "main.dart", "app.scala", "server.ex",
        "Main.hs", "core.erl", "core.clj", "main.zig", "main.nim",
        "main.v", "shader.wgsl", "contract.sol", "main.tf", "schema.prisma",
        "schema.graphql", "msg.proto", "CMakeLists.txt", "Dockerfile", "Makefile",
        "script.ps1", "build.bat", "settings.ini", "app.log", "transform.xslt",
        "paper.tex", "design.sv", "module.vhd", "main.f90", "program.cob",
        "boot.asm", "test.pl", "App.vb", "app.fs", "main.ml", "main.groovy", ".env.local"
    };

    for (const auto& f : sampleFiles) {
        ed.loadSyntaxForExtension(f);
        CHECK(ed.currentSyntax.active, "Syntax active for " + f);
        CHECK(!ed.currentSyntax.regexes.empty(), "Syntax regexes compiled for " + f);
        std::vector<SyntaxColor> cols;
        scanSyntaxHighlighting("// comment\n# hash\n/* block */ \"string\" 12345 true if return", ed.currentSyntax, false, cols);
        CHECK(cols.size() > 0, "Syntax scanner ran cleanly for " + f);
    }

    // Block comment cache test
    ed.newFile();
    ed.loadSyntaxForExtension("test.cpp");
    ed.insertAtCursors("int a = 1;\n/* start block\nstill in block\nend block */ int b = 2;\nint c = 3;");
    CHECK(ed.checkInBlockComment(0) == false, "Line 0 not in block comment");
    CHECK(ed.checkInBlockComment(1) == false, "Line 1 starts outside block comment");
    CHECK(ed.checkInBlockComment(2) == true, "Line 2 starts inside block comment");
    CHECK(ed.checkInBlockComment(3) == true, "Line 3 starts inside block comment");
    CHECK(ed.checkInBlockComment(4) == false, "Line 4 starts outside block comment after */");

    // Toggle comment (Cmd+/) for line comment (C++) and block comment (HTML)
    ed.newFile();
    ed.loadSyntaxForExtension("test.cpp");
    ed.insertAtCursors("    int x = 10;\n    int y = 20;");
    ed.selectAll();
    ed.toggleComment();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "    // int x = 10;\n    // int y = 20;", "C++ line comment toggled ON");
    ed.toggleComment();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "    int x = 10;\n    int y = 20;", "C++ line comment toggled OFF");

    ed.newFile();
    ed.loadSyntaxForExtension("index.html");
    ed.insertAtCursors("<div>Hello</div>");
    ed.selectAll();
    ed.toggleComment();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "<!-- <div>Hello</div> -->", "HTML block comment toggled ON");
    ed.toggleComment();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "<div>Hello</div>", "HTML block comment toggled OFF");

    // Native Emmet expansion & tabstop navigation
    ed.newFile();
    ed.insertAtCursors("i");
    CHECK(ed.tryExpandEmmet() == true, "Emmet expanded 'i' in unsaved file (Auto mode)");
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "<i></i>", "Emmet 'i' expanded to '<i></i>'");
    CHECK(ed.cursors[0].head == 3 && ed.cursors[0].anchor == 3, "Emmet 'i' placed caret inside '<i>|</i>'");

    ed.newFile();
    ed.loadSyntaxForExtension("index.html");
    ed.insertAtCursors("ul>li.item$*2>a");
    CHECK(ed.tryExpandEmmet() == true, "Emmet expanded ul>li.item$*2>a");
    std::string expandedHtml = ed.pt.getRange(0, ed.pt.length());
    CHECK(expandedHtml.find("<ul") != std::string::npos && expandedHtml.find("class=\"item1\"") != std::string::npos, "Emmet HTML contains expected tags and numbered class");
    CHECK(ed.activeEmmetSession.active == true, "Emmet tabstop session active after expansion with multiple tabstops");
    ed.insertAtCursors("https://example.com");
    CHECK(ed.jumpEmmetTabstop(true) == true, "Emmet Tab jumped to next tabstop");

    // Smart auto-indent between tags <div>|</div> and braces {|}
    ed.newFile();
    ed.loadSyntaxForExtension("index.html");
    ed.insertAtCursors("<div></div>");
    ed.cursors[0].head = ed.cursors[0].anchor = 5; // between <div> and </div>
    ed.insertNewlineWithAutoIndent();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "<div>\n    \n</div>", "Smart Enter between <div>|</div> indents inner line and places closing tag on next line");
}

static void testPhase3PreviewPeleAndLatex() {
    std::cout << "--- Running Phase 3: Live Preview, GFM Markdown, Pele 30 Diagrams, LaTeX Math, Task Toggle ---" << std::endl;
    Editor ed;
    ed.initGraphics();
    ed.newFile();
    ed.loadSyntaxForExtension("README.md");

    std::string md =
        "# Title\n"
        "- [ ] Task one\n"
        "- [x] Task two\n\n"
        "Inline math $E = mc^2$ and display math:\n"
        "$$\n\\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}\n$$\n\n"
        "```mermaid\n"
        "flowchart TD\n"
        "    A[Start] --> B{Decision}\n"
        "    B -->|Yes| C[Done]\n"
        "```\n\n"
        "[Safe](https://example.com) and [Bad](javascript:alert(1))\n"
        "<script>alert('xss')</script>\n";

    ed.insertAtCursors(md);
    ed.togglePreview();
    CHECK(ed.showPreview == true, "Preview toggled ON");
    auto res = ed.getPreviewRenderResult();
    CHECK(res.fullHtml.find("Content-Security-Policy") != std::string::npos, "CSP meta header generated");
    CHECK(res.bodyHtml.find("type=\"checkbox\"") != std::string::npos && res.bodyHtml.find("data-line=\"1\"") != std::string::npos, "Interactive task list checkbox with data-line generated");
    CHECK(res.bodyHtml.find("<math") != std::string::npos, "LaTeX math converted to MathML <math>");
    CHECK(res.bodyHtml.find("<svg") != std::string::npos, "Mermaid flowchart rendered to inline <svg> by Pele");
    CHECK(res.bodyHtml.find("javascript:alert") == std::string::npos, "Dangerous javascript: URL blocked");
    CHECK(res.bodyHtml.find("<script>alert") == std::string::npos, "Dangerous <script> tag escaped/blocked");

    // Interactive task list checkbox two-way toggle
    CHECK(ed.handlePreviewTaskToggle(1) == true, "Toggled task checkbox on line 1");
    CHECK(ed.pt.getRange(0, ed.pt.length()).find("- [x] Task one") != std::string::npos, "Line 1 updated from [ ] to [x] in Editor buffer");
    ed.performUndo();
    CHECK(ed.pt.getRange(0, ed.pt.length()).find("- [ ] Task one") != std::string::npos, "Undo restored checkbox from [x] to [ ]");
}

static void testPhase4ViewStateRecentFilesAndEncodings() {
    std::cout << "--- Running Phase 4: ViewState Persistence, Recent Files, External Mod, CED Encodings ---" << std::endl;
    std::string tempDir = (std::filesystem::temp_directory_path() / "miu_macos_test_appdata").string();
    std::filesystem::create_directories(tempDir);
    std::string testFilePath = tempDir + "/sample_test.py";

    {
        Editor ed;
        ed.customAppSupportDir = tempDir;
        ed.initGraphics();
        ed.newFile();
        std::string content;
        for (int i = 0; i < 50; ++i) content += "def func_" + std::to_string(i) + "(): pass\n";
        ed.insertAtCursors(content);
        ed.vScrollPos = 17;
        ed.wordWrapEnabled = true;
        ed.cursors[0].head = 120;
        ed.cursors[0].anchor = 100;
        CHECK(ed.saveFile(UTF8ToW(testFilePath)) == true, "Saved sample_test.py");
    }

    {
        Editor ed2;
        ed2.customAppSupportDir = tempDir;
        ed2.initGraphics();
        CHECK(ed2.openFileFromPath(testFilePath) == true, "Reopened sample_test.py");
        CHECK(ed2.vScrollPos == 17, "Restored vScrollPos == 17 from viewstates.bin");
        CHECK(ed2.wordWrapEnabled == true, "Restored wordWrapEnabled == true from viewstates.bin");
        CHECK(ed2.cursors[0].head == 120 && ed2.cursors[0].anchor == 100, "Restored cursor selection [100..120] from viewstates.bin");
        CHECK(!ed2.recentFiles.empty() && ed2.recentFiles.front() == testFilePath, "Added file to recentFiles list");

        // Test Always on Top toggle
        bool topCallbackState = false;
        ed2.cbSetAlwaysOnTop = [&](bool onTop) { topCallbackState = onTop; };
        ed2.toggleAlwaysOnTop();
        CHECK(ed2.isAlwaysOnTop == true && topCallbackState == true, "Always on Top toggled ON");
        ed2.toggleAlwaysOnTop();
        CHECK(ed2.isAlwaysOnTop == false && topCallbackState == false, "Always on Top toggled OFF");
    }

    // Test Shift_JIS encoding detection & round-trip save/load
    std::string sjisPath = tempDir + "/sjis_test.txt";
    {
        // "東京タワー\r\n" in Shift_JIS (CP932)
        const unsigned char sjisBytes[] = { 0x93, 0x8C, 0x8B, 0x9E, 0x83, 0x5E, 0x83, 0x8F, 0x81, 0x5B, '\r', '\n' };
        std::ofstream ofs(sjisPath, std::ios::binary);
        ofs.write((const char*)sjisBytes, sizeof(sjisBytes));
    }
    {
        Editor edSjis;
        edSjis.customAppSupportDir = tempDir;
        edSjis.initGraphics();
        CHECK(edSjis.openFileFromPath(sjisPath) == true, "Opened Shift_JIS file");
        CHECK(edSjis.currentEncoding == ENC_LOCAL, "Detected ENC_LOCAL for Shift_JIS file");
        CHECK(edSjis.newlineStr == "\r\n", "Detected CRLF newline style");
        CHECK(edSjis.pt.getRange(0, edSjis.pt.length()) == "東京タワー\r\n", "Shift_JIS decoded accurately to UTF-8");
    }
}

static void testPhase5RandomStressTest600Steps() {
    std::cout << "--- Running Phase 5: 600-Step Random Editing + Full Undo/Redo Stress Test ---" << std::endl;
    Editor ed;
    ed.initGraphics();
    ed.newFile();
    ed.loadSyntaxForExtension("stress.cpp");
    float viewW = 640.0f, viewH = 480.0f;
    ed.cbGetViewSize = [&](float& w, float& h) { w = viewW; h = viewH; };

    std::mt19937 rng(42);
    std::vector<std::string> historySnapshots;
    historySnapshots.push_back(ed.pt.getRange(0, ed.pt.length()));

    const std::vector<std::string> tokens = {
        "int ", "main()", "{", "}", "return 0;", "// comment", "/* block */",
        "こんにちは", "🚀👨‍👩‍👧‍👦❤️", "alpha_beta_123", "    ", "\t"
    };

    const int STEPS = 600;
    for (int step = 0; step < STEPS; ++step) {
        int action = rng() % 10;
        size_t docLen = ed.pt.length();
        if (docLen > 0 && (rng() % 3 == 0)) {
            size_t p1 = rng() % (docLen + 1);
            // Snap to UTF-8 codepoint boundary
            while (p1 > 0 && p1 < docLen && ((unsigned char)ed.pt.charAt(p1) & 0xC0) == 0x80) p1--;
            ed.cursors.clear();
            ed.cursors.push_back({ p1, p1, ed.getXFromPos(p1), ed.getXFromPos(p1), false, false });
        }

        switch (action) {
            case 0:
            case 1:
            case 2:
                ed.insertAtCursors(tokens[rng() % tokens.size()]);
                break;
            case 3:
                ed.insertNewlineWithAutoIndent();
                break;
            case 4:
                ed.backspaceAtCursors();
                break;
            case 5:
                ed.deleteForwardAtCursors();
                break;
            case 6:
                ed.duplicateLines((rng() % 2) == 0);
                break;
            case 7:
                ed.moveLines((rng() % 2) == 0);
                break;
            case 8:
                if (rng() % 2 == 0) ed.indentLines(true);
                else ed.unindentLines();
                break;
            case 9:
                ed.toggleComment();
                break;
        }
        verifyLineStartsInvariant(ed);
    }

    std::string finalDoc = ed.pt.getRange(0, ed.pt.length());
    int undoCount = (int)ed.undo.undoStack.size();
    CHECK(undoCount > 400, "Over 400 undo batches recorded during 600 random editing steps");

    while (!ed.undo.undoStack.empty()) {
        ed.performUndo();
        verifyLineStartsInvariant(ed);
    }
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "", "Full Undo across all 600 random steps returned cleanly to empty document");

    while (!ed.undo.redoStack.empty()) {
        ed.performRedo();
        verifyLineStartsInvariant(ed);
    }
    CHECK(ed.pt.getRange(0, ed.pt.length()) == finalDoc, "Full Redo across all 600 random steps reproduced exact final document");
}

static void testParityEdgeCases() {
    std::cout << "--- Running Parity Edge Cases: UTF-16LE/BE Save, Whole-Line Copy/Paste, Overwrite Mode & Soft-Wrap SubLine Scroll ---" << std::endl;
    Editor ed;
    std::string tempDir = (std::filesystem::temp_directory_path() / "miu_macos_test_appdata").string();
    std::filesystem::create_directories(tempDir);
    ed.customAppSupportDir = tempDir;
    float viewW = 240.0f, viewH = 120.0f;
    ed.cbGetViewSize = [&](float& w, float& h) { w = viewW; h = viewH; };

    std::string clipBuf;
    bool clipRect = false;
    ed.cbSetClipboard = [&](const std::string& s, bool isR) { clipBuf = s; clipRect = isR; };
    ed.cbGetClipboard = [&](bool& isR) -> std::string { isR = clipRect; return clipBuf; };

    // 1. UTF-16LE & UTF-16BE saveFile + openFileFromPath round-trip + newFile encoding reset
    std::string u16Sample = "Hello UTF-16 日本語 🚀\nLine 2";
    ed.pt.initFromFile(u16Sample.data(), u16Sample.size());
    ed.rebuildLineStarts();
    ed.currentEncoding = ENC_UTF16LE;
    std::string u16LePath = tempDir + "/test_utf16le.txt";
    CHECK(ed.saveFile(UTF8ToW(u16LePath)), "saveFile succeeded for ENC_UTF16LE");
    CHECK(ed.openFileFromPath(u16LePath), "openFileFromPath succeeded for ENC_UTF16LE");
    CHECK(ed.currentEncoding == ENC_UTF16LE, "Opened UTF-16LE file detected as ENC_UTF16LE");
    CHECK(ed.pt.getRange(0, ed.pt.length()) == u16Sample, "UTF-16LE round-trip preserved CJK and emoji content");

    ed.currentEncoding = ENC_UTF16BE;
    std::string u16BePath = tempDir + "/test_utf16be.txt";
    CHECK(ed.saveFile(UTF8ToW(u16BePath)), "saveFile succeeded for ENC_UTF16BE");
    CHECK(ed.openFileFromPath(u16BePath), "openFileFromPath succeeded for ENC_UTF16BE");
    CHECK(ed.currentEncoding == ENC_UTF16BE, "Opened UTF-16BE file detected as ENC_UTF16BE");
    CHECK(ed.pt.getRange(0, ed.pt.length()) == u16Sample, "UTF-16BE round-trip preserved CJK and emoji content");

    ed.isDirty = false;
    ed.newFile();
    CHECK(ed.currentEncoding == ENC_UTF8_NOBOM && ed.currentCodePage == 0, "newFile resets encoding to ENC_UTF8_NOBOM");

    // 2. Whole-line copy & paste above current line preserving cursor column
    std::string linesDoc = "first line\nsecond line";
    ed.pt.initFromFile(linesDoc.data(), linesDoc.size());
    ed.rebuildLineStarts();
    // Place cursor at column 3 of line 0 ("fir|st line") with no selection
    ed.cursors = { { 3, 3, ed.getXFromPos(3), ed.getXFromPos(3), false, false } };
    ed.copyToClipboard();
    CHECK(ed.lastCopyWasLine, "copyToClipboard without selection sets lastCopyWasLine");
    // Move cursor to column 4 of line 1 ("seco|nd line", pos = 11 + 4 = 15)
    ed.cursors = { { 15, 15, ed.getXFromPos(15), ed.getXFromPos(15), false, false } };
    ed.pasteFromClipboard();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "first line\nfirst line\nsecond line", "Whole-line paste inserted above current line");
    CHECK(ed.cursors[0].head == 26, "Whole-line paste preserved cursor column offset on original line");
    ed.performUndo();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == linesDoc && ed.cursors[0].head == 15, "Undo of whole-line paste restored text and cursor column");

    // 3. deleteLine at EOF removes preceding newline
    ed.cursors = { { 15, 15, ed.getXFromPos(15), ed.getXFromPos(15), false, false } };
    ed.deleteLine();
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "first line", "deleteLine on last line removes preceding newline");

    // 4. Overwrite mode (isOverwriteMode)
    std::string ovDoc = "abc\ndef";
    ed.pt.initFromFile(ovDoc.data(), ovDoc.size());
    ed.rebuildLineStarts();
    ed.isOverwriteMode = true;
    ed.cursors = { { 1, 1, ed.getXFromPos(1), ed.getXFromPos(1), false, false } };
    ed.insertAtCursors("Z");
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "aZc\ndef", "isOverwriteMode replaced character 'b' with 'Z'");
    // At end of line (before '\n'), overwrite mode should insert without eating '\n'
    ed.cursors = { { 3, 3, ed.getXFromPos(3), ed.getXFromPos(3), false, false } };
    ed.insertAtCursors("!");
    CHECK(ed.pt.getRange(0, ed.pt.length()) == "aZc!\ndef", "isOverwriteMode at EOL did not overwrite newline");
    ed.isOverwriteMode = false;

    // 5. Soft-wrap singleLineWrap getScrollBarLayout & ensureCaretVisible vScrollSubLine
    std::string longSingle(600, 'm');
    ed.pt.initFromFile(longSingle.data(), longSingle.size());
    ed.rebuildLineStarts();
    ed.wordWrapEnabled = true;
    ed.vScrollPos = 0;
    ed.vScrollSubLine = 0;
    ScrollBarLayout wrapLayout = ed.getScrollBarLayout(viewW, viewH);
    CHECK(wrapLayout.needV && wrapLayout.totalVisualLines > wrapLayout.visibleLines, "Single long wrapped line triggers vertical scrollbar in soft-wrap mode");
    ed.cursors = { { longSingle.size(), longSingle.size(), 0.0f, 0.0f, false, false } };
    ed.ensureCaretVisible();
    CHECK(ed.vScrollSubLine > 0, "ensureCaretVisible scrolls vScrollSubLine when caret is at end of wrapped single line");

    // 6. Japanese IME uncommitted composition (imeComp) rendering on empty and non-empty lines
    ed.isDirty = false;
    ed.initGraphics();
    ed.newFile();
    ed.wordWrapEnabled = false;
    ed.imeComp = "にほんご変換テスト";
    {
        int bmpW = 300, bmpH = 80;
        std::vector<uint8_t> pixels(bmpW * bmpH * 4, 0);
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(pixels.data(), bmpW, bmpH, 8, bmpW * 4, cs, kCGImageAlphaPremultipliedLast);
        CGContextTranslateCTM(ctx, 0, bmpH);
        CGContextScaleCTM(ctx, 1.0, -1.0);
        ed.render(ctx, (float)bmpW, (float)bmpH);
        CGContextRelease(ctx);
        CGColorSpaceRelease(cs);

        // Count drawn (non-zero alpha) pixels inside the text area between gutterWidth+4 and caret X
        int startX = (int)ed.gutterWidth + 4;
        int endX = std::min(bmpW - 10, startX + 80);
        int maxY = std::min(bmpH, (int)ed.lineHeight);
        int drawnPixels = 0;
        for (int y = 0; y < maxY; ++y) {
            for (int x = startX; x < endX; ++x) {
                int idx = (y * bmpW + x) * 4;
                if (pixels[idx + 3] > 10) {
                    drawnPixels++;
                }
            }
        }
        CHECK(drawnPixels > 20, "Uncommitted Japanese IME composition text (imeComp) is rendered on an empty line");
    }
    ed.imeComp = "";

    // 7. Live Preview toggle & theme background CSS verification (macOS gray window regression guard)
    {
        ed.newFile();
        ed.showHelpPopup = true;
        ed.isDarkMode = false;
        int previewCbCount = 0;
        ed.cbUpdatePreview = [&]() { previewCbCount++; };
        ed.togglePreview();
        CHECK(ed.showPreview == true && !ed.showHelpPopup && previewCbCount == 1, "togglePreview enables showPreview, hides showHelpPopup, and fires cbUpdatePreview");
        MiuPreview::PreviewRenderResult emptyLightRes = ed.getPreviewRenderResult();
        CHECK(emptyLightRes.isMdOrMermaidShell == true, "Empty untitled document uses Markdown/Mermaid shell in preview");
        CHECK(emptyLightRes.fullHtml.find("html{background:var(--pele-bg);color:var(--md-fg);min-height:100%}") != std::string::npos,
              "Preview shell sets explicit background on root html element so empty documents are not transparent");
        CHECK(emptyLightRes.fullHtml.find("<html data-theme=\"light\">") != std::string::npos,
              "Light mode preview sets data-theme=light on html element");
        CHECK(emptyLightRes.fullHtml.find("--sb-thumb:rgba(0,0,0,0.38)") != std::string::npos &&
              emptyLightRes.fullHtml.find(".__ag_sb_v") != std::string::npos &&
              emptyLightRes.fullHtml.find("window.__miuSbUpd=upd") != std::string::npos,
              "Light mode preview includes unified scrollbar CSS and JS controller");

        ed.isDarkMode = true;
        ed.updateThemeColors();
        CHECK(previewCbCount == 2 && ed.previewInitialLoaded == false, "updateThemeColors invalidates previewInitialLoaded and updates preview");
        MiuPreview::PreviewRenderResult emptyDarkRes = ed.getPreviewRenderResult();
        CHECK(emptyDarkRes.fullHtml.find("<html data-theme=\"dark\">") != std::string::npos,
              "Dark mode preview sets data-theme=dark on html element");
        CHECK(emptyDarkRes.fullHtml.find("--pele-bg:#000000;") != std::string::npos,
              "Dark mode preview sets --pele-bg to pure black (#000000)");
        CHECK(emptyDarkRes.fullHtml.find("--sb-thumb:rgba(255,255,255,0.42)") != std::string::npos,
              "Dark mode preview sets --sb-thumb to match MiuScroller dark mode knob");
        ed.togglePreview();
        CHECK(ed.showPreview == false && previewCbCount == 3, "togglePreview disables showPreview and fires cbUpdatePreview");
    }
}

int main() {
    testPhase1PieceTableAndFastLineStarts();
    testPhase1FastSearchReplaceAndReDoS();
    testPhase1Utf16MapAndScrollBarFix();
    testPhase1SoftWrapBracketsEmojiAndEditing();
    testPhase2SyntaxAll49CategoriesCommentAndEmmet();
    testPhase3PreviewPeleAndLatex();
    testPhase4ViewStateRecentFilesAndEncodings();
    testPhase5RandomStressTest600Steps();
    testParityEdgeCases();

    std::cout << "\n==========================================" << std::endl;
    std::cout << "Total Assertions Passed: " << g_passCount << std::endl;
    std::cout << "Total Assertions Failed: " << g_failCount << std::endl;
    std::cout << "==========================================" << std::endl;
    return (g_failCount == 0) ? 0 : 1;
}
