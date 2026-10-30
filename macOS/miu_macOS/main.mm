#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import "EditorCore.h"

static NSString *const kMiuRectangularSelectionType = @"jp.hack.miu.rectangular";
static NSString *const kMiuLineSelectionType = @"jp.hack.miu.lineselection";

@class EditorView;

@interface MiuPreviewMessageHandler : NSObject <WKScriptMessageHandler, WKNavigationDelegate>
@property (weak) EditorView *editorView;
@end

@interface MiuSplitterView : NSView
@property (weak) EditorView *editorView;
@end

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
    WKWebView *previewWebView;
    MiuSplitterView *splitterView;
    MiuPreviewMessageHandler *previewMsgHandler;
}
- (void)layoutSplitViews;
- (void)updatePreviewPane;
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
- (void)cut:(id)sender;
- (void)copy:(id)sender;
- (void)paste:(id)sender;
- (void)selectAll:(id)sender;
- (void)undo:(id)sender;
- (void)redo:(id)sender;
- (void)toggleCommentAction:(id)sender;
- (void)toggleWordWrapAction:(id)sender;
- (void)togglePreviewAction:(id)sender;
- (void)toggleAlwaysOnTopAction:(id)sender;
- (void)showHelpAction:(id)sender;
@end

@implementation MiuPreviewMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    EditorView *ev = self.editorView;
    if (!ev || !ev->editor) return;
    if ([message.name isEqualToString:@"miuPreview"] && [message.body isKindOfClass:[NSString class]]) {
        NSString *body = (NSString *)message.body;
        if ([body hasPrefix:@"toggle_task:"]) {
            int lineIdx = [[body substringFromIndex:12] intValue];
            ev->editor->handlePreviewTaskToggle(lineIdx);
        }
    }
}
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
    if (navigationAction.navigationType == WKNavigationTypeOther) {
        decisionHandler(WKNavigationActionPolicyAllow);
    } else {
        decisionHandler(WKNavigationActionPolicyCancel);
    }
}
@end

@implementation MiuSplitterView
- (void)resetCursorRects {
    [super resetCursorRects];
    [self addCursorRect:[self bounds] cursor:[NSCursor resizeLeftRightCursor]];
}
- (void)drawRect:(NSRect)dirtyRect {
    EditorView *ev = self.editorView;
    BOOL dark = (ev && ev->editor) ? ev->editor->isDarkMode : NO;
    NSColor *col = dark ? [NSColor colorWithCalibratedWhite:0.25 alpha:0.9] : [NSColor colorWithCalibratedWhite:0.82 alpha:0.9];
    [col setFill];
    NSRectFill(dirtyRect);
}
- (void)mouseDragged:(NSEvent *)event {
    EditorView *ev = self.editorView;
    if (!ev || !ev->editor || ![self superview]) return;
    NSView *container = [self superview];
    NSPoint pt = [container convertPoint:[event locationInWindow] fromView:nil];
    CGFloat totalW = container.bounds.size.width;
    if (totalW > 100.0) {
        float ratio = std::clamp((float)(pt.x / totalW), 0.15f, 0.85f);
        ev->editor->previewSplitRatio = ratio;
        [ev layoutSplitViews];
        [ev updateScrollers];
        [ev setNeedsDisplay:YES];
    }
}
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
            } else {
                if ([self.editorView isReplaceMode]) {
                    [self.editorView replaceAction:nil];
                } else {
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
    NSPoint dragStartPoint;
    BOOL isPotentialDragMove;
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
                w = (float)b.size.width;
                h = (float)b.size.height;
            }
        };
        editor->cbUpdateTitleBar = [weakSelf]() {
            __strong EditorView *strongSelf = weakSelf;
            if (!strongSelf || !strongSelf->editor) return;
            NSWindow *window = [strongSelf window];
            if (!window) return;
            std::wstring currentPathW = strongSelf->editor->currentFilePath;
            std::wstring untitledStr = UTF8ToW([NSLocalizedString(@"Untitled", @"新規ファイル名") UTF8String]);
            std::wstring fileName = currentPathW.empty() ? untitledStr : currentPathW.substr(currentPathW.find_last_of(L"/") + 1);
            std::wstring pinPrefix = strongSelf->editor->isAlwaysOnTop ? L"📌 " : L"";
            std::wstring titleText = pinPrefix + (strongSelf->editor->isDirty ? L"*" : L"") + fileName;
            [window setTitle:[NSString stringWithUTF8String:WToUTF8(titleText).c_str()]];
            [window setDocumentEdited:strongSelf->editor->isDirty];
            if (!currentPathW.empty()) {
                NSString *currentPathNS = [NSString stringWithUTF8String:WToUTF8(currentPathW).c_str()];
                [window setRepresentedURL:[NSURL fileURLWithPath:currentPathNS]];
            } else {
                [window setRepresentedURL:[[NSBundle mainBundle] bundleURL]];
            }
        };
        editor->cbSetAlwaysOnTop = [weakSelf](bool onTop) {
            __strong EditorView *strongSelf = weakSelf;
            if (!strongSelf || ![strongSelf window]) return;
            [[strongSelf window] setLevel:onTop ? NSFloatingWindowLevel : NSNormalWindowLevel];
        };
        editor->cbUpdatePreview = [weakSelf]() {
            __strong EditorView *strongSelf = weakSelf;
            if (strongSelf) {
                [strongSelf layoutSplitViews];
                [strongSelf updatePreviewPane];
            }
        };
        editor->cbShowExternalModifyAlert = [weakSelf]() -> bool {
            __strong EditorView *strongSelf = weakSelf;
            if (!strongSelf) return false;
            NSAlert *a = [NSAlert new];
            [a setMessageText:NSLocalizedString(@"File modified externally. Reload?", @"外部変更の確認")];
            [a addButtonWithTitle:NSLocalizedString(@"Save", @"OK/Reload")];
            [a addButtonWithTitle:NSLocalizedString(@"Cancel", @"キャンセル")];
            return ([a runModal] == NSAlertFirstButtonReturn);
        };
        editor->cbSetClipboard = [weakSelf](const std::string& text, bool isRect) {
            NSPasteboard *pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            [pb setString:[NSString stringWithUTF8String:text.c_str()] forType:NSPasteboardTypeString];
            if (isRect) [pb setData:[NSData data] forType:kMiuRectangularSelectionType];
            __strong EditorView *strongSelf = weakSelf;
            if (strongSelf && strongSelf->editor && strongSelf->editor->lastCopyWasLine) {
                [pb setData:[NSData data] forType:kMiuLineSelectionType];
            }
        };
        editor->cbGetClipboard = [weakSelf](bool& isRect) -> std::string {
            NSPasteboard *pb = [NSPasteboard generalPasteboard];
            NSString *s = [pb stringForType:NSPasteboardTypeString];
            isRect = ([pb dataForType:kMiuRectangularSelectionType] != nil);
            __strong EditorView *strongSelf = weakSelf;
            if (strongSelf && strongSelf->editor) {
                strongSelf->editor->lastCopyWasLine = ([pb dataForType:kMiuLineSelectionType] != nil);
            }
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
            if (!strongSelf) return true;
            NSAlert *a = [NSAlert new];
            [a setMessageText:NSLocalizedString(@"Save changes?", @"変更を保存しますか？")];
            [a addButtonWithTitle:NSLocalizedString(@"Save", @"保存")];
            [a addButtonWithTitle:NSLocalizedString(@"Cancel", @"キャンセル")];
            [a addButtonWithTitle:NSLocalizedString(@"Discard", @"破棄")];
            NSModalResponse r = [a runModal];
            if (r == NSAlertFirstButtonReturn) return strongSelf->editor->currentFilePath.empty() ? strongSelf->editor->saveFileAs() : strongSelf->editor->saveFile(strongSelf->editor->currentFilePath);
            if (r == NSAlertThirdButtonReturn) { strongSelf->editor->isDirty = false; strongSelf->editor->updateTitleBar(); return true; }
            return false;
        };
        editor->cbSaveFileAs = [weakSelf]() -> bool {
            __strong EditorView *strongSelf = weakSelf;
            if (!strongSelf) return false;
            NSSavePanel *p = [NSSavePanel savePanel];
            p.canCreateDirectories = YES;
            p.allowsOtherFileTypes = YES;
            p.extensionHidden = NO;
            if ([p runModal] == NSModalResponseOK) return strongSelf->editor->saveFile(UTF8ToW([p.URL.path UTF8String]));
            return false;
        };
        editor->cbOpenFile = [weakSelf]() -> bool {
            __strong EditorView *strongSelf = weakSelf;
            if (!strongSelf) return false;
            NSOpenPanel *p = [NSOpenPanel openPanel];
            if ([p runModal] == NSModalResponseOK) return strongSelf->editor->openFileFromPath([[[p URLs] objectAtIndex:0].path UTF8String]);
            return false;
        };
        CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
        vScroller = [[NSScroller alloc] initWithFrame:NSMakeRect(fr.size.width - sw, 0, sw, fr.size.height - sw)];
        [vScroller setAutoresizingMask:NSViewMinXMargin | NSViewHeightSizable];
        [vScroller setTarget:self];
        [vScroller setAction:@selector(scrollAction:)];
        [self addSubview:vScroller];
        hScroller = [[NSScroller alloc] initWithFrame:NSMakeRect(0, fr.size.height - sw, fr.size.width - sw, sw)];
        [hScroller setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
        [hScroller setTarget:self];
        [hScroller setAction:@selector(scrollAction:)];
        [self addSubview:hScroller];
        [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(systemColorsDidChange:)
                                                     name:NSSystemColorsDidChangeNotification
                                                   object:nil];
    }
    return self;
}

- (void)ensurePreviewViewsCreated {
    if (previewWebView || ![self superview]) return;
    NSView *container = [self superview];
    previewMsgHandler = [[MiuPreviewMessageHandler alloc] init];
    previewMsgHandler.editorView = self;

    WKWebViewConfiguration *cfg = [[WKWebViewConfiguration alloc] init];
    WKUserContentController *ucc = [[WKUserContentController alloc] init];
    [ucc addScriptMessageHandler:previewMsgHandler name:@"miuPreview"];
    cfg.userContentController = ucc;

    previewWebView = [[WKWebView alloc] initWithFrame:NSZeroRect configuration:cfg];
    previewWebView.navigationDelegate = previewMsgHandler;
    if ([previewWebView respondsToSelector:@selector(setValue:forKey:)]) {
        [previewWebView setValue:@(NO) forKey:@"drawsBackground"];
    }
    [previewWebView setHidden:YES];
    [container addSubview:previewWebView];

    splitterView = [[MiuSplitterView alloc] initWithFrame:NSZeroRect];
    splitterView.editorView = self;
    [splitterView setHidden:YES];
    [container addSubview:splitterView];
}

- (void)layoutSplitViews {
    NSView *container = [self superview];
    if (!container || !editor) return;
    [self ensurePreviewViewsCreated];
    NSRect cb = container.bounds;
    CGFloat topInset = 28.0;
    if (@available(macOS 11.0, *)) {
        topInset = container.safeAreaInsets.top;
        if (topInset < 28.0) topInset = 28.0;
    }
    CGFloat availH = std::max((CGFloat)1.0, cb.size.height - topInset);
    if (!editor->showPreview) {
        [self setFrame:NSMakeRect(0, 0, cb.size.width, availH)];
        [splitterView setHidden:YES];
        [previewWebView setHidden:YES];
    } else {
        CGFloat splitW = 5.0;
        CGFloat leftW = std::floor(cb.size.width * editor->previewSplitRatio);
        CGFloat rightW = std::max((CGFloat)10.0, cb.size.width - leftW - splitW);
        [self setFrame:NSMakeRect(0, 0, leftW, availH)];
        [splitterView setFrame:NSMakeRect(leftW, 0, splitW, availH)];
        [previewWebView setFrame:NSMakeRect(leftW + splitW, 0, rightW, availH)];
        [splitterView setHidden:NO];
        [previewWebView setHidden:NO];
        [splitterView setNeedsDisplay:YES];
    }
    [self updateScrollers];
    [self setNeedsDisplay:YES];
}

- (void)updatePreviewPane {
    if (!editor || !editor->showPreview) return;
    [self ensurePreviewViewsCreated];
    if (!previewWebView) return;
    MiuPreview::PreviewRenderResult res = editor->getPreviewRenderResult();
    NSString *fullHtmlNS = [NSString stringWithUTF8String:res.fullHtml.c_str()];
    if (!fullHtmlNS) return;

    NSURL *baseURL = nil;
    if (!editor->currentFilePath.empty()) {
        NSString *pathNS = [NSString stringWithUTF8String:WToUTF8(editor->currentFilePath).c_str()];
        if (pathNS) baseURL = [[NSURL fileURLWithPath:pathNS] URLByDeletingLastPathComponent];
    }

    if (!editor->previewInitialLoaded || !res.isMdOrMermaidShell) {
        editor->previewInitialLoaded = res.isMdOrMermaidShell;
        [previewWebView loadHTMLString:fullHtmlNS baseURL:baseURL];
    } else {
        std::string js = MiuPreview::buildDomUpdateJs(res.bodyContent, editor->isDarkMode);
        NSString *jsNS = [NSString stringWithUTF8String:js.c_str()];
        if (jsNS) {
            WKWebView *wv = previewWebView;
            [wv evaluateJavaScript:jsNS completionHandler:^(id result, NSError *error) {
                if (error) {
                    [wv loadHTMLString:fullHtmlNS baseURL:baseURL];
                }
            }];
        }
    }
}

- (void)applySystemHighlightColor {
    if (!editor) return;
    if (@available(macOS 10.14, *)) {
        [self.effectiveAppearance performAsCurrentDrawingAppearance:^{
            NSColor *highlightColor = [NSColor selectedTextBackgroundColor];
            NSColor *safeColor = [highlightColor colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
            if (!safeColor) safeColor = highlightColor;
            if (editor->colSel) CGColorRelease(editor->colSel);
            editor->colSel = [safeColor CGColor];
            CGColorRetain(editor->colSel);
        }];
    }
}

- (void)resetCursorRects {
    [super resetCursorRects];
    [self addCursorRect:[self bounds] cursor:[NSCursor IBeamCursor]];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (previewWebView) {
        [previewWebView.configuration.userContentController removeScriptMessageHandlerForName:@"miuPreview"];
    }
    if (findTextField) {
        [findTextField setDelegate:nil];
        [findTextField setTarget:nil];
    }
    if (replaceTextField) {
        [replaceTextField setDelegate:nil];
        [replaceTextField setTarget:nil];
    }
}

- (void)systemColorsDidChange:(NSNotification *)notification {
    [self applySystemHighlightColor];
    [self setNeedsDisplay:YES];
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
    NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
    NSString *appVersion = infoDict[@"CFBundleShortVersionString"];
    if (!appVersion) appVersion = @"1.0.44";
    NSString *versionString = [NSString stringWithFormat:@"miu v%@", appVersion];
    editor->appVersionStr = UTF8ToW([versionString UTF8String]);
    NSString *localizedHelp = NSLocalizedString(@"HelpText", @"ヘルプのショートカット一覧");
    editor->helpTextStr = UTF8ToW([localizedHelp UTF8String]);
    editor->initGraphics();
    BOOL isDark = [[self.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] isEqualToString:NSAppearanceNameDarkAqua];
    editor->isDarkMode = isDark;
    editor->updateThemeColors();
    [self applySystemHighlightColor];
    [self layoutSplitViews];
    [self updateScrollers];
    [[self window] makeFirstResponder:self];
}

- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    if (editor) {
        BOOL isDark = [[self.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] isEqualToString:NSAppearanceNameDarkAqua];
        if (editor->isDarkMode != isDark) {
            editor->isDarkMode = isDark;
            editor->updateThemeColors();
            [self applySystemHighlightColor];
            [self setNeedsDisplay:YES];
        }
    }
}

- (void)drawRect:(NSRect)r {
    if (editor) {
        BOOL isDark = [[self.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] isEqualToString:NSAppearanceNameDarkAqua];
        if (editor->isDarkMode != (bool)isDark) {
            editor->isDarkMode = isDark;
            editor->updateThemeColors();
            [self applySystemHighlightColor];
        }
        editor->render([[NSGraphicsContext currentContext] CGContext], (float)self.bounds.size.width, (float)self.bounds.size.height);
    }
}

- (NSMenu *)menuForEvent:(NSEvent *)event {
    if (editor && event) {
        NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
        if (p.x >= editor->gutterWidth && p.x <= self.bounds.size.width - editor->visibleVScrollWidth &&
            p.y >= 0 && p.y <= self.bounds.size.height - editor->visibleHScrollHeight) {
            bool hasSel = false;
            for (const auto& c : editor->cursors) if (c.hasSelection()) { hasSel = true; break; }
            if (!hasSel && editor->cursors.size() <= 1 && !editor->isRectSelecting) {
                size_t pos = editor->getDocPosFromPoint((float)p.x, (float)p.y);
                int li = editor->getLineIdx(pos);
                float physX = editor->getXInLine(li, pos);
                editor->cursors.clear();
                editor->cursors.push_back({ pos, pos, physX, physX, false, false });
                [self setNeedsDisplay:YES];
            }
        }
    }
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Context"];
    [menu addItemWithTitle:NSLocalizedString(@"Undo", @"元に戻す") action:@selector(undo:) keyEquivalent:@"z"];
    NSMenuItem *redoItem = [menu addItemWithTitle:NSLocalizedString(@"Redo", @"やり直し") action:@selector(redo:) keyEquivalent:@"Z"];
    [redoItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:NSLocalizedString(@"Cut", @"切り取り") action:@selector(cut:) keyEquivalent:@"x"];
    [menu addItemWithTitle:NSLocalizedString(@"Copy", @"コピー") action:@selector(copy:) keyEquivalent:@"c"];
    [menu addItemWithTitle:NSLocalizedString(@"Paste", @"貼り付け") action:@selector(paste:) keyEquivalent:@"v"];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:NSLocalizedString(@"Select All", @"すべて選択") action:@selector(selectAll:) keyEquivalent:@"a"];
    return menu;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if (!editor) return NO;
    SEL action = [menuItem action];
    if (action == @selector(undo:)) return !editor->undo.undoStack.empty();
    if (action == @selector(redo:)) return !editor->undo.redoStack.empty();
    if (action == @selector(paste:)) {
        NSString *s = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        return (s != nil && [s length] > 0);
    }
    if (action == @selector(selectAll:) || action == @selector(cut:) || action == @selector(copy:)) {
        return editor->pt.length() > 0;
    }
    return YES;
}

- (void)cut:(id)sender { if (editor) { editor->cutToClipboard(); [self setNeedsDisplay:YES]; } }
- (void)copy:(id)sender { if (editor) { editor->copyToClipboard(); } }
- (void)paste:(id)sender { if (editor) { editor->pasteFromClipboard(); [self setNeedsDisplay:YES]; } }
- (void)selectAll:(id)sender { if (editor) { editor->selectAll(); [self setNeedsDisplay:YES]; } }
- (void)undo:(id)sender { if (editor) { editor->performUndo(); [self setNeedsDisplay:YES]; } }
- (void)redo:(id)sender { if (editor) { editor->performRedo(); [self setNeedsDisplay:YES]; } }
- (void)toggleCommentAction:(id)sender { if (editor) { editor->toggleComment(); [self setNeedsDisplay:YES]; } }
- (void)toggleWordWrapAction:(id)sender { if (editor) { editor->toggleWordWrap(); [self setNeedsDisplay:YES]; } }
- (void)togglePreviewAction:(id)sender { if (editor) { editor->togglePreview(); [self setNeedsDisplay:YES]; } }
- (void)toggleAlwaysOnTopAction:(id)sender { if (editor) { editor->toggleAlwaysOnTop(); [self setNeedsDisplay:YES]; } }
- (void)showHelpAction:(id)sender { if (editor) { editor->showHelpPopup = !editor->showHelpPopup; [self setNeedsDisplay:YES]; } }

- (void)updateScrollers {
    NSRect b = [self bounds];
    CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    ScrollBarLayout L = editor->getScrollBarLayout((float)b.size.width, (float)b.size.height);
    bool needsV = L.needV;
    bool needsH = L.needH;
    float visibleWidth = L.textAreaW;
    int totalLines = (editor->wordWrapEnabled && L.totalVisualLines > L.totalLines) ? L.totalVisualLines : L.totalLines;
    float maxLineWidth = editor->maxLineWidth;

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
    editor->visibleVScrollWidth = needsV ? (float)sw : 0.0f;
    editor->visibleHScrollHeight = needsH ? (float)sw : 0.0f;

    int visibleLines = L.visibleLines;
    [vScroller setKnobProportion:std::min(1.0, (double)visibleLines / std::max(1, totalLines))];
    int maxV = std::max(0, totalLines - visibleLines);
    int currentV = (editor->wordWrapEnabled && L.totalLines == 1 && L.totalVisualLines > 1) ? editor->vScrollSubLine : editor->vScrollPos;
    if (needsV && maxV > 0) {
        [vScroller setDoubleValue:std::clamp((double)currentV / maxV, 0.0, 1.0)];
        [vScroller setEnabled:YES];
    } else {
        [vScroller setDoubleValue:0.0];
        [vScroller setEnabled:NO];
        if (!needsV) {
            editor->vScrollPos = 0;
            editor->vScrollSubLine = 0;
        }
    }

    if (visibleWidth < 1.0f) visibleWidth = 1.0f;
    [hScroller setKnobProportion:std::min(1.0, (double)visibleWidth / std::max(1.0f, maxLineWidth))];
    float maxH = maxLineWidth - visibleWidth;
    if (needsH && maxH > 0) {
        [hScroller setDoubleValue:std::clamp((double)editor->hScrollPos / maxH, 0.0, 1.0)];
        [hScroller setEnabled:YES];
    } else {
        [hScroller setDoubleValue:0.0];
        [hScroller setEnabled:NO];
        editor->hScrollPos = 0;
    }
    [self setNeedsDisplay:YES];
}

- (void)scrollAction:(NSScroller*)s {
    NSRect b = [self bounds];
    ScrollBarLayout L = editor->getScrollBarLayout((float)b.size.width, (float)b.size.height);
    if (s == vScroller) {
        int totalLines = (editor->wordWrapEnabled && L.totalVisualLines > L.totalLines) ? L.totalVisualLines : L.totalLines;
        int maxV = std::max(0, totalLines - L.visibleLines);
        if (editor->wordWrapEnabled && L.totalLines == 1 && L.totalVisualLines > 1) {
            editor->vScrollSubLine = std::clamp((int)std::round([s doubleValue] * maxV), 0, maxV);
        } else {
            editor->vScrollPos = std::clamp((int)std::round([s doubleValue] * maxV), 0, maxV);
        }
    } else {
        float maxH = std::max(0.0f, editor->maxLineWidth - L.textAreaW);
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
    bool isOption = ([e modifierFlags] & NSEventModifierFlagOption) != 0;
    bool isShift = ([e modifierFlags] & NSEventModifierFlagShift) != 0;
    bool isCommand = ([e modifierFlags] & NSEventModifierFlagCommand) != 0;

    isPotentialDragMove = NO;
    editor->isDraggingSelection = false;
    editor->isGutterDragging = false;
    editor->activeEmmetSession.active = false;
    editor->rollbackPadding();
    editor->isRectSelecting = false;

    size_t pos = editor->getDocPosFromPoint((float)p.x, (float)p.y);
    int li = editor->getLineIdx(pos);

    // Gutter click -> line selection
    if (p.x < editor->gutterWidth) {
        editor->isGutterDragging = true;
        editor->gutterAnchorLine = li;
        editor->selectLinesRange(li, li, isCommand);
        return;
    }

    if (clicks == 2) {
        editor->selectWordAt(pos, isCommand);
        return;
    } else if (clicks >= 3) {
        editor->selectLineAt(pos, isCommand);
        return;
    }

    if (isShift && !isOption && !editor->cursors.empty()) {
        Cursor& c = editor->cursors.back();
        c.head = pos;
        c.desiredX = editor->getXInLine(li, pos);
        c.isVirtual = false;
        [self setNeedsDisplay:YES];
        return;
    }

    // Check if clicking inside an existing selection to initiate drag-and-drop move
    if (!isOption && !isShift && !isCommand && editor->isPointInSelection(pos)) {
        isPotentialDragMove = YES;
        dragStartPoint = p;
        [self setNeedsDisplay:YES];
        return;
    }

    if (!isOption && !isShift && !isCommand) {
        editor->cursors.clear();
    }
    boxSelectStartPoint = p;
    cursorsSnapshotCount = editor->cursors.size();
    float docX = (float)p.x - editor->gutterWidth + (float)editor->hScrollPos;
    float snappedX = docX;
    if (editor->charWidth > 0) {
        int cols = (int)std::round(docX / editor->charWidth);
        if (cols < 0) cols = 0;
        snappedX = (float)cols * editor->charWidth;
    }
    float physX = editor->getXInLine(li, pos);
    Cursor newCursor;
    newCursor.head = pos;
    newCursor.anchor = pos;
    newCursor.isVirtual = false;
    if (isOption && (snappedX > physX + (editor->charWidth * 0.5f))) {
        newCursor.desiredX = snappedX;
        newCursor.isVirtual = true;
    } else {
        newCursor.desiredX = physX;
        newCursor.isVirtual = false;
    }
    newCursor.originalAnchorX = newCursor.desiredX;
    editor->cursors.push_back(newCursor);
    if (isCommand) editor->mergeCursors();
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)e {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    bool isOption = ([e modifierFlags] & NSEventModifierFlagOption) != 0;

    // Auto-scroll when dragging near/outside view edges
    NSRect b = [self bounds];
    if (p.y < 0) {
        editor->scrollVisualLines(-1);
    } else if (p.y > b.size.height - editor->visibleHScrollHeight) {
        editor->scrollVisualLines(1);
    }

    size_t pos = editor->getDocPosFromPoint((float)p.x, (float)p.y);

    if (editor->isGutterDragging) {
        int headLine = editor->getLineIdx(pos);
        editor->selectLinesRange(editor->gutterAnchorLine, headLine, false);
        return;
    }

    if (isPotentialDragMove) {
        double dx = p.x - dragStartPoint.x;
        double dy = p.y - dragStartPoint.y;
        if (std::hypot(dx, dy) > 3.0) {
            editor->isDraggingSelection = true;
        }
        if (editor->isDraggingSelection) {
            editor->dragDropTargetPos = pos;
            editor->ensureCaretVisible();
            [self setNeedsDisplay:YES];
        }
        return;
    }

    auto snapToGrid = [&](float rawX) -> float {
        if (editor->charWidth <= 0) return rawX;
        int cols = (int)std::round(rawX / editor->charWidth);
        if (cols < 0) cols = 0;
        return (float)cols * editor->charWidth;
    };
    if (isOption) {
        editor->cursors.clear();
        float startY = std::min((float)boxSelectStartPoint.y, (float)p.y);
        float endY = std::max((float)boxSelectStartPoint.y, (float)p.y);
        int startLine = (int)std::floor((startY + (float)editor->vScrollPos * editor->lineHeight) / editor->lineHeight);
        int endLine = (int)std::floor((endY + (float)editor->vScrollPos * editor->lineHeight) / editor->lineHeight);
        startLine = std::clamp(startLine, 0, (int)editor->lineStarts.size() - 1);
        endLine = std::clamp(endLine, 0, (int)editor->lineStarts.size() - 1);
        float rawStartX = (float)boxSelectStartPoint.x - editor->gutterWidth + (float)editor->hScrollPos;
        float rawEndX = (float)p.x - editor->gutterWidth + (float)editor->hScrollPos;
        float gridStartX = snapToGrid(rawStartX), gridEndX = snapToGrid(rawEndX);
        editor->isRectSelecting = true;
        editor->rectAnchorLine = startLine;
        editor->rectHeadLine = endLine;
        editor->rectAnchorX = gridStartX;
        editor->rectHeadX = gridEndX;
        editor->updateRectSelection();
    } else {
        if (!editor->cursors.empty()) {
            Cursor& c = editor->cursors.back();
            c.head = pos;
            int li = editor->getLineIdx(pos);
            c.desiredX = editor->getXInLine(li, pos);
            c.isVirtual = false;
        }
    }
    editor->ensureCaretVisible();
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)e {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    if (editor->isDraggingSelection) {
        size_t targetPos = editor->getDocPosFromPoint((float)p.x, (float)p.y);
        editor->isDraggingSelection = false;
        isPotentialDragMove = NO;
        editor->performDragMove(targetPos);
        return;
    }
    if (isPotentialDragMove) {
        isPotentialDragMove = NO;
        size_t pos = editor->getDocPosFromPoint((float)p.x, (float)p.y);
        int li = editor->getLineIdx(pos);
        float physX = editor->getXInLine(li, pos);
        editor->cursors.clear();
        editor->cursors.push_back({ pos, pos, physX, physX, false, false });
    }
    editor->isGutterDragging = false;
    editor->mergeCursors();
    [self setNeedsDisplay:YES];
}

- (void)showFindPanel:(BOOL)replaceMode {
    editor->isReplaceMode = replaceMode;
    auto [candidate, _] = editor->getHighlightTarget();
    bool hasSelection = !editor->cursors.empty() && editor->cursors.back().hasSelection();
    if (hasSelection && !candidate.empty()) editor->searchQuery = candidate;
    else if (editor->searchQuery.empty() && !candidate.empty()) editor->searchQuery = candidate;
    if (!findPanel) {
        findPanel = [[FindPanel alloc] initWithContentRect:NSMakeRect(0, 0, 360, 185)
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
    [findLabel setFrame:NSMakeRect(10, 150, 65, 20)];
    [findTextField setFrame:NSMakeRect(80, 150, 260, 22)];
    [replaceLabel setFrame:NSMakeRect(10, 120, 65, 20)];
    [replaceTextField setFrame:NSMakeRect(80, 120, 260, 22)];
    [matchCaseBtn setFrame:NSMakeRect(80, 95, 260, 20)];
    [wholeWordBtn setFrame:NSMakeRect(80, 75, 260, 20)];
    [regexBtn setFrame:NSMakeRect(80, 55, 260, 20)];
    [replaceAllBtn setFrame:NSMakeRect(70, 10, 105, 32)];
    [replaceBtn setFrame:NSMakeRect(175, 10, 85, 32)];
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
            [self findNextAction:nil];
            return YES;
        } else if (control == replaceTextField) {
            [self replaceAction:nil];
            return YES;
        }
    } else if (commandSelector == @selector(cancelOperation:)) {
        [findPanel orderOut:nil];
        [[self window] makeFirstResponder:self];
        return YES;
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
        if (editor->searchQuery.empty()) {
            [self showFindPanel:NO];
        } else {
            editor->findNext(forward);
        }
    }
}

- (BOOL)isReplaceMode {
    return editor ? editor->isReplaceMode : NO;
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
    editor->cursors.push_back({ newPos, newPos, 0.0f, 0.0f, false, false });
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
    unsigned short code = [e keyCode];
    NSString *chars = [e charactersIgnoringModifiers];
    bool cmd = ([e modifierFlags] & NSEventModifierFlagCommand) != 0;
    bool shift = ([e modifierFlags] & NSEventModifierFlagShift) != 0;
    bool ctrl = ([e modifierFlags] & NSEventModifierFlagControl) != 0;
    bool opt = ([e modifierFlags] & NSEventModifierFlagOption) != 0;

    if (editor->showHelpPopup) {
        editor->showHelpPopup = false;
        [self setNeedsDisplay:YES];
        if (code == 122) return;
    }
    if (code == 122) { // F1
        editor->showHelpPopup = true;
        [self setNeedsDisplay:YES];
        return;
    }
    if (code == 53) { // Escape
        editor->rollbackPadding();
        editor->isRectSelecting = false;
        editor->activeEmmetSession.active = false;
        if (editor->cursors.size() > 1 || (editor->cursors.size() == 1 && editor->cursors[0].hasSelection())) {
            Cursor lastC = editor->cursors.back();
            lastC.anchor = lastC.head;
            editor->cursors.clear();
            editor->cursors.push_back(lastC);
            [self setNeedsDisplay:YES];
            return;
        }
    }

    if (code == 114 && !cmd && !ctrl && !opt && !shift) { // Insert (Overwrite Mode)
        editor->isOverwriteMode = !editor->isOverwriteMode;
        [self setNeedsDisplay:YES];
        return;
    }
    if (code == 103 && !cmd && !ctrl && !opt && !shift) { // F11 FullScreen
        [[self window] toggleFullScreen:nil];
        return;
    }

    // Option+Z (Word Wrap), Option+P (Live Preview), Option+T (Always on Top)
    if (opt && !cmd && !ctrl && !shift) {
        if (code == 6) { editor->toggleWordWrap(); [self setNeedsDisplay:YES]; return; } // Z
        if (code == 35) { editor->togglePreview(); [self setNeedsDisplay:YES]; return; } // P
        if (code == 17) { editor->toggleAlwaysOnTop(); [self setNeedsDisplay:YES]; return; } // T
    }

    // Tab / Shift+Tab (Emmet expansion & tabstops, or indent/unindent)
    if (code == 48 && !cmd && !ctrl && !opt) {
        if (!shift && editor->tryExpandEmmet()) {
            [self setNeedsDisplay:YES];
            return;
        }
        if (editor->jumpEmmetTabstop(!shift)) {
            [self setNeedsDisplay:YES];
            return;
        }
        if (shift) {
            editor->unindentLines();
        } else {
            bool isRectMode = editor->cursors.size() > 1;
            if (isRectMode) editor->insertAtCursors("\t");
            else editor->indentLines(false);
        }
        [self setNeedsDisplay:YES];
        return;
    }

    if (cmd && (code == 126 || code == 115) && !opt) {
        editor->jumpToFileEdge(true, shift);
        return;
    }
    if (cmd && (code == 125 || code == 119) && !opt) {
        editor->jumpToFileEdge(false, shift);
        return;
    }
    if (code == 115 || code == 119) { // Home / End
        if (editor->wordWrapEnabled) {
            if (code == 115) editor->moveVisualHome(shift);
            else editor->moveVisualEnd(shift);
            return;
        }
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
        editor->ensureCaretVisible();
        [self setNeedsDisplay:YES];
        return;
    }
    if (code == 116 || code == 121) { // PageUp / PageDown
        float viewHeight = [self bounds].size.height - editor->visibleHScrollHeight;
        int pageLines = std::max(1, (int)(viewHeight / editor->lineHeight) - 1);
        if (editor->wordWrapEnabled) {
            for (int i = 0; i < pageLines; ++i) {
                editor->moveVisualUpDown(code == 116, shift);
            }
            [self setNeedsDisplay:YES];
            return;
        }
        int totalLines = (int)editor->lineStarts.size();
        for (auto& c : editor->cursors) {
            int currentLine = editor->getLineIdx(c.head);
            int newLine = currentLine;
            if (code == 116) { newLine = std::max(0, currentLine - pageLines); }
            else { newLine = std::min(totalLines - 1, currentLine + pageLines); }
            if (code == 121 && newLine == totalLines - 1 && currentLine == totalLines - 1) c.head = editor->pt.length();
            else c.head = editor->getPosFromLineAndX(newLine, c.desiredX);
            if (!shift) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
        }
        if (code == 116) editor->vScrollPos = std::max(0, editor->vScrollPos - pageLines);
        else editor->vScrollPos = std::min(totalLines - 1, editor->vScrollPos + pageLines);
        editor->ensureCaretVisible();
        [self setNeedsDisplay:YES];
        return;
    }
    if (code == 99) { // F3
        [self findNextWithDirection:!shift];
        return;
    }
    if (cmd && !opt) {
        NSString *lowerChar = [chars lowercaseString];
        if (ctrl && [lowerChar isEqualToString:@"f"]) { [[self window] toggleFullScreen:nil]; return; }
        if ([lowerChar isEqualToString:@"q"]) { [NSApp terminate:nil]; return; }
        if ([lowerChar isEqualToString:@"w"]) { [[self window] performClose:nil]; return; }
        if ([lowerChar isEqualToString:@"/"] || code == 44) { editor->toggleComment(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"b"] || code == 11) { editor->jumpToMatchingBracket(shift); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"u"]) { editor->convertSelectedText(!shift); return; }
        if (shift && ([lowerChar isEqualToString:@"k"] || [lowerChar isEqualToString:@"l"])) { editor->deleteLine(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"d"]) { editor->selectNextOccurrence(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"a"]) { editor->selectAll(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"s"]) { shift ? editor->saveFileAs() : (editor->currentFilePath.empty() ? editor->saveFileAs() : editor->saveFile(editor->currentFilePath)); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"c"]) { editor->copyToClipboard(); return; }
        if ([lowerChar isEqualToString:@"x"]) { editor->cutToClipboard(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"v"]) { editor->pasteFromClipboard(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"z"]) { shift ? editor->performRedo() : editor->performUndo(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"y"]) { editor->performRedo(); [self setNeedsDisplay:YES]; return; }
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

    // Arrow keys (123: Left, 124: Right, 125: Down, 126: Up)
    if (code >= 123 && code <= 126) {
        // Multi-cursor addition: Ctrl+Option+Up/Down or Cmd+Option+Up/Down
        if (opt && (ctrl || cmd) && !shift && (code == 126 || code == 125)) {
            editor->addCursor(code == 126);
            return;
        }
        // Keyboard rectangular selection: Option+Shift+Left/Right or Ctrl+Option+Shift+Arrows
        if (opt && shift && (code == 123 || code == 124 || ctrl)) {
            int dLine = (code == 126) ? -1 : ((code == 125) ? 1 : 0);
            int dCol = (code == 123) ? -1 : ((code == 124) ? 1 : 0);
            editor->moveRectSelection(dLine, dCol);
            return;
        }
        // Option+Up/Down: move lines; Option+Shift+Up/Down: duplicate lines
        if (code == 126 && opt && !cmd && !ctrl) {
            if (shift) editor->duplicateLines(true);
            else editor->moveLines(true);
            [self setNeedsDisplay:YES];
            return;
        }
        if (code == 125 && opt && !cmd && !ctrl) {
            if (shift) editor->duplicateLines(false);
            else editor->moveLines(false);
            [self setNeedsDisplay:YES];
            return;
        }
        // Visual Up/Down and Visual Home/End when soft wrap is active
        if ((code == 126 || code == 125) && editor->wordWrapEnabled && !opt && !cmd && !ctrl) {
            editor->moveVisualUpDown(code == 126, shift);
            return;
        }
        if ((code == 123 || code == 124) && editor->wordWrapEnabled && cmd && !opt && !ctrl) {
            if (code == 123) editor->moveVisualHome(shift);
            else editor->moveVisualEnd(shift);
            return;
        }

        editor->rollbackPadding();
        editor->isRectSelecting = false;
        float wrapW = editor->getEffectiveTextWidth((float)[self bounds].size.width);
        for (auto& c : editor->cursors) {
            if (code == 123) {
                if (c.hasSelection() && !shift && !opt && !cmd) {
                    c.head = c.start();
                    c.anchor = c.head;
                } else {
                    size_t p = c.head;
                    int li = editor->getLineIdx(p);
                    size_t lineStart = editor->lineStarts[li];
                    if (cmd) {
                        c.head = lineStart;
                    } else if (opt) {
                        if (p == lineStart && p > 0) p--;
                        else {
                            while (p > lineStart && !editor->isWordChar(editor->pt.charAt(p - 1))) p--;
                            while (p > lineStart && editor->isWordChar(editor->pt.charAt(p - 1))) p--;
                        }
                        c.head = p;
                    } else {
                        c.head = editor->moveCaretVisual(c.head, false);
                    }
                }
                c.isVirtual = false;
                c.affinity = false;
            } else if (code == 124) {
                if (c.hasSelection() && !shift && !opt && !cmd) {
                    c.head = c.end();
                    c.anchor = c.head;
                    c.affinity = false;
                } else {
                    size_t p = c.head;
                    size_t len = editor->pt.length();
                    int li = editor->getLineIdx(p);
                    size_t lineStart = editor->lineStarts[li];
                    size_t lineEnd = (li + 1 < (int)editor->lineStarts.size()) ? editor->lineStarts[li + 1] : len;
                    size_t physEnd = lineEnd;
                    if (physEnd > lineStart && editor->pt.charAt(physEnd - 1) == '\n') physEnd--;
                    if (physEnd > lineStart && editor->pt.charAt(physEnd - 1) == '\r') physEnd--;
                    if (cmd) {
                        c.head = physEnd;
                        c.affinity = false;
                    } else if (opt) {
                        if (p == physEnd && p < len) p = lineEnd;
                        else {
                            while (p < physEnd && editor->isWordChar(editor->pt.charAt(p))) p++;
                            while (p < physEnd && !editor->isWordChar(editor->pt.charAt(p))) p++;
                        }
                        c.head = p;
                        c.affinity = false;
                    } else {
                        size_t nextPos = editor->moveCaretVisual(c.head, true);
                        int nextLi = editor->getLineIdx(nextPos);
                        c.affinity = (nextPos > p) && editor->isVisualLineWrapBoundary(nextLi, nextPos, wrapW);
                        c.head = nextPos;
                    }
                }
                c.isVirtual = false;
            } else if (code == 126) {
                int l = editor->getLineIdx(c.head);
                if (l > 0) {
                    size_t nextPos = editor->getPosFromLineAndX(l - 1, c.desiredX);
                    size_t prevLineStart = editor->lineStarts[l - 1];
                    size_t prevLineEnd = editor->lineStarts[l];
                    if (prevLineEnd > prevLineStart && editor->pt.charAt(prevLineEnd - 1) == '\n') prevLineEnd--;
                    if (prevLineEnd > prevLineStart && editor->pt.charAt(prevLineEnd - 1) == '\r') prevLineEnd--;
                    c.head = std::min(nextPos, prevLineEnd);
                }
                c.isVirtual = false;
            } else if (code == 125) {
                int l = editor->getLineIdx(c.head);
                if (l + 1 < (int)editor->lineStarts.size()) {
                    size_t nextPos = editor->getPosFromLineAndX(l + 1, c.desiredX);
                    size_t nextLineStart = editor->lineStarts[l + 1];
                    size_t nextLineEnd = (l + 2 < (int)editor->lineStarts.size()) ? editor->lineStarts[l + 2] : editor->pt.length();
                    if (nextLineEnd > nextLineStart && editor->pt.charAt(nextLineEnd - 1) == '\n') nextLineEnd--;
                    if (nextLineEnd > nextLineStart && editor->pt.charAt(nextLineEnd - 1) == '\r') nextLineEnd--;
                    c.head = std::min(nextPos, nextLineEnd);
                }
                c.isVirtual = false;
            }
            if (code == 123 || code == 124) c.desiredX = editor->getXFromPos(c.head);
            if (!shift) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
        }
        editor->mergeCursors();
        editor->ensureCaretVisible();
        [self setNeedsDisplay:YES];
        return;
    }
    if (![self.inputContext handleEvent:e]) [super keyDown:e];
}

- (void)scrollWheel:(NSEvent *)e {
    if ([e modifierFlags] & NSEventModifierFlagCommand) {
        float dy = [e scrollingDeltaY];
        if (dy == 0) dy = [e deltaY];
        if (dy != 0) {
            float factor = (dy > 0) ? 1.1f : 0.9f;
            editor->updateFont(editor->currentFontSize * factor);
            editor->zoomPopupText = std::to_string((int)std::round(editor->currentFontSize)) + "px";
            editor->zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
            [self setNeedsDisplay:YES];
        }
        return;
    }
    int dy = (int)std::round([e deltaY]);
    if (editor->wordWrapEnabled) {
        if (dy != 0) editor->scrollVisualLines(-dy);
        return;
    }
    NSRect b = [self bounds];
    ScrollBarLayout L = editor->getScrollBarLayout((float)b.size.width, (float)b.size.height);
    int maxV = std::max(0, L.totalLines - L.visibleLines);
    editor->vScrollPos = std::clamp(editor->vScrollPos - dy, 0, maxV);
    int maxH = std::max(0, (int)std::ceil(editor->maxLineWidth - L.textAreaW));
    editor->hScrollPos = std::clamp(editor->hScrollPos - (int)[e deltaX], 0, maxH);
    [self updateScrollers];
    [self setNeedsDisplay:YES];
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

- (void)setMarkedText:(id)s selectedRange:(NSRange)sr replacementRange:(NSRange)rr {
    editor->imeComp = [([s isKindOfClass:[NSAttributedString class]] ? [s string] : s) UTF8String];
    [self setNeedsDisplay:YES];
}
- (void)unmarkText { editor->imeComp = ""; [self setNeedsDisplay:YES]; }
- (BOOL)hasMarkedText { return !editor->imeComp.empty(); }
- (NSRange)markedRange { return [self hasMarkedText] ? NSMakeRange(0, editor->imeComp.length()) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return NSMakeRange(NSNotFound, 0); }
- (void)doCommandBySelector:(SEL)s {
    if (s == @selector(deleteBackward:)) editor->backspaceAtCursors();
    else if (s == @selector(deleteForward:)) editor->deleteForwardAtCursors();
    else if (s == @selector(insertNewline:)) editor->insertNewlineWithAutoIndent();
    [self setNeedsDisplay:YES];
}
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)r actualRange:(NSRangePointer)ar { return nil; }
- (NSArray*)validAttributesForMarkedText { return @[]; }
- (NSRect)firstRectForCharacterRange:(NSRange)r actualRange:(NSRangePointer)ar {
    if (editor->cursors.empty()) return NSZeroRect;
    float x = editor->getXFromPos(editor->cursors.back().head);
    float y = (float)(editor->getLineIdx(editor->cursors.back().head) - editor->vScrollPos) * editor->lineHeight;
    return [[self window] convertRectToScreen:[self convertRect:NSMakeRect(editor->gutterWidth - (float)editor->hScrollPos + x, y, 2, editor->lineHeight) toView:nil]];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)p { return 0; }
@end

@interface CustomWindow : NSWindow @end
@implementation CustomWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSMutableArray<CustomWindow *> *windows;
@property (strong) NSMenu *recentFilesMenu;
@end

@implementation AppDelegate
- (instancetype)init {
    if (self = [super init]) {
        self.windows = [NSMutableArray array];
    }
    return self;
}

- (void)rebuildRecentFilesMenu {
    if (!self.recentFilesMenu) return;
    [self.recentFilesMenu removeAllItems];
    EditorView *v = [self findEditorViewInWindow:[NSApp keyWindow]];
    if (!v && self.windows.count > 0) v = [self findEditorViewInWindow:self.windows.firstObject];
    if (v && v->editor) {
        if (v->editor->recentFiles.empty()) v->editor->loadRecentFiles();
        for (const auto& pathStr : v->editor->recentFiles) {
            NSString *nsPath = [NSString stringWithUTF8String:pathStr.c_str()];
            if (nsPath.length > 0) {
                NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:[nsPath lastPathComponent]
                                                              action:@selector(openRecentFileItem:)
                                                       keyEquivalent:@""];
                [item setRepresentedObject:nsPath];
                [item setToolTip:nsPath];
                [item setTarget:self];
                [self.recentFilesMenu addItem:item];
            }
        }
    }
    if (self.recentFilesMenu.numberOfItems > 0) {
        [self.recentFilesMenu addItem:[NSMenuItem separatorItem]];
    }
    NSMenuItem *clearItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Clear Menu", @"履歴を消去")
                                                       action:@selector(clearRecentFilesAction:)
                                                keyEquivalent:@""];
    [clearItem setTarget:self];
    [self.recentFilesMenu addItem:clearItem];
}

- (void)openRecentFileItem:(NSMenuItem *)sender {
    NSString *path = [sender representedObject];
    if (!path) return;
    EditorView *v = [self findEditorViewInWindow:[NSApp keyWindow]];
    if (v && v->editor) {
        if (v->editor->checkUnsavedChanges()) {
            v->editor->openFileFromPath([path UTF8String]);
        }
    } else {
        [self createWindowWithPath:[path UTF8String]];
    }
}

- (void)clearRecentFilesAction:(id)sender {
    EditorView *v = [self findEditorViewInWindow:[NSApp keyWindow]];
    if (v && v->editor) {
        v->editor->clearRecentFiles();
    }
    [self rebuildRecentFilesMenu];
}

- (void)createWindowWithPath:(const char*)path {
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskFullSizeContentView;
    CustomWindow *win = [[CustomWindow alloc] initWithContentRect:NSMakeRect(0, 0, 860, 620) styleMask:style backing:NSBackingStoreBuffered defer:NO];
    [win setTitlebarAppearsTransparent:YES];
    [win setTitleVisibility:NSWindowTitleVisible];
    [win setTitlebarSeparatorStyle:NSTitlebarSeparatorStyleNone];
    NSVisualEffectView *visualEffectView = [[NSVisualEffectView alloc] initWithFrame:win.contentView.bounds];
    visualEffectView.material = NSVisualEffectMaterialSidebar;
    visualEffectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    visualEffectView.state = NSVisualEffectStateFollowsWindowActiveState;
    visualEffectView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [win setContentView:visualEffectView];

    CGFloat titleBarHeight = 28.0;
    NSRect contentRect = visualEffectView.bounds;
    contentRect.origin.y = 0;
    contentRect.size.height -= titleBarHeight;
    EditorView *v = [[EditorView alloc] initWithFrame:contentRect];
    [v setWantsLayer:YES];
    v.layer.backgroundColor = [NSColor clearColor].CGColor;
    v.layer.opaque = NO;

    __weak AppDelegate *weakDelegate = self;
    v->editor->cbUpdateRecentMenu = [weakDelegate]() {
        __strong AppDelegate *strongDelegate = weakDelegate;
        if (strongDelegate) [strongDelegate rebuildRecentFilesMenu];
    };

    if (path) {
        v->editor->openFileFromPath(path);
    } else {
        v->editor->newFile();
        v->editor->showHelpPopup = true;
    }
    [visualEffectView addSubview:v];
    [v layoutSplitViews];

    [win center];
    [win setDelegate:self];
    [win setBackgroundColor:[NSColor blackColor]];
    [win makeKeyAndOrderFront:nil];
    [win makeFirstResponder:v];
    [self.windows addObject:win];
    [self rebuildRecentFilesMenu];
}

- (void)windowDidResize:(NSNotification *)notification {
    CustomWindow *win = (CustomWindow *)notification.object;
    EditorView *v = [self findEditorViewInWindow:win];
    if (v) {
        [v layoutSplitViews];
        [v updateScrollers];
    }
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    CustomWindow *win = (CustomWindow *)notification.object;
    EditorView *v = [self findEditorViewInWindow:win];
    if (v && v->editor) {
        v->editor->checkFileModification();
        [self rebuildRecentFilesMenu];
    }
}

- (void)setupMenuBar {
    NSString *appTitleNS = [NSString stringWithUTF8String:WToUTF8(APP_TITLE).c_str()];
    NSMenu *mainMenu = [NSMenu new];
    [NSApp setMainMenu:mainMenu];

    // App menu
    NSMenuItem *appMenuItem = [NSMenuItem new];
    [mainMenu addItem:appMenuItem];
    NSMenu *appMenu = [NSMenu new];
    [appMenuItem setSubmenu:appMenu];
    NSString *quitTitle = [NSString stringWithFormat:NSLocalizedString(@"Quit", nil), appTitleNS];
    [appMenu addItemWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];

    // File menu
    NSMenuItem *fileMenuItem = [NSMenuItem new];
    [mainMenu addItem:fileMenuItem];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", @"ファイル")];
    [fileMenuItem setSubmenu:fileMenu];
    [fileMenu addItemWithTitle:NSLocalizedString(@"New", @"新規") action:@selector(newDocument:) keyEquivalent:@"n"];
    [fileMenu addItemWithTitle:NSLocalizedString(@"Open...", @"開く...") action:@selector(openDocument:) keyEquivalent:@"o"];
    NSMenuItem *recentItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Open Recent", @"最近使った項目を開く") action:nil keyEquivalent:@""];
    self.recentFilesMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Open Recent", @"最近使った項目を開く")];
    [recentItem setSubmenu:self.recentFilesMenu];
    [fileMenu addItem:recentItem];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:NSLocalizedString(@"Close", @"閉じる") action:@selector(performClose:) keyEquivalent:@"w"];

    // Edit menu
    NSMenuItem *editMenuItem = [NSMenuItem new];
    [mainMenu addItem:editMenuItem];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Edit", @"編集")];
    [editMenuItem setSubmenu:editMenu];
    [editMenu addItemWithTitle:NSLocalizedString(@"Undo", @"元に戻す") action:@selector(undo:) keyEquivalent:@"z"];
    NSMenuItem *redoMenuItem = [editMenu addItemWithTitle:NSLocalizedString(@"Redo", @"やり直し") action:@selector(redo:) keyEquivalent:@"Z"];
    [redoMenuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItemWithTitle:NSLocalizedString(@"Cut", @"切り取り") action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:NSLocalizedString(@"Copy", @"コピー") action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:NSLocalizedString(@"Paste", @"貼り付け") action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:NSLocalizedString(@"Select All", @"すべて選択") action:@selector(selectAll:) keyEquivalent:@"a"];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItemWithTitle:NSLocalizedString(@"Toggle Comment", @"コメント切替") action:@selector(toggleCommentAction:) keyEquivalent:@"/"];

    // View menu
    NSMenuItem *viewMenuItem = [NSMenuItem new];
    [mainMenu addItem:viewMenuItem];
    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", @"表示")];
    [viewMenuItem setSubmenu:viewMenu];
    NSMenuItem *wrapItem = [viewMenu addItemWithTitle:NSLocalizedString(@"Word Wrap", @"折り返し") action:@selector(toggleWordWrapAction:) keyEquivalent:@"z"];
    [wrapItem setKeyEquivalentModifierMask:NSEventModifierFlagOption];
    NSMenuItem *prevItem = [viewMenu addItemWithTitle:NSLocalizedString(@"Live Preview", @"プレビュー") action:@selector(togglePreviewAction:) keyEquivalent:@"p"];
    [prevItem setKeyEquivalentModifierMask:NSEventModifierFlagOption];
    NSMenuItem *topItem = [viewMenu addItemWithTitle:NSLocalizedString(@"Always on Top", @"最前面表示") action:@selector(toggleAlwaysOnTopAction:) keyEquivalent:@"t"];
    [topItem setKeyEquivalentModifierMask:NSEventModifierFlagOption];
}

- (void)applicationDidFinishLaunching:(NSNotification *)n {
    [self setupMenuBar];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    if (args.count > 1) [self createWindowWithPath:[args[1] UTF8String]];
    else if (self.windows.count == 0) [self createWindowWithPath:nullptr];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames {
    for (NSString *path in filenames) [self createWindowWithPath:[path UTF8String]];
    [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (void)newDocument:(id)sender {
    NSWindow *win = [NSApp keyWindow];
    EditorView *v = [self findEditorViewInWindow:win];
    if (v) {
        v->editor->newFile();
    } else {
        [self createWindowWithPath:nullptr];
    }
}

- (void)openDocument:(id)sender {
    NSWindow *win = [NSApp keyWindow];
    EditorView *v = [self findEditorViewInWindow:win];
    if (v) {
        v->editor->openFile();
    } else {
        [self createWindowWithPath:nullptr];
        CustomWindow *newWin = self.windows.lastObject;
        EditorView *newV = [self findEditorViewInWindow:newWin];
        if (newV) newV->editor->openFile();
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { return YES; }

- (BOOL)windowShouldClose:(NSWindow *)sender {
    EditorView *v = [self findEditorViewInWindow:sender];
    if (v && v->editor) {
        v->editor->saveCurrentViewState();
        if (!v->editor->checkUnsavedChanges()) return NO;
    }
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
    CustomWindow *win = (CustomWindow *)notification.object;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.windows removeObject:win];
    });
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    NSArray *windowsToClose = [self.windows copy];
    for (CustomWindow *win in windowsToClose) {
        EditorView *v = [self findEditorViewInWindow:win];
        if (v && v->editor) {
            v->editor->saveCurrentViewState();
            if (!v->editor->checkUnsavedChanges()) return NSTerminateCancel;
        }
    }
    return NSTerminateNow;
}

- (EditorView *)findEditorViewInWindow:(NSWindow *)window {
    if (!window) return nil;
    NSView *contentView = window.contentView;
    if ([contentView isKindOfClass:[EditorView class]]) {
        return (EditorView *)contentView;
    }
    for (NSView *subview in contentView.subviews) {
        if ([subview isKindOfClass:[EditorView class]]) {
            return (EditorView *)subview;
        }
    }
    return nil;
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
