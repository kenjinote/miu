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
        if (full[i] == '\r') {
            if (i + 1 < full.size() && full[i + 1] == '\n') {
                expected.push_back(i + 2);
                ++i;
            } else {
                expected.push_back(i + 1);
            }
        } else if (full[i] == '\n') {
            expected.push_back(i + 1);
        }
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

    // Regression test for Gantt 3-digit year (205-01-01) and malformed/in-progress diagram inputs
    std::string gantt205 =
        "gantt\n"
        "    title A Gantt Diagram\n"
        "    %% This is a comment\n"
        "    dateFormat YYYY-MM-DD\n"
        "    section Section\n"
        "        A task          :a1, 205-01-01, 30d\n"
        "        Another task    :after a1, 20d\n"
        "    section Another\n"
        "        Task in Another :2014-01-12, 12d\n"
        "        another task    :24d\n";
    auto ganttRes = MiuPreview::renderPreview(gantt205, "mmd", true);
    CHECK(ganttRes.bodyHtml.find("<svg") != std::string::npos, "Gantt with 205-01-01 renders SVG without crashing");

    // Malformed / mid-typing Gantt and C4 diagrams must return cleanly without std::terminate
    std::string ganttBad =
        "gantt\n"
        "    dateFormat YYYY-MM-DD\n"
        "    section S\n"
        "        Task :a1, 205-, 30d\n";
    auto ganttBadRes = MiuPreview::renderPreview(ganttBad, "mmd", true);
    CHECK(ganttBadRes.bodyHtml.find("data-empty=\"1\"") != std::string::npos, "Malformed Gantt returns empty placeholder without crashing");

    std::string c4Bad =
        "C4Context\n"
        "    Rel(unknownA, unknownB, \"Uses\")\n";
    auto c4BadRes = MiuPreview::renderPreview(c4Bad, "mmd", true);
    CHECK(c4BadRes.bodyHtml.find("data-empty=\"1\"") != std::string::npos, "C4 with unknown rel returns empty placeholder without crashing");
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
#if defined(__APPLE__)
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
#endif
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

    // 8. iOS Save As filename extension preservation & Dropbox overwrite GUID resolution
    {
        CHECK(MiuIsLikelyTemporaryGuidFileName("E621E1F8-C36C-495A-93FC-0C247A3E6E5F") == true,
              "Detects standard 8-4-4-4-12 UUID without extension as temporary GUID");
        CHECK(MiuIsLikelyTemporaryGuidFileName("/private/var/mobile/File Provider Storage/e621e1f8-c36c-495a-93fc-0c247a3e6e5f.txt") == true,
              "Detects UUID with extension in File Provider Storage path as temporary GUID");
        CHECK(MiuIsLikelyTemporaryGuidFileName("0123456789abcdef0123456789abcdef.md") == true,
              "Detects 32-hex string filename as temporary GUID");
        CHECK(MiuIsLikelyTemporaryGuidFileName(".dat.nosync1234.5678") == true,
              "Detects .dat.nosync staging file as temporary GUID");
        CHECK(MiuIsLikelyTemporaryGuidFileName("notes.txt.sb-1234abcd-5678ef") == true,
              "Detects .sb- atomic save staging file as temporary GUID");
        CHECK(MiuExtractRealFileNameFromStagingName("notes.txt.sb-1234abcd-5678ef") == "notes.txt",
              "Extracts real filename from .sb- atomic save staging name");
        CHECK(MiuIsLikelyTemporaryGuidFileName("notes.txt") == false &&
              MiuIsLikelyTemporaryGuidFileName("README.md") == false &&
              MiuIsLikelyTemporaryGuidFileName("2026-10-31-report.txt") == false,
              "Normal filenames are not misidentified as GUIDs");

        CHECK(MiuBuildExportFileName("/var/mobile/Documents/memo.txt", "", "無題") == "memo",
              "MiuBuildExportFileName strips extension for temp export URL so iOS does not force-append .txt");
        CHECK(MiuBuildExportFileName("/var/mobile/Documents/index.html", "", "無題") == "index",
              "MiuBuildExportFileName strips .html for temp export URL so custom extensions are not doubled");
        CHECK(MiuBuildExportFileName("", "", "無題") == "無題",
              "MiuBuildExportFileName returns extensionless Untitled so saving index.html does not become index.html.txt");
        CHECK(MiuBuildExportFileName("", "", "Untitled") == "Untitled",
              "MiuBuildExportFileName returns extensionless English Untitled");
        CHECK(MiuBuildExportFileName("/File Provider Storage/E621E1F8-C36C-495A-93FC-0C247A3E6E5F", "overwrite_target.txt", "無題") == "overwrite_target",
              "MiuBuildExportFileName uses currentDocumentDisplayName stem when currentFilePath is a Dropbox GUID");

        std::vector<std::string> guidOnlyCandidates = {
            "E621E1F8-C36C-495A-93FC-0C247A3E6E5F",
            "/private/var/mobile/Containers/Shared/AppGroup/1234/File Provider Storage/E621E1F8-C36C-495A-93FC-0C247A3E6E5F"
        };
        CHECK(MiuResolveDisplayFileName(guidOnlyCandidates, "memo.txt", "無題") == "memo.txt",
              "MiuResolveDisplayFileName falls back to real filename with extension when Dropbox returns GUID path on overwrite");

        std::vector<std::string> localizedWithoutExtCandidates = {
            "memo",
            "E621E1F8-C36C-495A-93FC-0C247A3E6E5F.txt"
        };
        CHECK(MiuResolveDisplayFileName(localizedWithoutExtCandidates, "", "無題") == "memo.txt",
              "MiuResolveDisplayFileName restores extension onto extensionless localized name");
    }

    // 9. Regression guard for iOS Open Recent crash: failed openFileFromPath must preserve previous MappedFile/PieceTable
    {
        std::string tetrisPath = tempDir + "/Tetris.html";
        std::string tetrisContent(15184, 'A');
        tetrisContent.replace(0, 15, "<!DOCTYPE html>");
        {
            std::ofstream ofs(tetrisPath, std::ios::binary);
            ofs.write(tetrisContent.data(), tetrisContent.size());
        }
        CHECK(ed.openFileFromPath(tetrisPath), "Opened 15KB Tetris.html successfully");
        ed.cursors = { { 300, 300, 0.0f, 0.0f, false, false } };
        CHECK(!ed.openFileFromPath(tempDir + "/nonexistent_deleted_recent_file.txt"),
              "openFileFromPath returns false for missing recent file");
        CHECK(ed.pt.length() == tetrisContent.size(),
              "Failed openFileFromPath preserves previous document length");
        CHECK(ed.getPositionByUTF16Offset(300, -1) == 299 && ed.pt.charAt(0) == '<',
              "getPositionByUTF16Offset and PieceTable::charAt remain valid after failed openFileFromPath (no dangling mmap pointer)");
        std::filesystem::remove(tetrisPath);
        CHECK(ed.pt.getRange(0, 15) == "<!DOCTYPE html>",
              "Unlinking opened file on disk does not invalidate heap-backed MappedFile buffer");
    }

    // 10. Off-screen caret visibility regression guard (scroll down past Line 1 & wrapped sub-line scrolled above viewport)
    {
        ed.newFile();
        std::string doc;
        for (int i = 0; i < 30; ++i) {
            doc += "          Line " + std::to_string(i + 1) + "\n";
        }
        ed.pt.initFromFile(doc.data(), doc.size());
        ed.rebuildLineStarts();
        ed.cursors = { { 0, 0, 0.0f, 0.0f, false, false } };
        ed.vScrollPos = 5;
        ed.vScrollSubLine = 0;
        CHECK(ed.getLineIdx(ed.cursors[0].head) == 0 && ed.vScrollPos == 5,
              "macOS/iOS off-screen cursor on Line 1 is below start=vScrollPos(5) and remains at offset 0");
    }
}

static void testPhase6EncodingsNewlinesRectCursorSearch() {
    std::cout << "--- Running Phase 6: Comprehensive Encodings, CR/CRLF/LF Line Endings, Syntax, Rect Select, Cursor, Search/Replace ---" << std::endl;
    std::string tempDir = (std::filesystem::temp_directory_path() / "miu_macos_test_phase6").string();
    std::filesystem::create_directories(tempDir);

    Editor ed;
    ed.customAppSupportDir = tempDir;
    ed.initGraphics();
    float viewW = 600.0f, viewH = 400.0f;
    ed.cbGetViewSize = [&](float& w, float& h) { w = viewW; h = viewH; };

    std::string clipBuf;
    bool clipRect = false;
    ed.cbSetClipboard = [&](const std::string& s, bool isR) { clipBuf = s; clipRect = isR; };
    ed.cbGetClipboard = [&](bool& isR) -> std::string { isR = clipRect; return clipBuf; };

    // 1. Test encodings: Shift_JIS, EUC-JP, ISO-2022-JP (JIS), BOM-less UTF-16LE/BE, >64KB Shift_JIS, partially corrupted Shift_JIS, 4-byte UTF-8 surrogate pair
    {
        // ISO-2022-JP ("東京" = ESC $ B 45 6C 35 7E ESC ( B)
        const unsigned char jisBytes[] = { 0x1B, '$', 'B', 0x45, 0x6C, 0x35, 0x7E, 0x1B, '(', 'B', '\r', 'A', 'B', 'C' };
        std::string jisPath = tempDir + "/jis_cr.txt";
        { std::ofstream ofs(jisPath, std::ios::binary); ofs.write((const char*)jisBytes, sizeof(jisBytes)); }
        CHECK(ed.openFileFromPath(jisPath), "Opened ISO-2022-JP + CR file");
        CHECK(ed.currentEncoding == ENC_LOCAL, "ISO-2022-JP detected as ENC_LOCAL (not misidentified as UTF-8)");
        CHECK(ed.newlineStr == "\r", "Detected CR newline style in ISO-2022-JP file");
        CHECK(ed.lineStarts.size() == 2 && ed.pt.getRange(0, ed.pt.length()) == "東京\rABC", "Decoded ISO-2022-JP and split CR lines");
        verifyLineStartsInvariant(ed);

        // EUC-JP ("東京タワー\n大阪")
        const unsigned char eucBytes[] = { 0xC5, 0xEC, 0xB5, 0xFE, 0xA5, 0xBF, 0xA5, 0xEF, 0xA1, 0xBC, '\n', 0xC2, 0xE7, 0xBA, 0xE5 };
        std::string eucPath = tempDir + "/euc.txt";
        { std::ofstream ofs(eucPath, std::ios::binary); ofs.write((const char*)eucBytes, sizeof(eucBytes)); }
        CHECK(ed.openFileFromPath(eucPath), "Opened EUC-JP file");
        CHECK(ed.currentEncoding == ENC_LOCAL && ed.pt.getRange(0, ed.pt.length()) == "東京タワー\n大阪", "Decoded EUC-JP file accurately");

        // >64KB ASCII followed by Shift_JIS ("東京タワー")
        std::string largeSjis(70000, 'a');
        largeSjis[100] = '\r';
        const unsigned char sjisTail[] = { 0x93, 0x8C, 0x8B, 0x9E, 0x83, 0x5E, 0x83, 0x8F, 0x81, 0x5B };
        largeSjis.append((const char*)sjisTail, sizeof(sjisTail));
        std::string largeSjisPath = tempDir + "/large_sjis.txt";
        { std::ofstream ofs(largeSjisPath, std::ios::binary); ofs.write(largeSjis.data(), largeSjis.size()); }
        CHECK(ed.openFileFromPath(largeSjisPath), "Opened >64KB ASCII + Shift_JIS file");
        CHECK(ed.currentEncoding == ENC_LOCAL, "Detected ENC_LOCAL even when non-ASCII bytes start past 64KB");
        CHECK(ed.pt.getRange(ed.pt.length() - 15, 15) == "東京タワー", "Decoded Shift_JIS tail past 64KB");

        // Partially corrupted Shift_JIS (valid line + invalid byte 0xFF + valid line)
        std::string corruptSjis((const char*)sjisTail, sizeof(sjisTail));
        corruptSjis += "\r\n\xFF\r\n";
        corruptSjis.append((const char*)sjisTail, sizeof(sjisTail));
        std::string corruptPath = tempDir + "/corrupt_sjis.txt";
        { std::ofstream ofs(corruptPath, std::ios::binary); ofs.write(corruptSjis.data(), corruptSjis.size()); }
        CHECK(ed.openFileFromPath(corruptPath), "Opened partially corrupted Shift_JIS file without empty fallback");
        CHECK(ed.pt.getRange(0, ed.pt.length()).find("東京タワー") == 0, "Resilient line-by-line decoding preserved valid Shift_JIS lines");

        // BOM-less UTF-16LE ("Hello\rWorld")
        const unsigned char u16leNoBom[] = { 'H',0, 'e',0, 'l',0, 'l',0, 'o',0, '\r',0, 'W',0, 'o',0, 'r',0, 'l',0, 'd',0 };
        std::string u16leNoBomPath = tempDir + "/u16le_nobom.txt";
        { std::ofstream ofs(u16leNoBomPath, std::ios::binary); ofs.write((const char*)u16leNoBom, sizeof(u16leNoBom)); }
        CHECK(ed.openFileFromPath(u16leNoBomPath), "Opened BOM-less UTF-16LE file");
        CHECK(ed.currentEncoding == ENC_UTF16LE && ed.pt.getRange(0, ed.pt.length()) == "Hello\rWorld" && ed.lineStarts.size() == 2,
              "BOM-less UTF-16LE detected and CR split into 2 lines");

        // Surrogate pairs ("𠮟𠮷😀") in UTF-8 & UTF-16LE round-trip
        std::string surrDoc = "𠮟𠮷😀\rLine2";
        ed.pt.initFromFile(surrDoc.data(), surrDoc.size());
        ed.newlineStr = "\r";
        ed.currentEncoding = ENC_UTF16LE;
        ed.rebuildLineStarts();
        std::string surrPath = tempDir + "/surrogate_u16.txt";
        CHECK(ed.saveFile(UTF8ToW(surrPath)) && ed.openFileFromPath(surrPath), "Saved and reopened UTF-16LE with surrogate pairs");
        CHECK(ed.pt.getRange(0, ed.pt.length()) == surrDoc && ed.newlineStr == "\r", "Preserved surrogate pairs (𠮟𠮷😀) and CR newline");
    }

    // 2. Test all 3 line endings (LF, CRLF, CR) + cross-piece CRLF for cursor movement, syntax highlighting, rectangular selection, and search/replace
    for (const std::string& nl : { std::string("\n"), std::string("\r\n"), std::string("\r") }) {
        ed.newFile();
        ed.newlineStr = nl;
        std::string doc = "int alpha = 1; // comment 1" + nl + "int beta = 2; // comment 2" + nl + "int gamma = 3;";
        ed.pt.initFromFile(doc.data(), doc.size());
        ed.rebuildLineStarts();
        verifyLineStartsInvariant(ed);
        CHECK(ed.lineStarts.size() == 3, "3 lines built for newline style");

        // Cross-piece CRLF check
        if (nl == "\r\n") {
            ed.newFile();
            ed.insertAtCursors("line1\r");
            // Force a second piece by inserting at start then appending \n
            ed.pt.pieces.push_back({ false, ed.pt.addBuf.size(), 5 });
            ed.pt.addBuf.insert(ed.pt.addBuf.end(), { '\n', 'l', 'i', 'n', 'e' });
            ed.pt.totalLength += 5;
            ed.rebuildLineStarts();
            verifyLineStartsInvariant(ed);
            CHECK(ed.lineStarts.size() == 2, "Cross-piece CRLF produces 2 lines, not 3");
            ed.pt.initFromFile(doc.data(), doc.size());
            ed.newlineStr = nl;
            ed.rebuildLineStarts();
        }

        // Syntax highlighting: line comment on line 0 must NOT bleed into line 1
        ed.loadSyntaxForExtension("test.cpp");
        std::vector<std::pair<size_t, size_t>> commentSpans;
        std::vector<std::pair<size_t, size_t>> keywordSpans;
        scanSyntaxHighlighting(ed.currentSyntax, doc, false, [&](size_t s, size_t l, int cat) {
            if (cat == (int)SyntaxTokenCategory::Comment) commentSpans.push_back({ s, l });
            else if (cat == (int)SyntaxTokenCategory::Keyword) keywordSpans.push_back({ s, l });
        });
        CHECK(commentSpans.size() == 2 && keywordSpans.size() == 3, "Syntax highlighting stops line comments at CR/CRLF/LF and highlights all 3 'int' keywords");

        // Cursor movement: moveCaretVisual forward/backward across line break, moveVisualEnd/Home, getWordBoundaries
        ed.cursors = { { 0, 0, 0.0f, 0.0f, false, false } };
        ed.moveVisualEnd(false);
        size_t line0End = ed.cursors[0].head;
        CHECK(line0End == 27, "moveVisualEnd strips CR/CRLF/LF accurately");
        size_t line1Start = ed.lineStarts[1];
        CHECK(ed.moveCaretVisual(line0End, true) == line1Start, "moveCaretVisual(forward) steps over entire line break in 1 step");
        CHECK(ed.moveCaretVisual(line1Start, false) == line0End, "moveCaretVisual(backward) steps back over entire line break in 1 step");
        size_t wStart = 0, wEnd = 0;
        ed.getWordBoundaries(line1Start, wStart, wEnd);
        CHECK(wStart == line1Start && wEnd == line1Start + 3, "getWordBoundaries on 'int' at line 1 start stops cleanly at word boundary");
        ed.getWordBoundaries(line0End - 1, wStart, wEnd);
        CHECK(wEnd == line0End, "getWordBoundaries at end of line 0 does not cross CR/CRLF/LF");

        // Backspace at line1Start merges line 1 into line 0 cleanly
        ed.cursors = { { line1Start, line1Start, 0.0f, 0.0f, false, false } };
        ed.backspaceAtCursors();
        verifyLineStartsInvariant(ed);
        CHECK(ed.lineStarts.size() == 2, "Backspace at line start deleted full newline sequence");
        ed.performUndo();
        verifyLineStartsInvariant(ed);
        CHECK(ed.lineStarts.size() == 3, "Undo restored 3 lines");

        // Delete forward at line0End merges line 1 into line 0 cleanly
        ed.cursors = { { line0End, line0End, 0.0f, 0.0f, false, false } };
        ed.deleteForwardAtCursors();
        verifyLineStartsInvariant(ed);
        CHECK(ed.lineStarts.size() == 2, "Delete forward at line end deleted full newline sequence");
        ed.performUndo();
        verifyLineStartsInvariant(ed);

        // Clipboard paste normalization
        clipBuf = "pasted1\r\npasted2\rpasted3\npasted4";
        clipRect = false;
        ed.lastCopyWasLine = false;
        ed.cursors = { { 0, 0, 0.0f, 0.0f, false, false } };
        ed.pasteFromClipboard();
        verifyLineStartsInvariant(ed);
        std::string expectedPrefix = "pasted1" + nl + "pasted2" + nl + "pasted3" + nl + "pasted4";
        CHECK(ed.pt.getRange(0, expectedPrefix.size()) == expectedPrefix, "pasteFromClipboard normalized mixed CRLF/CR/LF to document newlineStr");
        ed.performUndo();
        verifyLineStartsInvariant(ed);

        // Rectangular block insert with CR/CRLF/LF lines
        ed.cursors = { { 0, 0, 0.0f, 0.0f, false, false } };
        ed.insertRectangularBlock("X\rY\r\nZ");
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(ed.lineStarts[0], 4) == "Xint" &&
              ed.pt.getRange(ed.lineStarts[1], 4) == "Yint" &&
              ed.pt.getRange(ed.lineStarts[2], 4) == "Zint",
              "insertRectangularBlock splits CR/CRLF/LF and inserts at each line without corrupting line endings");
        ed.performUndo();
        verifyLineStartsInvariant(ed);

        // Regex search & replace: ^, $, ., capture groups $1
        size_t mLen = 0;
        size_t pCaret = ed.findText(0, "^int beta", true, true, false, true, &mLen);
        CHECK(pCaret == ed.lineStarts[1] && mLen == 8, "Regex '^int beta' matched at start of line 1");
        size_t pDollar = ed.findText(0, "comment 1$", true, true, false, true, &mLen);
        CHECK(pDollar == 18 && mLen == 9, "Regex 'comment 1$' matched before CR/CRLF/LF at end of line 0");
        size_t pDot = ed.findText(0, "int .*", true, true, false, true, &mLen);
        CHECK(pDot == 0 && mLen == 27, "Regex 'int .*' did not cross CR/CRLF/LF line break");

        // Capture group $1 without ^ and with ^
        ed.searchQuery = "int ([a-z]+)";
        ed.replaceQuery = "var_$1";
        ed.searchRegex = true;
        ed.searchMatchCase = true;
        ed.searchWholeWord = false;
        ed.replaceAll();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(ed.lineStarts[0], 9) == "var_alpha" &&
              ed.pt.getRange(ed.lineStarts[1], 8) == "var_beta" &&
              ed.pt.getRange(ed.lineStarts[2], 9) == "var_gamma",
              "Regex replaceAll with $1 capture group (without ^) replaced accurately");
        ed.performUndo();
        verifyLineStartsInvariant(ed);

        ed.searchQuery = "^int ([a-z]+)";
        ed.replaceQuery = "let_$1";
        ed.replaceAll();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(ed.lineStarts[0], 9) == "let_alpha" &&
              ed.pt.getRange(ed.lineStarts[1], 8) == "let_beta" &&
              ed.pt.getRange(ed.lineStarts[2], 9) == "let_gamma",
              "Regex replaceAll with ^ and $1 capture group replaced accurately across CR/CRLF/LF");
        ed.performUndo();
        verifyLineStartsInvariant(ed);

        // Line Operations: duplicateLines (up/down including EOF), moveLines (up/down including EOF), deleteLines (including EOF), indent/unindent
        {
            std::string ldoc = "first" + nl + "second" + nl + "third";
            ed.newFile();
            ed.newlineStr = nl;
            ed.pt.initFromFile(ldoc.data(), ldoc.size());
            ed.rebuildLineStarts();
            verifyLineStartsInvariant(ed);

            // Duplicate EOF line 2 down
            ed.cursors = { { ed.lineStarts[2], ed.lineStarts[2], 0.0f, 0.0f, false, false } };
            ed.duplicateLines(false);
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "first" + nl + "second" + nl + "third" + nl + "third",
                  "duplicateLines(down) on EOF line uses document newlineStr");
            ed.performUndo();
            verifyLineStartsInvariant(ed);

            // Duplicate line 1 up
            ed.cursors = { { ed.lineStarts[1], ed.lineStarts[1], 0.0f, 0.0f, false, false } };
            ed.duplicateLines(true);
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "first" + nl + "second" + nl + "second" + nl + "third",
                  "duplicateLines(up) preserves document newlineStr");
            ed.performUndo();
            verifyLineStartsInvariant(ed);

            // Move line 1 down (swapping with EOF line 2 that has no trailing newline)
            ed.cursors = { { ed.lineStarts[1], ed.lineStarts[1], 0.0f, 0.0f, false, false } };
            ed.moveLines(false);
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "first" + nl + "third" + nl + "second",
                  "moveLines(down) across EOF line preserves document newlineStr");

            // Move EOF line 2 back up to line 1
            ed.moveLines(true);
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "first" + nl + "second" + nl + "third",
                  "moveLines(up) from EOF line restores exact document");

            // Delete EOF line 2 (must strip preceding CR/CRLF/LF)
            ed.cursors = { { ed.lineStarts[2], ed.lineStarts[2], 0.0f, 0.0f, false, false } };
            ed.deleteLine();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "first" + nl + "second",
                  "deleteLine on EOF line strips preceding newline");
            ed.performUndo();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "first" + nl + "second" + nl + "third",
                  "Undo deleteLine restores exact document");

            // Indent & Unindent all lines
            ed.selectAll();
            ed.indentLines();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "\tfirst" + nl + "\tsecond" + nl + "\tthird",
                  "indentLines prepends tab across CR/CRLF/LF lines");
            ed.unindentLines();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "first" + nl + "second" + nl + "third",
                  "unindentLines removes tab across CR/CRLF/LF lines");
        }

        // Toggle Comment (Line & Block), Case Conversion, Bracket Jump, Emmet, SelectNextOccurrence
        {
            std::string cdoc = "int a = 1;" + nl + "int b = 2;";
            ed.newFile();
            ed.newlineStr = nl;
            ed.pt.initFromFile(cdoc.data(), cdoc.size());
            ed.rebuildLineStarts();
            ed.loadSyntaxForExtension("main.cpp");
            ed.selectAll();
            ed.toggleComment();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "// int a = 1;" + nl + "// int b = 2;",
                  "toggleComment ON for C++ across CR/CRLF/LF");
            ed.toggleComment();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == cdoc,
                  "toggleComment OFF for C++ across CR/CRLF/LF");

            std::string cssDoc = "color: red;" + nl + "margin: 0;";
            ed.newFile();
            ed.newlineStr = nl;
            ed.pt.initFromFile(cssDoc.data(), cssDoc.size());
            ed.rebuildLineStarts();
            ed.loadSyntaxForExtension("style.css");
            ed.selectAll();
            ed.toggleComment();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "/* color: red; */" + nl + "/* margin: 0; */",
                  "toggleComment block comment ON for CSS across CR/CRLF/LF");
            ed.toggleComment();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == cssDoc,
                  "toggleComment block comment OFF for CSS across CR/CRLF/LF");

            // Case conversion
            std::string caseDoc = "hello ＡＢＣ" + nl + "world";
            ed.newFile();
            ed.newlineStr = nl;
            ed.pt.initFromFile(caseDoc.data(), caseDoc.size());
            ed.rebuildLineStarts();
            ed.selectAll();
            ed.convertSelectedText(true);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "HELLO ＡＢＣ" + nl + "WORLD",
                  "convertSelectedText(toUpper) across CR/CRLF/LF");
            ed.convertSelectedText(false);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "hello ａｂｃ" + nl + "world",
                  "convertSelectedText(toLower) across CR/CRLF/LF");

            // Emmet multi-line expansion (ul>li*2)
            std::string emDoc = "ul>li*2";
            ed.newFile();
            ed.newlineStr = nl;
            ed.pt.initFromFile(emDoc.data(), emDoc.size());
            ed.rebuildLineStarts();
            ed.loadSyntaxForExtension("index.html");
            ed.cursors = { { emDoc.size(), emDoc.size(), 0.0f, 0.0f, false, false } };
            bool emOk = ed.tryExpandEmmet();
            verifyLineStartsInvariant(ed);
            CHECK(emOk && ed.pt.getRange(0, ed.pt.totalLength) == "<ul>" + nl + "\t<li></li>" + nl + "\t<li></li>" + nl + "</ul>",
                  "tryExpandEmmet ul>li*2 uses document newlineStr across CR/CRLF/LF");
        }

        // Rectangular selection with virtual space padding on ragged lines before CR/CRLF/LF
        {
            std::string rdoc = "abcdef" + nl + "xy" + nl + "123456";
            ed.newFile();
            ed.newlineStr = nl;
            ed.pt.initFromFile(rdoc.data(), rdoc.size());
            ed.rebuildLineStarts();
            ed.isRectSelecting = true;
            ed.rectAnchorLine = 0;
            ed.rectHeadLine = 2;
            ed.rectAnchorX = 4.0f * ed.charWidth; // col 4 (line 1 "xy" only has 2 chars!)
            ed.rectHeadX = 4.0f * ed.charWidth;
            ed.updateRectSelection();
            ed.insertAtCursors("Z");
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "abcdZef" + nl + "xy  Z" + nl + "1234Z56",
                  "Rectangular virtual space padding inserts spaces BEFORE CR/CRLF/LF on short line");
            ed.performUndo();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == rdoc,
                  "Undo rectangular virtual space edit restores exact ragged lines");
        }

        // Live Preview (Markdown + GFM Table + Mermaid + Task Toggle) across LF, CRLF, and standalone CR!
        {
            std::string mdDoc =
                "# Title 1" + nl +
                "- [ ] Task A" + nl +
                "- [x] Task B" + nl +
                "| H1 | H2 |" + nl +
                "| --- | --- |" + nl +
                "| C1 | C2 |" + nl +
                "```mermaid" + nl +
                "flowchart TD" + nl +
                "    A --> B" + nl +
                "```";
            ed.newFile();
            ed.newlineStr = nl;
            ed.pt.initFromFile(mdDoc.data(), mdDoc.size());
            ed.rebuildLineStarts();
            auto res = MiuPreview::renderPreview(mdDoc, ".md", false, "testnonce");
            CHECK(res.fullHtml.find("<h1>Title 1</h1>") != std::string::npos,
                  "Live Preview renders <h1>Title 1</h1> across CR/CRLF/LF");
            CHECK(res.fullHtml.find("<table>") != std::string::npos && res.fullHtml.find("<td>C1</td>") != std::string::npos,
                  "Live Preview renders GFM table across CR/CRLF/LF");
            CHECK(res.fullHtml.find("data-line=\"1\"") != std::string::npos && res.fullHtml.find("data-line=\"2\"") != std::string::npos,
                  "Live Preview renders task list checkboxes with exact data-line indices across CR/CRLF/LF");
            CHECK(res.fullHtml.find("pele-diagram") != std::string::npos && res.fullHtml.find("<svg") != std::string::npos,
                  "Live Preview renders Mermaid block to SVG across CR/CRLF/LF");

            bool toggled1 = ed.handlePreviewTaskToggle(1);
            verifyLineStartsInvariant(ed);
            CHECK(toggled1 && ed.pt.getRange(0, ed.pt.totalLength).find("- [x] Task A" + nl + "- [x] Task B") != std::string::npos,
                  "handlePreviewTaskToggle toggles line 1 from [ ] to [x] across CR/CRLF/LF");
            bool toggled2 = ed.handlePreviewTaskToggle(2);
            verifyLineStartsInvariant(ed);
            CHECK(toggled2 && ed.pt.getRange(0, ed.pt.totalLength).find("- [x] Task A" + nl + "- [ ] Task B") != std::string::npos,
                  "handlePreviewTaskToggle toggles line 2 from [x] to [ ] across CR/CRLF/LF");
        }

        // Multi-language UTF-8 Regex Search & Replace (Japanese, Korean, Chinese, European 2-byte, Emoji 4-byte)
        {
            std::string doc = "あいう" + nl + "한글A" + nl + "中文😀é";
            ed.newFile();
            ed.newlineStr = nl;
            ed.pt.initFromFile(doc.data(), doc.size());
            ed.rebuildLineStarts();
            ed.searchQuery = ".";
            ed.replaceQuery = "*";
            ed.searchRegex = true;
            ed.searchMatchCase = true;
            ed.replaceAll();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "***" + nl + "***" + nl + "****",
                  "macOS/iOS Regex '.' replaces Japanese/Korean/Chinese/Emoji/European 1 char -> 1 '*' across CR/CRLF/LF");

            std::string doc2 = "あいうABC가나다中文";
            ed.pt.initFromFile(doc2.data(), doc2.size());
            ed.rebuildLineStarts();
            ed.searchQuery = "[あ-ん]";
            ed.replaceQuery = "H";
            ed.replaceAll();
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "HHHABC가나다中文",
                  "macOS/iOS Regex '[あ-ん]' matches Hiragana range only");

            ed.searchQuery = "[가-힣]";
            ed.replaceQuery = "K";
            ed.replaceAll();
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "HHHABCKKK中文",
                  "macOS/iOS Regex '[가-힣]' matches Korean Hangul range only");

            ed.searchQuery = "[一-龥]";
            ed.replaceQuery = "C";
            ed.replaceAll();
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "HHHABCKKKCC",
                  "macOS/iOS Regex '[一-龥]' matches CJK Unified Ideographs range only");
        }
    }
}

static void testPhase7LispSyntaxAndCommentToggle() {
    std::cout << "--- Running Phase 7: Exhaustive Lisp Family Syntax Highlighting & Comment Toggle (;) ---" << std::endl;
    Editor ed;
    ed.initGraphics();

    // 1. Test all 21 Lisp extensions and dotfiles (case-insensitive + full paths)
    const std::vector<std::string> lispFiles = {
        "main.lisp", "init.lsp", "core.cl", "project.asd", "script.ros",
        "init.el", "sicp.scm", "chez.ss", "library.sld", "lang.rkt",
        "core.clj", "app.cljs", "shared.cljc", "deps.edn",
        "macro.hy", "config.fnl", "build.janet",
        ".emacs", ".sbclrc", ".clisprc", ".guile",
        "/Users/dev/.emacs", "C:\\Users\\dev\\.sbclrc",
        "UPPER.LISP", "UPPER.EL", "UPPER.SCM", "UPPER.RKT", "UPPER.CLJ"
    };
    for (const auto& f : lispFiles) {
        ed.loadSyntaxForExtension(f);
        CHECK(ed.currentSyntax.active, "Lisp syntax active for " + f);
        CHECK(ed.currentSyntax.lineComment == ";", "Lisp lineComment is ';' for " + f);
        CHECK(ed.currentSyntax.blockCommentStart == "#|" && ed.currentSyntax.blockCommentEnd == "|#",
              "Lisp blockComment is '#| |#' for " + f);
        CHECK(ed.currentSyntax.disableSingleQuoteStrings == true,
              "Lisp disableSingleQuoteStrings is true for " + f);
        CHECK(ed.currentSyntax.lispWords == true,
              "Lisp lispWords is true for " + f);
    }

    ed.loadSyntaxForExtension("test.lisp");

    // Helper to check the category of a substring in scanned text
    auto checkTokenCat = [&](const std::string& code, const std::string& token, int expectedCat, const std::string& desc, size_t searchFrom = 0) {
        std::vector<int> cats(code.size(), -999);
        scanSyntaxHighlighting(ed.currentSyntax, code, false, [&](size_t s, size_t l, int cat) {
            for (size_t k = s; k < s + l && k < code.size(); ++k) cats[k] = cat;
        });
        size_t pos = code.find(token, searchFrom);
        if (pos == std::string::npos) {
            CHECK(false, desc + " (token not found: " + token + ")");
            return;
        }
        bool allMatch = true;
        for (size_t k = pos; k < pos + token.size(); ++k) {
            if (cats[k] != expectedCat) { allMatch = false; break; }
        }
        CHECK(allMatch, desc + " [" + token + "]");
    };

    // Rule indices in kSyntax49:
    // 0: (:keyword / #:uninterned)
    // 1: ('symbol / #'func)
    // 2: (*earmuffs*)
    // 3: (+constant+)
    // 4: URL
    // 5: Number

    // 2. Keywords across Common Lisp, Emacs Lisp, Scheme, Racket, Clojure
    {
        std::string kwCode =
            "(defun defmacro defvar defparameter defconstant defstruct defclass defgeneric defmethod "
            "lambda let let* flet labels macrolet destructuring-bind multiple-value-bind unwind-protect "
            "handler-case restart-case if when unless cond case typecase loop dolist dotimes progn prog1 "
            "block return-from setq setf incf decf format mapcar cons car cdr + - * / = /= < > <= >= 1+ 1- "
            "defcustom defface cl-defun cl-loop pcase pcase-let* condition-case with-current-buffer save-excursion interactive "
            "#lang define define-syntax define-record-type define/contract syntax-rules syntax-case letrec letrec* "
            "parameterize begin set! set-car! set-cdr! call-with-current-continuation call/cc dynamic-wind "
            "null? pair? list? symbol? number? zero? eq? eqv? equal? string=? for/list for/fold match "
            "defn defn- defonce defmulti defprotocol defrecord ns fn recur try catch finally swap! reset! "
            "-> ->> as-> cond-> some-> if-let when-let if-some when-some doseq mapv filterv reduce-kv nil? pos-int?)";
        const std::vector<std::string> kwList = {
            "defun", "defmacro", "defparameter", "let*", "destructuring-bind", "multiple-value-bind",
            "unwind-protect", "handler-case", "return-from", "1+", "1-", "+", "-", "*", "/", "/=", "<=", ">=",
            "cl-defun", "cl-loop", "pcase-let*", "condition-case", "with-current-buffer", "save-excursion",
            "#lang", "define-syntax", "define/contract", "syntax-rules", "letrec*", "set!", "set-car!",
            "call/cc", "null?", "pair?", "zero?", "equal?", "string=?", "for/list",
            "defn", "defn-", "defprotocol", "swap!", "->", "->>", "cond->", "if-let", "when-some", "reduce-kv", "pos-int?"
        };
        for (const auto& kw : kwList) {
            checkTokenCat(kwCode, kw, (int)SyntaxTokenCategory::Keyword, "Lisp keyword highlighted");
        }
    }

    // 3. Compound user identifiers must NOT have false-positive keyword/literal/number matches
    {
        std::string userCode = "(my-if if-foo foo-let-bar custom-defun let*-helper not-null? my-set! foo/bar 1+x x+1 utf-8 iso-8859-1 #t-shirt nil-val &my-opt)";
        const std::vector<std::string> userSyms = {
            "my-if", "if-foo", "foo-let-bar", "custom-defun", "let*-helper", "not-null?",
            "my-set!", "foo/bar", "1+x", "x+1", "utf-8", "iso-8859-1", "#t-shirt", "nil-val", "&my-opt"
        };
        for (const auto& sym : userSyms) {
            checkTokenCat(userCode, sym, -999, "User compound symbol NOT falsely highlighted");
        }
    }

    // 4. Literals & lambda-list keywords
    {
        std::string litCode = "(t nil true false #t #f #true #false #nil &optional &rest &key &body &aux &allow-other-keys &whole &environment &)";
        const std::vector<std::string> lits = {
            "t", "nil", "true", "false", "#t", "#f", "#true", "#false", "#nil",
            "&optional", "&rest", "&key", "&body", "&aux", "&allow-other-keys", "&whole", "&environment", "&"
        };
        for (const auto& lit : lits) {
            size_t pos = litCode.find(" " + lit + " ");
            if (pos == std::string::npos) pos = litCode.find("(" + lit + " ");
            if (pos == std::string::npos) pos = litCode.find(" " + lit + ")");
            CHECK(pos != std::string::npos, "Found literal in test string: " + lit);
            checkTokenCat(litCode, lit, (int)SyntaxTokenCategory::Literal, "Lisp literal highlighted", pos);
        }
    }

    // 5. Keyword symbols (:kw / #:uninterned), Quoted symbols ('sym / #'fn), Earmuffs (*var* / +const+)
    {
        std::string rxCode = "(:initarg :accessor :if :defun :nil #:uninterned #:if 'quoted-sym 'if 'nil #'my-fn #'defun #'+ *standard-output* *if* +max-size+ +let+)";
        checkTokenCat(rxCode, ":initarg", 0, "Keyword symbol :initarg");
        checkTokenCat(rxCode, ":if", 0, "Keyword symbol :if not overwritten by keyword 'if'");
        checkTokenCat(rxCode, ":defun", 0, "Keyword symbol :defun not overwritten by keyword 'defun'");
        checkTokenCat(rxCode, ":nil", 0, "Keyword symbol :nil not overwritten by literal 'nil'");
        checkTokenCat(rxCode, "#:uninterned", 0, "Uninterned symbol #:uninterned");
        checkTokenCat(rxCode, "#:if", 0, "Uninterned symbol #:if not overwritten by keyword 'if'");
        checkTokenCat(rxCode, "'quoted-sym", 1, "Quoted symbol 'quoted-sym");
        checkTokenCat(rxCode, "'if", 1, "Quoted symbol 'if not overwritten by keyword 'if'");
        checkTokenCat(rxCode, "'nil", 1, "Quoted symbol 'nil not overwritten by literal 'nil'");
        checkTokenCat(rxCode, "#'my-fn", 1, "Function-quoted symbol #'my-fn");
        checkTokenCat(rxCode, "#'defun", 1, "Function-quoted symbol #'defun");
        checkTokenCat(rxCode, "#'+", 1, "Function-quoted operator #'+");
        checkTokenCat(rxCode, "*standard-output*", 2, "Earmuff dynamic variable *standard-output*");
        checkTokenCat(rxCode, "*if*", 2, "Earmuff variable *if* not overwritten by keyword 'if'");
        checkTokenCat(rxCode, "+max-size+", 3, "Constant earmuff +max-size+");
        checkTokenCat(rxCode, "+let+", 3, "Constant earmuff +let+ not overwritten by keyword 'let'");
    }

    // 6. Lisp Number literals (integers, signed, floats, exponents d/f/s/l/e, ratios, radix #x/#b/#o/0x, BigInt N/M)
    {
        std::string numCode = "(42 -42 +42 3.14 -3.14 +0.5 1.5e-3 1.0d0 2.5d-3 1.0f0 1.0s0 1.0l0 1/2 -3/4 +22/7 #xFF #XDeadBeef #b1010 #o755 0xFF 42N 3.14M)";
        const std::vector<std::string> nums = {
            "42", "-42", "+42", "3.14", "-3.14", "+0.5", "1.5e-3", "1.0d0", "2.5d-3",
            "1.0f0", "1.0s0", "1.0l0", "1/2", "-3/4", "+22/7", "#xFF", "#XDeadBeef",
            "#b1010", "#o755", "0xFF", "42N", "3.14M"
        };
        size_t cur = 0;
        for (const auto& num : nums) {
            size_t pos = numCode.find(num, cur);
            CHECK(pos != std::string::npos, "Found number token: " + num);
            checkTokenCat(numCode, num, 5, "Lisp number highlighted", pos);
            cur = pos + num.size();
        }
    }

    // 7. Single-quote reader macro must NOT start a string literal or break comments/keywords
    {
        std::string quoteLine = "(setq x '(defun foo (a) (+ a 10))) ; trailing comment";
        checkTokenCat(quoteLine, "setq", (int)SyntaxTokenCategory::Keyword, "setq before quote");
        checkTokenCat(quoteLine, "defun", (int)SyntaxTokenCategory::Keyword, "defun inside quoted list '(...) is highlighted as keyword, NOT string");
        checkTokenCat(quoteLine, "+", (int)SyntaxTokenCategory::Keyword, "+ inside quoted list is highlighted as keyword");
        checkTokenCat(quoteLine, "10", 5, "10 inside quoted list is highlighted as number");
        checkTokenCat(quoteLine, "; trailing comment", (int)SyntaxTokenCategory::Comment, "Trailing ; comment after single quote is highlighted as Comment");
    }

    // 8. Character literal escapes (#\", #\;, #\|, ?\", ?\;, ?", ?;, \", \;) must NOT corrupt strings or comments
    {
        std::string charEscLine1 = "(list #\\\" #\\; #\\| #\\space #\\newline) (defun after-cl-chars () 123) ; real comment";
        checkTokenCat(charEscLine1, "#\\\"", (int)SyntaxTokenCategory::String, "CL char literal #\\\" highlighted as char/string");
        checkTokenCat(charEscLine1, "#\\;", (int)SyntaxTokenCategory::String, "CL char literal #\\; does not start comment");
        checkTokenCat(charEscLine1, "#\\space", (int)SyntaxTokenCategory::String, "CL named char literal #\\space");
        checkTokenCat(charEscLine1, "defun", (int)SyntaxTokenCategory::Keyword, "defun after #\\\" and #\\; is still highlighted as Keyword");
        checkTokenCat(charEscLine1, "; real comment", (int)SyntaxTokenCategory::Comment, "Real comment after char literals");

        std::string charEscLine2 = "(list ?\\\" ?\\; ?\" ?;) (defun after-elisp-chars () 456)";
        checkTokenCat(charEscLine2, "?\\\"", (int)SyntaxTokenCategory::String, "Elisp char literal ?\\\"");
        checkTokenCat(charEscLine2, "?\\;", (int)SyntaxTokenCategory::String, "Elisp char literal ?\\;");
        checkTokenCat(charEscLine2, "?\"", (int)SyntaxTokenCategory::String, "Elisp char literal ?\"");
        checkTokenCat(charEscLine2, "?;", (int)SyntaxTokenCategory::String, "Elisp char literal ?;");
        checkTokenCat(charEscLine2, "defun", (int)SyntaxTokenCategory::Keyword, "defun after Elisp char literals is highlighted as Keyword");

        std::string charEscLine3 = "(vector \\\" \\; \\newline) (defn after-clj-chars [] 789)";
        checkTokenCat(charEscLine3, "\\\"", (int)SyntaxTokenCategory::String, "Clojure char literal \\\"");
        checkTokenCat(charEscLine3, "\\;", (int)SyntaxTokenCategory::String, "Clojure char literal \\;");
        checkTokenCat(charEscLine3, "\\newline", (int)SyntaxTokenCategory::String, "Clojure char literal \\newline");
        checkTokenCat(charEscLine3, "defn", (int)SyntaxTokenCategory::Keyword, "defn after Clojure char literals is highlighted as Keyword");
    }

    // 9. Multi-line block comments (#| ... |#) and interaction with ', ;, strings, and char literals
    {
        ed.newFile();
        ed.loadSyntaxForExtension("block.lisp");
        std::string blockDoc =
            "(setq a 'quoted) #| block comment start\n"
            "(defun inside-block () 123)\n"
            "end of block |# (defun outside-block () 456)\n"
            "; #| commented-out block start\n"
            "(defun not-in-block () 789)\n"
            "\"#| string block start\"\n"
            "(defun also-not-in-block () 101)\n"
            "(list #\\\" #\\; ?\" ?;) #| block after char literals\n"
            "(defun inside-block-2 () 202)\n"
            "|#\n"
            "(defun final-outside () 303)";
        ed.insertAtCursors(blockDoc);
        CHECK(ed.checkInBlockComment(0) == false, "Line 0 starts outside block comment");
        CHECK(ed.checkInBlockComment(1) == true, "Line 1 is inside #| ... |# even after single quote 'quoted on line 0");
        CHECK(ed.checkInBlockComment(2) == true, "Line 2 starts inside #| ... |#");
        CHECK(ed.checkInBlockComment(3) == false, "Line 3 starts outside after |# on line 2");
        CHECK(ed.checkInBlockComment(4) == false, "Line 4 is NOT in block comment because '; #|' on line 3 was a line comment");
        CHECK(ed.checkInBlockComment(6) == false, "Line 6 is NOT in block comment because '\"#|' on line 5 was inside a string");
        CHECK(ed.checkInBlockComment(8) == true, "Line 8 IS inside block comment started after #\\\" #\\; ?\" ?; on line 7");
        CHECK(ed.checkInBlockComment(10) == false, "Line 10 is outside block comment after |# on line 9");
    }

    // 10. Toggle Comment (;) across LF, CRLF, and CR (single line, indented multi-line, mixed, multi-cursor, undo/redo)
    for (const std::string& nl : { std::string("\n"), std::string("\r\n"), std::string("\r") }) {
        // Single-line toggle ON/OFF
        ed.newFile();
        ed.newlineStr = nl;
        ed.loadSyntaxForExtension("core.clj");
        std::string singleDoc = "(defn greet [name] (println \"Hello\" name))";
        ed.pt.initFromFile(singleDoc.data(), singleDoc.size());
        ed.rebuildLineStarts();
        ed.cursors = { { 5, 5, 0.0f, 0.0f, false, false } };
        ed.toggleComment();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "; " + singleDoc, "Lisp single-line toggleComment ON");
        ed.toggleComment();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == singleDoc, "Lisp single-line toggleComment OFF");

        // Multi-line toggle ON/OFF (minIndent == 0) + Undo/Redo
        std::string multiDoc =
            "(defun factorial (n)" + nl +
            "  (if (<= n 1)" + nl +
            "      1" + nl +
            "      (* n (factorial (1- n)))))";
        ed.newFile();
        ed.newlineStr = nl;
        ed.loadSyntaxForExtension("fact.lisp");
        ed.pt.initFromFile(multiDoc.data(), multiDoc.size());
        ed.rebuildLineStarts();
        ed.selectAll();
        ed.toggleComment();
        verifyLineStartsInvariant(ed);
        std::string expectedCommented =
            "; (defun factorial (n)" + nl +
            ";   (if (<= n 1)" + nl +
            ";       1" + nl +
            ";       (* n (factorial (1- n)))))";
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == expectedCommented, "Lisp multi-line toggleComment ON");
        ed.toggleComment();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == multiDoc, "Lisp multi-line toggleComment OFF");
        ed.performUndo();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == expectedCommented, "Lisp toggleComment Undo restores commented state");
        ed.performUndo();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == multiDoc, "Lisp toggleComment second Undo restores original state");
        ed.performRedo();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == expectedCommented, "Lisp toggleComment Redo re-applies comment");

        // Multi-line indented block toggle ON/OFF (minIndent == 2)
        std::string indentedDoc =
            "  (if (<= n 1)" + nl +
            "      1" + nl +
            "      (* n (factorial (1- n))))";
        ed.newFile();
        ed.newlineStr = nl;
        ed.loadSyntaxForExtension("fact.scm");
        ed.pt.initFromFile(indentedDoc.data(), indentedDoc.size());
        ed.rebuildLineStarts();
        ed.selectAll();
        ed.toggleComment();
        verifyLineStartsInvariant(ed);
        std::string expectedIndentedCommented =
            "  ; (if (<= n 1)" + nl +
            "  ;     1" + nl +
            "  ;     (* n (factorial (1- n))))";
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == expectedIndentedCommented, "Lisp indented block toggleComment ON at minIndent=2");
        ed.toggleComment();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == indentedDoc, "Lisp indented block toggleComment OFF restores exact indentation");

        // Multi-cursor toggleComment on lines 0 and 2 (skipping line 1)
        ed.newFile();
        ed.newlineStr = nl;
        ed.loadSyntaxForExtension("init.el");
        std::string mcDoc = "(setq a 1)" + nl + "(setq b 2)" + nl + "(setq c 3)";
        ed.pt.initFromFile(mcDoc.data(), mcDoc.size());
        ed.rebuildLineStarts();
        ed.cursors = {
            { ed.lineStarts[0], ed.lineStarts[0], 0.0f, 0.0f, false, false },
            { ed.lineStarts[2], ed.lineStarts[2], 0.0f, 0.0f, false, false }
        };
        ed.toggleComment();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == "; (setq a 1)" + nl + "(setq b 2)" + nl + "; (setq c 3)",
              "Lisp multi-cursor toggleComment ON comments only cursor lines 0 and 2");
        ed.toggleComment();
        verifyLineStartsInvariant(ed);
        CHECK(ed.pt.getRange(0, ed.pt.totalLength) == mcDoc,
              "Lisp multi-cursor toggleComment OFF uncomments lines 0 and 2");
    }
}

void testPhase8AssemblyAndNewSyntaxCategories() {
    std::cout << "=== Phase 8: Assembly & 15 New Syntax Categories (kSyntax50..kSyntax64) + Additional Extensions ===" << std::endl;

    Editor ed;
    auto isColor = [](const SyntaxColor& a, const SyntaxColor& b) {
        return std::abs(a.r - b.r) < 0.02f && std::abs(a.g - b.g) < 0.02f &&
               std::abs(a.b - b.b) < 0.02f && std::abs(a.a - b.a) < 0.02f;
    };
    SyntaxColor colComment = {0.0f, 0.50f, 0.0f, 1.0f};
    SyntaxColor colString  = {0.64f, 0.08f, 0.08f, 1.0f};
    SyntaxColor colKeyword = {0.0f, 0.0f, 1.0f, 1.0f};
    SyntaxColor colLiteral = {0.0f, 0.35f, 0.70f, 1.0f};
    SyntaxColor colDefault = {0.0f, 0.0f, 0.0f, 0.0f};

    // 1. Enhanced Assembly (kSyntax44)
    const char* asmFiles[] = {
        "boot.asm", "start.s", "start.S", "macros.inc", "kernel.nasm", "fast.yasm",
        "win64.masm", "mcu.s51", "mcu.a51", "blink.avr", "cpu.mips", "rv64.riscv",
        "module.wat", "test.wast"
    };
    for (const char* f : asmFiles) {
        ed.loadSyntaxForExtension(f);
        CHECK(ed.currentSyntax.active, std::string("Assembly syntax active for ") + f);
        CHECK(ed.currentSyntax.commentLine == ";", std::string("Assembly commentLine is ';' for ") + f);
        CHECK(ed.currentSyntax.commentBlockStart == "/*", std::string("Assembly commentBlockStart is '/*' for ") + f);
        CHECK(ed.currentSyntax.commentBlockEnd == "*/", std::string("Assembly commentBlockEnd is '*/' for ") + f);
    }
    {
        ed.loadSyntaxForExtension("kernel.asm");
        std::string asmCode =
            ".section .text\n"
            ".globl _start\n"
            "_start:\n"
            "    mov rax, 0FFh ; NASM/MASM hex\n"
            "    vaddps zmm0, zmm1, zmm2\n"
            "    stp x29, x30, [sp, #-16]!\n"
            "    addi a0, zero, 42\n"
            "    movq %rax, %rbx /* GAS block comment */\n"
            "    (module (func $add (param i32 i32) (result i32) local.get 0))\n";
        std::vector<SyntaxColor> colors;
        scanSyntaxHighlighting(asmCode, ed.currentSyntax, false, colors, false);
        CHECK(colors.size() == asmCode.size(), "Asm colors vector matches code length");

        size_t posSection = asmCode.find(".section");
        CHECK(!isColor(colors[posSection], colDefault), "Asm directive .section highlighted");
        size_t posLabel = asmCode.find("_start:");
        CHECK(!isColor(colors[posLabel], colDefault), "Asm label _start: highlighted");
        size_t posMov = asmCode.find("mov ");
        CHECK(isColor(colors[posMov], colKeyword), "Asm mnemonic mov highlighted as Keyword");
        size_t posRax = asmCode.find("rax,");
        CHECK(isColor(colors[posRax], colLiteral), "Asm register rax highlighted as Literal");
        size_t posHex = asmCode.find("0FFh");
        CHECK(!isColor(colors[posHex], colDefault), "Asm MASM/NASM hex 0FFh highlighted");
        size_t posComment = asmCode.find("; NASM");
        CHECK(isColor(colors[posComment], colComment), "Asm ';' line comment highlighted");
        size_t posZmm = asmCode.find("zmm0");
        CHECK(isColor(colors[posZmm], colLiteral), "Asm AVX-512 register zmm0 highlighted");
        size_t posStp = asmCode.find("stp ");
        CHECK(isColor(colors[posStp], colKeyword), "ARM64 mnemonic stp highlighted");
        size_t posAddi = asmCode.find("addi ");
        CHECK(isColor(colors[posAddi], colKeyword), "RISC-V mnemonic addi highlighted");
        size_t posAttReg = asmCode.find("%rax");
        CHECK(!isColor(colors[posAttReg], colDefault), "AT&T register %rax highlighted");
        size_t posBlk = asmCode.find("/* GAS");
        CHECK(isColor(colors[posBlk], colComment), "GAS /* ... */ block comment highlighted");
        size_t posWasm = asmCode.find("module");
        CHECK(isColor(colors[posWasm], colKeyword), "WebAssembly .wat module keyword highlighted");
    }

    // 2. All 15 New Language Categories (kSyntax50..kSyntax64)
    struct LangTestSpec {
        const char* filename;
        const char* expectedCommentLine;
        const char* expectedBlockStart;
        const char* expectedBlockEnd;
        const char* sampleKeyword;
        const char* sampleLiteral;
        const char* sampleCode;
    };
    const LangTestSpec specs[] = {
        { "ViewController.m", "//", "/*", "*/", "@interface", "YES",
          "#import <Foundation/Foundation.h>\n@interface Foo : NSObject\n@property (nonatomic, strong) NSString *name;\n@end\nBOOL ok = YES; // comment\n" },
        { "Bridge.mm", "//", "/*", "*/", "@implementation", "nil",
          "@implementation Bridge\n- (id)init { return nil; }\n@end\n" },
        { "build.gradle", "//", "/*", "*/", "dependencies", "true",
          "@CompileStatic\ndef ver = \"${project.version}\"\ndependencies { implementation 'org.codehaus.groovy:groovy:3.0.9'; boolean ok = true }\n" },
        { "Jenkinsfile", "//", "/*", "*/", "pipeline", "true",
          "pipeline {\n  agent any\n  stages { stage('Build') { steps { sh 'make' } } }\n}\nboolean x = true\n" },
        { "parser.ml", "", "(*", "*)", "match", "true",
          "(* OCaml comment *)\nlet rec map (f : 'a -> 'b) xs = match xs with | [] -> [] | h :: t -> if true then f h :: map f t else []\n" },
        { "component.res", "", "(*", "*)", "switch", "true",
          "let x = switch opt { | Some(v) => true | None => false }\n" },
        { "server.erl", "%", "", "", "receive", "ok",
          "-module(server).\n-export([loop/0]).\nloop() -> receive {From, Msg} -> From ! ok, loop() end. % comment\n" },
        { "solver.f90", "!", "", "", "program", ".true.",
          "program main\n  implicit none\n  logical :: flag = .true.\n  real :: x = 1.0\n  ! Fortran comment\nend program main\n" },
        { "payroll.cbl", "*>", "", "", "IDENTIFICATION", "ZERO",
          "       IDENTIFICATION DIVISION.\n       PROGRAM-ID. PAYROLL.\n       DATA DIVISION.\n       WORKING-STORAGE SECTION.\n       01 WS-COUNT PIC 9(4) VALUE ZERO.\n       *> COBOL comment\n       PROCEDURE DIVISION.\n           STOP RUN.\n" },
        { "schema.prisma", "//", "", "", "model", "Int",
          "datasource db { provider = \"postgresql\" }\nmodel User {\n  id Int @id @default(autoincrement())\n  email String @unique\n  @@index([email])\n}\n" },
        { "Vault.sol", "//", "/*", "*/", "contract", "uint256",
          "pragma solidity ^0.8.20;\ncontract Vault {\n  mapping(address => uint256) public balances;\n  function deposit() external payable { balances[msg.sender] += msg.value; }\n}\n" },
        { "main.nim", "#", "#[", "]#", "proc", "int",
          "#[ Nim block comment ]#\nproc add(a, b: int): int {.inline.} =\n  result = a + b # Nim line comment\n" },
        { "app.d", "//", "/*", "*/", "module", "int",
          "module app;\nimport std.stdio;\n@safe @nogc pure nothrow int square(int x) { return x * x; }\n" },
        { "Unit1.pas", "//", "{", "}", "procedure", "Integer",
          "{ Delphi block comment }\nunit Unit1;\ninterface\nprocedure Test(x: Integer);\nimplementation\nprocedure Test(x: Integer);\nvar h: Integer;\nbegin\n  h := $FF;\nend;\nend.\n" },
        { "main.adb", "--", "", "", "procedure", "Integer",
          "with Ada.Text_IO; use Ada.Text_IO;\nprocedure Main is\n  X : Integer := 42;\nbegin\n  Put_Line(Integer'Image(X)); -- Ada comment\nend Main;\n" },
        { "family.pro", "%", "/*", "*/", "findall", "true",
          "/* Prolog block */\nancestor(X, Y) :- parent(X, Y), true.\nquery(L) :- findall(X, ancestor(X, bob), L). % Prolog line\n" },
        { ".vimrc", "\"", "", "", "nnoremap", "v:true",
          "\" Full-line Vimscript comment\nlet g:mapleader = \" \"\nnnoremap <Leader>w :w<CR>\nlet s:flag = v:true \" trailing comment\n" },
        { "flake.nix", "#", "/*", "*/", "let", "mkDerivation",
          "/* Nix block */\nlet\n  pkgs = import <nixpkgs> {};\nin pkgs.stdenv.mkDerivation {\n  pname = \"miu\";\n  src = ./.; # Nix comment\n}\n" }
    };

    for (const auto& sp : specs) {
        ed.newFile();
        ed.loadSyntaxForExtension(sp.filename);
        CHECK(ed.currentSyntax.active, std::string("Syntax active for ") + sp.filename);
        CHECK(ed.currentSyntax.commentLine == sp.expectedCommentLine, std::string("commentLine matches for ") + sp.filename);
        CHECK(ed.currentSyntax.commentBlockStart == sp.expectedBlockStart, std::string("commentBlockStart matches for ") + sp.filename);
        CHECK(ed.currentSyntax.commentBlockEnd == sp.expectedBlockEnd, std::string("commentBlockEnd matches for ") + sp.filename);

        std::string code = sp.sampleCode;
        std::vector<SyntaxColor> colors;
        scanSyntaxHighlighting(code, ed.currentSyntax, false, colors, false);
        CHECK(colors.size() == code.size(), std::string("Colors size matches for ") + sp.filename);

        size_t kwPos = code.find(sp.sampleKeyword);
        CHECK(kwPos != std::string::npos && !isColor(colors[kwPos], colDefault),
              std::string("Keyword '") + sp.sampleKeyword + "' highlighted in " + sp.filename);

        size_t litPos = code.find(sp.sampleLiteral);
        CHECK(litPos != std::string::npos && !isColor(colors[litPos], colDefault),
              std::string("Literal '") + sp.sampleLiteral + "' highlighted in " + sp.filename);

        // Test toggleComment() roundtrip when commentLine is non-empty
        if (!ed.currentSyntax.commentLine.empty()) {
            std::string lineDoc = "first_line_code\nsecond_line_code";
            ed.pt.initFromFile(lineDoc.data(), lineDoc.size());
            ed.rebuildLineStarts();
            ed.cursors = {{0, 0, 0.0f, 0.0f, false, false}};
            ed.toggleComment();
            verifyLineStartsInvariant(ed);
            std::string afterOn = ed.pt.getRange(0, ed.pt.totalLength);
            CHECK(afterOn.find(std::string(sp.expectedCommentLine) + " first_line_code") == 0,
                  std::string("toggleComment ON inserts '") + sp.expectedCommentLine + " ' for " + sp.filename);
            ed.toggleComment();
            verifyLineStartsInvariant(ed);
            CHECK(ed.pt.getRange(0, ed.pt.totalLength) == lineDoc,
                  std::string("toggleComment OFF restores exact line for ") + sp.filename);
        }
    }

    // 3. Special language edge cases:
    // 3a. OCaml ('a -> 'b) polymorphic type variables must NOT swallow strings or break comments
    {
        ed.loadSyntaxForExtension("types.ml");
        std::string mlCode = "type 'a option = None | Some of 'a\nlet s = \"hello\" (* comment *)\n";
        std::vector<SyntaxColor> colors;
        scanSyntaxHighlighting(mlCode, ed.currentSyntax, false, colors, false);
        size_t posSome = mlCode.find("Some");
        CHECK(!isColor(colors[posSome], colString), "OCaml 'a does not swallow subsequent code into a string");
        size_t posStr = mlCode.find("\"hello\"");
        CHECK(isColor(colors[posStr], colString), "OCaml double-quoted string highlighted as String");
        size_t posCmt = mlCode.find("(* comment *)");
        CHECK(isColor(colors[posCmt], colComment), "OCaml (* ... *) comment highlighted as Comment");
    }
    // 3b. Ada attribute ticks (Integer'Image, Arr'Range) must NOT swallow strings or comments
    {
        ed.loadSyntaxForExtension("attr.adb");
        std::string adaCode = "X := Integer'First;\nY := Integer'Last; -- comment after attributes\n";
        std::vector<SyntaxColor> colors;
        scanSyntaxHighlighting(adaCode, ed.currentSyntax, false, colors, false);
        size_t posFirst = adaCode.find("'First");
        CHECK(!isColor(colors[posFirst], colString), "Ada 'First attribute tick is not treated as unclosed string");
        size_t posCmt = adaCode.find("-- comment");
        CHECK(isColor(colors[posCmt], colComment), "Ada -- comment after two attributes is highlighted as Comment");
    }
    // 3c. Vimscript: distinguish full-line/trailing `"` comments from `"string"` literals
    {
        ed.loadSyntaxForExtension(".vimrc");
        std::string vimCode =
            "\" Comment at start of line\n"
            "  \" Indented comment with \"quoted\" word\n"
            "let g:name = \"miu editor\"\n"
            "set number \" trailing comment\n";
        std::vector<SyntaxColor> colors;
        scanSyntaxHighlighting(vimCode, ed.currentSyntax, false, colors, false);
        CHECK(isColor(colors[0], colComment), "Vimscript line-start '\"' is Comment");
        size_t posIndented = vimCode.find("\" Indented");
        CHECK(isColor(colors[posIndented], colComment), "Vimscript indented '\"' is Comment");
        size_t posStr = vimCode.find("\"miu editor\"");
        CHECK(isColor(colors[posStr], colString), "Vimscript '\"miu editor\"' in let assignment is String");
        size_t posTrailing = vimCode.find("\" trailing");
        CHECK(isColor(colors[posTrailing], colComment), "Vimscript trailing '\" trailing comment' is Comment");
    }
    // 3d. COBOL hyphenated keywords (PROGRAM-ID, WORKING-STORAGE, END-IF)
    {
        ed.loadSyntaxForExtension("app.cob");
        std::string cobCode = "PROGRAM-ID. APP.\nWORKING-STORAGE SECTION.\nEND-IF.\n";
        std::vector<SyntaxColor> colors;
        scanSyntaxHighlighting(cobCode, ed.currentSyntax, false, colors, false);
        CHECK(isColor(colors[cobCode.find("PROGRAM-ID")], colKeyword), "COBOL hyphenated keyword PROGRAM-ID highlighted");
        CHECK(isColor(colors[cobCode.find("WORKING-STORAGE")], colKeyword), "COBOL hyphenated keyword WORKING-STORAGE highlighted");
        CHECK(isColor(colors[cobCode.find("END-IF")], colKeyword), "COBOL hyphenated keyword END-IF highlighted");
    }
    // 3e. Metal Shading Language (.metal)
    {
        ed.loadSyntaxForExtension("Shaders.metal");
        CHECK(ed.currentSyntax.active, "Metal shader (.metal) syntax active");
        std::string metalCode = "vertex float4 vertex_main(VertexIn in [[stage_in]], uint vid [[vertex_id]]) { return float4(1.0); }\n";
        std::vector<SyntaxColor> colors;
        scanSyntaxHighlighting(metalCode, ed.currentSyntax, false, colors, false);
        CHECK(isColor(colors[metalCode.find("vertex ")], colKeyword), "Metal 'vertex' keyword highlighted");
        CHECK(isColor(colors[metalCode.find("float4")], colKeyword), "Metal 'float4' type keyword highlighted");
        CHECK(isColor(colors[metalCode.find("stage_in")], colLiteral), "Metal 'stage_in' attribute literal highlighted");
        CHECK(!isColor(colors[metalCode.find("[[vertex_id]]")], colDefault), "Metal [[vertex_id]] attribute highlighted");
    }

    // 4. Category 3 Additional Extensions & Dotfiles/Buildfiles
    const char* extraFiles[] = {
        "main.c++", "main.cp", "script.csx",
        "fast.pyx", "fast.pxd", "fast.pxi", "rules.bzl", "MODULE.bazel", "config.star",
        "BUILD", "BUILD.bazel", "WORKSPACE", "WORKSPACE.bazel", "SConstruct", "SConscript", "Tiltfile",
        "index.mts", "index.cts", "app.es6", "proxy.pac",
        "events.jsonl", "logs.ndjson", "map.geojson", "site.webmanifest", "trace.har",
        ".prettierrc", ".babelrc", ".eslintrc", ".stylelintrc", ".hintrc", ".swcrc",
        "service.wsdl", "Main.storyboard", "View.xib", "layout.fxml", "window.ui", "resources.qrc",
        "styles.pcss", "styles.postcss", "page.wxss",
        "notes.mkd", "notes.mdown", "analysis.rmd", "report.qmd",
        "config.fish", "run.ksh", "run.csh", "run.tcsh", "pkg.ebuild", ".zprofile", ".zshenv", "PKGBUILD",
        "template.phtml",
        "miu.gemspec", "config.ru", "tasks.rake", "App.podspec",
        "Gemfile", "Rakefile", "Podfile", "Fastfile", "Brewfile", "Vagrantfile",
        ".editorconfig", ".gitconfig", ".gitmodules", ".npmrc", ".yarnrc", ".flake8", ".pylintrc", "app.service",
        ".gitignore", ".gitattributes", ".dockerignore", ".npmignore", ".clang-format", ".clang-tidy", ".htaccess",
        "refs.bib", "theme.sty", "article.cls",
        "build.sbt", "01_basic.t", "BSDMakefile", "Justfile",
        "pass.vsh", "pass.fsh", "pass.glslv", "pass.glslf"
    };
    for (const char* f : extraFiles) {
        ed.loadSyntaxForExtension(f);
        CHECK(ed.currentSyntax.active, std::string("Additional extension/dotfile syntax active for: ") + f);
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
    testPhase6EncodingsNewlinesRectCursorSearch();
    testPhase7LispSyntaxAndCommentToggle();
    testPhase8AssemblyAndNewSyntaxCategories();

    std::cout << "\n==========================================" << std::endl;
    std::cout << "Total Assertions Passed: " << g_passCount << std::endl;
    std::cout << "Total Assertions Failed: " << g_failCount << std::endl;
    std::cout << "==========================================" << std::endl;
    return (g_failCount == 0) ? 0 : 1;
}
