#import <Cocoa/Cocoa.h>
#import "EditorCore.h"
static NSString *const kMiuRectangularSelectionType = @"jp.hack.miu.rectangular";
@interface EditorView : NSView <NSTextInputClient, NSTextFieldDelegate>
{
@public
    std::shared_ptr<Editor> editor;
    NSPanel *findPanel;
    NSTextField *findLabel;
    NSTextField *findTextField;
    NSTextField *replaceTextField;
    NSTextField *replaceLabel;
    NSButton *findBtn;
    NSButton *replaceBtn;
    NSButton *replaceAllBtn;
    NSButton *matchCaseBtn;
    NSButton *wholeWordBtn;
    NSButton *regexBtn;
}
- (void)updateScrollers;
- (void)applyZoom:(float)val relative:(bool)rel;
- (void)showFindPanel:(BOOL)replaceMode;
- (void)updateFindQueries;
- (void)findNextWithDirection:(BOOL)forward;
- (BOOL)isReplaceMode;
- (void)findNextAction:(id)sender;
- (void)replaceAction:(id)sender;
- (void)replaceAllAction:(id)sender;
- (void)showGoToLinePanel;
- (void)jumpToLine:(NSInteger)line;
@end
@interface FindPanel : NSPanel
@property (weak) EditorView *editorView;
@end
@implementation FindPanel
- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if ([event keyCode] == 99) {
        BOOL shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
        [self.editorView updateFindQueries];
        [self.editorView findNextWithDirection:!shift];
        return YES;
    }
    if (([event modifierFlags] & NSEventModifierFlagCommand)) {
        NSString *ch = [[event charactersIgnoringModifiers] lowercaseString];
        if ([ch isEqualToString:@"f"]) {
            [self.editorView showFindPanel:NO];
            return YES;
        }
        if ([ch isEqualToString:@"h"] || [ch isEqualToString:@"r"]) {
            if ([event modifierFlags] & NSEventModifierFlagOption) {
                [self.editorView replaceAllAction:nil];
            }
            else {
                if ([self.editorView isReplaceMode]) {
                    [self.editorView replaceAction:nil];
                }
                else {
                    [self.editorView showFindPanel:YES];
                }
            }
            return YES;
        }
        if ([ch isEqualToString:@"x"]) { return [NSApp sendAction:@selector(cut:) to:nil from:self]; }
        if ([ch isEqualToString:@"c"]) { return [NSApp sendAction:@selector(copy:) to:nil from:self]; }
        if ([ch isEqualToString:@"v"]) { return [NSApp sendAction:@selector(paste:) to:nil from:self]; }
        if ([ch isEqualToString:@"a"]) { return [NSApp sendAction:@selector(selectAll:) to:nil from:self]; }
        if ([ch isEqualToString:@"z"]) {
            if ([event modifierFlags] & NSEventModifierFlagShift) {
                return [NSApp sendAction:NSSelectorFromString(@"redo:") to:nil from:self];
            } else {
                return [NSApp sendAction:NSSelectorFromString(@"undo:") to:nil from:self];
            }
        }
    }
    return [super performKeyEquivalent:event];
}
@end
@implementation EditorView {
    NSScroller *vScroller, *hScroller;
    NSPoint boxSelectStartPoint;
    size_t cursorsSnapshotCount;
    NSWindow *goToLineWindow;
    NSTextField *goToLineTextField;
}
- (instancetype)initWithFrame:(NSRect)fr {
    if (self = [super initWithFrame:fr]) {
        editor = std::make_shared<Editor>();
        __weak EditorView* weakSelf = self;
        editor->cbNeedsDisplay = [weakSelf]() {
            __strong EditorView *strongSelf = weakSelf;
            if (strongSelf) [strongSelf setNeedsDisplay:YES];
        };
        editor->cbUpdateScrollers = [weakSelf]() {
            __strong EditorView *strongSelf = weakSelf;
            if (strongSelf) [strongSelf updateScrollers];
        };
        editor->cbBeep = []() { NSBeep(); };
        editor->cbGetViewSize = [weakSelf](float& w, float& h) {
            __strong EditorView *strongSelf = weakSelf;
            if (strongSelf) {
                NSRect b = [strongSelf bounds];
                w = b.size.width; h = b.size.height;
            }
        };
        editor->cbUpdateTitleBar = [weakSelf]() {
            __strong EditorView *strongSelf = weakSelf;
            if (!strongSelf || !strongSelf->editor) return;
            std::wstring untitledStr = UTF8ToW([NSLocalizedString(@"Untitled", @"新規ファイル名") UTF8String]);
            std::wstring t = (strongSelf->editor->isDirty ? L"*" : L"") + (strongSelf->editor->currentFilePath.empty() ? untitledStr : strongSelf->editor->currentFilePath.substr(strongSelf->editor->currentFilePath.find_last_of(L"/")+1)) + L" - " + APP_TITLE;
            [[strongSelf window] setTitle:[NSString stringWithUTF8String:WToUTF8(t).c_str()]];
            [[strongSelf window] setDocumentEdited:strongSelf->editor->isDirty];
        };
        editor->cbSetClipboard = [](const std::string& text, bool isRect) {
            NSPasteboard *pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            [pb setString:[NSString stringWithUTF8String:text.c_str()] forType:NSPasteboardTypeString];
            if(isRect) [pb setData:[NSData data] forType:kMiuRectangularSelectionType];
        };
        editor->cbGetClipboard = [](bool& isRect) -> std::string {
            NSPasteboard *pb = [NSPasteboard generalPasteboard];
            NSString *s = [pb stringForType:NSPasteboardTypeString];
            isRect = ([pb dataForType:kMiuRectangularSelectionType] != nil);
            return s ? [s UTF8String] : "";
        };
        editor->cbShowReplaceAlert = [](int count) {
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:NSLocalizedString(@"Replace All", @"すべて置換のアラートタイトル")];
            NSString *infoFormat = NSLocalizedString(@"%lu occurrence(s) replaced.", @"置換した件数");
            [alert setInformativeText:[NSString stringWithFormat:infoFormat, (unsigned long)count]];
            [alert runModal];
        };
        editor->cbShowUnsavedAlert = [weakSelf]() -> bool {
            __strong EditorView *strongSelf = weakSelf;
            if(!strongSelf) return true;
            NSAlert *a = [NSAlert new];
            [a setMessageText:NSLocalizedString(@"Save changes?", @"変更を保存しますか？")];
            [a addButtonWithTitle:NSLocalizedString(@"Save", @"保存")];
            [a addButtonWithTitle:NSLocalizedString(@"Cancel", @"キャンセル")];
            [a addButtonWithTitle:NSLocalizedString(@"Discard", @"破棄")];
            NSModalResponse r = [a runModal];
            if(r==NSAlertFirstButtonReturn) return strongSelf->editor->currentFilePath.empty() ? strongSelf->editor->saveFileAs() : strongSelf->editor->saveFile(strongSelf->editor->currentFilePath);
            if(r==NSAlertThirdButtonReturn){ strongSelf->editor->isDirty=false; strongSelf->editor->updateTitleBar(); return true; }
            return false;
        };
        editor->cbSaveFileAs = [weakSelf]() -> bool {
            __strong EditorView *strongSelf = weakSelf;
            if (!strongSelf) return false;
            NSSavePanel *p = [NSSavePanel savePanel];
            if([p runModal]==NSModalResponseOK) return strongSelf->editor->saveFile(UTF8ToW([p.URL.path UTF8String]));
            return false;
        };
        editor->cbOpenFile = [weakSelf]() -> bool {
            __strong EditorView *strongSelf = weakSelf;
            if (!strongSelf) return false;
            NSOpenPanel *p = [NSOpenPanel openPanel];
            if([p runModal]==NSModalResponseOK) return strongSelf->editor->openFileFromPath([[[p URLs] objectAtIndex:0].path UTF8String]);
            return false;
        };
        CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
        vScroller = [[NSScroller alloc] initWithFrame:NSMakeRect(fr.size.width-sw, 0, sw, fr.size.height-sw)];
        [vScroller setAutoresizingMask:NSViewMinXMargin|NSViewHeightSizable];
        [vScroller setTarget:self]; [vScroller setAction:@selector(scrollAction:)]; [self addSubview:vScroller];
        hScroller = [[NSScroller alloc] initWithFrame:NSMakeRect(0, fr.size.height-sw, fr.size.width-sw, sw)];
        [hScroller setAutoresizingMask:NSViewWidthSizable|NSViewMinYMargin];
        [hScroller setTarget:self]; [hScroller setAction:@selector(scrollAction:)]; [self addSubview:hScroller];
        [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    }
    return self;
}
- (void)resetCursorRects {
    [super resetCursorRects];
    [self addCursorRect:[self bounds] cursor:[NSCursor IBeamCursor]];
}
- (void)dealloc {
    if (findTextField) {
        [findTextField setDelegate:nil];
        [findTextField setTarget:nil];
    }
    if (replaceTextField) {
        [replaceTextField setDelegate:nil];
        [replaceTextField setTarget:nil];
    }
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([[pboard types] containsObject:NSPasteboardTypeFileURL]) return NSDragOperationCopy;
    return NSDragOperationNone;
}
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([[pboard types] containsObject:NSPasteboardTypeFileURL]) {
        NSArray *urls = [pboard readObjectsForClasses:@[[NSURL class]] options:nil];
        if (urls.count > 0) {
            if (editor->checkUnsavedChanges()) {
                editor->openFileFromPath([[urls[0] path] UTF8String]);
                [[self window] makeKeyAndOrderFront:nil];
                return YES;
            }
        }
    }
    return NO;
}
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)becomeFirstResponder { return YES; }
- (BOOL)isFlipped { return YES; }
- (void)viewDidMoveToWindow {
    if (![self window]) return;
    editor->helpTextStr = UTF8ToW([NSLocalizedString(@"HelpText", @"ヘルプのショートカット一覧") UTF8String]);
    editor->initGraphics();
    BOOL isDark = [[self.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] isEqualToString:NSAppearanceNameDarkAqua];
    editor->isDarkMode = isDark;
    editor->updateThemeColors(); [self updateScrollers];
    [[self window] makeFirstResponder:self];
}
- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    if (editor) {
        BOOL isDark = [[self.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] isEqualToString:NSAppearanceNameDarkAqua];
        if (editor->isDarkMode != isDark) {
            editor->isDarkMode = isDark;
            editor->updateThemeColors();
            [self setNeedsDisplay:YES];
        }
    }
}
- (void)drawRect:(NSRect)r {
    editor->render([[NSGraphicsContext currentContext] CGContext], (float)self.bounds.size.width, (float)self.bounds.size.height);
}
- (void)undo:(id)sender {
    if (editor) {
        editor->performUndo();
        [self setNeedsDisplay:YES];
    }
}
- (void)redo:(id)sender {
    if (editor) {
        editor->performRedo();
        [self setNeedsDisplay:YES];
    }
}
- (void)updateScrollers {
    NSRect b = [self bounds];
    CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    float visibleHeight = b.size.height;
    float visibleWidth = b.size.width - editor->gutterWidth;
    int totalLines = (int)editor->lineStarts.size();
    float maxLineWidth = editor->maxLineWidth;
    bool needsV = totalLines > 1;
    if (needsV) visibleWidth -= sw;
    bool needsH = maxLineWidth > visibleWidth;
    if (needsH) visibleHeight -= sw;
    if (needsV) {
        CGFloat vHeight = needsH ? b.size.height - sw : b.size.height;
        [vScroller setFrame:NSMakeRect(b.size.width - sw, 0, sw, vHeight)];
    }
    if (needsH) {
        CGFloat hWidth = needsV ? b.size.width - sw : b.size.width;
        [hScroller setFrame:NSMakeRect(0, b.size.height - sw, hWidth, sw)];
    }
    [vScroller setHidden:!needsV];
    [hScroller setHidden:!needsH];
    editor->visibleVScrollWidth = needsV ? sw : 0.0f;
    editor->visibleHScrollHeight = needsH ? sw : 0.0f;
    int visibleLines = (int)std::floor(visibleHeight / editor->lineHeight); if (visibleLines < 1) visibleLines = 1;
    [vScroller setKnobProportion:std::min(1.0, (double)visibleLines / (totalLines + visibleLines - 1))];
    int maxV = std::max(0, totalLines - 1);
    if (needsV && maxV > 0) { [vScroller setDoubleValue:(double)editor->vScrollPos / maxV]; [vScroller setEnabled:YES]; } else { [vScroller setDoubleValue:0.0]; [vScroller setEnabled:NO]; editor->vScrollPos = 0; }
    if (visibleWidth < 1) visibleWidth = 1;
    [hScroller setKnobProportion:std::min(1.0, (double)visibleWidth / maxLineWidth)];
    float maxH = maxLineWidth - visibleWidth;
    if (needsH && maxH > 0) { [hScroller setDoubleValue:(double)editor->hScrollPos / maxH]; [hScroller setEnabled:YES]; } else { [hScroller setDoubleValue:0.0]; [hScroller setEnabled:NO]; editor->hScrollPos = 0; }
    [self setNeedsDisplay:YES];
}
- (void)scrollAction:(NSScroller*)s {
    NSRect b = [self bounds]; CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    if (s == vScroller) {
        int maxV = std::max(0, (int)editor->lineStarts.size() - 1);
        editor->vScrollPos = std::max(0, (int)std::round([s doubleValue] * maxV));
    } else {
        float visibleWidth = b.size.width - editor->gutterWidth - sw; float maxH = editor->maxLineWidth - visibleWidth;
        editor->hScrollPos = std::max(0, (int)([s doubleValue] * maxH));
    }
    [self setNeedsDisplay:YES];
}
- (void)applyZoom:(float)val relative:(bool)rel {
    editor->updateFont(rel ? editor->currentFontSize * val : val);
    editor->zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    editor->zoomPopupText = std::to_string((int)std::round(editor->currentFontSize)) + "px";
    [self setNeedsDisplay:YES];
}
- (void)mouseDown:(NSEvent *)e {
    if (editor->showHelpPopup) { editor->showHelpPopup = false; [self setNeedsDisplay:YES]; }
    [[self window] makeFirstResponder:self];
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    if (p.x > self.bounds.size.width - editor->visibleVScrollWidth || p.y > self.bounds.size.height - editor->visibleHScrollHeight) return;
    NSInteger clicks = [e clickCount];
    bool isOption = ([e modifierFlags] & NSEventModifierFlagOption);
    bool isShift = ([e modifierFlags] & NSEventModifierFlagShift);
    if (!isOption && !isShift) editor->cursors.clear();
    boxSelectStartPoint = p;
    cursorsSnapshotCount = editor->cursors.size();
    float docX = (float)p.x - editor->gutterWidth + (float)editor->hScrollPos;
    float snappedX = docX;
    if (editor->charWidth > 0) { int cols = (int)std::round(docX / editor->charWidth); if (cols < 0) cols = 0; snappedX = (float)cols * editor->charWidth; }
    size_t pos = editor->getDocPosFromPoint((float)p.x, (float)p.y);
    int li = editor->getLineIdx(pos);
    float physX = editor->getXInLine(li, pos);
    Cursor newCursor; newCursor.head = pos; newCursor.anchor = pos; newCursor.isVirtual = false;
    if (isOption && (snappedX > physX + (editor->charWidth * 0.5f))) { newCursor.desiredX = snappedX; newCursor.isVirtual = true; }
    else { newCursor.desiredX = physX; newCursor.isVirtual = false; }
    newCursor.originalAnchorX = newCursor.desiredX;
    if (clicks == 2) {
        size_t s, end; editor->getWordBoundaries(pos, s, end);
        newCursor.head = end; newCursor.anchor = s; newCursor.desiredX = editor->getXInLine(editor->getLineIdx(end), end); newCursor.originalAnchorX = newCursor.desiredX; newCursor.isVirtual = false;
        editor->cursors.push_back(newCursor);
    } else if (clicks >= 3) {
        size_t s = editor->lineStarts[li], end = (li + 1 < (int)editor->lineStarts.size()) ? editor->lineStarts[li+1] : editor->pt.length();
        newCursor.head = end; newCursor.anchor = s; newCursor.desiredX = editor->getXInLine(editor->getLineIdx(end), end); newCursor.originalAnchorX = newCursor.desiredX; newCursor.isVirtual = false;
        editor->cursors.push_back(newCursor);
    } else { editor->cursors.push_back(newCursor); }
    [self setNeedsDisplay:YES];
}
- (void)mouseDragged:(NSEvent *)e {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    bool isOption = ([e modifierFlags] & NSEventModifierFlagOption);
    auto snapToGrid = [&](float rawX) -> float {
        if (editor->charWidth <= 0) return rawX; int cols = (int)std::round(rawX / editor->charWidth); if (cols < 0) cols = 0; return (float)cols * editor->charWidth;
    };
    if (isOption) {
        editor->cursors.clear();
        float startY = std::min((float)boxSelectStartPoint.y, (float)p.y), endY = std::max((float)boxSelectStartPoint.y, (float)p.y);
        int startLine = (int)std::floor((startY + (float)editor->vScrollPos * editor->lineHeight) / editor->lineHeight);
        int endLine = (int)std::floor((endY + (float)editor->vScrollPos * editor->lineHeight) / editor->lineHeight);
        startLine = std::clamp(startLine, 0, (int)editor->lineStarts.size() - 1); endLine = std::clamp(endLine, 0, (int)editor->lineStarts.size() - 1);
        float rawStartX = (float)boxSelectStartPoint.x - editor->gutterWidth + (float)editor->hScrollPos;
        float rawEndX = (float)p.x - editor->gutterWidth + (float)editor->hScrollPos;
        float gridStartX = snapToGrid(rawStartX), gridEndX = snapToGrid(rawEndX);
        for (int i = startLine; i <= endLine; ++i) {
            size_t pHead = editor->getPosFromLineAndX(i, gridEndX), pAnchor = editor->getPosFromLineAndX(i, gridStartX);
            Cursor c; c.head = pHead; c.anchor = pAnchor; c.desiredX = gridEndX; c.originalAnchorX = gridStartX; c.isVirtual = true;
            editor->cursors.push_back(c);
        }
    } else {
        size_t pos = editor->getDocPosFromPoint((float)p.x, (float)p.y);
        if (!editor->cursors.empty()) { Cursor& c = editor->cursors.back(); c.head = pos; int li = editor->getLineIdx(pos); c.desiredX = editor->getXInLine(li, pos); c.isVirtual = false; }
    }
    editor->ensureCaretVisible(); [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)e { [self setNeedsDisplay:YES]; }
- (void)showFindPanel:(BOOL)replaceMode {
    editor->isReplaceMode = replaceMode;
    auto [candidate, _] = editor->getHighlightTarget();
    bool hasSelection = !editor->cursors.empty() && editor->cursors.back().hasSelection();
    if (hasSelection && !candidate.empty()) editor->searchQuery = candidate;
    else if (editor->searchQuery.empty() && !candidate.empty()) editor->searchQuery = candidate;
    if (!findPanel) {
        findPanel = [[FindPanel alloc] initWithContentRect:NSMakeRect(0, 0, 360, 160)
                                                 styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskNonactivatingPanel
                                                   backing:NSBackingStoreBuffered defer:NO];
        [(FindPanel*)findPanel setEditorView:self];
        [findPanel setLevel:NSFloatingWindowLevel];
        [findPanel setHidesOnDeactivate:NO];
        [findPanel setReleasedWhenClosed:NO];
        NSRect parentFrame = [[self window] frame];
        NSRect panelFrame = [findPanel frame];
        panelFrame.origin.x = parentFrame.origin.x + (parentFrame.size.width - panelFrame.size.width) / 2.0;
        panelFrame.origin.y = parentFrame.origin.y + (parentFrame.size.height - panelFrame.size.height) / 2.0;
        [findPanel setFrame:panelFrame display:NO];
        NSView *cv = [findPanel contentView];
        findLabel = [NSTextField labelWithString:NSLocalizedString(@"Find:", @"検索ラベル")];
        [cv addSubview:findLabel];
        findTextField = [NSTextField textFieldWithString:@""];
        [findTextField setDelegate:self];
        [findTextField setTarget:self];
        [findTextField setAction:@selector(findNextAction:)];
        [cv addSubview:findTextField];
        replaceLabel = [NSTextField labelWithString:NSLocalizedString(@"Replace:", @"置換ラベル")];
        [cv addSubview:replaceLabel];
        replaceTextField = [NSTextField textFieldWithString:@""];
        [replaceTextField setDelegate:self];
        [cv addSubview:replaceTextField];
        matchCaseBtn = [NSButton checkboxWithTitle:NSLocalizedString(@"Match Case", @"大文字小文字を区別") target:self action:@selector(findOptionsChanged:)];
        [cv addSubview:matchCaseBtn];
        wholeWordBtn = [NSButton checkboxWithTitle:NSLocalizedString(@"Whole Word", @"単語単位") target:self action:@selector(findOptionsChanged:)];
        [cv addSubview:wholeWordBtn];
        regexBtn = [NSButton checkboxWithTitle:NSLocalizedString(@"Regex", @"正規表現") target:self action:@selector(findOptionsChanged:)];
        [cv addSubview:regexBtn];
        findBtn = [NSButton buttonWithTitle:NSLocalizedString(@"Find Next", @"次を検索") target:self action:@selector(findNextAction:)];
        [findBtn setKeyEquivalent:@"\r"];
        [findBtn setToolTip:NSLocalizedString(@"Find Next (F3)", @"次を検索のツールチップ")];
        [cv addSubview:findBtn];
        replaceBtn = [NSButton buttonWithTitle:NSLocalizedString(@"Replace", @"置換") target:self action:@selector(replaceAction:)];
        [replaceBtn setKeyEquivalent:@"r"];
        [replaceBtn setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
        [replaceBtn setToolTip:NSLocalizedString(@"Replace (⌘R)", @"置換のツールチップ")];
        [cv addSubview:replaceBtn];
        replaceAllBtn = [NSButton buttonWithTitle:NSLocalizedString(@"Replace All", @"すべて置換") target:self action:@selector(replaceAllAction:)];
        [replaceAllBtn setKeyEquivalent:@"r"];
        [replaceAllBtn setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
        [replaceAllBtn setToolTip:NSLocalizedString(@"Replace All (⌥⌘R)", @"すべて置換のツールチップ")];
        [cv addSubview:replaceAllBtn];
        [matchCaseBtn setState:editor->searchMatchCase ? NSControlStateValueOn : NSControlStateValueOff];
        [wholeWordBtn setState:editor->searchWholeWord ? NSControlStateValueOn : NSControlStateValueOff];
        [regexBtn setState:editor->searchRegex ? NSControlStateValueOn : NSControlStateValueOff];
    }
    [findPanel setTitle:replaceMode ? NSLocalizedString(@"Replace", @"置換パネルタイトル") : NSLocalizedString(@"Find", @"検索パネルタイトル")];
    [replaceLabel setHidden:!replaceMode];
    [replaceTextField setHidden:!replaceMode];
    [replaceBtn setHidden:!replaceMode];
    [replaceAllBtn setHidden:!replaceMode];
    CGFloat panelHeight = 185;
    [findPanel setContentSize:NSMakeSize(360, panelHeight)];
    [findLabel setFrame:NSMakeRect(10, 150, 60, 20)];
    [findTextField setFrame:NSMakeRect(80, 150, 260, 22)];
    [replaceLabel setFrame:NSMakeRect(10, 120, 60, 20)];
    [replaceTextField setFrame:NSMakeRect(80, 120, 260, 22)];
    [matchCaseBtn setFrame:NSMakeRect(80, 95, 260, 20)];
    [wholeWordBtn setFrame:NSMakeRect(80, 75, 260, 20)];
    [regexBtn setFrame:NSMakeRect(80, 55, 260, 20)];
    [replaceAllBtn setFrame:NSMakeRect(80, 10, 100, 32)];
    [replaceBtn setFrame:NSMakeRect(180, 10, 80, 32)];
    [findBtn setFrame:NSMakeRect(260, 10, 90, 32)];
    if (!editor->searchQuery.empty()) [findTextField setStringValue:[NSString stringWithUTF8String:editor->searchQuery.c_str()]];
    if (!editor->replaceQuery.empty()) [replaceTextField setStringValue:[NSString stringWithUTF8String:editor->replaceQuery.c_str()]];
    [[self window] addChildWindow:findPanel ordered:NSWindowAbove];
    [findPanel makeKeyAndOrderFront:nil];
    [findPanel makeFirstResponder:findTextField];
}
- (void)updateFindQueries {
    editor->searchQuery = [[findTextField stringValue] UTF8String];
    editor->replaceQuery = [[replaceTextField stringValue] UTF8String];
}
- (void)controlTextDidChange:(NSNotification *)obj {
    if ([obj object] == findTextField || [obj object] == replaceTextField) {
        [self updateFindQueries];
        [self setNeedsDisplay:YES];
    }
}
- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector {
    if (commandSelector == @selector(insertNewline:)) {
        if (control == findTextField) {
            [self findNextAction:nil]; return YES;
        } else if (control == replaceTextField) {
            [self replaceAction:nil]; return YES;
        }
    }
    return NO;
}
- (void)findOptionsChanged:(NSButton *)sender {
    editor->searchMatchCase = ([matchCaseBtn state] == NSControlStateValueOn);
    editor->searchWholeWord = ([wholeWordBtn state] == NSControlStateValueOn);
    editor->searchRegex = ([regexBtn state] == NSControlStateValueOn);
    [self setNeedsDisplay:YES];
}
- (void)findNextWithDirection:(BOOL)forward {
    if (editor) {
        editor->findNext(forward);
    }
}
- (BOOL)isReplaceMode {
    if (editor) {
        return editor->isReplaceMode;
    }
    return NO;
}
- (void)findNextAction:(id)sender { [self updateFindQueries]; editor->findNext(true); }
- (void)replaceAction:(id)sender { [self updateFindQueries]; editor->replaceNext(); }
- (void)replaceAllAction:(id)sender { [self updateFindQueries]; editor->replaceAll(); }
- (void)jumpToLine:(NSInteger)line {
    if (!editor) return;
    if (line < 1) line = 1;
    int totalLines = (int)editor->lineStarts.size();
    if (line > totalLines) line = totalLines;
    int lineIdx = (int)line - 1;
    size_t newPos = editor->lineStarts[lineIdx];
    editor->cursors.clear();
    Cursor c;
    c.head = newPos;
    c.anchor = newPos;
    c.desiredX = 0;
    c.originalAnchorX = 0;
    c.isVirtual = false;
    editor->cursors.push_back(c);
    editor->ensureCaretVisible();
    [self setNeedsDisplay:YES];
}
- (void)showGoToLinePanel {
    if (goToLineWindow && [goToLineWindow isVisible]) return;
    NSRect rect = NSMakeRect(0, 0, 300, 120);
    goToLineWindow = [[NSWindow alloc] initWithContentRect:rect
                                                 styleMask:NSWindowStyleMaskTitled
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
    [goToLineWindow setReleasedWhenClosed:NO];
    NSTextField *label = [NSTextField labelWithString:NSLocalizedString(@"Line Number:", nil)];
    [label setFrame:NSMakeRect(20, 80, 260, 20)];
    [goToLineWindow.contentView addSubview:label];
    goToLineTextField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 50, 260, 24)];
    [goToLineTextField setPlaceholderString:@"1..."];
    if (!editor->cursors.empty()) {
        int currentLine = editor->getLineIdx(editor->cursors.back().head) + 1;
        [goToLineTextField setStringValue:[NSString stringWithFormat:@"%d", currentLine]];
    }
    [goToLineWindow.contentView addSubview:goToLineTextField];
    NSButton *goBtn = [NSButton buttonWithTitle:NSLocalizedString(@"Go", nil) target:self action:@selector(performGoToLineAction:)];
    [goBtn setFrame:NSMakeRect(190, 10, 90, 30)];
    [goBtn setKeyEquivalent:@"\r"];
    [goToLineWindow.contentView addSubview:goBtn];
    NSButton *cancelBtn = [NSButton buttonWithTitle:NSLocalizedString(@"Cancel", nil) target:self action:@selector(cancelGoToLineAction:)];
    [cancelBtn setFrame:NSMakeRect(100, 10, 90, 30)];
    [cancelBtn setKeyEquivalent:@"\033"];
    [goToLineWindow.contentView addSubview:cancelBtn];
    [goToLineWindow setInitialFirstResponder:goToLineTextField];
    [self.window beginSheet:goToLineWindow completionHandler:nil];
}
- (void)performGoToLineAction:(id)sender {
    NSInteger line = [goToLineTextField integerValue];
    [self jumpToLine:line];
    [self.window endSheet:goToLineWindow];
    [goToLineWindow orderOut:nil];
    goToLineWindow = nil;
    goToLineTextField = nil;
    [self.window makeFirstResponder:self];
}
- (void)cancelGoToLineAction:(id)sender {
    [self.window endSheet:goToLineWindow];
    [goToLineWindow orderOut:nil];
    goToLineWindow = nil;
    goToLineTextField = nil;
    [self.window makeFirstResponder:self];
}
- (void)keyDown:(NSEvent *)e {
    unsigned short code = [e keyCode]; NSString *chars = [e charactersIgnoringModifiers];
    bool cmd = ([e modifierFlags] & NSEventModifierFlagCommand);
    bool shift = ([e modifierFlags] & NSEventModifierFlagShift);
    bool ctrl = ([e modifierFlags] & NSEventModifierFlagControl);
    if (editor->showHelpPopup) { editor->showHelpPopup = false; [self setNeedsDisplay:YES]; if (code == 122) return; }
    if (code == 122) { editor->showHelpPopup = true; [self setNeedsDisplay:YES]; return; }
    if (code == 53) { if (editor->cursors.size() > 1 || (editor->cursors.size() == 1 && editor->cursors[0].hasSelection())) { Cursor lastC = editor->cursors.back(); lastC.anchor = lastC.head; editor->cursors.clear(); editor->cursors.push_back(lastC); [self setNeedsDisplay:YES]; return; } }
    if (code == 48) {
        if (shift) { editor->unindentLines(); } else {
            bool isRectMode = editor->cursors.size() > 1;
            if (isRectMode) { editor->insertAtCursors("\t"); } else { editor->indentLines(false); }
        }
        [self setNeedsDisplay:YES];
        return;
    }
    if (cmd && (code == 126 || code == 115)) {
        editor->jumpToFileEdge(true, shift);
        return;
    }
    if (cmd && (code == 125 || code == 119)) {
        editor->jumpToFileEdge(false, shift);
        return;
    }
    if (code == 115 || code == 119) {
        for (auto& c : editor->cursors) {
            if (code == 115) {
                c.head = editor->lineStarts[editor->getLineIdx(c.head)];
            } else {
                int li = editor->getLineIdx(c.head);
                size_t nextLineStart = (li + 1 < (int)editor->lineStarts.size()) ? editor->lineStarts[li + 1] : editor->pt.length();
                if (nextLineStart > editor->lineStarts[li] && editor->pt.charAt(nextLineStart - 1) == '\n') {
                    nextLineStart--;
                    if (nextLineStart > editor->lineStarts[li] && editor->pt.charAt(nextLineStart - 1) == '\r') nextLineStart--;
                }
                c.head = nextLineStart;
            }
            c.desiredX = editor->getXFromPos(c.head);
            if (!shift) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
        }
        editor->ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (code == 116 || code == 121) {
        float viewHeight = [self bounds].size.height - editor->visibleHScrollHeight;
        int pageLines = std::max(1, (int)(viewHeight / editor->lineHeight) - 1);
        int totalLines = (int)editor->lineStarts.size();
        for (auto& c : editor->cursors) {
            int currentLine = editor->getLineIdx(c.head); int newLine = currentLine;
            if (code == 116) { newLine = currentLine - pageLines; if (newLine < 0) newLine = 0; }
            else { newLine = currentLine + pageLines; if (newLine >= totalLines) newLine = totalLines - 1; }
            if (code == 121 && newLine == totalLines - 1 && currentLine == totalLines - 1) c.head = editor->pt.length();
            else c.head = editor->getPosFromLineAndX(newLine, c.desiredX);
            if (!shift) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
        }
        if (code == 116) editor->vScrollPos = std::max(0, editor->vScrollPos - pageLines); else editor->vScrollPos = std::min(totalLines - 1, editor->vScrollPos + pageLines);
        editor->ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (code == 99) {
        editor->findNext(!shift);
        return;
    }
    if (cmd) {
        NSString *lowerChar = [chars lowercaseString];
        if (ctrl && [lowerChar isEqualToString:@"f"]) { [[self window] toggleFullScreen:nil]; return; }
        if ([lowerChar isEqualToString:@"q"]) { [NSApp terminate:nil]; return; }
        if ([lowerChar isEqualToString:@"u"]) { editor->convertSelectedText(!shift); return; }
        if (shift && [lowerChar isEqualToString:@"k"]) { editor->deleteLine(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"d"]) { editor->selectNextOccurrence(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"a"]) { editor->selectAll(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"s"]) { shift ? editor->saveFileAs() : (editor->currentFilePath.empty() ? editor->saveFileAs() : editor->saveFile(editor->currentFilePath)); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"c"]) { editor->copyToClipboard(); return; }
        if ([lowerChar isEqualToString:@"x"]) { editor->cutToClipboard(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"v"]) { editor->pasteFromClipboard(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"z"]) { shift ? editor->performRedo() : editor->performUndo(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"o"]) { [NSApp sendAction:@selector(openDocument:) to:nil from:self]; return; }
        if ([lowerChar isEqualToString:@"n"]) { [NSApp sendAction:@selector(newDocument:) to:nil from:self]; return; }
        if ([lowerChar isEqualToString:@"0"]) { [self applyZoom:14.0f relative:false]; return; }
        if ([lowerChar isEqualToString:@"+"] || [lowerChar isEqualToString:@"="]) { [self applyZoom:1.1f relative:true]; return; }
        if ([lowerChar isEqualToString:@"-"]) { [self applyZoom:0.9f relative:true]; return; }
        if ([lowerChar isEqualToString:@"]"]) { editor->indentLines(true); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"["]) { editor->unindentLines(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"f"]) { [self showFindPanel:NO]; return; }
        if ([lowerChar isEqualToString:@"h"] || [lowerChar isEqualToString:@"r"]) { [self showFindPanel:YES]; return; }
        if ([lowerChar isEqualToString:@"l"] || [lowerChar isEqualToString:@"g"]) { [self showGoToLinePanel]; return; }
    }
    if (code >= 123 && code <= 126) {
        bool opt = ([e modifierFlags] & NSEventModifierFlagOption);
        if (code == 126 && opt) { if (shift) editor->copyLines(true); else editor->moveLines(true); [self setNeedsDisplay:YES]; return; }
        if (code == 125 && opt) { if (shift) editor->copyLines(false); else editor->moveLines(false); [self setNeedsDisplay:YES]; return; }
        for (auto& c : editor->cursors) {
            if (code == 123) {
                size_t p = c.head; int li = editor->getLineIdx(p); size_t lineStart = editor->lineStarts[li];
                if (cmd) { if (p == lineStart && p > 0) p--; else { while (p > lineStart && !editor->isWordChar(editor->pt.charAt(p - 1))) p--; while (p > lineStart && editor->isWordChar(editor->pt.charAt(p - 1))) p--; } c.head = p; }
                else c.head = editor->moveCaretVisual(c.head, false);
                c.isVirtual = false;
            } else if (code == 124) {
                size_t p = c.head; size_t len = editor->pt.length(); int li = editor->getLineIdx(p);
                size_t lineStart = editor->lineStarts[li], lineEnd = (li + 1 < (int)editor->lineStarts.size()) ? editor->lineStarts[li + 1] : len;
                size_t physEnd = lineEnd; if (physEnd > lineStart && editor->pt.charAt(physEnd - 1) == '\n') physEnd--; if (physEnd > lineStart && editor->pt.charAt(physEnd - 1) == '\r') physEnd--;
                if (cmd) { if (p == physEnd && p < len) p = lineEnd; else { while (p < physEnd && editor->isWordChar(editor->pt.charAt(p))) p++; while (p < physEnd && !editor->isWordChar(editor->pt.charAt(p))) p++; } c.head = p; }
                else c.head = editor->moveCaretVisual(c.head, true);
                c.isVirtual = false;
            } else if (code == 126) {
                int l = editor->getLineIdx(c.head);
                if (l > 0) { size_t nextPos = editor->getPosFromLineAndX(l - 1, c.desiredX); size_t prevLineStart = editor->lineStarts[l - 1]; size_t prevLineEnd = editor->lineStarts[l]; if (prevLineEnd > prevLineStart && editor->pt.charAt(prevLineEnd - 1) == '\n') prevLineEnd--; if (prevLineEnd > prevLineStart && editor->pt.charAt(prevLineEnd - 1) == '\r') prevLineEnd--; if (nextPos > prevLineEnd) c.head = prevLineEnd; else c.head = nextPos; }
                c.isVirtual = false;
            } else if (code == 125) {
                int l = editor->getLineIdx(c.head);
                if (l + 1 < (int)editor->lineStarts.size()) { size_t nextPos = editor->getPosFromLineAndX(l + 1, c.desiredX); size_t nextLineStart = editor->lineStarts[l + 1]; size_t nextLineEnd = (l + 2 < (int)editor->lineStarts.size()) ? editor->lineStarts[l + 2] : editor->pt.length(); if (nextLineEnd > nextLineStart && editor->pt.charAt(nextLineEnd - 1) == '\n') nextLineEnd--; if (nextLineEnd > nextLineStart && editor->pt.charAt(nextLineEnd - 1) == '\r') nextLineEnd--; if (nextPos > nextLineEnd) c.head = nextLineEnd; else c.head = nextPos; }
                c.isVirtual = false;
            }
            if (code == 123 || code == 124) c.desiredX = editor->getXFromPos(c.head);
            if (!shift) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
        }
        editor->ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (![self.inputContext handleEvent:e]) [super keyDown:e];
}
- (void)scrollWheel:(NSEvent *)e {
    if ([e modifierFlags] & NSEventModifierFlagCommand) { float dy = [e scrollingDeltaY]; if (dy == 0) dy = [e deltaY]; if (dy != 0) { float factor = (dy > 0) ? 1.1f : 0.9f; editor->updateFont(editor->currentFontSize * factor); editor->zoomPopupText = std::to_string((int)std::round(editor->currentFontSize)) + "px"; editor->zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000); [self setNeedsDisplay:YES]; } return; }
    NSRect b = [self bounds];
    int totalLines = (int)editor->lineStarts.size();
    int maxV = std::max(0, totalLines - 1);
    editor->vScrollPos = std::clamp(editor->vScrollPos - (int)std::round([e deltaY]), 0, maxV);
    float visibleWidth = b.size.width - editor->gutterWidth - editor->visibleVScrollWidth;
    int maxH = std::max(0, (int)(editor->maxLineWidth - visibleWidth + editor->charWidth * 4));
    editor->hScrollPos = std::clamp(editor->hScrollPos - (int)[e deltaX], 0, maxH);
    [self updateScrollers]; [self setNeedsDisplay:YES];
}
- (void)insertText:(id)s replacementRange:(NSRange)r {
    NSString* t = [s isKindOfClass:[NSAttributedString class]] ? [s string] : s;
    if ([t isEqualToString:@"\r"] || [t isEqualToString:@"\n"]) {
        editor->insertNewlineWithAutoIndent();
    } else {
        bool usePadding = false;
        for (const auto& c : editor->cursors) {
            if (c.isVirtual) {
                usePadding = true;
                break;
            }
        }
        if (usePadding) {
            editor->insertAtCursorsWithPadding([t UTF8String]);
        } else {
            editor->insertAtCursors([t UTF8String]);
        }
    }
    editor->imeComp = "";
    [self setNeedsDisplay:YES];
}
- (void)setMarkedText:(id)s selectedRange:(NSRange)sr replacementRange:(NSRange)rr { editor->imeComp = [([s isKindOfClass:[NSAttributedString class]] ? [s string] : s) UTF8String]; [self setNeedsDisplay:YES]; }
- (void)unmarkText { editor->imeComp = ""; [self setNeedsDisplay:YES]; }
- (BOOL)hasMarkedText { return !editor->imeComp.empty(); }
- (NSRange)markedRange { return [self hasMarkedText] ? NSMakeRange(0, editor->imeComp.length()) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return NSMakeRange(NSNotFound, 0); }
- (void)doCommandBySelector:(SEL)s {
    if (s == @selector(deleteBackward:)) editor->backspaceAtCursors();
    else if (s == @selector(deleteForward:)) editor->deleteForwardAtCursors();
    else if (s == @selector(insertNewline:)) editor->insertNewlineWithAutoIndent();
    [self setNeedsDisplay:YES]; }
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)r actualRange:(NSRangePointer)ar { return nil; }
- (NSArray*)validAttributesForMarkedText { return @[]; }
- (NSRect)firstRectForCharacterRange:(NSRange)r actualRange:(NSRangePointer)ar { if (editor->cursors.empty()) return NSZeroRect; float x = editor->getXFromPos(editor->cursors.back().head), y = (float)(editor->getLineIdx(editor->cursors.back().head) - editor->vScrollPos) * editor->lineHeight; return [[self window] convertRectToScreen:[self convertRect:NSMakeRect(editor->gutterWidth-(float)editor->hScrollPos+x, y, 2, editor->lineHeight) toView:nil]]; }
- (NSUInteger)characterIndexForPoint:(NSPoint)p { return 0; }
@end
@interface CustomWindow : NSWindow @end
@implementation CustomWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end
@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSMutableArray<CustomWindow *> *windows;
@end
@implementation AppDelegate
- (instancetype)init { if (self = [super init]) { self.windows = [NSMutableArray array]; } return self; }
- (void)createWindowWithPath:(const char*)path {
    CustomWindow *win = [[CustomWindow alloc] initWithContentRect:NSMakeRect(0,0,800,600) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    [win center]; [win setDelegate:self];
    if (self.windows.count > 0) { NSRect lastFrame = self.windows.lastObject.frame; [win setFrameTopLeftPoint:NSMakePoint(lastFrame.origin.x + 22, lastFrame.origin.y - 22)]; }
    EditorView *v = [[EditorView alloc] initWithFrame:win.contentView.bounds];
    if (path) v->editor->openFileFromPath(path); else v->editor->newFile();
    [win setContentView:v]; [win makeKeyAndOrderFront:nil]; [win makeFirstResponder:v];
    [self.windows addObject:win];
}
- (void)setupMenuBar {
    NSString *appTitleNS = [NSString stringWithUTF8String:WToUTF8(APP_TITLE).c_str()];
    NSMenu *mainMenu = [NSMenu new];
    [NSApp setMainMenu:mainMenu];
    NSMenuItem *appMenuItem = [NSMenuItem new];
    [mainMenu addItem:appMenuItem];
    NSMenu *appMenu = [NSMenu new];
    [appMenuItem setSubmenu:appMenu];
    NSString *quitTitle = [NSString stringWithFormat:NSLocalizedString(@"Quit", nil), appTitleNS];
    [appMenu addItemWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
}
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    [self setupMenuBar];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular]; [NSApp activateIgnoringOtherApps:YES];
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    if (args.count > 1) [self createWindowWithPath:[args[1] UTF8String]]; else if (self.windows.count == 0) [self createWindowWithPath:nullptr];
}
- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames { for (NSString *path in filenames) [self createWindowWithPath:[path UTF8String]]; [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess]; }
- (void)newDocument:(id)sender { NSWindow *win = [NSApp keyWindow]; if (win && [win isKindOfClass:[CustomWindow class]]) { EditorView *v = (EditorView *)win.contentView; v->editor->newFile(); } else { [self createWindowWithPath:nullptr]; } }
- (void)openDocument:(id)sender { NSWindow *win = [NSApp keyWindow]; if (win && [win isKindOfClass:[CustomWindow class]]) { EditorView *v = (EditorView *)win.contentView; v->editor->openFile(); } else { [self createWindowWithPath:nullptr]; CustomWindow *newWin = self.windows.lastObject; EditorView *v = (EditorView *)newWin.contentView; if (!v->editor->openFile()) {} } }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { return YES; }
- (BOOL)windowShouldClose:(NSWindow *)sender { EditorView *v = (EditorView *)sender.contentView; if (v && v->editor) { if (!v->editor->checkUnsavedChanges()) return NO; } return YES; }
- (void)windowWillClose:(NSNotification *)notification {
    CustomWindow *win = (CustomWindow *)notification.object;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.windows removeObject:win];
    });
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    NSArray *windowsToClose = [self.windows copy];
    for (CustomWindow *win in windowsToClose) {
        EditorView *v = (EditorView *)win.contentView;
        if (v && v->editor) {
            if (!v->editor->checkUnsavedChanges()) return NSTerminateCancel;
        }
    }
    return NSTerminateNow;
}
@end
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *a = [NSApplication sharedApplication];
        static AppDelegate *d = nil;
        d = [AppDelegate new];
        [a setDelegate:d];
        [a run];
    }
    return 0;
}
