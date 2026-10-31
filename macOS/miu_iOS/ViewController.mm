#import "ViewController.h"
#import "EditorCore.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <GameController/GameController.h>
#import <WebKit/WebKit.h>

@interface MiuIOSPreviewMessageHandler : NSObject <WKScriptMessageHandler, WKNavigationDelegate>
@property (nonatomic, assign) Editor *editor;
@property (nonatomic, assign) BOOL previewPageLoaded;
@property (nonatomic, copy) NSString *pendingHtml;
@end

@implementation MiuIOSPreviewMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    if (!self.editor) return;
    if ([message.name isEqualToString:@"miuPreview"] && [message.body isKindOfClass:[NSString class]]) {
        NSString *body = (NSString *)message.body;
        if ([body hasPrefix:@"toggle_task:"]) {
            int lineIdx = [[body substringFromIndex:12] intValue];
            self.editor->handlePreviewTaskToggle(lineIdx);
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
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    self.previewPageLoaded = YES;
    if (self.editor && self.editor->showPreview && self.editor->previewInitialLoaded) {
        MiuPreview::PreviewRenderResult res = self.editor->getPreviewRenderResult();
        if (res.isMdOrMermaidShell) {
            std::string js = MiuPreview::buildDomUpdateJs(res.bodyContent, self.editor->isDarkMode);
            NSString *jsNS = [NSString stringWithUTF8String:js.c_str()];
            if (jsNS) {
                [webView evaluateJavaScript:jsNS completionHandler:nil];
            }
        }
    }
}
- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    self.previewPageLoaded = NO;
    if (error.code == NSURLErrorCancelled) return;
    NSString *html = self.pendingHtml;
    if (html) {
        self.pendingHtml = nil;
        [webView loadHTMLString:html baseURL:nil];
    }
}
- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    self.previewPageLoaded = NO;
    if (error.code == NSURLErrorCancelled) return;
    NSString *html = self.pendingHtml;
    if (html) {
        self.pendingHtml = nil;
        [webView loadHTMLString:html baseURL:nil];
    }
}
- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView {
    self.previewPageLoaded = NO;
    if (self.editor && self.editor->showPreview) {
        self.editor->previewInitialLoaded = false;
        if (self.editor->cbUpdatePreview) self.editor->cbUpdatePreview();
    }
}
@end

@interface MiuIOSPreviewSplitterView : UIView
@property (nonatomic, assign) Editor *editor;
@property (nonatomic, assign) BOOL isVerticalSplit;
@property (nonatomic, copy) void (^onRatioChanged)(void);
@end

@implementation MiuIOSPreviewSplitterView
- (instancetype)initWithFrame:(CGRect)frame {
    if (self = [super initWithFrame:frame]) {
        self.clipsToBounds = YES;
        self.opaque = YES;
        UIPanGestureRecognizer *pan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handleSplitterPan:)];
        [self addGestureRecognizer:pan];
    }
    return self;
}
- (void)drawRect:(CGRect)rect {
    BOOL dark = self.editor ? self.editor->isDarkMode : NO;
    UIColor *bgCol = dark ? [UIColor blackColor] : [UIColor whiteColor];
    [bgCol setFill];
    UIRectFill(CGRectIntersection(rect, self.bounds));
    UIColor *lineCol = dark ? [UIColor colorWithWhite:1.0 alpha:0.06] : [UIColor colorWithWhite:0.0 alpha:0.06];
    [lineCol setFill];
    if (self.bounds.size.width < self.bounds.size.height) {
        UIRectFillUsingBlendMode(CGRectMake(std::floor(self.bounds.size.width * 0.5), 0, 1.0, self.bounds.size.height), kCGBlendModeNormal);
    } else {
        UIRectFillUsingBlendMode(CGRectMake(0, std::floor(self.bounds.size.height * 0.5), self.bounds.size.width, 1.0), kCGBlendModeNormal);
    }
    UIColor *pillCol = dark ? [UIColor colorWithWhite:1.0 alpha:0.22] : [UIColor colorWithWhite:0.0 alpha:0.22];
    [pillCol setFill];
    CGRect pillRect;
    if (self.bounds.size.width < self.bounds.size.height) {
        pillRect = CGRectMake((self.bounds.size.width - 2.0) * 0.5, (self.bounds.size.height - 28.0) * 0.5, 2.0, 28.0);
    } else {
        pillRect = CGRectMake((self.bounds.size.width - 28.0) * 0.5, (self.bounds.size.height - 2.0) * 0.5, 28.0, 2.0);
    }
    UIBezierPath *bp = [UIBezierPath bezierPathWithRoundedRect:pillRect cornerRadius:1.0];
    [bp fill];
}
- (void)handleSplitterPan:(UIPanGestureRecognizer *)pan {
    if (!self.editor || !self.superview) return;
    UIView *container = self.superview;
    CGPoint pt = [pan locationInView:container];
    if (self.isVerticalSplit) {
        CGFloat totalH = container.bounds.size.height;
        if (totalH > 100.0) {
            self.editor->previewSplitRatio = std::clamp((float)(pt.y / totalH), 0.15f, 0.85f);
            if (self.onRatioChanged) self.onRatioChanged();
        }
    } else {
        CGFloat totalW = container.bounds.size.width;
        if (totalW > 100.0) {
            self.editor->previewSplitRatio = std::clamp((float)(pt.x / totalW), 0.15f, 0.85f);
            if (self.onRatioChanged) self.onRatioChanged();
        }
    }
}
@end

@interface iOSTextPosition : UITextPosition <NSCopying>
@property (nonatomic, assign) size_t index;
+ (instancetype)positionWithIndex:(size_t)index;
@end
@implementation iOSTextPosition
+ (instancetype)positionWithIndex:(size_t)index {
    iOSTextPosition *p = [[iOSTextPosition alloc] init];
    p.index = index; return p;
}
- (id)copyWithZone:(NSZone *)zone {
    iOSTextPosition *copy = [[iOSTextPosition allocWithZone:zone] init];
    copy.index = self.index;
    return copy;
}
@end
@interface iOSTextRange : UITextRange
@property (nonatomic, strong) iOSTextPosition *startPos;
@property (nonatomic, strong) iOSTextPosition *endPos;
+ (instancetype)rangeWithStart:(size_t)start end:(size_t)end;
@end
@implementation iOSTextRange
+ (instancetype)rangeWithStart:(size_t)start end:(size_t)end {
    iOSTextRange *r = [[iOSTextRange alloc] init];
    r.startPos = [iOSTextPosition positionWithIndex:start];
    r.endPos = [iOSTextPosition positionWithIndex:end]; return r;
}
- (UITextPosition *)start { return self.startPos; }
- (UITextPosition *)end { return self.endPos; }
- (BOOL)isEmpty { return self.startPos.index == self.endPos.index; }
@end
@interface iOSSelectionRect : UITextSelectionRect
@property (nonatomic, assign) CGRect rectValue;
@property (nonatomic, assign) NSWritingDirection writingDirectionValue;
@property (nonatomic, assign) BOOL containsStartValue;
@property (nonatomic, assign) BOOL containsEndValue;
@property (nonatomic, assign) BOOL isVerticalValue;
@end
@implementation iOSSelectionRect
- (CGRect)rect { return _rectValue; }
- (NSWritingDirection)writingDirection { return _writingDirectionValue; }
- (BOOL)containsStart { return _containsStartValue; }
- (BOOL)containsEnd { return _containsEndValue; }
- (BOOL)isVertical { return _isVerticalValue; }
@end
@interface iOSEditorView : UIView <UITextInput, UIGestureRecognizerDelegate, UIEditMenuInteractionDelegate>
@property (nonatomic) Editor *editor;
@property (nonatomic, copy) void (^onNewDocumentAction)(void);
@property (nonatomic, copy) void (^onGoToLineAction)(void);
@property (nonatomic, copy) void (^onOpenRecentAction)(UIView *sourceView);
@property (nonatomic, copy) BOOL (^onDismissOpenRecentIfVisible)(void);
@property (nonatomic, copy) void (^onDismissOpenRecentThenPerform)(void (^block)(void));
@property (nonatomic, copy) void (^onSyncKeyboardLayout)(void);
@property (nonatomic, strong) UIEditMenuInteraction *editMenuInteraction API_AVAILABLE(ios(16.0));
@property (nonatomic, assign) CGFloat topRenderMargin;
- (UIView *)activeAccessoryView;
- (CGFloat)unobstructedBottomY;
- (CGFloat)effectiveViewportHeight;
- (void)jumpToLine:(NSInteger)lineNumber;
- (void)getSmartWordRangeAtPos:(size_t)pos outStart:(size_t*)outStart outEnd:(size_t*)outEnd;
- (void)showMenuForCurrentSelection;
- (void)handleTabKey:(id)sender;
- (void)handleShiftTabKey:(id)sender;
- (void)toggleCommentAction:(id)sender;
- (void)toggleWordWrapAction:(id)sender;
- (void)togglePreviewAction:(id)sender;
- (void)toggleHelp:(id)sender;
@end
@implementation iOSEditorView {
    CGPoint _lastPanTranslation;
    CGFloat _vScrollAccumulator;
    UITextInputStringTokenizer *_tokenizer;
    CADisplayLink *_displayLink;
    CGPoint _scrollVelocity;
    NSTimeInterval _lastUpdateTime;
    CGFloat _initialFontSize;
    UIView *_customAccessoryView;
    BOOL _isMouseSelecting;
    BOOL _isPotentialDragMove;
    CGPoint _dragStartPoint;
    CGPoint _boxSelectStartPoint;
    UIKeyModifierFlags _activeTouchModifiers;
    UIPanGestureRecognizer *_panRecognizer;
    NSInteger _selectionGranularity;
    NSMutableArray<id<UIInteraction>> *_textInteractions;
    BOOL _isScrolling;
    UITapGestureRecognizer *_singleTapGR;
    UITapGestureRecognizer *_doubleTapGR;
    UITapGestureRecognizer *_tripleTapGR;
    UITapGestureRecognizer *_accessoryTapGR;
    CGSize _lastRenderedBoundsSize;
}
@synthesize inputDelegate = _inputDelegate;
@synthesize markedTextStyle = _markedTextStyle;
@synthesize selectedTextRange = _selectedTextRange;
- (instancetype)initWithFrame:(CGRect)frame {
    if (self = [super initWithFrame:frame]) {
        self.contentMode = UIViewContentModeRedraw;
        self.clipsToBounds = YES;
        self.multipleTouchEnabled = YES;
        if (@available(iOS 9.0, *)) {
            self.inputAssistantItem.leadingBarButtonGroups = @[];
            self.inputAssistantItem.trailingBarButtonGroups = @[];
        }
        if (@available(iOS 14.0, *)) {
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(hardwareKeyboardStateChanged:) name:GCKeyboardDidConnectNotification object:nil];
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(hardwareKeyboardStateChanged:) name:GCKeyboardDidDisconnectNotification object:nil];
        }
        _tokenizer = [[UITextInputStringTokenizer alloc] initWithTextInput:self];
        if (@available(iOS 13.0, *)) {
            UITextInteraction *interaction = [UITextInteraction textInteractionForMode:UITextInteractionModeEditable];
            interaction.textInput = self;
            [self addInteraction:interaction];
        }
        _tripleTapGR = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onTripleTap:)];
        _tripleTapGR.numberOfTapsRequired = 3;
        [self addGestureRecognizer:_tripleTapGR];
        _doubleTapGR = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onDoubleTap:)];
        _doubleTapGR.numberOfTapsRequired = 2;
        [self addGestureRecognizer:_doubleTapGR];
        _singleTapGR = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onSingleTap:)];
        _singleTapGR.numberOfTapsRequired = 1;
        [self addGestureRecognizer:_singleTapGR];
        _panRecognizer = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handlePan:)];
        if (@available(iOS 13.4, *)) {
            _panRecognizer.allowedScrollTypesMask = UIScrollTypeMaskAll;
        }
        _panRecognizer.cancelsTouchesInView = NO;
        _panRecognizer.delegate = self;
        [self addGestureRecognizer:_panRecognizer];
        UIPinchGestureRecognizer *pinch = [[UIPinchGestureRecognizer alloc] initWithTarget:self action:@selector(handlePinch:)];
        [self addGestureRecognizer:pinch];
        if (@available(iOS 16.0, *)) {
            _editMenuInteraction = [[UIEditMenuInteraction alloc] initWithDelegate:self];
            [self addInteraction:_editMenuInteraction];
        }
    }
    return self;
}
- (void)hideHelpIfNeeded {
    if (self.editor && self.editor->showHelpPopup) {
        self.editor->showHelpPopup = false;
        [self setNeedsDisplay];
    }
}
- (void)hardwareKeyboardStateChanged:(NSNotification *)note {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self reloadInputViews];
    });
}
- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}
- (void)showEditMenuAtRect:(CGRect)rect {
    if (@available(iOS 16.0, *)) {
        UIEditMenuConfiguration *config = [UIEditMenuConfiguration configurationWithIdentifier:nil sourcePoint:CGPointMake(CGRectGetMidX(rect), CGRectGetMinY(rect))];
        [self.editMenuInteraction presentEditMenuWithConfiguration:config];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        UIMenuController *menu = [UIMenuController sharedMenuController];
        if (menu.isMenuVisible) return;
        [menu showMenuFromView:self rect:rect];
#pragma clang diagnostic pop
    }
}
- (void)hideEditMenuIfNeeded {
    if (@available(iOS 16.0, *)) {
        [self.editMenuInteraction dismissMenu];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        UIMenuController *menu = [UIMenuController sharedMenuController];
        if (menu.isMenuVisible) {
            [menu hideMenuFromView:self];
        }
#pragma clang diagnostic pop
    }
}
- (void)showMenuForCurrentSelection {
    if (!self.editor || self.editor->cursors.empty()) return;
    Cursor c = self.editor->cursors.back();
    CGRect targetRect;
    if (c.hasSelection()) {
        NSArray<UITextSelectionRect *> *rects = [self selectionRectsForRange:self.selectedTextRange];
        if (rects.count > 0) {
            targetRect = rects.firstObject.rect;
            for (UITextSelectionRect *r in rects) {
                targetRect = CGRectUnion(targetRect, r.rect);
            }
        } else {
            targetRect = [self caretRectForPosition:self.selectedTextRange.end];
        }
    } else {
        targetRect = [self caretRectForPosition:self.selectedTextRange.end];
    }
    if (targetRect.size.height == 0) {
        targetRect.size.height = self.editor->lineHeight;
    }
    [self showEditMenuAtRect:targetRect];
}
- (void)onSingleTap:(UITapGestureRecognizer *)sender {
    if (sender.state == UIGestureRecognizerStateEnded) {
        CGPoint p = [sender locationInView:self];
        [self handleSingleTapAtPoint:p];
    }
}
- (void)onDoubleTap:(UITapGestureRecognizer *)sender {
    if (sender.state == UIGestureRecognizerStateEnded) {
        CGPoint p = [sender locationInView:self];
        _selectionGranularity = 1;
        [self handleDoubleTapAtPoint:p];
    }
}
- (void)onTripleTap:(UITapGestureRecognizer *)sender {
    if (sender.state == UIGestureRecognizerStateEnded) {
        CGPoint p = [sender locationInView:self];
        _selectionGranularity = 2;
        [self handleTripleTapAtPoint:p];
    }
}
- (void)setFrame:(CGRect)frame {
    CGSize oldSize = self.bounds.size;
    [super setFrame:frame];
    if (!CGSizeEqualToSize(oldSize, self.bounds.size)) {
        [self setNeedsDisplay];
    }
}
- (void)setBounds:(CGRect)bounds {
    CGSize oldSize = self.bounds.size;
    [super setBounds:bounds];
    if (!CGSizeEqualToSize(oldSize, self.bounds.size)) {
        [self setNeedsDisplay];
    }
}
- (void)layoutSubviews {
    [super layoutSubviews];
    if (!CGSizeEqualToSize(_lastRenderedBoundsSize, self.bounds.size)) {
        _lastRenderedBoundsSize = self.bounds.size;
        [self setNeedsDisplay];
    }
}
- (UIView *)textInputView {
    return self;
}
- (void)drawRect:(CGRect)rect {
    if (!self.editor) return;
    BOOL isDark = (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
    if (self.editor->isDarkMode != (bool)isDark) {
        self.editor->isDarkMode = isDark;
        self.editor->updateThemeColors();
    }
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    CGContextTranslateCTM(ctx, 0, self.topRenderMargin);
    self.editor->render(ctx, self.bounds.size.width, self.bounds.size.height - self.topRenderMargin);
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (self.editor && self.editor->showHelpPopup) {
        self.editor->showHelpPopup = false;
        [self setNeedsDisplay];
    }
    [self stopMomentumScroll];
    UITouch *touch = [touches anyObject];
    _activeTouchModifiers = 0;
    if (@available(iOS 13.4, *)) {
        _activeTouchModifiers = event.modifierFlags;
    }
    if (touch.type == UITouchTypeIndirectPointer || touch.type == UITouchTypePencil) {
        _isMouseSelecting = YES;
        _isPotentialDragMove = NO;
        if (!self.editor) return;
        self.editor->isDraggingSelection = false;
        self.editor->isGutterDragging = false;
        self.editor->activeEmmetSession.active = false;
        self.editor->rollbackPadding();
        self.editor->isRectSelecting = false;

        CGPoint rawPt = [touch locationInView:self];
        CGPoint p = rawPt;
        p.y -= self.topRenderMargin;
        bool isOption = (_activeTouchModifiers & UIKeyModifierAlternate) != 0;
        bool isShift = (_activeTouchModifiers & UIKeyModifierShift) != 0;
        bool isCommand = (_activeTouchModifiers & UIKeyModifierCommand) != 0;

        size_t pos = self.editor->getDocPosFromPoint((float)p.x, (float)p.y);
        int li = self.editor->getLineIdx(pos);

        if (p.x < self.editor->gutterWidth) {
            [self.inputDelegate selectionWillChange:self];
            self.editor->isGutterDragging = true;
            self.editor->gutterAnchorLine = li;
            self.editor->selectLinesRange(li, li, isCommand);
            [self.inputDelegate selectionDidChange:self];
            [self setNeedsDisplay];
            return;
        }
        if (touch.tapCount == 2) {
            _selectionGranularity = 1;
            [self.inputDelegate selectionWillChange:self];
            self.editor->selectWordAt(pos, isCommand);
            [self.inputDelegate selectionDidChange:self];
            [self setNeedsDisplay];
            return;
        } else if (touch.tapCount >= 3) {
            _selectionGranularity = 2;
            [self.inputDelegate selectionWillChange:self];
            self.editor->selectLineAt(pos, isCommand);
            [self.inputDelegate selectionDidChange:self];
            [self setNeedsDisplay];
            return;
        }
        _selectionGranularity = 0;
        if (isShift && !isOption && !self.editor->cursors.empty()) {
            [self.inputDelegate selectionWillChange:self];
            Cursor& c = self.editor->cursors.back();
            c.head = pos;
            c.desiredX = self.editor->getXInLine(li, pos);
            c.isVirtual = false;
            [self.inputDelegate selectionDidChange:self];
            [self setNeedsDisplay];
            return;
        }
        if (!isOption && !isShift && !isCommand && self.editor->isPointInSelection(pos)) {
            _isPotentialDragMove = YES;
            _dragStartPoint = p;
            [self setNeedsDisplay];
            return;
        }
        [self.inputDelegate selectionWillChange:self];
        if (!isOption && !isShift && !isCommand) {
            self.editor->cursors.clear();
        }
        _boxSelectStartPoint = p;
        float docX = (float)p.x - self.editor->gutterWidth + (float)self.editor->hScrollPos;
        float snappedX = docX;
        if (self.editor->charWidth > 0) {
            int cols = (int)std::round(docX / self.editor->charWidth);
            if (cols < 0) cols = 0;
            snappedX = (float)cols * self.editor->charWidth;
        }
        float physX = self.editor->getXInLine(li, pos);
        Cursor newCursor;
        newCursor.head = pos;
        newCursor.anchor = pos;
        if (isOption && (snappedX > physX + (self.editor->charWidth * 0.5f))) {
            newCursor.desiredX = snappedX;
            newCursor.isVirtual = true;
        } else {
            newCursor.desiredX = physX;
            newCursor.isVirtual = false;
        }
        newCursor.originalAnchorX = newCursor.desiredX;
        self.editor->cursors.push_back(newCursor);
        if (isCommand) self.editor->mergeCursors();
        [self.inputDelegate selectionDidChange:self];
        [self setNeedsDisplay];
        [self becomeFirstResponder];
    } else {
        _isMouseSelecting = NO;
        _isPotentialDragMove = NO;
        _selectionGranularity = 0;
    }
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [super touchesMoved:touches withEvent:event];
    if (!_isMouseSelecting || !self.editor) return;
    UITouch *touch = [touches anyObject];
    CGPoint p = [touch locationInView:self];
    p.y -= self.topRenderMargin;
    CGFloat availH = MAX(1.0, self.bounds.size.height - self.topRenderMargin);
    if (p.y < 0) {
        self.editor->scrollVisualLines(-1);
    } else if (p.y > availH) {
        self.editor->scrollVisualLines(1);
    }
    size_t rawPos = self.editor->getDocPosFromPoint(p.x, p.y);
    if (self.editor->isGutterDragging) {
        [self.inputDelegate selectionWillChange:self];
        int headLine = self.editor->getLineIdx(rawPos);
        self.editor->selectLinesRange(self.editor->gutterAnchorLine, headLine, false);
        [self.inputDelegate selectionDidChange:self];
        [self setNeedsDisplay];
        return;
    }
    if (_isPotentialDragMove) {
        double dx = p.x - _dragStartPoint.x;
        double dy = p.y - _dragStartPoint.y;
        if (std::hypot(dx, dy) > 3.0) {
            self.editor->isDraggingSelection = true;
        }
        if (self.editor->isDraggingSelection) {
            self.editor->dragDropTargetPos = rawPos;
            self.editor->ensureCaretVisible();
            [self setNeedsDisplay];
        }
        return;
    }
    bool isOption = (_activeTouchModifiers & UIKeyModifierAlternate) != 0;
    if (isOption) {
        [self.inputDelegate selectionWillChange:self];
        auto snapToGrid = [&](float rawX) -> float {
            if (self.editor->charWidth <= 0) return rawX;
            int cols = (int)std::round(rawX / self.editor->charWidth);
            if (cols < 0) cols = 0;
            return (float)cols * self.editor->charWidth;
        };
        self.editor->cursors.clear();
        float startY = std::min((float)_boxSelectStartPoint.y, (float)p.y);
        float endY = std::max((float)_boxSelectStartPoint.y, (float)p.y);
        int startLine = (int)std::floor((startY + (float)self.editor->vScrollPos * self.editor->lineHeight) / self.editor->lineHeight);
        int endLine = (int)std::floor((endY + (float)self.editor->vScrollPos * self.editor->lineHeight) / self.editor->lineHeight);
        startLine = std::clamp(startLine, 0, (int)self.editor->lineStarts.size() - 1);
        endLine = std::clamp(endLine, 0, (int)self.editor->lineStarts.size() - 1);
        float rawStartX = (float)_boxSelectStartPoint.x - self.editor->gutterWidth + (float)self.editor->hScrollPos;
        float rawEndX = (float)p.x - self.editor->gutterWidth + (float)self.editor->hScrollPos;
        self.editor->isRectSelecting = true;
        self.editor->rectAnchorLine = startLine;
        self.editor->rectHeadLine = endLine;
        self.editor->rectAnchorX = snapToGrid(rawStartX);
        self.editor->rectHeadX = snapToGrid(rawEndX);
        self.editor->updateRectSelection();
        self.editor->ensureCaretVisible();
        [self.inputDelegate selectionDidChange:self];
        [self setNeedsDisplay];
        return;
    }
    if (self.editor->cursors.empty()) return;
    Cursor &c = self.editor->cursors.back();
    size_t anchor = c.anchor;
    size_t newHead = rawPos;
    if (_selectionGranularity == 1) {
        size_t wStart, wEnd;
        [self getSmartWordRangeAtPos:rawPos outStart:&wStart outEnd:&wEnd];
        newHead = (rawPos >= anchor) ? wEnd : wStart;
    } else if (_selectionGranularity == 2) {
        int lineIdx = self.editor->getLineIdx(rawPos);
        size_t lStart = self.editor->lineStarts[lineIdx];
        size_t lEnd = (lineIdx + 1 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[lineIdx + 1] : self.editor->pt.length();
        newHead = (rawPos >= anchor) ? lEnd : lStart;
    }
    if (c.head != newHead) {
        [self.inputDelegate selectionWillChange:self];
        c.head = newHead;
        int li = self.editor->getLineIdx(newHead);
        c.desiredX = self.editor->getXInLine(li, newHead);
        c.isVirtual = false;
        [self.inputDelegate selectionDidChange:self];
        self.editor->ensureCaretVisible();
        [self setNeedsDisplay];
    }
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (_isMouseSelecting) {
        UITouch *touch = [touches anyObject];
        CGPoint p = [touch locationInView:self];
        p.y -= self.topRenderMargin;
        if (self.editor && self.editor->isDraggingSelection) {
            size_t targetPos = self.editor->getDocPosFromPoint((float)p.x, (float)p.y);
            self.editor->isDraggingSelection = false;
            _isPotentialDragMove = NO;
            _isMouseSelecting = NO;
            [self.inputDelegate selectionWillChange:self];
            [self.inputDelegate textWillChange:self];
            self.editor->performDragMove(targetPos);
            [self.inputDelegate textDidChange:self];
            [self.inputDelegate selectionDidChange:self];
            [self setNeedsDisplay];
            return;
        }
        if (self.editor && _isPotentialDragMove) {
            _isPotentialDragMove = NO;
            size_t pos = self.editor->getDocPosFromPoint((float)p.x, (float)p.y);
            int li = self.editor->getLineIdx(pos);
            float physX = self.editor->getXInLine(li, pos);
            [self.inputDelegate selectionWillChange:self];
            self.editor->cursors.clear();
            self.editor->cursors.push_back({ pos, pos, physX, physX, false, false });
            [self.inputDelegate selectionDidChange:self];
        }
        if (self.editor) {
            self.editor->isGutterDragging = false;
            self.editor->mergeCursors();
        }
        _isMouseSelecting = NO;
        _selectionGranularity = 0;
        [self setNeedsDisplay];
        [self showMenuForCurrentSelection];
        return;
    }
    [super touchesEnded:touches withEvent:event];
}
- (void)handleSingleTapAtPoint:(CGPoint)p {
    if (!self.editor) return;
    self.editor->activeEmmetSession.active = false;
    self.editor->rollbackPadding();
    self.editor->isRectSelecting = false;
    self.editor->isDraggingSelection = false;
    self.editor->isGutterDragging = false;
    p.y -= self.topRenderMargin;
    size_t pos = self.editor->getDocPosFromPoint(p.x, p.y);
    int li = self.editor->getLineIdx(pos);
    [self.inputDelegate selectionWillChange:self];
    if (p.x < self.editor->gutterWidth) {
        self.editor->selectLinesRange(li, li, false);
    } else {
        self.editor->cursors.clear();
        Cursor c; c.head = pos; c.anchor = pos;
        c.desiredX = self.editor->getXInLine(li, pos);
        c.originalAnchorX = c.desiredX; c.isVirtual = false;
        self.editor->cursors.push_back(c);
    }
    [self.inputDelegate selectionDidChange:self];
    self.editor->ensureCaretVisible();
    [self setNeedsDisplay];
    [self becomeFirstResponder];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self showMenuForCurrentSelection];
    });
}
- (void)getSmartWordRangeAtPos:(size_t)pos outStart:(size_t*)outStart outEnd:(size_t*)outEnd {
    *outStart = pos;
    *outEnd = pos;
    if (!self.editor) return;
    size_t docLen = self.editor->pt.length();
    if (docLen == 0) return;
    if (pos >= docLen) pos = docLen - 1;
    int lineIdx = self.editor->getLineIdx(pos);
    size_t lineStart = self.editor->lineStarts[lineIdx];
    size_t lineEnd = (lineIdx + 1 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[lineIdx + 1] : docLen;
    size_t lineByteLen = lineEnd - lineStart;
    if (lineByteLen == 0) return;
    std::string lineBytes = self.editor->pt.getRange(lineStart, lineByteLen);
    NSString *lineStr = [NSString stringWithUTF8String:lineBytes.c_str()];
    if (!lineStr || lineStr.length == 0) return;
    size_t relativeTapByte = pos - lineStart;
    if (relativeTapByte > lineBytes.size()) relativeTapByte = lineBytes.size();
    NSString *preTapStr = [[NSString alloc] initWithBytes:lineBytes.c_str() length:relativeTapByte encoding:NSUTF8StringEncoding];
    NSUInteger tapCharIdx = preTapStr.length;
    if (tapCharIdx >= lineStr.length && tapCharIdx > 0) tapCharIdx = lineStr.length - 1;
    CFRange tokenRange = CFRangeMake(kCFNotFound, 0);
    CFStringTokenizerRef tokenizer = CFStringTokenizerCreate(kCFAllocatorDefault, (__bridge CFStringRef)lineStr, CFRangeMake(0, lineStr.length), kCFStringTokenizerUnitWordBoundary, NULL);
    if (tokenizer) {
        CFStringTokenizerTokenType tokenType = CFStringTokenizerGoToTokenAtIndex(tokenizer, tapCharIdx);
        if (tokenType != kCFStringTokenizerTokenNone) {
            tokenRange = CFStringTokenizerGetCurrentTokenRange(tokenizer);
        }
        CFRelease(tokenizer);
    }
    NSRange finalRange;
    if (tokenRange.location != kCFNotFound) {
        finalRange = NSMakeRange(tokenRange.location, tokenRange.length);
        NSCharacterSet *wordSet = [NSCharacterSet characterSetWithCharactersInString:@"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"];
        unichar c = [lineStr characterAtIndex:tapCharIdx];
        if ([wordSet characterIsMember:c]) {
            while (finalRange.location > 0) {
                if ([wordSet characterIsMember:[lineStr characterAtIndex:finalRange.location - 1]]) {
                    finalRange.location--; finalRange.length++;
                } else break;
            }
            while (finalRange.location + finalRange.length < lineStr.length) {
                if ([wordSet characterIsMember:[lineStr characterAtIndex:finalRange.location + finalRange.length]]) {
                    finalRange.length++;
                } else break;
            }
        }
    } else {
        finalRange = NSMakeRange(tapCharIdx, 1);
    }
    NSString *preMatchStr = [lineStr substringToIndex:finalRange.location];
    NSString *matchStr = [lineStr substringWithRange:finalRange];
    size_t preBytes = [preMatchStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    size_t matchBytes = [matchStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    *outStart = lineStart + preBytes;
    *outEnd = *outStart + matchBytes;
}
- (void)handleDoubleTapAtPoint:(CGPoint)p {
    if (!self.editor) return;
    self.editor->activeEmmetSession.active = false;
    self.editor->rollbackPadding();
    self.editor->isRectSelecting = false;
    p.y -= self.topRenderMargin;
    size_t pos = self.editor->getDocPosFromPoint(p.x, p.y);
    size_t start, end;
    [self getSmartWordRangeAtPos:pos outStart:&start outEnd:&end];
    [self.inputDelegate selectionWillChange:self];
    self.editor->cursors.clear();
    Cursor c; c.anchor = start; c.head = end;
    int li = self.editor->getLineIdx(end);
    c.desiredX = self.editor->getXInLine(li, end);
    c.originalAnchorX = c.desiredX; c.isVirtual = false;
    self.editor->cursors.push_back(c);
    [self.inputDelegate selectionDidChange:self];
    self.editor->ensureCaretVisible();
    [self setNeedsDisplay];
}
- (void)handleTripleTapAtPoint:(CGPoint)p {
    if (!self.editor) return;
    self.editor->activeEmmetSession.active = false;
    self.editor->rollbackPadding();
    self.editor->isRectSelecting = false;
    p.y -= self.topRenderMargin;
    size_t pos = self.editor->getDocPosFromPoint(p.x, p.y);
    [self.inputDelegate selectionWillChange:self];
    self.editor->selectLineAt(pos, false);
    [self.inputDelegate selectionDidChange:self];
    self.editor->ensureCaretVisible();
    [self setNeedsDisplay];
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch {
    if (gestureRecognizer == _accessoryTapGR) {
        UIView *v = touch.view;
        while (v && v != _customAccessoryView) {
            if ([v isKindOfClass:[UIControl class]]) {
                return NO;
            }
            v = v.superview;
        }
    }
    return YES;
}
- (void)onAccessoryBackgroundTap:(UITapGestureRecognizer *)sender {
    if (sender.state == UIGestureRecognizerStateEnded) {
        if (self.onDismissOpenRecentIfVisible) {
            self.onDismissOpenRecentIfVisible();
        }
    }
}
- (UIView *)activeAccessoryView {
    return _customAccessoryView;
}
- (CGFloat)unobstructedBottomY {
    return self.bounds.size.height;
}
- (CGFloat)effectiveViewportHeight {
    CGFloat bottomY = [self unobstructedBottomY];
    CGFloat availH = MAX(1.0, bottomY - self.topRenderMargin);
    if (self.isFirstResponder && self.editor && self.editor->lineHeight > 0.0f) {
        CGFloat lineH = (CGFloat)self.editor->lineHeight;
        if (availH > lineH * 2.0) {
            availH -= lineH;
        }
    }
    return availH;
}
- (void)applyNewFontSize:(CGFloat)newFontSize {
    if (!self.editor) return;
    newFontSize = std::clamp((float)newFontSize, 6.0f, 200.0f);
    if (fabs(newFontSize - self.editor->currentFontSize) < 0.5f) {
        return;
    }
    self.editor->updateFont((float)newFontSize);
    float vw = self.bounds.size.width;
    float vh = (float)[self effectiveViewportHeight];
    ScrollBarLayout L = self.editor->getScrollBarLayout(vw, vh);
    int maxH = L.needH ? std::max(0, (int)std::ceil(self.editor->maxLineWidth - L.textAreaW)) : 0;
    self.editor->hScrollPos = std::clamp(self.editor->hScrollPos, 0, maxH);
    self.editor->ensureCaretVisible();
    self.editor->zoomPopupText = std::to_string((int)std::round(newFontSize)) + "px";
    self.editor->zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    [self setNeedsDisplay];
    [self setNeedsLayout];
    [self.inputDelegate selectionDidChange:self];
}
- (void)handlePan:(UIPanGestureRecognizer *)gesture {
    if (!self.editor) return;
    if (_isMouseSelecting) {
        [gesture setTranslation:CGPointZero inView:self];
        return;
    }
    if (@available(iOS 13.4, *)) {
        if (gesture.modifierFlags & UIKeyModifierCommand) {
            if (gesture.state == UIGestureRecognizerStateBegan) {
                [self stopMomentumScroll];
            } else if (gesture.state == UIGestureRecognizerStateChanged) {
                CGPoint translation = [gesture translationInView:self];
                CGFloat sensitivity = 0.5;
                CGFloat targetSize = self.editor->currentFontSize - (translation.y * sensitivity);
                [self applyNewFontSize:targetSize];
                [gesture setTranslation:CGPointZero inView:self];
            }
            return;
        }
    }
    if (gesture.state == UIGestureRecognizerStateBegan) {
        [self stopMomentumScroll];
        _vScrollAccumulator = 0.0;
        _isScrolling = YES;
        [self.inputDelegate selectionDidChange:self];
    } else if (gesture.state == UIGestureRecognizerStateChanged) {
        CGPoint translation = [gesture translationInView:self];
        CGFloat dx = 0;
        CGFloat dy = 0;
        if (gesture.numberOfTouches == 0) {
            CGFloat scrollMultiplier = -10.0;
            dx = translation.x * scrollMultiplier;
            dy = translation.y * scrollMultiplier;
        } else {
            CGFloat scrollMultiplier = 1.0;
            dx = translation.x * scrollMultiplier;
            dy = translation.y * scrollMultiplier;
        }
        [self applyScrollDeltaX:dx deltaY:dy];
        [gesture setTranslation:CGPointZero inView:self];
        [self setNeedsDisplay];
    } else if (gesture.state == UIGestureRecognizerStateEnded || gesture.state == UIGestureRecognizerStateCancelled) {
        _scrollVelocity = [gesture velocityInView:self];
        if (fabs(_scrollVelocity.x) < 10.0 && fabs(_scrollVelocity.y) < 10.0) {
            _isScrolling = NO;
            [self.inputDelegate selectionDidChange:self];
        } else {
            [self startMomentumScroll];
        }
    }
}
- (void)handlePinch:(UIPinchGestureRecognizer *)gesture {
    if (!self.editor) return;
    if (gesture.state == UIGestureRecognizerStateBegan) {
        _initialFontSize = self.editor->currentFontSize;
        [self stopMomentumScroll];
    } else if (gesture.state == UIGestureRecognizerStateChanged) {
        CGFloat targetSize = _initialFontSize * gesture.scale;
        [self applyNewFontSize:targetSize];
    }
}
- (void)applyScrollDeltaX:(CGFloat)dx deltaY:(CGFloat)dy {
    _vScrollAccumulator -= dy;
    float vw = self.bounds.size.width;
    float vh = (float)[self effectiveViewportHeight];
    if (self.editor->wordWrapEnabled) {
        self.editor->hScrollPos = 0;
        _scrollVelocity.x = 0;
        if (fabs(_vScrollAccumulator) >= self.editor->lineHeight) {
            int linesToScroll = (int)(_vScrollAccumulator / self.editor->lineHeight);
            self.editor->scrollVisualLines(linesToScroll);
            _vScrollAccumulator -= linesToScroll * self.editor->lineHeight;
        }
        if (self.editor->vScrollPos == 0 && self.editor->vScrollSubLine == 0 && _scrollVelocity.y > 0) {
            _scrollVelocity.y = 0;
        }
    } else {
        ScrollBarLayout L = self.editor->getScrollBarLayout(vw, vh);
        if (fabs(_vScrollAccumulator) >= self.editor->lineHeight) {
            int linesToScroll = (int)(_vScrollAccumulator / self.editor->lineHeight);
            self.editor->vScrollPos += linesToScroll;
            _vScrollAccumulator -= linesToScroll * self.editor->lineHeight;
        }
        self.editor->hScrollPos -= dx;
        int maxV = std::max(0, L.totalLines - L.visibleLines);
        self.editor->vScrollPos = std::clamp(self.editor->vScrollPos, 0, maxV);
        int maxH = L.needH ? std::max(0, (int)std::ceil(self.editor->maxLineWidth - L.textAreaW)) : 0;
        self.editor->hScrollPos = std::clamp(self.editor->hScrollPos, 0, maxH);
        if (self.editor->vScrollPos == 0 || self.editor->vScrollPos == maxV) {
            _scrollVelocity.y = 0;
        }
        if (self.editor->hScrollPos == 0 || self.editor->hScrollPos == maxH) {
            _scrollVelocity.x = 0;
        }
    }
    [self setNeedsDisplay];
    [self.inputDelegate selectionDidChange:self];
    [self checkCaretVisibilityAndDismissKeyboardIfNeeded];
    if (self.editor) {
        self.editor->updateScrollBars();
    }
}
- (void)startMomentumScroll {
    [self stopMomentumScroll];
    _isScrolling = YES;
    _lastUpdateTime = CACurrentMediaTime();
    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(updateMomentumScroll:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}
- (void)stopMomentumScroll {
    if (_displayLink) {
        [_displayLink invalidate];
        _displayLink = nil;
        if (_isScrolling) {
            _isScrolling = NO;
            [self.inputDelegate selectionDidChange:self];
        }
    }
}
- (void)updateMomentumScroll:(CADisplayLink *)link {
    if (!self.editor) {
        [self stopMomentumScroll];
        return;
    }
    NSTimeInterval currentTime = CACurrentMediaTime();
    NSTimeInterval dt = currentTime - _lastUpdateTime;
    _lastUpdateTime = currentTime;
    CGFloat decay = pow(0.998, dt * 1000.0);
    _scrollVelocity.x *= decay;
    _scrollVelocity.y *= decay;
    if (fabs(_scrollVelocity.x) < 10.0 && fabs(_scrollVelocity.y) < 10.0) {
        [self stopMomentumScroll];
        return;
    }
    CGFloat dx = _scrollVelocity.x * dt;
    CGFloat dy = _scrollVelocity.y * dt;
    [self applyScrollDeltaX:dx deltaY:dy];
}
- (BOOL)canBecomeFirstResponder { return YES; }
- (BOOL)hasText { return self.editor && (self.editor->pt.length() > 0 || !self.editor->imeComp.empty()); }
- (void)setMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    if (self.onSyncKeyboardLayout) {
        self.onSyncKeyboardLayout();
    }
    NSString *str = [markedText isKindOfClass:[NSAttributedString class]]
        ? [(NSAttributedString *)markedText string]
        : (NSString *)markedText;
    [self.inputDelegate textWillChange:self];
    self.editor->imeComp = (str && str.length > 0) ? [str UTF8String] : "";
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (void)setAttributedMarkedText:(NSAttributedString *)markedText selectedRange:(NSRange)selectedRange {
    [self setMarkedText:markedText.string selectedRange:selectedRange];
}
- (void)unmarkText {
    if (!self.editor) return;
    if (self.onSyncKeyboardLayout) {
        self.onSyncKeyboardLayout();
    }
    [self.inputDelegate textWillChange:self];
    if (!self.editor->imeComp.empty()) {
        self.editor->insertAtCursors(self.editor->imeComp);
        self.editor->imeComp = "";
        self.editor->ensureCaretVisible();
    }
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (void)insertText:(NSString *)text {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    if (self.onSyncKeyboardLayout) {
        self.onSyncKeyboardLayout();
    }
    int prevVScroll = self.editor->vScrollPos;
    int prevVSubLine = self.editor->vScrollSubLine;
    int prevHScroll = self.editor->hScrollPos;
    [self.inputDelegate textWillChange:self];
    if ([text isEqualToString:@"\n"] || [text isEqualToString:@"\r"]) {
        if (!self.editor->imeComp.empty()) {
            self.editor->insertAtCursors(self.editor->imeComp);
            self.editor->imeComp = "";
        }
        self.editor->insertNewlineWithAutoIndent();
    } else if ([text isEqualToString:@"\t"]) {
        if (!self.editor->imeComp.empty()) {
            self.editor->insertAtCursors(self.editor->imeComp);
            self.editor->imeComp = "";
        }
        if (!self.editor->tryExpandEmmet() && !self.editor->jumpEmmetTabstop(true)) {
            if (self.editor->cursors.size() > 1) {
                self.editor->insertAtCursors("\t");
            } else {
                self.editor->indentLines(false);
            }
        }
    } else {
        self.editor->imeComp = "";
        bool usePadding = false;
        for (const auto& c : self.editor->cursors) {
            if (c.isVirtual) {
                usePadding = true;
                break;
            }
        }
        if (usePadding) {
            self.editor->insertAtCursorsWithPadding([text UTF8String]);
        } else {
            self.editor->insertAtCursors([text UTF8String]);
        }
    }
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    if (self.editor->vScrollPos != prevVScroll ||
        self.editor->vScrollSubLine != prevVSubLine ||
        self.editor->hScrollPos != prevHScroll) {
        [self.inputDelegate selectionWillChange:self];
        [self.inputDelegate selectionDidChange:self];
    }
    [self setNeedsDisplay];
}
- (void)deleteBackward {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    if (self.onSyncKeyboardLayout) {
        self.onSyncKeyboardLayout();
    }
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->backspaceAtCursors();
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
    [self setNeedsLayout];
}
- (UITextRange *)selectedTextRange {
    if (!self.editor || self.editor->cursors.empty()) return nil;
    Cursor c = self.editor->cursors.back();
    return [iOSTextRange rangeWithStart:c.start() end:c.end()];
}
- (void)setSelectedTextRange:(UITextRange *)selectedTextRange {
    if (!self.editor) return;
    size_t start = ((iOSTextPosition*)selectedTextRange.start).index;
    size_t end = ((iOSTextPosition*)selectedTextRange.end).index;
    self.editor->cursors.clear();
    Cursor c;
    c.anchor = start;
    c.head = end;
    int li = self.editor->getLineIdx(end);
    c.desiredX = self.editor->getXInLine(li, end);
    c.originalAnchorX = c.desiredX;
    c.isVirtual = false;
    self.editor->cursors.push_back(c);
    [self setNeedsDisplay];
}
- (UITextRange *)markedTextRange {
    if (self.editor && !self.editor->imeComp.empty() && !self.editor->cursors.empty()) {
        size_t head = self.editor->cursors.back().head;
        return [iOSTextRange rangeWithStart:head end:head + self.editor->imeComp.size()];
    }
    return nil;
}
- (NSString *)textInRange:(UITextRange *)range {
    if (!self.editor || !range) return @"";
    size_t start = ((iOSTextPosition*)range.start).index;
    size_t end = ((iOSTextPosition*)range.end).index;
    if (start > end) std::swap(start, end);
    size_t len = end - start;
    if (!self.editor->imeComp.empty() && !self.editor->cursors.empty()) {
        size_t head = self.editor->cursors.back().head;
        if (start == head && len == self.editor->imeComp.size()) {
            return [NSString stringWithUTF8String:self.editor->imeComp.c_str()];
        }
    }
    size_t ptLen = self.editor->pt.length();
    if (start >= ptLen) return @"";
    if (start + len > ptLen) len = ptLen - start;
    if (len == 0) return @"";
    std::string text = self.editor->pt.getRange(start, len);
    NSString *nsStr = [NSString stringWithUTF8String:text.c_str()];
    return nsStr ? nsStr : @"";
}
- (void)replaceRange:(UITextRange *)range withText:(NSString *)text {
    if (!self.editor || !range) return;
    [self.inputDelegate textWillChange:self];
    size_t start = ((iOSTextPosition*)range.start).index;
    size_t end = ((iOSTextPosition*)range.end).index;
    if (start > end) std::swap(start, end);
    if (!self.editor->imeComp.empty() && !self.editor->cursors.empty()) {
        size_t head = self.editor->cursors.back().head;
        if (start == head && (end - start) == self.editor->imeComp.size()) {
            self.editor->imeComp = "";
            end = start;
        }
    }
    size_t ptLen = self.editor->pt.length();
    start = MIN(start, ptLen);
    end = MIN(end, ptLen);
    self.editor->cursors.clear();
    Cursor c; c.head = end; c.anchor = start;
    self.editor->cursors.push_back(c);
    self.editor->insertAtCursors([text UTF8String]);
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (UITextPosition *)beginningOfDocument { return [iOSTextPosition positionWithIndex:0]; }
- (UITextPosition *)endOfDocument { return [iOSTextPosition positionWithIndex:(self.editor ? (self.editor->pt.length() + self.editor->imeComp.size()) : 0)]; }
- (UITextRange *)textRangeFromPosition:(UITextPosition *)f toPosition:(UITextPosition *)t { return [iOSTextRange rangeWithStart:((iOSTextPosition*)f).index end:((iOSTextPosition*)t).index]; }
- (UITextPosition *)positionFromPosition:(UITextPosition *)position offset:(NSInteger)offset {
    if (!self.editor) return position;
    size_t startPos = ((iOSTextPosition*)position).index;
    if (offset == 0) return position;
    size_t newPos = self.editor->getPositionByUTF16Offset(startPos, (int)offset);
    return [iOSTextPosition positionWithIndex:newPos];
}
- (UITextPosition *)positionFromPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction offset:(NSInteger)offset {
    return position;
}
- (NSComparisonResult)comparePosition:(UITextPosition *)p1 toPosition:(UITextPosition *)p2 {
    size_t i1 = ((iOSTextPosition*)p1).index; size_t i2 = ((iOSTextPosition*)p2).index;
    if (i1 < i2) return NSOrderedAscending; if (i1 > i2) return NSOrderedDescending; return NSOrderedSame;
}
- (NSInteger)offsetFromPosition:(UITextPosition *)f toPosition:(UITextPosition *)t {
    if (!self.editor) return 0;
    size_t fromIdx = ((iOSTextPosition*)f).index;
    size_t toIdx = ((iOSTextPosition*)t).index;
    if (fromIdx == toIdx) return 0;
    size_t start = MIN(fromIdx, toIdx);
    size_t end = MAX(fromIdx, toIdx);
    if (!self.editor->imeComp.empty() && !self.editor->cursors.empty()) {
        size_t head = self.editor->cursors.back().head;
        if (start == head && (end - start) == self.editor->imeComp.size()) {
            NSString *imeStr = [NSString stringWithUTF8String:self.editor->imeComp.c_str()];
            NSInteger imeCount = (NSInteger)(imeStr ? imeStr.length : 0);
            return (fromIdx <= toIdx) ? imeCount : -imeCount;
        }
    }
    size_t charCount = self.editor->countUTF16Length(start, end - start);
    return (fromIdx <= toIdx) ? (NSInteger)charCount : -(NSInteger)charCount;
}
- (id<UITextInputTokenizer>)tokenizer { return _tokenizer; }
- (UITextPosition *)positionWithinRange:(UITextRange *)r farthestInDirection:(UITextLayoutDirection)d { return r.start; }
- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)p inDirection:(UITextLayoutDirection)d { return nil; }
- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition *)p inDirection:(UITextStorageDirection)d { return NSWritingDirectionNatural; }
- (void)setBaseWritingDirection:(NSWritingDirection)w forRange:(UITextRange *)r {}
- (CGFloat)visualBaseYForLine:(int)targetLine wrapWidth:(float)vw {
    if (!self.editor || self.editor->lineStarts.empty()) return self.topRenderMargin;
    int start = std::clamp(self.editor->vScrollPos, 0, std::max(0, (int)self.editor->lineStarts.size() - 1));
    if (!self.editor->wordWrapEnabled) {
        return (CGFloat)(targetLine - start) * self.editor->lineHeight + self.topRenderMargin;
    }
    int startSub = std::clamp(self.editor->vScrollSubLine, 0, std::max(0, self.editor->getLineVisualCount(start, vw) - 1));
    int visualRows = -startSub;
    if (targetLine >= start) {
        for (int l = start; l < targetLine; ++l) {
            visualRows += self.editor->getLineVisualCount(l, vw);
        }
    } else {
        for (int l = start - 1; l >= targetLine; --l) {
            visualRows -= self.editor->getLineVisualCount(l, vw);
        }
    }
    return (CGFloat)visualRows * self.editor->lineHeight + self.topRenderMargin;
}
- (CGRect)visualCaretRectForPos:(size_t)pos affinity:(BOOL)affinity {
    if (!self.editor) return CGRectZero;
    int li = self.editor->getLineIdx(pos);
    float physX = self.editor->getXInLine(li, pos);
    if (!self.editor->wordWrapEnabled) {
        float drawX = self.editor->gutterWidth - self.editor->hScrollPos + physX;
        float drawY = (li - self.editor->vScrollPos) * self.editor->lineHeight + self.topRenderMargin;
        return CGRectMake(drawX, drawY, 2.0, self.editor->lineHeight);
    }
    float vw = std::max(1.0f, (float)self.bounds.size.width - self.editor->gutterWidth - self.editor->visibleVScrollWidth);
    CGFloat lineBaseY = [self visualBaseYForLine:li wrapWidth:vw];
    auto subs = self.editor->getVisualSubLines(li, vw);
    size_t rel = (pos >= self.editor->lineStarts[li]) ? (pos - self.editor->lineStarts[li]) : 0;
    int subIdx = 0;
    for (int sIdx = 0; sIdx < (int)subs.size(); ++sIdx) {
        size_t se = subs[sIdx].byteStart + subs[sIdx].byteLen;
        if (rel < se || (rel == se && (affinity || sIdx == (int)subs.size() - 1))) {
            subIdx = sIdx;
            break;
        }
    }
    float drawX = self.editor->gutterWidth + (physX - self.editor->getXInLine(li, self.editor->lineStarts[li] + subs[subIdx].byteStart));
    float drawY = (float)lineBaseY + (float)subIdx * self.editor->lineHeight;
    return CGRectMake(drawX, drawY, 2.0, self.editor->lineHeight);
}
- (CGRect)firstRectForRange:(UITextRange *)range {
    if (!self.editor || !range) return CGRectZero;
    size_t start = ((iOSTextPosition*)range.start).index;
    return [self visualCaretRectForPos:start affinity:NO];
}
- (NSArray<UITextSelectionRect *> *)selectionRectsForRange:(UITextRange *)range {
    if (_isScrolling) {
        return @[];
    }
    if (!self.editor || !range) return @[];
    NSMutableArray *rects = [NSMutableArray array];
    size_t start = ((iOSTextPosition*)range.start).index;
    size_t end = ((iOSTextPosition*)range.end).index;
    if (start > end) std::swap(start, end);
    int startLine = self.editor->getLineIdx(start);
    int endLine = self.editor->getLineIdx(end);
    float vw = std::max(1.0f, (float)self.bounds.size.width - self.editor->gutterWidth - self.editor->visibleVScrollWidth);
    for (int line = startLine; line <= endLine; line++) {
        size_t lineStartIdx = self.editor->lineStarts[line];
        size_t nextLineStartIdx = (line + 1 < (int)self.editor->lineStarts.size())
                                  ? self.editor->lineStarts[line + 1]
                                  : self.editor->pt.length();
        size_t segStart = (line == startLine) ? start : lineStartIdx;
        size_t segEnd = (line == endLine) ? end : nextLineStartIdx;
        if (!self.editor->wordWrapEnabled) {
            float x1 = self.editor->getXInLine(line, segStart);
            float x2 = self.editor->getXInLine(line, segEnd);
            if (x2 < x1) x2 = x1;
            float w = x2 - x1;
            if (line != endLine && w == 0) w = self.editor->charWidth / 2;
            if (w == 0) w = 2.0;
            float drawX = self.editor->gutterWidth - self.editor->hScrollPos + x1;
            float drawY = (line - self.editor->vScrollPos) * self.editor->lineHeight + self.topRenderMargin;
            CGRect r = CGRectMake(drawX, drawY, w, self.editor->lineHeight);
            iOSSelectionRect *selRect = [[iOSSelectionRect alloc] init];
            selRect.rectValue = r;
            selRect.writingDirectionValue = NSWritingDirectionNatural;
            selRect.containsStartValue = (line == startLine && segStart == start);
            selRect.containsEndValue = (line == endLine && segEnd == end);
            selRect.isVerticalValue = NO;
            [rects addObject:selRect];
        } else {
            CGFloat lineBaseY = [self visualBaseYForLine:line wrapWidth:vw];
            auto subs = self.editor->getVisualSubLines(line, vw);
            for (int subIdx = 0; subIdx < (int)subs.size(); ++subIdx) {
                size_t subDocS = lineStartIdx + subs[subIdx].byteStart;
                size_t subDocE = subDocS + subs[subIdx].byteLen;
                size_t ovS = std::max(segStart, subDocS);
                size_t ovE = std::min(segEnd, subDocE);
                if (ovS < ovE || (segStart == segEnd && segStart == ovS && subIdx == (int)subs.size() - 1)) {
                    float subOriginX = self.editor->getXInLine(line, subDocS);
                    float x1 = self.editor->getXInLine(line, ovS) - subOriginX;
                    float x2 = self.editor->getXInLine(line, ovE) - subOriginX;
                    float w = std::max(2.0f, x2 - x1);
                    float drawX = self.editor->gutterWidth + x1;
                    float drawY = (float)lineBaseY + (float)subIdx * self.editor->lineHeight;
                    iOSSelectionRect *selRect = [[iOSSelectionRect alloc] init];
                    selRect.rectValue = CGRectMake(drawX, drawY, w, self.editor->lineHeight);
                    selRect.writingDirectionValue = NSWritingDirectionNatural;
                    selRect.containsStartValue = (line == startLine && ovS == start);
                    selRect.containsEndValue = (line == endLine && ovE == end);
                    selRect.isVerticalValue = NO;
                    [rects addObject:selRect];
                }
            }
        }
    }
    return rects;
}
- (UITextPosition *)closestPositionToPoint:(CGPoint)point {
    if (!self.editor) return [self beginningOfDocument];
    CGPoint p = point;
    p.y -= self.topRenderMargin;
    size_t idx = self.editor->getDocPosFromPoint(p.x, p.y);
    return [iOSTextPosition positionWithIndex:idx];
}
- (UITextPosition *)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange *)range { return [self.beginningOfDocument copy]; }
- (UITextRange *)characterRangeAtPoint:(CGPoint)point { return nil; }
- (CGRect)caretRectForPosition:(UITextPosition *)position {
    if (_isScrolling) {
        return CGRectZero;
    }
    if (!self.editor) return CGRectZero;
    size_t p = ((iOSTextPosition*)position).index;
    BOOL affinity = (!self.editor->cursors.empty() && self.editor->cursors.back().head == p) ? self.editor->cursors.back().affinity : NO;
    CGRect cr = [self visualCaretRectForPos:p affinity:affinity];
    return CGRectMake(cr.origin.x, cr.origin.y + self.editor->lineHeight / 2.0, 0, 0);
}
- (UITextAutocorrectionType)autocorrectionType { return UITextAutocorrectionTypeNo; }
- (UITextSpellCheckingType)spellCheckingType {
    return UITextSpellCheckingTypeNo;
}
- (UITextAutocapitalizationType)autocapitalizationType { return UITextAutocapitalizationTypeNone; }
- (UITextSmartQuotesType)smartQuotesType { return UITextSmartQuotesTypeNo; }
- (UITextSmartDashesType)smartDashesType { return UITextSmartDashesTypeNo; }
- (NSUndoManager *)undoManager {
    return nil;
}
#pragma mark - Dictation Support (音声入力のバグ回避・強制リセット)
- (void)insertDictationResult:(NSArray<UIDictationPhrase *> *)dictationResult {
    NSMutableString *resultString = [NSMutableString string];
    for (UIDictationPhrase *phrase in dictationResult) {
        [resultString appendString:phrase.text];
    }
    if (self.editor && !self.editor->imeComp.empty()) {
        [self unmarkText];
    }
    [self insertText:resultString];
}
- (id)insertDictationResultPlaceholder {
    return [[NSObject alloc] init];
}
- (CGRect)frameForDictationResultPlaceholder:(id)placeholder {
    if (!self.editor || self.editor->cursors.empty()) return CGRectZero;
    Cursor c = self.editor->cursors.back();
    return [self caretRectForPosition:[iOSTextPosition positionWithIndex:c.head]];
}
- (void)removeDictationResultPlaceholder:(id)placeholder willInsertResult:(BOOL)willInsertResult {
}
- (void)dictationRecordingDidEnd {
    [self.inputDelegate selectionDidChange:self];
}
- (void)dictationRecognitionFailed {
    [self.inputDelegate selectionDidChange:self];
}
- (NSArray<UIKeyCommand *> *)keyCommands {
    NSMutableArray<UIKeyCommand *> *commands = [NSMutableArray arrayWithArray:@[
        [UIKeyCommand keyCommandWithInput:@"a" modifierFlags:UIKeyModifierCommand action:@selector(selectAll:)],
        [UIKeyCommand keyCommandWithInput:@"c" modifierFlags:UIKeyModifierCommand action:@selector(copy:)],
        [UIKeyCommand keyCommandWithInput:@"x" modifierFlags:UIKeyModifierCommand action:@selector(cut:)],
        [UIKeyCommand keyCommandWithInput:@"v" modifierFlags:UIKeyModifierCommand action:@selector(paste:)],
        [UIKeyCommand keyCommandWithInput:@"z" modifierFlags:UIKeyModifierCommand action:@selector(handleUndo:)],
        [UIKeyCommand keyCommandWithInput:@"z" modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(handleRedo:)],
        [UIKeyCommand keyCommandWithInput:@"y" modifierFlags:UIKeyModifierCommand action:@selector(handleRedo:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:0 action:@selector(moveLeft:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:0 action:@selector(moveRight:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:0 action:@selector(moveUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:0 action:@selector(moveDown:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierShift action:@selector(moveLeftAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierShift action:@selector(moveRightAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierShift action:@selector(moveUpAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierShift action:@selector(moveDownAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierAlternate action:@selector(moveWordLeft:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierAlternate action:@selector(moveWordRight:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierAlternate | UIKeyModifierShift action:@selector(rectSelectLeft:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierAlternate | UIKeyModifierShift action:@selector(rectSelectRight:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierControl | UIKeyModifierAlternate | UIKeyModifierShift action:@selector(rectSelectLeft:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierControl | UIKeyModifierAlternate | UIKeyModifierShift action:@selector(rectSelectRight:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierControl | UIKeyModifierAlternate | UIKeyModifierShift action:@selector(rectSelectUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierControl | UIKeyModifierAlternate | UIKeyModifierShift action:@selector(rectSelectDown:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierAlternate action:@selector(moveLinesUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierAlternate action:@selector(moveLinesDown:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierAlternate | UIKeyModifierShift action:@selector(duplicateLinesUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierAlternate | UIKeyModifierShift action:@selector(duplicateLinesDown:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierControl | UIKeyModifierAlternate action:@selector(addCursorUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierControl | UIKeyModifierAlternate action:@selector(addCursorDown:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate action:@selector(addCursorUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate action:@selector(addCursorDown:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputHome modifierFlags:0 action:@selector(moveToHome:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputEnd modifierFlags:0 action:@selector(moveToEnd:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputHome modifierFlags:UIKeyModifierShift action:@selector(moveToHomeAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputEnd modifierFlags:UIKeyModifierShift action:@selector(moveToEndAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputPageUp modifierFlags:0 action:@selector(pageUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputPageDown modifierFlags:0 action:@selector(pageDown:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputPageUp modifierFlags:UIKeyModifierShift action:@selector(pageUpAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputPageDown modifierFlags:UIKeyModifierShift action:@selector(pageDownAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierCommand action:@selector(moveToHome:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierCommand action:@selector(moveToEnd:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(moveToHomeAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(moveToEndAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierCommand action:@selector(moveToDocStart:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierCommand action:@selector(moveToDocEnd:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(moveToDocStartAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(moveToDocEndAndSelect:)],
        [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:0 action:@selector(handleTabKey:)],
        [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:UIKeyModifierShift action:@selector(handleShiftTabKey:)],
        [UIKeyCommand keyCommandWithInput:@"/" modifierFlags:UIKeyModifierCommand action:@selector(toggleCommentAction:)],
        [UIKeyCommand keyCommandWithInput:@"z" modifierFlags:UIKeyModifierAlternate action:@selector(toggleWordWrapAction:)],
        [UIKeyCommand keyCommandWithInput:@"p" modifierFlags:UIKeyModifierAlternate action:@selector(togglePreviewAction:)],
        [UIKeyCommand keyCommandWithInput:@"b" modifierFlags:UIKeyModifierCommand action:@selector(jumpToBracket:)],
        [UIKeyCommand keyCommandWithInput:@"b" modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(selectToBracket:)],
        [UIKeyCommand keyCommandWithInput:@"u" modifierFlags:UIKeyModifierCommand action:@selector(uppercaseSelection:)],
        [UIKeyCommand keyCommandWithInput:@"u" modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(lowercaseSelection:)],
        [UIKeyCommand keyCommandWithInput:@"k" modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(deleteLineAction:)],
        [UIKeyCommand keyCommandWithInput:@"]" modifierFlags:UIKeyModifierCommand action:@selector(indentLinesAction:)],
        [UIKeyCommand keyCommandWithInput:@"[" modifierFlags:UIKeyModifierCommand action:@selector(unindentLinesAction:)],
        [UIKeyCommand keyCommandWithInput:@"n" modifierFlags:UIKeyModifierCommand action:@selector(newDocument:)],
        [UIKeyCommand keyCommandWithInput:@"o" modifierFlags:UIKeyModifierCommand action:@selector(openDocument:)],
        [UIKeyCommand keyCommandWithInput:@"s" modifierFlags:UIKeyModifierCommand action:@selector(saveDocument:)],
        [UIKeyCommand keyCommandWithInput:@"s" modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(saveDocumentAs:)],
        [UIKeyCommand keyCommandWithInput:@"f" modifierFlags:UIKeyModifierCommand action:@selector(cmdFind:)],
        [UIKeyCommand keyCommandWithInput:@"r" modifierFlags:UIKeyModifierCommand action:@selector(cmdReplace:)],
        [UIKeyCommand keyCommandWithInput:@"h" modifierFlags:UIKeyModifierCommand action:@selector(cmdReplace:)],
        [UIKeyCommand keyCommandWithInput:@"f" modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate action:@selector(cmdReplace:)],
        [UIKeyCommand keyCommandWithInput:@"j" modifierFlags:UIKeyModifierCommand action:@selector(cmdGoToLine:)],
        [UIKeyCommand keyCommandWithInput:@"g" modifierFlags:UIKeyModifierCommand action:@selector(cmdGoToLine:)],
        [UIKeyCommand keyCommandWithInput:@"l" modifierFlags:UIKeyModifierCommand action:@selector(cmdGoToLine:)],
        [UIKeyCommand keyCommandWithInput:@"d" modifierFlags:UIKeyModifierCommand action:@selector(selectWordOrNextOccurrence:)],
        [UIKeyCommand keyCommandWithInput:@"+" modifierFlags:UIKeyModifierCommand action:@selector(zoomIn:)],
        [UIKeyCommand keyCommandWithInput:@"=" modifierFlags:UIKeyModifierCommand action:@selector(zoomIn:)],
        [UIKeyCommand keyCommandWithInput:@"-" modifierFlags:UIKeyModifierCommand action:@selector(zoomOut:)],
        [UIKeyCommand keyCommandWithInput:@"0" modifierFlags:UIKeyModifierCommand action:@selector(resetZoom:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputF1 modifierFlags:0 action:@selector(toggleHelp:)]
    ]];
    if (@available(iOS 13.4, *)) {
        [commands addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputF3 modifierFlags:0 action:@selector(cmdFindNext:)]];
        [commands addObject:[UIKeyCommand keyCommandWithInput:UIKeyInputF3 modifierFlags:UIKeyModifierShift action:@selector(cmdFindPrev:)]];
    }
    if (@available(iOS 15.0, *)) {
        for (UIKeyCommand *cmd in commands) {
            cmd.wantsPriorityOverSystemBehavior = YES;
        }
    }
    return commands;
}
- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
    if (action == @selector(newDocument:) || action == @selector(openDocument:) ||
        action == @selector(openRecentDocument:) ||
        action == @selector(saveDocument:) || action == @selector(saveDocumentAs:) ||
        action == @selector(cmdFind:) || action == @selector(cmdReplace:) ||
        action == @selector(cmdFindNext:) || action == @selector(cmdFindPrev:) ||
        action == @selector(cmdGoToLine:) || action == @selector(selectWordOrNextOccurrence:) ||
        action == @selector(handleUndo:) || action == @selector(handleRedo:) ||
        action == @selector(handleTabKey:) || action == @selector(handleShiftTabKey:) ||
        action == @selector(toggleCommentAction:) || action == @selector(toggleWordWrapAction:) ||
        action == @selector(togglePreviewAction:) || action == @selector(jumpToBracket:) ||
        action == @selector(selectToBracket:) || action == @selector(uppercaseSelection:) ||
        action == @selector(lowercaseSelection:) || action == @selector(deleteLineAction:) ||
        action == @selector(indentLinesAction:) || action == @selector(unindentLinesAction:) ||
        action == @selector(moveLinesUp:) || action == @selector(moveLinesDown:) ||
        action == @selector(duplicateLinesUp:) || action == @selector(duplicateLinesDown:) ||
        action == @selector(addCursorUp:) || action == @selector(addCursorDown:) ||
        action == @selector(rectSelectLeft:) || action == @selector(rectSelectRight:) ||
        action == @selector(rectSelectUp:) || action == @selector(rectSelectDown:) ||
        action == @selector(moveWordLeft:) || action == @selector(moveWordRight:) ||
        action == @selector(pageUp:) || action == @selector(pageDown:) ||
        action == @selector(pageUpAndSelect:) || action == @selector(pageDownAndSelect:) ||
        action == @selector(zoomIn:) || action == @selector(zoomOut:) ||
        action == @selector(resetZoom:) || action == @selector(toggleHelp:)) {
        return YES;
    }
    if (action == @selector(copy:) || action == @selector(cut:)) {
        return self.editor && !self.editor->cursors.empty();
    }
    if (action == @selector(paste:)) {
        return [UIPasteboard generalPasteboard].hasStrings;
    }
    if (action == @selector(selectAll:)) {
        return self.editor && self.editor->pt.length() > 0;
    }
    if (action == @selector(moveLeft:) || action == @selector(moveRight:) ||
        action == @selector(moveUp:) || action == @selector(moveDown:) ||
        action == @selector(moveLeftAndSelect:) || action == @selector(moveRightAndSelect:) ||
        action == @selector(moveUpAndSelect:) || action == @selector(moveDownAndSelect:) ||
        action == @selector(moveToHome:) || action == @selector(moveToEnd:) ||
        action == @selector(moveToHomeAndSelect:) || action == @selector(moveToEndAndSelect:) ||
        action == @selector(moveToDocStart:) || action == @selector(moveToDocEnd:) ||
        action == @selector(moveToDocStartAndSelect:) || action == @selector(moveToDocEndAndSelect:)) {
        return YES;
    }
    return [super canPerformAction:action withSender:sender];
}
- (void)newDocument:(id)sender {
    [self hideHelpIfNeeded];
    [self hideEditMenuIfNeeded];
    if (self.onNewDocumentAction) {
        self.onNewDocumentAction();
    }
}
- (void)openRecentDocument:(id)sender {
    [self hideHelpIfNeeded];
    [self hideEditMenuIfNeeded];
    if (self.onOpenRecentAction) {
        UIView *sourceView = [sender isKindOfClass:[UIView class]] ? (UIView *)sender : nil;
        self.onOpenRecentAction(sourceView);
    }
}
- (void)selectAll:(id)sender {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->selectAll();
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)handleTabKey:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    if (self.onSyncKeyboardLayout) {
        self.onSyncKeyboardLayout();
    }
    if (!self.editor->imeComp.empty()) {
        [self unmarkText];
    }
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    if (!self.editor->tryExpandEmmet() && !self.editor->jumpEmmetTabstop(true)) {
        if (self.editor->cursors.size() > 1) {
            self.editor->insertAtCursors("\t");
        } else {
            self.editor->indentLines(false);
        }
    }
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)handleShiftTabKey:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    if (self.onSyncKeyboardLayout) {
        self.onSyncKeyboardLayout();
    }
    if (!self.editor->imeComp.empty()) {
        [self unmarkText];
    }
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    if (!self.editor->jumpEmmetTabstop(false)) {
        self.editor->unindentLines();
    }
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)toggleCommentAction:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->toggleComment();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)toggleWordWrapAction:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    self.editor->toggleWordWrap();
    [self setNeedsDisplay];
}
- (void)togglePreviewAction:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    self.editor->togglePreview();
    [self setNeedsDisplay];
}
- (void)jumpToBracket:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->jumpToMatchingBracket(false);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)selectToBracket:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->jumpToMatchingBracket(true);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)uppercaseSelection:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->convertSelectedText(true);
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)lowercaseSelection:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->convertSelectedText(false);
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)deleteLineAction:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->deleteLine();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)indentLinesAction:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->indentLines(true);
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)unindentLinesAction:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->unindentLines();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveLinesUp:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->moveLines(true);
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveLinesDown:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->moveLines(false);
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)duplicateLinesUp:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->duplicateLines(true);
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)duplicateLinesDown:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->duplicateLines(false);
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)addCursorUp:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->addCursor(true);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)addCursorDown:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->addCursor(false);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)rectSelectLeft:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->moveRectSelection(0, -1);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)rectSelectRight:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->moveRectSelection(0, 1);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)rectSelectUp:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->moveRectSelection(-1, 0);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)rectSelectDown:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->moveRectSelection(1, 0);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveWordLeft:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->rollbackPadding();
    self.editor->isRectSelecting = false;
    for (auto& c : self.editor->cursors) {
        size_t p = c.head;
        int li = self.editor->getLineIdx(p);
        size_t lineStart = self.editor->lineStarts[li];
        if (p == lineStart && p > 0) p--;
        else {
            while (p > lineStart && !self.editor->isWordChar(self.editor->pt.charAt(p - 1))) p--;
            while (p > lineStart && self.editor->isWordChar(self.editor->pt.charAt(p - 1))) p--;
        }
        c.head = p;
        c.anchor = p;
        c.desiredX = self.editor->getXFromPos(p);
        c.originalAnchorX = c.desiredX;
        c.isVirtual = false;
        c.affinity = false;
    }
    self.editor->mergeCursors();
    self.editor->ensureCaretVisible();
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveWordRight:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->rollbackPadding();
    self.editor->isRectSelecting = false;
    size_t len = self.editor->pt.length();
    for (auto& c : self.editor->cursors) {
        size_t p = c.head;
        int li = self.editor->getLineIdx(p);
        size_t lineStart = self.editor->lineStarts[li];
        size_t lineEnd = (li + 1 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[li + 1] : len;
        size_t physEnd = lineEnd;
        if (physEnd > lineStart && self.editor->pt.charAt(physEnd - 1) == '\n') physEnd--;
        if (physEnd > lineStart && self.editor->pt.charAt(physEnd - 1) == '\r') physEnd--;
        if (p == physEnd && p < len) p = lineEnd;
        else {
            while (p < physEnd && self.editor->isWordChar(self.editor->pt.charAt(p))) p++;
            while (p < physEnd && !self.editor->isWordChar(self.editor->pt.charAt(p))) p++;
        }
        c.head = p;
        c.anchor = p;
        c.desiredX = self.editor->getXFromPos(p);
        c.originalAnchorX = c.desiredX;
        c.isVirtual = false;
        c.affinity = false;
    }
    self.editor->mergeCursors();
    self.editor->ensureCaretVisible();
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)doPageMove:(BOOL)up keepSelection:(BOOL)keep {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    float viewHeight = (float)[self effectiveViewportHeight];
    int pageLines = std::max(1, (int)(viewHeight / self.editor->lineHeight) - 1);
    if (self.editor->wordWrapEnabled) {
        for (int i = 0; i < pageLines; ++i) {
            self.editor->moveVisualUpDown(up, keep);
        }
    } else {
        int totalLines = (int)self.editor->lineStarts.size();
        for (auto& c : self.editor->cursors) {
            int currentLine = self.editor->getLineIdx(c.head);
            int newLine = up ? std::max(0, currentLine - pageLines) : std::min(totalLines - 1, currentLine + pageLines);
            if (!up && newLine == totalLines - 1 && currentLine == totalLines - 1) c.head = self.editor->pt.length();
            else c.head = self.editor->getPosFromLineAndX(newLine, c.desiredX);
            if (!keep) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
        }
        if (up) self.editor->vScrollPos = std::max(0, self.editor->vScrollPos - pageLines);
        else self.editor->vScrollPos = std::min(totalLines - 1, self.editor->vScrollPos + pageLines);
        self.editor->ensureCaretVisible();
    }
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)pageUp:(id)sender { [self doPageMove:YES keepSelection:NO]; }
- (void)pageDown:(id)sender { [self doPageMove:NO keepSelection:NO]; }
- (void)pageUpAndSelect:(id)sender { [self doPageMove:YES keepSelection:YES]; }
- (void)pageDownAndSelect:(id)sender { [self doPageMove:NO keepSelection:YES]; }
- (void)moveLeft:(id)sender { [self doMoveLeft:NO]; }
- (void)moveLeftAndSelect:(id)sender { [self doMoveLeft:YES]; }
- (void)doMoveLeft:(BOOL)keep {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->rollbackPadding();
    self.editor->isRectSelecting = false;
    for (auto& c : self.editor->cursors) {
        if (c.hasSelection() && !keep) {
            c.head = c.start();
            c.anchor = c.head;
        } else {
            c.head = self.editor->moveCaretVisual(c.head, false);
            if (!keep) c.anchor = c.head;
        }
        c.desiredX = self.editor->getXFromPos(c.head);
        if (!keep) c.originalAnchorX = c.desiredX;
        c.isVirtual = false;
        c.affinity = false;
    }
    self.editor->mergeCursors();
    self.editor->ensureCaretVisible();
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveRight:(id)sender { [self doMoveRight:NO]; }
- (void)moveRightAndSelect:(id)sender { [self doMoveRight:YES]; }
- (void)doMoveRight:(BOOL)keep {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->rollbackPadding();
    self.editor->isRectSelecting = false;
    float wrapW = self.editor->getEffectiveTextWidth((float)self.bounds.size.width);
    for (auto& c : self.editor->cursors) {
        if (c.hasSelection() && !keep) {
            c.head = c.end();
            c.anchor = c.head;
            c.affinity = false;
        } else {
            size_t prev = c.head;
            size_t nextPos = self.editor->moveCaretVisual(prev, true);
            int nextLi = self.editor->getLineIdx(nextPos);
            c.affinity = (nextPos > prev) && self.editor->isVisualLineWrapBoundary(nextLi, nextPos, wrapW);
            c.head = nextPos;
            if (!keep) c.anchor = c.head;
        }
        c.desiredX = self.editor->getXFromPos(c.head);
        if (!keep) c.originalAnchorX = c.desiredX;
        c.isVirtual = false;
    }
    self.editor->mergeCursors();
    self.editor->ensureCaretVisible();
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveUp:(id)sender { [self doMoveUp:NO]; }
- (void)moveUpAndSelect:(id)sender { [self doMoveUp:YES]; }
- (void)doMoveUp:(BOOL)keep {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    if (self.editor->wordWrapEnabled) {
        self.editor->moveVisualUpDown(true, keep);
    } else {
        self.editor->rollbackPadding();
        self.editor->isRectSelecting = false;
        for (auto& c : self.editor->cursors) {
            int l = self.editor->getLineIdx(c.head);
            if (l > 0) {
                size_t nextPos = self.editor->getPosFromLineAndX(l - 1, c.desiredX);
                size_t prevLineStart = self.editor->lineStarts[l - 1];
                size_t prevLineEnd = self.editor->lineStarts[l];
                if (prevLineEnd > prevLineStart && self.editor->pt.charAt(prevLineEnd - 1) == '\n') prevLineEnd--;
                if (prevLineEnd > prevLineStart && self.editor->pt.charAt(prevLineEnd - 1) == '\r') prevLineEnd--;
                c.head = std::min(nextPos, prevLineEnd);
            } else {
                c.head = 0;
            }
            if (!keep) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
        }
        self.editor->mergeCursors();
        self.editor->ensureCaretVisible();
    }
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveDown:(id)sender { [self doMoveDown:NO]; }
- (void)moveDownAndSelect:(id)sender { [self doMoveDown:YES]; }
- (void)doMoveDown:(BOOL)keep {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    if (self.editor->wordWrapEnabled) {
        self.editor->moveVisualUpDown(false, keep);
    } else {
        self.editor->rollbackPadding();
        self.editor->isRectSelecting = false;
        for (auto& c : self.editor->cursors) {
            int l = self.editor->getLineIdx(c.head);
            if (l + 1 < (int)self.editor->lineStarts.size()) {
                size_t nextPos = self.editor->getPosFromLineAndX(l + 1, c.desiredX);
                size_t nextLineStart = self.editor->lineStarts[l + 1];
                size_t nextLineEnd = (l + 2 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[l + 2] : self.editor->pt.length();
                if (nextLineEnd > nextLineStart && self.editor->pt.charAt(nextLineEnd - 1) == '\n') nextLineEnd--;
                if (nextLineEnd > nextLineStart && self.editor->pt.charAt(nextLineEnd - 1) == '\r') nextLineEnd--;
                c.head = std::min(nextPos, nextLineEnd);
            } else {
                c.head = self.editor->pt.length();
            }
            if (!keep) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
        }
        self.editor->mergeCursors();
        self.editor->ensureCaretVisible();
    }
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveToHome:(id)sender { [self doMoveHome:NO]; }
- (void)moveToHomeAndSelect:(id)sender { [self doMoveHome:YES]; }
- (void)doMoveHome:(BOOL)keep {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    if (self.editor->wordWrapEnabled) {
        self.editor->moveVisualHome(keep);
    } else {
        self.editor->rollbackPadding();
        self.editor->isRectSelecting = false;
        for (auto& c : self.editor->cursors) {
            int li = self.editor->getLineIdx(c.head);
            c.head = self.editor->lineStarts[li];
            c.desiredX = 0.0f;
            if (!keep) { c.anchor = c.head; c.originalAnchorX = 0.0f; }
            c.isVirtual = false;
            c.affinity = false;
        }
        self.editor->mergeCursors();
        self.editor->ensureCaretVisible();
    }
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveToEnd:(id)sender { [self doMoveEnd:NO]; }
- (void)moveToEndAndSelect:(id)sender { [self doMoveEnd:YES]; }
- (void)doMoveEnd:(BOOL)keep {
    [self hideHelpIfNeeded];
    if (!self.editor || self.editor->cursors.empty()) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    if (self.editor->wordWrapEnabled) {
        self.editor->moveVisualEnd(keep);
    } else {
        self.editor->rollbackPadding();
        self.editor->isRectSelecting = false;
        for (auto& c : self.editor->cursors) {
            int li = self.editor->getLineIdx(c.head);
            size_t lineStart = self.editor->lineStarts[li];
            size_t nextLineStart = (li + 1 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[li + 1] : self.editor->pt.length();
            if (nextLineStart > lineStart && self.editor->pt.charAt(nextLineStart - 1) == '\n') nextLineStart--;
            if (nextLineStart > lineStart && self.editor->pt.charAt(nextLineStart - 1) == '\r') nextLineStart--;
            c.head = nextLineStart;
            c.desiredX = self.editor->getXFromPos(c.head);
            if (!keep) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
            c.affinity = false;
        }
        self.editor->mergeCursors();
        self.editor->ensureCaretVisible();
    }
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveToDocStart:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->jumpToFileEdge(true, false);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveToDocStartAndSelect:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->jumpToFileEdge(true, true);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveToDocEnd:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->jumpToFileEdge(false, false);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveToDocEndAndSelect:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    self.editor->jumpToFileEdge(false, true);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)selectWordOrNextOccurrence:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self.inputDelegate selectionWillChange:self];
    self.editor->selectNextOccurrence();
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)openDocument:(id)sender {
    [self hideHelpIfNeeded];
    if (self.editor) self.editor->openFile();
}
- (void)saveDocument:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    if (self.editor->currentFilePath.empty()) {
        self.editor->saveFileAs();
    } else {
        self.editor->saveFile(self.editor->currentFilePath);
    }
}
- (void)saveDocumentAs:(id)sender {
    [self hideHelpIfNeeded];
    if (self.editor) self.editor->saveFileAs();
}
- (void)copy:(id)sender {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    self.editor->copyToClipboard();
}
- (void)cut:(id)sender {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->cutToClipboard();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)paste:(id)sender {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->pasteFromClipboard();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)handleUndo:(id)sender {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->performUndo();
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)handleRedo:(id)sender {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->performRedo();
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)zoomIn:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self applyNewFontSize:self.editor->currentFontSize + 2.0f];
}
- (void)zoomOut:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self applyNewFontSize:self.editor->currentFontSize - 2.0f];
}
- (void)resetZoom:(id)sender {
    [self hideHelpIfNeeded];
    if (!self.editor) return;
    [self applyNewFontSize:14.0f];
}
- (void)toggleHelp:(id)sender {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    self.editor->showHelpPopup = !self.editor->showHelpPopup;
    if (self.editor->showHelpPopup) {
        [self dismissKeyboard];
        [self.window endEditing:YES];
    }
    [self setNeedsDisplay];
}
- (UIView *)inputAccessoryView {
    if (@available(iOS 14.0, *)) {
        if ([GCKeyboard coalescedKeyboard] != nil) {
            return nil;
        }
    }
    if (_customAccessoryView) return _customAccessoryView;
    CGFloat barHeight = 36.0;
    _customAccessoryView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, barHeight)];
    _customAccessoryView.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    UIBlurEffect *blurEffect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterial];
    UIVisualEffectView *effectView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    effectView.frame = _customAccessoryView.bounds;
    effectView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [_customAccessoryView addSubview:effectView];
    UIScrollView *scrollView = [[UIScrollView alloc] initWithFrame:_customAccessoryView.bounds];
    scrollView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    scrollView.showsHorizontalScrollIndicator = NO;
    scrollView.alwaysBounceHorizontal = YES;
    scrollView.scrollsToTop = NO;
    [_customAccessoryView addSubview:scrollView];
    UIStackView *stackView = [[UIStackView alloc] init];
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.spacing = 1.0;
    stackView.alignment = UIStackViewAlignmentFill;
    stackView.distribution = UIStackViewDistributionFill;
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [scrollView addSubview:stackView];
    [NSLayoutConstraint activateConstraints:@[
        [stackView.topAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor],
        [stackView.bottomAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor],
        [stackView.leadingAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.leadingAnchor],
        [stackView.trailingAnchor constraintEqualToAnchor:scrollView.contentLayoutGuide.trailingAnchor],
        [stackView.heightAnchor constraintEqualToAnchor:scrollView.frameLayoutGuide.heightAnchor]
    ]];
    _accessoryTapGR = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onAccessoryBackgroundTap:)];
    _accessoryTapGR.cancelsTouchesInView = NO;
    _accessoryTapGR.delegate = self;
    [_customAccessoryView addGestureRecognizer:_accessoryTapGR];
    __weak typeof(self) weakSelf = self;
    auto createBtn = ^UIButton*(NSString *title, SEL action) {
        UIButton *btn = [UIButton buttonWithType:UIButtonTypeCustom];
        [btn setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        [btn setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        if (@available(iOS 15.0, *)) {
            UIButtonConfiguration *config = [UIButtonConfiguration plainButtonConfiguration];
            NSDictionary *attributes = @{NSFontAttributeName: [UIFont systemFontOfSize:13 weight:UIFontWeightMedium]};
            config.attributedTitle = [[NSAttributedString alloc] initWithString:title attributes:attributes];
            config.baseForegroundColor = [UIColor labelColor];
            config.background.cornerRadius = 0;
            config.contentInsets = NSDirectionalEdgeInsetsMake(0, 10, 0, 10);
            btn.configuration = config;
            btn.configurationUpdateHandler = ^(UIButton *updatedBtn) {
                UIButtonConfiguration *updatedConfig = updatedBtn.configuration;
                if (updatedBtn.isHighlighted) {
                    updatedConfig.background.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
                        return trait.userInterfaceStyle == UIUserInterfaceStyleDark ? [UIColor colorWithWhite:1.0 alpha:0.3] : [UIColor colorWithWhite:0.0 alpha:0.15];
                    }];
                } else {
                    updatedConfig.background.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
                        return trait.userInterfaceStyle == UIUserInterfaceStyleDark ? [UIColor colorWithWhite:1.0 alpha:0.1] : [UIColor colorWithWhite:0.0 alpha:0.05];
                    }];
                }
                updatedBtn.configuration = updatedConfig;
            };
        } else {
            [btn setTitle:title forState:UIControlStateNormal];
            [btn setTitleColor:[UIColor labelColor] forState:UIControlStateNormal];
            btn.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightMedium];
            btn.backgroundColor = [UIColor colorWithWhite:0.5 alpha:0.2];
            btn.layer.cornerRadius = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            btn.contentEdgeInsets = UIEdgeInsetsMake(0, 10, 0, 10);
            btn.showsTouchWhenHighlighted = YES;
#pragma clang diagnostic pop
        }
        [btn addAction:[UIAction actionWithHandler:^(__kindof UIAction * _Nonnull uiAction) {
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) return;
            if (action == @selector(openRecentDocument:)) {
                if (strongSelf.onDismissOpenRecentIfVisible && strongSelf.onDismissOpenRecentIfVisible()) {
                    return;
                }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
                [strongSelf performSelector:action withObject:uiAction.sender];
#pragma clang diagnostic pop
                return;
            }
            id senderObj = uiAction.sender;
            void (^fireAction)(void) = ^{
                __strong typeof(weakSelf) innerSelf = weakSelf;
                if (!innerSelf) return;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
                [innerSelf performSelector:action withObject:senderObj];
#pragma clang diagnostic pop
            };
            if (strongSelf.onDismissOpenRecentThenPerform) {
                strongSelf.onDismissOpenRecentThenPerform(fireAction);
            } else {
                fireAction();
            }
        }] forControlEvents:UIControlEventTouchUpInside];
        return btn;
    };
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"New", nil), @selector(newDocument:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Open", nil), @selector(openDocument:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Open Recent", nil), @selector(openRecentDocument:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Save", nil), @selector(saveDocument:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Save As", nil), @selector(saveDocumentAs:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Find", nil), @selector(cmdFind:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Replace", nil), @selector(cmdReplace:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Live Preview", nil), @selector(togglePreviewAction:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Word Wrap", nil), @selector(toggleWordWrapAction:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Toggle Comment", nil), @selector(toggleCommentAction:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Tab", nil), @selector(handleTabKey:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Top", nil), @selector(moveToDocStart:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Bottom", nil), @selector(moveToDocEnd:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Go To", nil), @selector(cmdGoToLine:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Undo", nil), @selector(handleUndo:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Redo", nil), @selector(handleRedo:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Select All", nil), @selector(selectAll:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Cut", nil), @selector(cut:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Copy", nil), @selector(copy:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Paste", nil), @selector(paste:))];
    [stackView addArrangedSubview:createBtn(@"?", @selector(toggleHelp:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Done", nil), @selector(dismissKeyboard))];
    return _customAccessoryView;
}
- (void)dismissKeyboard {
    [self resignFirstResponder];
}
- (void)checkCaretVisibilityAndDismissKeyboardIfNeeded {
    if (!self.isFirstResponder) return;
    if (!self.editor || self.editor->cursors.empty()) return;
    Cursor c = self.editor->cursors.back();
    CGRect caretRect = [self visualCaretRectForPos:c.head affinity:c.affinity];
    CGFloat visHeight = MAX(1.0, [self unobstructedBottomY] - self.topRenderMargin);
    CGRect visibleRect = CGRectMake(0, self.topRenderMargin, self.bounds.size.width, visHeight);
    if (!CGRectIntersectsRect(visibleRect, caretRect)) {
        [self dismissKeyboard];
    }
}
- (void)cmdFind:(id)sender {
    [self hideHelpIfNeeded];
    [self hideEditMenuIfNeeded];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"miuShowSearch" object:nil];
}
- (void)cmdReplace:(id)sender {
    [self hideHelpIfNeeded];
    [self hideEditMenuIfNeeded];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"miuShowReplace" object:nil];
}
- (void)cmdFindNext:(id)sender {
    [self hideHelpIfNeeded];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"miuFindNext" object:nil];
}
- (void)cmdFindPrev:(id)sender {
    [self hideHelpIfNeeded];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"miuFindPrev" object:nil];
}
- (void)cmdGoToLine:(id)sender {
    [self hideHelpIfNeeded];
    if (self.onGoToLineAction) {
        self.onGoToLineAction();
    }
}
- (void)jumpToLine:(NSInteger)lineNumber {
    if (!self.editor) return;
    [self hideEditMenuIfNeeded];
    if (lineNumber < 1) lineNumber = 1;
    size_t totalLines = self.editor->lineStarts.size();
    if (lineNumber > totalLines) lineNumber = totalLines;
    NSInteger lineIndex = lineNumber - 1;
    size_t newPos = self.editor->lineStarts[lineIndex];
    [self.inputDelegate selectionWillChange:self];
    self.editor->cursors.clear();
    Cursor c;
    c.head = newPos;
    c.anchor = newPos;
    c.desiredX = 0;
    c.originalAnchorX = 0;
    c.isVirtual = false;
    self.editor->cursors.push_back(c);
    [self.inputDelegate selectionDidChange:self];
    self.editor->ensureCaretVisible();
    [self setNeedsDisplay];
    [self setNeedsLayout];
}
#pragma mark - UIEditMenuInteractionDelegate
- (CGRect)editMenuInteraction:(UIEditMenuInteraction *)interaction targetRectForConfiguration:(UIEditMenuConfiguration *)configuration API_AVAILABLE(ios(16.0)) {
    if (!self.editor || self.editor->cursors.empty()) return CGRectZero;
    Cursor c = self.editor->cursors.back();
    if (c.hasSelection()) {
        NSArray *rects = [self selectionRectsForRange:self.selectedTextRange];
        if (rects.count > 0) return [rects.firstObject rect];
    }
    CGRect rect = [self caretRectForPosition:self.selectedTextRange.end];
    if (rect.size.height == 0) {
        rect.size.height = self.editor->lineHeight;
    }
    return rect;
}
@end
static BOOL s_iosClipboardIsRect = NO;
static BOOL s_iosClipboardIsLine = NO;
static NSString *s_iosClipboardSnapshot = nil;

@interface MiuWorkspaceContainerView : UIView
@property (nonatomic, copy) void (^onLayoutSubviews)(void);
@end

@implementation MiuWorkspaceContainerView
- (void)layoutSubviews {
    [super layoutSubviews];
    if (self.onLayoutSubviews) {
        self.onLayoutSubviews();
    }
}
@end

@interface ViewController () <UIDocumentPickerDelegate, UITextFieldDelegate, UIScrollViewDelegate>
@property (nonatomic, strong) MiuWorkspaceContainerView *workspaceContainer;
@property (nonatomic, strong) iOSEditorView *editorView;
@property (nonatomic, strong) WKWebView *previewWebView;
@property (nonatomic, strong) MiuIOSPreviewSplitterView *splitterView;
@property (nonatomic, strong) MiuIOSPreviewMessageHandler *previewMsgHandler;
@property (nonatomic, strong) NSURL *currentDocumentURL;
@property (nonatomic, strong) UIStackView *headerStack;
@property (nonatomic, strong) UIView *titleBarView;
@property (nonatomic, strong) UILabel *titleLabel;
@property (nonatomic, strong) UIView *searchContainer;
@property (nonatomic, strong) UITextField *searchField;
@property (nonatomic, strong) UITextField *replaceField;
@property (nonatomic, strong) UIStackView *replaceRowStack;
@property (nonatomic, strong) UIButton *matchCaseBtn;
@property (nonatomic, strong) UIButton *wholeWordBtn;
@property (nonatomic, strong) UIButton *regexBtn;
@property (nonatomic, strong) NSLayoutConstraint *buttonsWidthConstraint;
@property (nonatomic, strong) UIView *goToLineContainer;
@property (nonatomic, strong) UITextField *goToLineField;
@property (nonatomic, strong) UIImageView *titleIconView;
@property (nonatomic, assign) CGSize lastLayoutSize;
@property (nonatomic, strong) UIScrollView *scrollToTopHelper;
@property (nonatomic, strong) UIView *topFillView;
@property (nonatomic, strong) NSMutableArray<UIViewPropertyAnimator *> *blurAnimators;
@property (nonatomic, strong) UIView *verticalScrollBar;
@property (nonatomic, strong) UIView *horizontalScrollBar;
@property (nonatomic, strong) UIStackView *footerStack;
@property (nonatomic, strong) NSLayoutConstraint *footerBottomConstraint;
@property (nonatomic, weak) UIAlertController *openRecentAlert;
@end
@implementation ViewController {
    std::shared_ptr<Editor> _editorEngine;
    BOOL _isFirstLayoutDone;
    NSLayoutConstraint *_editorBottomConstraint;
    BOOL _isExportingDocument;
    CGSize _lastTopFillSize;
    CGRect _lastKeyboardScreenFrame;
    CGRect _lastKeyboardScreenBounds;
    BOOL _isKeyboardVisible;
    BOOL _isTransitioningSize;
}
- (CGFloat)computeCurrentKeyboardShrinkAmount {
    if (!self.isViewLoaded || self.view.bounds.size.height <= 0 || !_isKeyboardVisible) return 0.0;
    CGFloat shrinkAmount = 0.0;
    UIScreen *screen = self.view.window.windowScene.screen ?: [UIScreen mainScreen];
    CGRect screenBounds = screen ? screen.bounds : self.view.bounds;
    if (!CGRectIsEmpty(_lastKeyboardScreenFrame) && !CGRectIsNull(_lastKeyboardScreenFrame) &&
        !CGRectIsEmpty(_lastKeyboardScreenBounds) && !CGRectIsNull(_lastKeyboardScreenBounds) &&
        CGSizeEqualToSize(_lastKeyboardScreenBounds.size, screenBounds.size) && screen) {
        CGRect screenIntersection = CGRectIntersection(screenBounds, _lastKeyboardScreenFrame);
        if (!CGRectIsNull(screenIntersection) && !CGRectIsEmpty(screenIntersection) && screenIntersection.size.height > 0.0) {
            CGFloat kbHeightOnScreen = screenIntersection.size.height;
            BOOL viewOrientationMatchesScreen = ((self.view.bounds.size.width >= self.view.bounds.size.height) ==
                                                 (screenBounds.size.width >= screenBounds.size.height));
            if (viewOrientationMatchesScreen && !_isTransitioningSize) {
                CGRect convertedFrame = [self.view convertRect:screenIntersection fromCoordinateSpace:screen.coordinateSpace];
                CGRect viewIntersection = CGRectIntersection(self.view.bounds, convertedFrame);
                if (!CGRectIsNull(viewIntersection) && !CGRectIsEmpty(viewIntersection)) {
                    CGFloat fromNotif = self.view.bounds.size.height - viewIntersection.origin.y;
                    shrinkAmount = MIN(kbHeightOnScreen, MAX(0.0, fromNotif));
                }
            } else {
                shrinkAmount = kbHeightOnScreen;
            }
        }
    } else if (!_isTransitioningSize) {
        if (@available(iOS 15.0, *)) {
            CGRect guideFrame = self.view.keyboardLayoutGuide.layoutFrame;
            if (!CGRectIsEmpty(guideFrame) && !CGRectIsNull(guideFrame) &&
                guideFrame.origin.y > 0.0 && guideFrame.origin.y < self.view.bounds.size.height) {
                CGFloat fromGuide = self.view.bounds.size.height - guideFrame.origin.y;
                if (fromGuide > 0.0 && fromGuide <= self.view.bounds.size.height * 0.65) {
                    shrinkAmount = MAX(shrinkAmount, fromGuide);
                }
            }
        }
    }
    if (!_isTransitioningSize && shrinkAmount > 0.0 && self.editorView.isFirstResponder) {
        UIView *accView = [self.editorView activeAccessoryView];
        if (accView && accView.window && !accView.hidden && accView.alpha > 0.01) {
            UIScreen *accScreen = accView.window.windowScene.screen ?: screen;
            if (accScreen && CGSizeEqualToSize(accScreen.bounds.size, screenBounds.size)) {
                CGRect accInScreen = [accView convertRect:accView.bounds toCoordinateSpace:accScreen.coordinateSpace];
                CGRect accInView = [self.view convertRect:accInScreen fromCoordinateSpace:accScreen.coordinateSpace];
                if (!CGRectIsEmpty(accInView) && !CGRectIsNull(accInView) &&
                    accInView.origin.y > 0.0 && accInView.origin.y < self.view.bounds.size.height) {
                    CGFloat fromAcc = self.view.bounds.size.height - accInView.origin.y;
                    if (fromAcc > shrinkAmount && fromAcc <= shrinkAmount + 44.0) {
                        shrinkAmount = fromAcc;
                    }
                }
            }
        }
    }
    CGFloat maxAllowedShrink = MAX(0.0, self.view.bounds.size.height - 120.0);
    return MAX(0.0, MIN(shrinkAmount, maxAllowedShrink));
}
- (void)syncKeyboardAvoidanceAndEnsureCaret:(BOOL)ensureCaret {
    if (!self.isViewLoaded) return;
    CGFloat shrinkAmount = [self computeCurrentKeyboardShrinkAmount];
    BOOL changed = (fabs(self.footerBottomConstraint.constant - (-shrinkAmount)) > 0.5);
    if (changed) {
        [UIView performWithoutAnimation:^{
            self.footerBottomConstraint.constant = -shrinkAmount;
            [self.view layoutIfNeeded];
            [self layoutSplitViews];
        }];
    }
    if (ensureCaret && _editorEngine) {
        _editorEngine->ensureCaretVisible();
        [self.editorView setNeedsDisplay];
        _editorEngine->updateScrollBars();
    }
}
- (CGFloat)effectiveEditorViewportHeight {
    if (!self.editorView) return 0.0;
    return [self.editorView effectiveViewportHeight];
}
- (void)ensurePreviewViewsCreated {
    if (self.previewWebView || !self.workspaceContainer || !_editorEngine) return;
    self.previewMsgHandler = [[MiuIOSPreviewMessageHandler alloc] init];
    self.previewMsgHandler.editor = _editorEngine.get();

    WKWebViewConfiguration *cfg = [[WKWebViewConfiguration alloc] init];
    WKUserContentController *ucc = [[WKUserContentController alloc] init];
    [ucc addScriptMessageHandler:self.previewMsgHandler name:@"miuPreview"];
    std::string sbJsStr = std::string("(function(){") + MiuPreview::getPreviewScrollbarJs() + "})();";
    NSString *sbJsNS = [NSString stringWithUTF8String:sbJsStr.c_str()];
    if (sbJsNS) {
        WKUserScript *sbUserScript = [[WKUserScript alloc] initWithSource:sbJsNS
                                                            injectionTime:WKUserScriptInjectionTimeAtDocumentEnd
                                                         forMainFrameOnly:YES];
        [ucc addUserScript:sbUserScript];
    }
    cfg.userContentController = ucc;

    self.previewWebView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:cfg];
    self.previewWebView.navigationDelegate = self.previewMsgHandler;
    self.previewWebView.clipsToBounds = YES;
    self.previewWebView.opaque = YES;
    UIColor *previewBg = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
        return (trait.userInterfaceStyle == UIUserInterfaceStyleDark)
            ? [UIColor blackColor]
            : [UIColor whiteColor];
    }];
    self.previewWebView.backgroundColor = previewBg;
    self.previewWebView.scrollView.backgroundColor = previewBg;
    if (@available(iOS 15.0, *)) {
        self.previewWebView.underPageBackgroundColor = previewBg;
    }
    self.previewWebView.scrollView.scrollsToTop = NO;
    self.previewWebView.scrollView.showsVerticalScrollIndicator = NO;
    self.previewWebView.scrollView.showsHorizontalScrollIndicator = NO;
    self.previewWebView.hidden = YES;
    [self.workspaceContainer addSubview:self.previewWebView];

    self.splitterView = [[MiuIOSPreviewSplitterView alloc] initWithFrame:CGRectZero];
    self.splitterView.editor = _editorEngine.get();
    self.splitterView.hidden = YES;
    __weak typeof(self) weakSelf = self;
    self.splitterView.onRatioChanged = ^{
        __strong typeof(self) strongSelf = weakSelf;
        if (!strongSelf || !strongSelf->_editorEngine) return;
        [strongSelf layoutSplitViews];
        strongSelf->_editorEngine->updateScrollBars();
        [strongSelf.editorView setNeedsDisplay];
    };
    [self.workspaceContainer addSubview:self.splitterView];
}
- (void)layoutSplitViews {
    if (!self.workspaceContainer || !self.editorView) return;
    CGRect cb = self.workspaceContainer.bounds;
    CGRect oldEditorFrame = self.editorView.frame;
    if (!_editorEngine || !_editorEngine->showPreview) {
        self.editorView.frame = cb;
        if (self.splitterView) self.splitterView.hidden = YES;
        if (self.previewWebView) self.previewWebView.hidden = YES;
    } else {
        [self ensurePreviewViewsCreated];
        BOOL verticalSplit = (cb.size.width < 600.0 && cb.size.height > cb.size.width);
        self.splitterView.isVerticalSplit = verticalSplit;
        if (!verticalSplit) {
            CGFloat splitW = 8.0;
            CGFloat availW = MAX(20.0, cb.size.width - splitW);
            CGFloat leftW = floor(availW * _editorEngine->previewSplitRatio);
            CGFloat rightW = MAX(10.0, availW - leftW);
            CGFloat topMargin = self.editorView.topRenderMargin;
            self.editorView.frame = CGRectMake(0, 0, leftW, cb.size.height);
            self.splitterView.frame = CGRectMake(leftW, topMargin, splitW, MAX(0.0, cb.size.height - topMargin));
            self.previewWebView.frame = CGRectMake(leftW + splitW, topMargin, rightW, MAX(0.0, cb.size.height - topMargin));
        } else {
            CGFloat splitH = 8.0;
            CGFloat availH = MAX(20.0, cb.size.height - splitH);
            CGFloat topH = floor(availH * _editorEngine->previewSplitRatio);
            CGFloat bottomH = MAX(10.0, availH - topH);
            self.editorView.frame = CGRectMake(0, 0, cb.size.width, topH);
            self.splitterView.frame = CGRectMake(0, topH, cb.size.width, splitH);
            self.previewWebView.frame = CGRectMake(0, topH + splitH, cb.size.width, bottomH);
        }
        self.splitterView.hidden = NO;
        self.previewWebView.hidden = NO;
        [self.splitterView setNeedsDisplay];
    }
    if (_editorEngine && !CGRectEqualToRect(oldEditorFrame, self.editorView.frame)) {
        float vw = self.editorView.bounds.size.width;
        float vh = MAX(1.0f, (float)[self effectiveEditorViewportHeight]);
        ScrollBarLayout L = _editorEngine->getScrollBarLayout(vw, vh);
        int totalLines = (_editorEngine->wordWrapEnabled && L.totalVisualLines > L.totalLines)
            ? L.totalVisualLines
            : L.totalLines;
        int maxV = std::max(0, totalLines - L.visibleLines);
        _editorEngine->vScrollPos = std::clamp(_editorEngine->vScrollPos, 0, maxV);
        int maxH = (_editorEngine->wordWrapEnabled || !L.needH)
            ? 0
            : std::max(0, (int)std::ceil(_editorEngine->maxLineWidth - L.textAreaW));
        _editorEngine->hScrollPos = std::clamp(_editorEngine->hScrollPos, 0, maxH);
        if (self.editorView.isFirstResponder) {
            _editorEngine->ensureCaretVisible();
        }
        _editorEngine->updateScrollBars();
        [self.editorView setNeedsDisplay];
    }
}
- (void)updatePreviewPane {
    if (!_editorEngine || !_editorEngine->showPreview) return;
    [self ensurePreviewViewsCreated];
    if (!self.previewWebView) return;
    MiuPreview::PreviewRenderResult res = _editorEngine->getPreviewRenderResult();
    NSString *fullHtmlNS = [NSString stringWithUTF8String:res.fullHtml.c_str()];
    if (!fullHtmlNS) return;

    self.previewMsgHandler.pendingHtml = fullHtmlNS;

    if (!_editorEngine->previewInitialLoaded || !res.isMdOrMermaidShell || !self.previewMsgHandler.previewPageLoaded) {
        self.previewMsgHandler.previewPageLoaded = NO;
        _editorEngine->previewInitialLoaded = res.isMdOrMermaidShell;
        [self.previewWebView loadHTMLString:fullHtmlNS baseURL:nil];
    } else {
        std::string js = MiuPreview::buildDomUpdateJs(res.bodyContent, _editorEngine->isDarkMode);
        NSString *jsNS = [NSString stringWithUTF8String:js.c_str()];
        if (jsNS) {
            WKWebView *wv = self.previewWebView;
            MiuIOSPreviewMessageHandler *mh = self.previewMsgHandler;
            [wv evaluateJavaScript:jsNS completionHandler:^(id result, NSError *error) {
                if (error) {
                    mh.previewPageLoaded = NO;
                    [wv loadHTMLString:fullHtmlNS baseURL:nil];
                }
            }];
        }
    }
}
- (void)saveBookmarkForURL:(NSURL *)url {
    if (!url || !url.path) return;
    NSData *bm = [url bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
    if (!bm) return;
    NSDictionary *existing = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"MiuRecentBookmarks"];
    NSMutableDictionary *dict = existing ? [existing mutableCopy] : [NSMutableDictionary dictionary];
    dict[url.path] = bm;
    [[NSUserDefaults standardUserDefaults] setObject:dict forKey:@"MiuRecentBookmarks"];
}
- (void)openRecentFileAtPath:(const std::string &)path {
    if (!_editorEngine) return;
    if (self.currentDocumentURL) {
        [self.currentDocumentURL stopAccessingSecurityScopedResource];
        self.currentDocumentURL = nil;
    }
    NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
    NSURL *targetURL = nsPath ? [NSURL fileURLWithPath:nsPath] : nil;
    BOOL accessGranted = NO;
    if (nsPath) {
        NSDictionary *dict = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"MiuRecentBookmarks"];
        NSData *bm = dict[nsPath];
        if (bm) {
            BOOL isStale = NO;
            NSURL *resolved = [NSURL URLByResolvingBookmarkData:bm options:0 relativeToURL:nil bookmarkDataIsStale:&isStale error:nil];
            if (resolved) {
                targetURL = resolved;
                accessGranted = [targetURL startAccessingSecurityScopedResource];
                if (isStale) {
                    [self saveBookmarkForURL:targetURL];
                }
            }
        }
    }
    if (!accessGranted && targetURL) {
        accessGranted = [targetURL startAccessingSecurityScopedResource];
    }
    self.currentDocumentURL = accessGranted ? targetURL : nil;
    std::string actualPath = (targetURL && targetURL.path) ? std::string(targetURL.path.UTF8String) : path;
    [self.editorView unmarkText];
    [self.editorView.inputDelegate selectionWillChange:self.editorView];
    [self.editorView.inputDelegate textWillChange:self.editorView];
    bool success = _editorEngine->openFileFromPath(actualPath);
    if (success && self.searchField.text.length > 0) {
        _editorEngine->searchQuery = self.searchField.text.UTF8String;
    }
    [self.editorView.inputDelegate textDidChange:self.editorView];
    [self.editorView.inputDelegate selectionDidChange:self.editorView];
    [self.editorView setNeedsDisplay];
}
- (BOOL)dismissOpenRecentIfVisible {
    if (self.openRecentAlert) {
        if (self.openRecentAlert.isBeingDismissed) {
            return YES;
        }
        if (self.presentedViewController == self.openRecentAlert || self.openRecentAlert.presentingViewController != nil) {
            __weak typeof(self) weakSelf = self;
            [self.openRecentAlert dismissViewControllerAnimated:YES completion:^{
                weakSelf.openRecentAlert = nil;
            }];
            return YES;
        }
        self.openRecentAlert = nil;
    }
    return NO;
}
- (void)dismissOpenRecentThenPerform:(void (^)(void))block {
    if (self.openRecentAlert) {
        if (self.openRecentAlert.isBeingDismissed) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.25 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                self.openRecentAlert = nil;
                if (block) block();
            });
            return;
        }
        if (self.presentedViewController == self.openRecentAlert || self.openRecentAlert.presentingViewController != nil) {
            __weak typeof(self) weakSelf = self;
            [self.openRecentAlert dismissViewControllerAnimated:NO completion:^{
                weakSelf.openRecentAlert = nil;
                if (block) block();
            }];
            return;
        }
        self.openRecentAlert = nil;
    }
    if (block) block();
}
- (void)presentOpenRecentMenuFromView:(UIView *)sourceView {
    if ([self dismissOpenRecentIfVisible]) {
        return;
    }
    if (!_editorEngine) return;
    _editorEngine->loadRecentFiles();
    UIAlertController *sheet = [UIAlertController alertControllerWithTitle:NSLocalizedString(@"Open Recent", nil)
                                                                   message:nil
                                                            preferredStyle:UIAlertControllerStyleActionSheet];
    __weak typeof(self) weakSelf = self;
    if (_editorEngine->recentFiles.empty()) {
        UIAlertAction *emptyAction = [UIAlertAction actionWithTitle:NSLocalizedString(@"No Recent Files", nil)
                                                              style:UIAlertActionStyleDefault
                                                            handler:nil];
        emptyAction.enabled = NO;
        [sheet addAction:emptyAction];
    } else {
        for (const auto& path : _editorEngine->recentFiles) {
            NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
            if (!nsPath || nsPath.length == 0) continue;
            NSString *displayTitle = [nsPath lastPathComponent];
            [sheet addAction:[UIAlertAction actionWithTitle:displayTitle style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
                __strong typeof(self) strongSelf = weakSelf;
                if (!strongSelf || !strongSelf->_editorEngine) return;
                [strongSelf confirmSaveIfNeededWithAction:^{
                    [strongSelf openRecentFileAtPath:path];
                }];
            }]];
        }
        [sheet addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Clear Menu", nil) style:UIAlertActionStyleDestructive handler:^(UIAlertAction * _Nonnull action) {
            __strong typeof(self) strongSelf = weakSelf;
            if (strongSelf && strongSelf->_editorEngine) {
                strongSelf->_editorEngine->clearRecentFiles();
                [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"MiuRecentBookmarks"];
            }
        }]];
    }
    [sheet addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Cancel", nil) style:UIAlertActionStyleCancel handler:nil]];
    UIPopoverPresentationController *pop = sheet.popoverPresentationController;
    if (pop) {
        if (sourceView && sourceView.window && self.view.window) {
            if (sourceView.window == self.view.window) {
                pop.sourceView = sourceView;
                pop.sourceRect = sourceView.bounds;
            } else {
                CGRect rectInView = [sourceView convertRect:sourceView.bounds toCoordinateSpace:self.view];
                rectInView = CGRectIntersection(rectInView, self.view.bounds);
                if (!CGRectIsNull(rectInView) && rectInView.size.width > 0 && rectInView.size.height > 0) {
                    pop.sourceView = self.view;
                    pop.sourceRect = rectInView;
                } else {
                    pop.sourceView = self.view;
                    pop.sourceRect = CGRectMake(self.view.bounds.size.width * 0.5, 40, 1, 1);
                }
            }
        } else if (self.titleBarView && self.titleBarView.bounds.size.height > 0) {
            pop.sourceView = self.titleBarView;
            pop.sourceRect = self.titleBarView.bounds;
        } else {
            pop.sourceView = self.view;
            pop.sourceRect = CGRectMake(self.view.bounds.size.width * 0.5, 40, 1, 1);
        }
    }
    self.openRecentAlert = sheet;
    [self presentViewController:sheet animated:YES completion:nil];
}
- (void)performNewDocument {
    if (!_editorEngine) return;
    [self.editorView unmarkText];
    [self.editorView.inputDelegate selectionWillChange:self.editorView];
    [self.editorView.inputDelegate textWillChange:self.editorView];
    _editorEngine->newFile();
    if (self.searchField.text.length > 0) {
        _editorEngine->searchQuery = self.searchField.text.UTF8String;
    }
    _editorEngine->ensureCaretVisible();
    [self.editorView.inputDelegate textDidChange:self.editorView];
    [self.editorView.inputDelegate selectionDidChange:self.editorView];
    [self.editorView setNeedsDisplay];
    if (_editorEngine->cbUpdateTitleBar) {
        _editorEngine->cbUpdateTitleBar();
    }
}
- (void)setupGoToLineUI {
    self.goToLineContainer = [[UIView alloc] init];
    self.goToLineContainer.backgroundColor = [UIColor secondarySystemBackgroundColor];
    self.goToLineContainer.hidden = YES;
    UIView *bottomLine = [[UIView alloc] init];
    bottomLine.backgroundColor = [UIColor separatorColor];
    bottomLine.translatesAutoresizingMaskIntoConstraints = NO;
    [self.goToLineContainer addSubview:bottomLine];
    [self.footerStack addArrangedSubview:self.goToLineContainer];
    UIStackView *mainStack = [[UIStackView alloc] init];
    mainStack.axis = UILayoutConstraintAxisHorizontal;
    mainStack.spacing = 12;
    mainStack.alignment = UIStackViewAlignmentCenter;
    mainStack.translatesAutoresizingMaskIntoConstraints = NO;
    mainStack.layoutMargins = UIEdgeInsetsMake(8, 12, 8, 12);
    mainStack.layoutMarginsRelativeArrangement = YES;
    [self.goToLineContainer addSubview:mainStack];
    [NSLayoutConstraint activateConstraints:@[
        [bottomLine.bottomAnchor constraintEqualToAnchor:self.goToLineContainer.bottomAnchor],
        [bottomLine.leadingAnchor constraintEqualToAnchor:self.goToLineContainer.leadingAnchor],
        [bottomLine.trailingAnchor constraintEqualToAnchor:self.goToLineContainer.trailingAnchor],
        [bottomLine.heightAnchor constraintEqualToConstant:0.5],
        [mainStack.topAnchor constraintEqualToAnchor:self.goToLineContainer.topAnchor],
        [mainStack.bottomAnchor constraintEqualToAnchor:self.goToLineContainer.bottomAnchor],
        [mainStack.leadingAnchor constraintEqualToAnchor:self.goToLineContainer.leadingAnchor],
        [mainStack.trailingAnchor constraintEqualToAnchor:self.goToLineContainer.trailingAnchor]
    ]];
    UILabel *label = [[UILabel alloc] init];
    label.text = NSLocalizedString(@"Line:", @"行番号ラベル");
    label.font = [UIFont systemFontOfSize:14];
    [mainStack addArrangedSubview:label];
    self.goToLineField = [[UITextField alloc] init];
    self.goToLineField.placeholder = @"1...";
    self.goToLineField.borderStyle = UITextBorderStyleRoundedRect;
    self.goToLineField.keyboardType = UIKeyboardTypeNumbersAndPunctuation;
    self.goToLineField.returnKeyType = UIReturnKeyGo;
    self.goToLineField.delegate = self;
    self.goToLineField.tag = 1001;
    [self.goToLineField setContentHuggingPriority:249 forAxis:UILayoutConstraintAxisHorizontal];
    [mainStack addArrangedSubview:self.goToLineField];
    UIButton *goBtn = [UIButton buttonWithType:UIButtonTypeSystem];
    [goBtn setTitle:NSLocalizedString(@"Go", nil) forState:UIControlStateNormal];
    [goBtn addTarget:self action:@selector(performGoToLine) forControlEvents:UIControlEventTouchUpInside];
    [mainStack addArrangedSubview:goBtn];
    UIButton *closeBtn = [UIButton buttonWithType:UIButtonTypeSystem];
    [closeBtn setTitle:NSLocalizedString(@"Cancel", nil) forState:UIControlStateNormal];
    [closeBtn addTarget:self action:@selector(closeGoToLine) forControlEvents:UIControlEventTouchUpInside];
    [mainStack addArrangedSubview:closeBtn];
}
- (void)viewDidLoad {
    [super viewDidLoad];
    _lastKeyboardScreenFrame = CGRectNull;
    _lastKeyboardScreenBounds = CGRectNull;
    _isKeyboardVisible = NO;
    _isTransitioningSize = NO;
    self.view.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
        return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor blackColor] : [UIColor systemBackgroundColor];
    }];
    self.headerStack = [[UIStackView alloc] init];
    self.headerStack.axis = UILayoutConstraintAxisVertical;
    self.headerStack.translatesAutoresizingMaskIntoConstraints = NO;
    self.headerStack.layer.zPosition = 10;
    [self.view addSubview:self.headerStack];
    self.titleBarView = [[UIView alloc] init];
    self.titleBarView.translatesAutoresizingMaskIntoConstraints = NO;
    self.titleBarView.backgroundColor = [UIColor clearColor];
    [self.headerStack addArrangedSubview:self.titleBarView];
    UIStackView *titleStack = [[UIStackView alloc] init];
    titleStack.axis = UILayoutConstraintAxisHorizontal;
    titleStack.alignment = UIStackViewAlignmentCenter;
    titleStack.spacing = 6.0;
    titleStack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.titleBarView addSubview:titleStack];
    self.titleIconView = [[UIImageView alloc] init];
    self.titleIconView.contentMode = UIViewContentModeScaleAspectFit;
    self.titleIconView.translatesAutoresizingMaskIntoConstraints = NO;
    [titleStack addArrangedSubview:self.titleIconView];
    self.titleLabel = [[UILabel alloc] init];
    self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.titleLabel.textColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
        return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor colorWithWhite:0.7 alpha:1.0] : [UIColor secondaryLabelColor];
    }];
    self.titleLabel.font = [UIFont boldSystemFontOfSize:13];
    self.titleLabel.textAlignment = NSTextAlignmentCenter;
    self.titleLabel.text = NSLocalizedString(@"Untitled", @"新規ファイル名");
    [titleStack addArrangedSubview:self.titleLabel];
    self.topFillView = [[UIView alloc] init];
    self.topFillView.translatesAutoresizingMaskIntoConstraints = NO;
    self.topFillView.userInteractionEnabled = NO;
    [self.view addSubview:self.topFillView];
    [self.view insertSubview:self.topFillView belowSubview:self.headerStack];

    self.footerStack = [[UIStackView alloc] init];
    self.footerStack.axis = UILayoutConstraintAxisVertical;
    self.footerStack.translatesAutoresizingMaskIntoConstraints = NO;
    self.footerStack.layer.zPosition = 10;
    [self.view addSubview:self.footerStack];

    [self setupSearchUI];
    [self setupGoToLineUI];

    self.workspaceContainer = [[MiuWorkspaceContainerView alloc] initWithFrame:CGRectZero];
    self.workspaceContainer.translatesAutoresizingMaskIntoConstraints = NO;
    self.workspaceContainer.backgroundColor = [UIColor clearColor];
    __weak typeof(self) weakContainerSelf = self;
    self.workspaceContainer.onLayoutSubviews = ^{
        __strong typeof(weakContainerSelf) strongContainerSelf = weakContainerSelf;
        if (strongContainerSelf) {
            [strongContainerSelf layoutSplitViews];
        }
    };
    [self.view addSubview:self.workspaceContainer];
    [self.view sendSubviewToBack:self.workspaceContainer];

    self.editorView = [[iOSEditorView alloc] initWithFrame:CGRectZero];
    self.editorView.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
        return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor blackColor] : [UIColor colorNamed:@"EditorBgColor"];
    }];
    [self.workspaceContainer addSubview:self.editorView];

    UIColor *scrollBarThumbColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
        return (trait.userInterfaceStyle == UIUserInterfaceStyleDark)
            ? [UIColor colorWithWhite:1.0 alpha:0.42]
            : [UIColor colorWithWhite:0.0 alpha:0.38];
    }];
    self.verticalScrollBar = [[UIView alloc] initWithFrame:CGRectZero];
    self.verticalScrollBar.backgroundColor = scrollBarThumbColor;
    self.verticalScrollBar.layer.cornerRadius = 3.0;
    self.verticalScrollBar.userInteractionEnabled = NO;
    [self.editorView addSubview:self.verticalScrollBar];

    self.horizontalScrollBar = [[UIView alloc] initWithFrame:CGRectZero];
    self.horizontalScrollBar.backgroundColor = scrollBarThumbColor;
    self.horizontalScrollBar.layer.cornerRadius = 3.0;
    self.horizontalScrollBar.userInteractionEnabled = NO;
    [self.editorView addSubview:self.horizontalScrollBar];

    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    _editorBottomConstraint = [self.workspaceContainer.bottomAnchor constraintEqualToAnchor:self.footerStack.topAnchor];
    self.footerBottomConstraint = [self.footerStack.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
        [self.topFillView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [self.topFillView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.topFillView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.topFillView.bottomAnchor constraintEqualToAnchor:self.headerStack.bottomAnchor constant:10],
        [self.headerStack.topAnchor constraintEqualToAnchor:safeArea.topAnchor constant:-12],
        [self.headerStack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.headerStack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.titleBarView.heightAnchor constraintEqualToConstant:17],
        [titleStack.centerXAnchor constraintEqualToAnchor:self.titleBarView.centerXAnchor],
        [titleStack.centerYAnchor constraintEqualToAnchor:self.titleBarView.centerYAnchor],
        [self.titleIconView.widthAnchor constraintEqualToConstant:16],
        [self.titleIconView.heightAnchor constraintEqualToConstant:16],
        [self.workspaceContainer.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        _editorBottomConstraint,
        [self.workspaceContainer.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.workspaceContainer.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.footerStack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.footerStack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        self.footerBottomConstraint
    ]];

    _editorEngine = std::make_shared<Editor>();
    NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
    NSString *appVersion = infoDict[@"CFBundleShortVersionString"];
    if (!appVersion) appVersion = @"1.0.44";
    NSString *versionString = [NSString stringWithFormat:@"miu v%@", appVersion];
    _editorEngine->appVersionStr = UTF8ToW([versionString UTF8String]);
    NSString *localizedHelp = NSLocalizedString(@"HelpText_iOS", nil);
    _editorEngine->helpTextStr = UTF8ToW([localizedHelp UTF8String]);
    __weak typeof(self) weakSelf = self;
    _editorEngine->cbUpdateTitleBar = [weakSelf]() {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(self) strongSelf = weakSelf;
            if (strongSelf && strongSelf->_editorEngine) {
                NSString *fileName = NSLocalizedString(@"Untitled", @"新規ファイル名");
                UIImage *iconImage = nil;
                if (!strongSelf->_editorEngine->currentFilePath.empty()) {
                    std::string utf8Name = WToUTF8(strongSelf->_editorEngine->currentFilePath);
                    NSString *fullPath = [NSString stringWithUTF8String:utf8Name.c_str()];
                    fileName = [fullPath lastPathComponent];
                    NSURL *fileURL = [NSURL fileURLWithPath:fullPath];
                    UIDocumentInteractionController *docController = [UIDocumentInteractionController interactionControllerWithURL:fileURL];
                    if (docController.icons.count > 0) {
                        iconImage = docController.icons.lastObject;
                    }
                } else {
                    iconImage = [UIImage imageNamed:@"TitleIcon"];
                }
                NSString *dirtyMark = strongSelf->_editorEngine->isDirty ? @"*" : @"";
                strongSelf.titleLabel.text = [NSString stringWithFormat:@"%@%@", dirtyMark, fileName];
                strongSelf.titleIconView.image = iconImage;
                strongSelf.titleIconView.hidden = (iconImage == nil);
            }
        });
    };
    _editorEngine->cbNeedsDisplay = [weakSelf]() {
        __strong typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            [strongSelf.editorView setNeedsDisplay];
            if (strongSelf->_editorEngine && strongSelf->_editorEngine->cbUpdateTitleBar) {
                strongSelf->_editorEngine->cbUpdateTitleBar();
            }
        }
    };
    _editorEngine->cbUpdatePreview = [weakSelf]() {
        void (^work)(void) = ^{
            __strong typeof(self) strongSelf = weakSelf;
            if (strongSelf) {
                [strongSelf layoutSplitViews];
                [strongSelf updatePreviewPane];
                [strongSelf.editorView setNeedsDisplay];
            }
        };
        if ([NSThread isMainThread]) {
            work();
        } else {
            dispatch_async(dispatch_get_main_queue(), work);
        }
    };
    _editorEngine->cbGetViewSize = [weakSelf](float& w, float& h) {
        __strong typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            w = strongSelf.editorView.bounds.size.width;
            h = MAX(0.0f, (float)[strongSelf effectiveEditorViewportHeight]);
        }
    };
    _editorEngine->cbUpdateScrollers = [weakSelf]() {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(self) strongSelf = weakSelf;
            if (strongSelf && strongSelf->_editorEngine) {
                float vw = strongSelf.editorView.bounds.size.width;
                float vh = MAX(0.0f, (float)[strongSelf effectiveEditorViewportHeight]);
                float topMargin = strongSelf.editorView.topRenderMargin;
                ScrollBarLayout L = strongSelf->_editorEngine->getScrollBarLayout(vw, vh);
                int totalLines = (strongSelf->_editorEngine->wordWrapEnabled && L.totalVisualLines > L.totalLines)
                    ? L.totalVisualLines
                    : L.totalLines;
                int maxV = std::max(0, totalLines - L.visibleLines);
                if (!L.needV || maxV <= 0) {
                    strongSelf.verticalScrollBar.hidden = YES;
                } else {
                    strongSelf.verticalScrollBar.hidden = NO;
                    float proportion = std::min(1.0f, (float)L.visibleLines / (float)std::max(1, totalLines));
                    float barHeight = MAX(20.0f, vh * proportion);
                    int currentV = (strongSelf->_editorEngine->wordWrapEnabled && L.totalLines == 1 && L.totalVisualLines > 1)
                        ? strongSelf->_editorEngine->vScrollSubLine
                        : strongSelf->_editorEngine->vScrollPos;
                    float scrollRatio = std::clamp((float)currentV / (float)maxV, 0.0f, 1.0f);
                    float barY = topMargin + scrollRatio * MAX(0.0f, vh - barHeight);
                    strongSelf.verticalScrollBar.frame = CGRectMake(vw - 10.0f, barY, 6.0f, barHeight);
                }
                float editorVw = L.textAreaW;
                float maxOffsetX = std::max(0.0f, strongSelf->_editorEngine->maxLineWidth - editorVw);
                if (!L.needH || maxOffsetX <= 0.5f || strongSelf->_editorEngine->wordWrapEnabled) {
                    strongSelf.horizontalScrollBar.hidden = YES;
                } else {
                    strongSelf.horizontalScrollBar.hidden = NO;
                    float totalWidth = strongSelf->_editorEngine->maxLineWidth;
                    float barWidth = MAX(20.0f, editorVw * std::min(1.0f, editorVw / std::max(1.0f, totalWidth)));
                    float currentOffsetX = (float)strongSelf->_editorEngine->hScrollPos;
                    float scrollRatio = std::clamp(currentOffsetX / maxOffsetX, 0.0f, 1.0f);
                    float barX = strongSelf->_editorEngine->gutterWidth + scrollRatio * MAX(0.0f, editorVw - barWidth);
                    strongSelf.horizontalScrollBar.frame = CGRectMake(barX, strongSelf.editorView.bounds.size.height - 10.0f, barWidth, 6.0f);
                }
            }
        });
    };
    _editorEngine->cbOpenFile = [weakSelf]() -> bool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf confirmSaveIfNeededWithAction:^{
                [weakSelf presentDocumentPickerForOpening];
            }];
        });
        return true;
    };
    _editorEngine->cbSaveFileAs = [weakSelf]() -> bool {
        __strong typeof(self) strongSelf = weakSelf;
        if (strongSelf) [strongSelf presentDocumentPickerForSaving];
        return true;
    };
    _editorEngine->cbSetClipboard = [weakSelf](const std::string& text, bool isRect) {
        if (text.empty()) return;
        NSString *nsText = [[NSString alloc] initWithBytes:text.data() length:text.length() encoding:NSUTF8StringEncoding];
        if (nsText) {
            [UIPasteboard generalPasteboard].string = nsText;
            s_iosClipboardSnapshot = [nsText copy];
            s_iosClipboardIsRect = isRect ? YES : NO;
            __strong typeof(self) strongSelf = weakSelf;
            s_iosClipboardIsLine = (strongSelf && strongSelf->_editorEngine && strongSelf->_editorEngine->lastCopyWasLine) ? YES : NO;
        }
    };
    _editorEngine->cbGetClipboard = [weakSelf](bool& isRectMarker) -> std::string {
        isRectMarker = false;
        NSString *str = [UIPasteboard generalPasteboard].string;
        if (!str || str.length == 0) return "";
        BOOL sameAsInternal = (s_iosClipboardSnapshot && [str isEqualToString:s_iosClipboardSnapshot]);
        isRectMarker = (sameAsInternal && s_iosClipboardIsRect) ? true : false;
        __strong typeof(self) strongSelf = weakSelf;
        if (strongSelf && strongSelf->_editorEngine) {
            strongSelf->_editorEngine->lastCopyWasLine = (sameAsInternal && s_iosClipboardIsLine) ? true : false;
        }
        const char *utf8Str = [str UTF8String];
        if (utf8Str) return std::string(utf8Str);
        return "";
    };
    _editorEngine->cbShowReplaceAlert = [weakSelf](int count) {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(self) strongSelf = weakSelf;
            if (!strongSelf) return;
            NSString *title = NSLocalizedString(@"Replace All", nil);
            NSString *infoFormat = NSLocalizedString(@"%lu occurrence(s) replaced.", nil);
            NSString *msg = [NSString stringWithFormat:infoFormat, (unsigned long)count];
            UIAlertController *alert = [UIAlertController alertControllerWithTitle:title message:msg preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
            [strongSelf presentViewController:alert animated:YES completion:nil];
        });
    };
    _editorEngine->cbShowExternalModifyAlert = [weakSelf]() -> bool {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(self) strongSelf = weakSelf;
            if (!strongSelf || !strongSelf->_editorEngine) return;
            UIAlertController *alert = [UIAlertController alertControllerWithTitle:NSLocalizedString(@"File modified externally. Reload?", nil)
                                                                           message:nil
                                                                    preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Save", nil) style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
                if (strongSelf->_editorEngine && !strongSelf->_editorEngine->currentFilePath.empty()) {
                    strongSelf->_editorEngine->isDirty = false;
                    strongSelf->_editorEngine->openFileFromPath(WToUTF8(strongSelf->_editorEngine->currentFilePath));
                    [strongSelf.editorView setNeedsDisplay];
                }
            }]];
            [alert addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Cancel", nil) style:UIAlertActionStyleCancel handler:nil]];
            [strongSelf presentViewController:alert animated:YES completion:nil];
        });
        return false;
    };
    BOOL isDark = (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
    _editorEngine->isDarkMode = isDark;
    _editorEngine->initGraphics();
    self.editorView.editor = _editorEngine.get();
    self.editorView.onSyncKeyboardLayout = ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return;
        [strongSelf syncKeyboardAvoidanceAndEnsureCaret:NO];
    };
    self.editorView.onNewDocumentAction = ^{
        [weakSelf confirmSaveIfNeededWithAction:^{
            [weakSelf performNewDocument];
        }];
    };
    self.editorView.onOpenRecentAction = ^(UIView *sourceView) {
        [weakSelf presentOpenRecentMenuFromView:sourceView];
    };
    self.editorView.onDismissOpenRecentIfVisible = ^BOOL{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return NO;
        return [strongSelf dismissOpenRecentIfVisible];
    };
    self.editorView.onDismissOpenRecentThenPerform = ^(void (^block)(void)) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) return;
        [strongSelf dismissOpenRecentThenPerform:block];
    };
    self.editorView.onGoToLineAction = ^{
        __strong typeof(self) strongSelf = weakSelf;
        if (!strongSelf) return;
        if (!strongSelf.searchContainer.hidden) {
            [strongSelf closeSearch];
        }
        if (strongSelf->_editorEngine && !strongSelf->_editorEngine->cursors.empty()) {
            size_t head = strongSelf->_editorEngine->cursors.back().head;
            int currentLine = strongSelf->_editorEngine->getLineIdx(head) + 1;
            strongSelf.goToLineField.text = [NSString stringWithFormat:@"%d", currentLine];
        }
        [UIView performWithoutAnimation:^{
            [strongSelf.goToLineField becomeFirstResponder];
            [strongSelf.view layoutIfNeeded];
        }];
        [UIView animateWithDuration:0.1 animations:^{
            strongSelf.goToLineContainer.hidden = NO;
            [strongSelf.view layoutIfNeeded];
        } completion:nil];
    };
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardDidShow:) name:UIKeyboardDidShowNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillChangeFrame:) name:UIKeyboardWillChangeFrameNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardDidChangeFrame:) name:UIKeyboardDidChangeFrameNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardDidHide:) name:UIKeyboardDidHideNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showSearchNotif) name:@"miuShowSearch" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showReplaceNotif) name:@"miuShowReplace" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(closeSearch) name:@"miuCloseSearch" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(doFindNext) name:@"miuFindNext" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(doFindPrev) name:@"miuFindPrev" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(appWillEnterForeground) name:UIApplicationWillEnterForegroundNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(appDidEnterBackground) name:UIApplicationDidEnterBackgroundNotification object:nil];
    if (@available(iOS 17.0, *)) {
        [self registerForTraitChanges:@[[UITraitUserInterfaceStyle class]] withAction:@selector(updateThemeIfNeeded)];
    }
    self.scrollToTopHelper = [[UIScrollView alloc] initWithFrame:self.view.bounds];
    self.scrollToTopHelper.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.scrollToTopHelper.contentSize = CGSizeMake(self.view.bounds.size.width, 2000);
    self.scrollToTopHelper.contentOffset = CGPointMake(0, 1000);
    self.scrollToTopHelper.scrollsToTop = YES;
    self.scrollToTopHelper.delegate = self;
    self.scrollToTopHelper.showsVerticalScrollIndicator = NO;
    self.scrollToTopHelper.showsHorizontalScrollIndicator = NO;
    self.scrollToTopHelper.backgroundColor = [UIColor clearColor];
    self.scrollToTopHelper.userInteractionEnabled = YES;
    [self.view insertSubview:self.scrollToTopHelper atIndex:0];
}
#pragma mark - UIScrollViewDelegate (Scroll To Top)
- (BOOL)scrollViewShouldScrollToTop:(UIScrollView *)scrollView {
    if (scrollView == self.scrollToTopHelper) {
        [self.view endEditing:YES];
        if (_editorEngine) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self.editorView.inputDelegate selectionWillChange:self.editorView];
                self->_editorEngine->vScrollPos = 0;
                self->_editorEngine->vScrollSubLine = 0;
                self->_editorEngine->hScrollPos = 0;
                [self.editorView.inputDelegate selectionDidChange:self.editorView];
                [self.editorView setNeedsDisplay];
                if (@available(iOS 16.0, *)) {
                    [self.editorView.editMenuInteraction dismissMenu];
                } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                    [[UIMenuController sharedMenuController] hideMenuFromView:self.editorView];
#pragma clang diagnostic pop
                }
            });
        }
        return YES;
    }
    return YES;
}
- (void)scrollViewDidScrollToTop:(UIScrollView *)scrollView {
    if (scrollView == self.scrollToTopHelper) {
        scrollView.contentOffset = CGPointMake(0, 1000);
    }
}
- (void)confirmSaveIfNeededWithAction:(void(^)(void))action {
    if (!_editorEngine || !_editorEngine->isDirty) {
        action();
        return;
    }
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:NSLocalizedString(@"Unsaved Changes", nil)
                                                                   message:NSLocalizedString(@"Save changes?", nil)
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Save", nil) style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull alertAction) {
        if (!self->_editorEngine->currentFilePath.empty()) {
            self->_editorEngine->saveFile(self->_editorEngine->currentFilePath);
            action();
        } else {
            [self presentDocumentPickerForSaving];
        }
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Don't Save", nil) style:UIAlertActionStyleDestructive handler:^(UIAlertAction * _Nonnull alertAction) {
        action();
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Cancel", nil) style:UIAlertActionStyleCancel handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}
- (void)dealloc {
    if (_editorEngine) {
        _editorEngine->saveCurrentViewState();
    }
    if (self.previewWebView) {
        [self.previewWebView.configuration.userContentController removeScriptMessageHandlerForName:@"miuPreview"];
    }
    if (_blurAnimators) {
        for (UIViewPropertyAnimator *animator in _blurAnimators) {
            [animator stopAnimation:YES];
        }
    }
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}
- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    _isTransitioningSize = YES;
    _lastKeyboardScreenFrame = CGRectNull;
    _lastKeyboardScreenBounds = CGRectNull;
    if (!_isKeyboardVisible && self.footerBottomConstraint) {
        self.footerBottomConstraint.constant = 0.0;
    }
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        CGFloat shrinkAmount = [self computeCurrentKeyboardShrinkAmount];
        self.footerBottomConstraint.constant = -shrinkAmount;
        [self.view layoutIfNeeded];
        [self layoutSplitViews];
        [self rebuildBlur];
        [self.editorView setNeedsDisplay];
    } completion:^(id<UIViewControllerTransitionCoordinatorContext>  _Nonnull context) {
        self->_isTransitioningSize = NO;
        [self syncKeyboardAvoidanceAndEnsureCaret:YES];
        [self layoutSplitViews];
        [self rebuildBlur];
        [self.editorView setNeedsDisplay];
    }];
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    if (self.scrollToTopHelper) {
        self.scrollToTopHelper.contentSize = CGSizeMake(self.view.bounds.size.width, 2000);
    }
    BOOL hideTitleBar = (self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact) || (self.view.bounds.size.height < 480.0);
    for (NSLayoutConstraint *c in self.titleBarView.constraints) {
        if (c.firstAttribute == NSLayoutAttributeHeight && c.relation == NSLayoutRelationEqual) {
            c.constant = hideTitleBar ? 0 : 17;
            break;
        }
    }
    CGFloat desiredShrink = [self computeCurrentKeyboardShrinkAmount];
    if (fabs(self.footerBottomConstraint.constant - (-desiredShrink)) > 0.5) {
        self.footerBottomConstraint.constant = -desiredShrink;
        [self.workspaceContainer layoutIfNeeded];
    }
    if (_editorEngine) {
        CGFloat safeTop = self.view.safeAreaInsets.top;
        if (safeTop == 0) {
            UIWindow *firstWindow = nil;
            for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
                if ([scene isKindOfClass:[UIWindowScene class]]) {
                    firstWindow = ((UIWindowScene *)scene).windows.firstObject;
                    if (firstWindow) break;
                }
            }
            if (firstWindow) {
                safeTop = firstWindow.safeAreaInsets.top;
            }
        }
        self.editorView.topRenderMargin = safeTop + 15;
        [self layoutSplitViews];
        if (self.topFillView.bounds.size.height > 0) {
            CGSize curFillSize = self.topFillView.bounds.size;
            if (self.topFillView.subviews.count == 0 ||
                fabs(curFillSize.width - _lastTopFillSize.width) > 0.5 ||
                fabs(curFillSize.height - _lastTopFillSize.height) > 0.5) {
                _lastTopFillSize = curFillSize;
                [self buildProgressiveBlur];
            }
        }
        if (!_isFirstLayoutDone) {
            _isFirstLayoutDone = YES;
            _editorEngine->vScrollPos = 0;
            _editorEngine->hScrollPos = 0;
            self.lastLayoutSize = self.editorView.bounds.size;
        } else {
            if (!CGSizeEqualToSize(self.editorView.bounds.size, self.lastLayoutSize)) {
                _editorEngine->updateMaxLineWidth();
                float vw = self.editorView.bounds.size.width;
                float vh = MAX(1.0f, (float)[self effectiveEditorViewportHeight]);
                ScrollBarLayout L = _editorEngine->getScrollBarLayout(vw, vh);
                int totalLines = (_editorEngine->wordWrapEnabled && L.totalVisualLines > L.totalLines)
                    ? L.totalVisualLines
                    : L.totalLines;
                int maxV = std::max(0, totalLines - L.visibleLines);
                _editorEngine->vScrollPos = std::clamp(_editorEngine->vScrollPos, 0, maxV);
                int maxH = (_editorEngine->wordWrapEnabled || !L.needH)
                    ? 0
                    : std::max(0, (int)std::ceil(_editorEngine->maxLineWidth - L.textAreaW));
                _editorEngine->hScrollPos = std::clamp(_editorEngine->hScrollPos, 0, maxH);
                _editorEngine->ensureCaretVisible();
                self.lastLayoutSize = self.editorView.bounds.size;
            }
        }
        [self.editorView setNeedsDisplay];
        _editorEngine->updateScrollBars();
    }
}
- (void)buildProgressiveBlur {
    if (self.blurAnimators) {
        for (UIViewPropertyAnimator *animator in self.blurAnimators) {
            [animator stopAnimation:YES];
        }
    }
    self.blurAnimators = [NSMutableArray array];
    [self.topFillView.subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];
    int steps = 8;
    CGFloat stepHeight = self.topFillView.bounds.size.height / (CGFloat)steps;
    for (int i = steps - 1; i >= 0; i--) {
        CGFloat fraction = (CGFloat)(steps - i - 1) / (CGFloat)(steps - 1);
        fraction = pow(fraction, 1.2);
        fraction *= 0.10;
        UIVisualEffectView *blurView = [[UIVisualEffectView alloc] initWithEffect:nil];
        blurView.frame = CGRectMake(0, i * stepHeight, self.topFillView.bounds.size.width, stepHeight * 2.0);
        [self.topFillView addSubview:blurView];
        UIViewPropertyAnimator *animator = [[UIViewPropertyAnimator alloc] initWithDuration:1 curve:UIViewAnimationCurveLinear animations:^{
            blurView.effect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleRegular];
        }];
        animator.fractionComplete = MAX(fraction, 0.001);
        [animator pauseAnimation];
        [self.blurAnimators addObject:animator];
        CAGradientLayer *mask = [CAGradientLayer layer];
        mask.frame = blurView.bounds;
        mask.colors = @[(id)[UIColor whiteColor].CGColor, (id)[UIColor clearColor].CGColor];
        mask.locations = @[@0.5, @1.0];
        blurView.layer.mask = mask;
    }
}
- (void)rebuildBlur {
    if (self.topFillView.bounds.size.height > 0) {
        [self buildProgressiveBlur];
    }
}
- (void)appDidEnterBackground {
    if (_editorEngine) {
        _editorEngine->saveCurrentViewState();
    }
}
- (void)appWillEnterForeground {
    [self rebuildBlur];
    if (_editorEngine) {
        _editorEngine->checkFileModification();
    }
    if (!self.searchContainer.hidden) {
        return;
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (self.editorView) {
            [self.editorView showMenuForCurrentSelection];
        }
    });
}
- (void)disableOtherScrollsToTop:(UIView *)view {
    if ([view isKindOfClass:[UIScrollView class]] && view != self.scrollToTopHelper) {
        ((UIScrollView *)view).scrollsToTop = NO;
    }
    for (UIView *subview in view.subviews) {
        [self disableOtherScrollsToTop:subview];
    }
}
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (self.editorView) {
        [self.editorView becomeFirstResponder];
    }
    [self disableOtherScrollsToTop:self.view];
}
- (void)handleKeyboardNotification:(NSNotification *)notification isHiding:(BOOL)isHiding {
    UIScreen *screen = self.view.window.windowScene.screen ?: [UIScreen mainScreen];
    CGRect screenBounds = screen ? screen.bounds : self.view.bounds;
    if (isHiding) {
        _isKeyboardVisible = NO;
        _lastKeyboardScreenFrame = CGRectNull;
        _lastKeyboardScreenBounds = CGRectNull;
    } else {
        NSDictionary *userInfo = notification.userInfo;
        NSValue *endFrameVal = userInfo[UIKeyboardFrameEndUserInfoKey];
        if (endFrameVal) {
            CGRect endFrame = [endFrameVal CGRectValue];
            CGRect screenIntersection = CGRectIntersection(screenBounds, endFrame);
            BOOL isDockedAtBottom = !CGRectIsNull(screenIntersection) &&
                                    !CGRectIsEmpty(screenIntersection) &&
                                    screenIntersection.size.height > 1.0 &&
                                    endFrame.origin.y < screenBounds.size.height - 1.0 &&
                                    CGRectGetMaxY(screenIntersection) >= screenBounds.size.height - 10.0 &&
                                    screenIntersection.size.width >= screenBounds.size.width * 0.5;
            if (isDockedAtBottom) {
                _isKeyboardVisible = YES;
                _lastKeyboardScreenFrame = endFrame;
                _lastKeyboardScreenBounds = screenBounds;
            } else {
                _isKeyboardVisible = NO;
                _lastKeyboardScreenFrame = CGRectNull;
                _lastKeyboardScreenBounds = CGRectNull;
            }
        }
    }
    CGFloat shrinkAmount = [self computeCurrentKeyboardShrinkAmount];
    [UIView performWithoutAnimation:^{
        self.footerBottomConstraint.constant = -shrinkAmount;
        [self.view layoutIfNeeded];
        [self layoutSplitViews];
        if (_editorEngine) {
            _editorEngine->ensureCaretVisible();
            [self.editorView setNeedsDisplay];
            _editorEngine->updateScrollBars();
        }
    }];
    if (!_isTransitioningSize) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!self->_isTransitioningSize) {
                [self syncKeyboardAvoidanceAndEnsureCaret:YES];
            }
        });
    }
}
- (void)keyboardWillShow:(NSNotification *)notification {
    [self handleKeyboardNotification:notification isHiding:NO];
}
- (void)keyboardDidShow:(NSNotification *)notification {
    [self handleKeyboardNotification:notification isHiding:NO];
}
- (void)keyboardWillChangeFrame:(NSNotification *)notification {
    if (!_isKeyboardVisible && !self.editorView.isFirstResponder && !self.searchField.isFirstResponder && !self.replaceField.isFirstResponder && !self.goToLineField.isFirstResponder) {
        return;
    }
    [self handleKeyboardNotification:notification isHiding:NO];
}
- (void)keyboardDidChangeFrame:(NSNotification *)notification {
    if (!_isKeyboardVisible && !self.editorView.isFirstResponder && !self.searchField.isFirstResponder && !self.replaceField.isFirstResponder && !self.goToLineField.isFirstResponder) {
        return;
    }
    [self handleKeyboardNotification:notification isHiding:NO];
}
- (void)keyboardWillHide:(NSNotification *)notification {
    [self handleKeyboardNotification:notification isHiding:YES];
}
- (void)keyboardDidHide:(NSNotification *)notification {
    [self handleKeyboardNotification:notification isHiding:YES];
}
- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection {
    [super traitCollectionDidChange:previousTraitCollection];
    if ([self.traitCollection hasDifferentColorAppearanceComparedToTraitCollection:previousTraitCollection]) {
        [self updateThemeIfNeeded];
    }
}
- (void)updateThemeIfNeeded {
    if (_editorEngine) {
        BOOL isDark = (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
        _editorEngine->isDarkMode = isDark;
        _editorEngine->updateThemeColors();
        [self.editorView setNeedsDisplay];
        if (self.splitterView) [self.splitterView setNeedsDisplay];
        if (_editorEngine->showPreview) {
            _editorEngine->previewInitialLoaded = false;
            [self updatePreviewPane];
        }
    }
    [self rebuildBlur];
}
- (void)presentDocumentPickerForOpening {
    _isExportingDocument = NO;
    NSArray<UTType *> *types = @[UTTypePlainText, UTTypeSourceCode, UTTypeData, UTTypeContent];
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}
- (void)presentDocumentPickerForSaving {
    if (!_editorEngine) return;
    _isExportingDocument = YES;
    std::string content = _editorEngine->pt.getRange(0, _editorEngine->pt.length());
    NSData *data = [NSData dataWithBytes:content.c_str() length:content.length()];
    std::string utf8Name = WToUTF8(_editorEngine->currentFilePath);
    NSString *fileName = nil;
    if (utf8Name.empty()) {
        fileName = NSLocalizedString(@"Untitled", @"新規ファイル名");
    } else {
        fileName = [[[NSString stringWithUTF8String:utf8Name.c_str()] lastPathComponent] stringByDeletingPathExtension];
        if (fileName.length == 0) {
            fileName = NSLocalizedString(@"Untitled", @"新規ファイル名");
        }
    }
    NSString *tempFilePath = [NSTemporaryDirectory() stringByAppendingPathComponent:fileName];
    [data writeToFile:tempFilePath atomically:YES];
    NSURL *tempURL = [NSURL fileURLWithPath:tempFilePath];
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[tempURL] asCopy:YES];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    picker.shouldShowFileExtensions = YES;
#pragma clang diagnostic pop
    picker.delegate = self;
    [self presentViewController:picker animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *url = urls.firstObject;
    if (!url || !_editorEngine) return;
    if (self.currentDocumentURL) {
        [self.currentDocumentURL stopAccessingSecurityScopedResource];
        self.currentDocumentURL = nil;
    }
    BOOL accessGranted = [url startAccessingSecurityScopedResource];
    [self saveBookmarkForURL:url];
    self.currentDocumentURL = accessGranted ? url : nil;
    if (!_isExportingDocument) {
        std::string path = url.path.UTF8String;
        [self.editorView unmarkText];
        [self.editorView.inputDelegate selectionWillChange:self.editorView];
        [self.editorView.inputDelegate textWillChange:self.editorView];
        bool success = _editorEngine->openFileFromPath(path);
        if (success) {
            if (self.searchField.text.length > 0) {
                _editorEngine->searchQuery = self.searchField.text.UTF8String;
            }
        }
        [self.editorView.inputDelegate textDidChange:self.editorView];
        [self.editorView.inputDelegate selectionDidChange:self.editorView];
        [self.editorView setNeedsDisplay];
    } else {
        std::string path = url.path.UTF8String;
        std::wstring wpath = UTF8ToW(path);
        if (!_editorEngine->saveFile(wpath)) {
            _editorEngine->currentFilePath = wpath;
            _editorEngine->loadSyntaxForExtension(path);
            _editorEngine->recordFileMtime(path);
            _editorEngine->addToRecentFiles(path);
            _editorEngine->saveCurrentViewState();
            _editorEngine->undo.markSaved();
            _editorEngine->isDirty = false;
            _editorEngine->updateTitleBar();
        }
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self.editorView becomeFirstResponder];
    });
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self.editorView becomeFirstResponder];
    });
}
- (void)setupSearchUI {
    self.searchContainer = [[UIView alloc] init];
    self.searchContainer.backgroundColor = [UIColor secondarySystemBackgroundColor];
    self.searchContainer.hidden = YES;
    UIView *bottomLine = [[UIView alloc] init];
    bottomLine.backgroundColor = [UIColor separatorColor];
    bottomLine.translatesAutoresizingMaskIntoConstraints = NO;
    [self.searchContainer addSubview:bottomLine];
    [self.footerStack addArrangedSubview:self.searchContainer];
    UIStackView *mainStack = [[UIStackView alloc] init];
    mainStack.axis = UILayoutConstraintAxisVertical;
    mainStack.spacing = 8;
    mainStack.translatesAutoresizingMaskIntoConstraints = NO;
    mainStack.layoutMargins = UIEdgeInsetsMake(8, 12, 8, 12);
    mainStack.layoutMarginsRelativeArrangement = YES;
    [self.searchContainer addSubview:mainStack];
    [NSLayoutConstraint activateConstraints:@[
        [bottomLine.bottomAnchor constraintEqualToAnchor:self.searchContainer.bottomAnchor],
        [bottomLine.leadingAnchor constraintEqualToAnchor:self.searchContainer.leadingAnchor],
        [bottomLine.trailingAnchor constraintEqualToAnchor:self.searchContainer.trailingAnchor],
        [bottomLine.heightAnchor constraintEqualToConstant:0.5],
        [mainStack.topAnchor constraintEqualToAnchor:self.searchContainer.topAnchor],
        [mainStack.bottomAnchor constraintEqualToAnchor:self.searchContainer.bottomAnchor],
        [mainStack.leadingAnchor constraintEqualToAnchor:self.searchContainer.leadingAnchor],
        [mainStack.trailingAnchor constraintEqualToAnchor:self.searchContainer.trailingAnchor]
    ]];
    auto createField = ^UITextField*(NSString *placeholder) {
        UITextField *tf = [[UITextField alloc] init];
        tf.placeholder = placeholder;
        tf.borderStyle = UITextBorderStyleRoundedRect;
        tf.autocapitalizationType = UITextAutocapitalizationTypeNone;
        tf.autocorrectionType = UITextAutocorrectionTypeNo;
        tf.returnKeyType = UIReturnKeySearch;
        tf.delegate = self;
        [tf setContentHuggingPriority:249 forAxis:UILayoutConstraintAxisHorizontal];
        [tf setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
        return tf;
    };
    auto createBtn = ^UIButton*(NSString *title, SEL action) {
        UIButton *btn = [UIButton buttonWithType:UIButtonTypeSystem];
        [btn setTitle:title forState:UIControlStateNormal];
        [btn addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
        [btn setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        [btn setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        return btn;
    };
    auto createToggleBtn = ^UIButton*(NSString *title, SEL action) {
        UIButton *btn = [UIButton buttonWithType:UIButtonTypeCustom];
        if (@available(iOS 15.0, *)) {
            UIButtonConfiguration *config = [UIButtonConfiguration plainButtonConfiguration];
            NSDictionary *attrs = @{NSFontAttributeName: [UIFont systemFontOfSize:12 weight:UIFontWeightBold]};
            config.attributedTitle = [[NSAttributedString alloc] initWithString:title attributes:attrs];
            config.baseForegroundColor = [UIColor labelColor];
            config.contentInsets = NSDirectionalEdgeInsetsMake(4, 8, 4, 8);
            config.background.strokeColor = [UIColor separatorColor];
            config.background.strokeWidth = 1.0;
            config.background.cornerRadius = 4;
            btn.configuration = config;
            btn.configurationUpdateHandler = ^(UIButton *updatedBtn) {
                UIButtonConfiguration *updatedConfig = updatedBtn.configuration;
                if (updatedBtn.selected) {
                    updatedConfig.baseForegroundColor = [UIColor whiteColor];
                    updatedConfig.background.backgroundColor = [UIColor systemBlueColor];
                    updatedConfig.background.strokeColor = [UIColor systemBlueColor];
                } else {
                    updatedConfig.baseForegroundColor = [UIColor labelColor];
                    updatedConfig.background.backgroundColor = [UIColor clearColor];
                    updatedConfig.background.strokeColor = [UIColor separatorColor];
                }
                updatedBtn.configuration = updatedConfig;
            };
        } else {
            [btn setTitle:title forState:UIControlStateNormal];
            [btn setTitleColor:[UIColor labelColor] forState:UIControlStateNormal];
            [btn setTitleColor:[UIColor whiteColor] forState:UIControlStateSelected];
            btn.titleLabel.font = [UIFont systemFontOfSize:12 weight:UIFontWeightBold];
            btn.layer.cornerRadius = 4;
            btn.layer.borderWidth = 1;
            btn.layer.borderColor = [UIColor separatorColor].CGColor;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            btn.contentEdgeInsets = UIEdgeInsetsMake(4, 8, 4, 8);
#pragma clang diagnostic pop
        }
        [btn addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
        [btn setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        [btn setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        return btn;
    };
    auto createSpacer = ^UIView*() {
        UIView *v = [[UIView alloc] init];
        v.backgroundColor = [UIColor clearColor];
        [v setContentHuggingPriority:50 forAxis:UILayoutConstraintAxisHorizontal];
        [v setContentCompressionResistancePriority:50 forAxis:UILayoutConstraintAxisHorizontal];
        return v;
    };
    self.searchField = createField(NSLocalizedString(@"Search...", nil));
    [self.searchField addTarget:self action:@selector(searchTextChanged:) forControlEvents:UIControlEventEditingChanged];
    UIStackView *searchBtnsStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        createBtn(@"<", @selector(doFindPrev)),
        createBtn(@">", @selector(doFindNext)),
        createBtn(NSLocalizedString(@"Done", nil), @selector(closeSearch))
    ]];
    searchBtnsStack.spacing = 12;
    searchBtnsStack.distribution = UIStackViewDistributionFill;
    UIStackView *searchRow = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.searchField,
        searchBtnsStack
    ]];
    searchRow.spacing = 8;
    [mainStack addArrangedSubview:searchRow];
    self.replaceField = createField(NSLocalizedString(@"Replace...", nil));
    self.replaceField.returnKeyType = UIReturnKeyDone;
    UIStackView *replaceBtnsStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        createSpacer(),
        createBtn(NSLocalizedString(@"Replace", nil), @selector(doReplace)),
        createBtn(NSLocalizedString(@"All", nil), @selector(doReplaceAll))
    ]];
    replaceBtnsStack.spacing = 12;
    replaceBtnsStack.distribution = UIStackViewDistributionFill;
    self.replaceRowStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.replaceField,
        replaceBtnsStack
    ]];
    self.replaceRowStack.spacing = 8;
    self.replaceRowStack.hidden = YES;
    self.replaceRowStack.alpha = 0.0;
    [mainStack addArrangedSubview:self.replaceRowStack];
    self.matchCaseBtn = createToggleBtn(NSLocalizedString(@"Match Case", nil), @selector(toggleMatchCase:));
    self.wholeWordBtn = createToggleBtn(NSLocalizedString(@"Whole Word", nil), @selector(toggleWholeWord:));
    self.regexBtn = createToggleBtn(NSLocalizedString(@"Regex", nil), @selector(toggleRegex:));
    UIStackView *optionRow = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.matchCaseBtn,
        self.wholeWordBtn,
        self.regexBtn,
        createSpacer()
    ]];
    optionRow.axis = UILayoutConstraintAxisHorizontal;
    optionRow.spacing = 6;
    optionRow.alignment = UIStackViewAlignmentFill;
    optionRow.distribution = UIStackViewDistributionFill;
    [mainStack addArrangedSubview:optionRow];
    self.buttonsWidthConstraint = [searchBtnsStack.widthAnchor constraintEqualToAnchor:replaceBtnsStack.widthAnchor];
    self.buttonsWidthConstraint.priority = UILayoutPriorityDefaultHigh;
}
- (void)updateOptionButtonVisual:(UIButton *)btn isSelected:(BOOL)isSelected {
    btn.selected = isSelected;
    if (@available(iOS 15.0, *)) {
        [btn setNeedsUpdateConfiguration];
    } else {
        btn.backgroundColor = isSelected ? [UIColor systemBlueColor] : [UIColor clearColor];
        btn.layer.borderColor = isSelected ? [UIColor systemBlueColor].CGColor : [UIColor separatorColor].CGColor;
    }
}
- (void)toggleMatchCase:(UIButton *)sender {
    if (!_editorEngine) return;
    _editorEngine->searchMatchCase = !sender.selected;
    [self updateOptionButtonVisual:sender isSelected:_editorEngine->searchMatchCase];
    [self.editorView setNeedsDisplay];
}
- (void)toggleWholeWord:(UIButton *)sender {
    if (!_editorEngine) return;
    _editorEngine->searchWholeWord = !sender.selected;
    [self updateOptionButtonVisual:sender isSelected:_editorEngine->searchWholeWord];
    [self.editorView setNeedsDisplay];
}
- (void)toggleRegex:(UIButton *)sender {
    if (!_editorEngine) return;
    _editorEngine->searchRegex = !sender.selected;
    [self updateOptionButtonVisual:sender isSelected:_editorEngine->searchRegex];
    [self.editorView setNeedsDisplay];
}
- (void)prepareSearchQuery {
    if (!_editorEngine || _editorEngine->cursors.empty()) return;
    Cursor c = _editorEngine->cursors.back();
    if (c.hasSelection()) {
        size_t start = c.start();
        size_t end = c.end();
        std::string selectedText = _editorEngine->pt.getRange(start, end - start);
        if (!selectedText.empty()) {
            NSString *nsText = [NSString stringWithUTF8String:selectedText.c_str()];
            self.searchField.text = nsText;
            _editorEngine->searchQuery = selectedText;
            dispatch_async(dispatch_get_main_queue(), ^{
                [self.editorView setNeedsDisplay];
            });
            return;
        }
    }
    if (self.searchField.text.length > 0) {
        _editorEngine->searchQuery = self.searchField.text.UTF8String;
        [self.editorView setNeedsDisplay];
        return;
    }
    size_t head = c.head;
    size_t wStart, wEnd;
    [self.editorView getSmartWordRangeAtPos:head outStart:&wStart outEnd:&wEnd];
    if (wEnd > wStart) {
        std::string rawStr = _editorEngine->pt.getRange(wStart, wEnd - wStart);
        NSString *candidate = [NSString stringWithUTF8String:rawStr.c_str()];
        if (candidate.length > 0) {
            NSString *trimmed = [candidate stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            if (trimmed.length > 0) {
                self.searchField.text = candidate;
                _editorEngine->searchQuery = rawStr;
                dispatch_async(dispatch_get_main_queue(), ^{
                    [self.editorView setNeedsDisplay];
                });
            }
        }
    }
}
- (void)showSearchNotif {
    if (_editorEngine) {
        _editorEngine->isReplaceMode = false;
        [self updateOptionButtonVisual:self.matchCaseBtn isSelected:_editorEngine->searchMatchCase];
        [self updateOptionButtonVisual:self.wholeWordBtn isSelected:_editorEngine->searchWholeWord];
        [self updateOptionButtonVisual:self.regexBtn isSelected:_editorEngine->searchRegex];
    }
    [self prepareSearchQuery];
    [UIView performWithoutAnimation:^{
        [self.searchField becomeFirstResponder];
        [self.view layoutIfNeeded];
    }];
    self.searchContainer.hidden = NO;
    self.replaceRowStack.hidden = YES;
    self.replaceRowStack.alpha = 0.0;
    self.buttonsWidthConstraint.active = NO;
    [UIView animateWithDuration:0.1 delay:0 options:UIViewAnimationOptionCurveEaseOut animations:^{
        [self.view layoutIfNeeded];
    } completion:nil];
}
- (void)showReplaceNotif {
    if (_editorEngine) {
        _editorEngine->isReplaceMode = true;
        [self updateOptionButtonVisual:self.matchCaseBtn isSelected:_editorEngine->searchMatchCase];
        [self updateOptionButtonVisual:self.wholeWordBtn isSelected:_editorEngine->searchWholeWord];
        [self updateOptionButtonVisual:self.regexBtn isSelected:_editorEngine->searchRegex];
    }
    [self prepareSearchQuery];
    [UIView performWithoutAnimation:^{
        if (self.searchField.text.length == 0) {
            [self.searchField becomeFirstResponder];
        } else {
            [self.replaceField becomeFirstResponder];
        }
        [self.view layoutIfNeeded];
    }];
    self.searchContainer.hidden = NO;
    self.replaceRowStack.hidden = NO;
    self.replaceRowStack.alpha = 1.0;
    self.buttonsWidthConstraint.active = YES;
    [UIView animateWithDuration:0.1 delay:0 options:UIViewAnimationOptionCurveEaseOut animations:^{
        [self.view layoutIfNeeded];
    } completion:nil];
}
- (BOOL)canBecomeFirstResponder {
    return YES;
}
- (NSArray<UIKeyCommand *> *)keyCommands {
    return @[
        [UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:0 action:@selector(handleEscape:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputF1 modifierFlags:0 action:@selector(handleF1Key:)]
    ];
}
- (void)handleF1Key:(id)sender {
    if (self.editorView) {
        [self.editorView toggleHelp:sender];
    }
}
- (void)closeSearch {
    if (_editorEngine) {
        _editorEngine->isReplaceMode = false;
    }
    [self.editorView becomeFirstResponder];
    [UIView animateWithDuration:0.08 animations:^{
        self.searchContainer.hidden = YES;
        [self.view layoutIfNeeded];
    } completion:^(BOOL finished) {
        [self.editorView setNeedsDisplay];
    }];
}
- (void)handleEscape:(id)sender {
    if (_editorEngine && _editorEngine->showHelpPopup) {
        _editorEngine->showHelpPopup = false;
        [self.editorView setNeedsDisplay];
        return;
    }
    if (!self.searchContainer.hidden) {
        [self closeSearch];
        return;
    }
    if (!self.goToLineContainer.hidden) {
        [self closeGoToLine];
        return;
    }
    if (_editorEngine) {
        _editorEngine->rollbackPadding();
        _editorEngine->isRectSelecting = false;
        _editorEngine->activeEmmetSession.active = false;
        if (!_editorEngine->cursors.empty()) {
            Cursor lastCursor = _editorEngine->cursors.back();
            if (_editorEngine->cursors.size() > 1 || lastCursor.hasSelection() || lastCursor.isVirtual) {
                _editorEngine->cursors.clear();
                lastCursor.anchor = lastCursor.head;
                lastCursor.desiredX = _editorEngine->getXFromPos(lastCursor.head);
                lastCursor.originalAnchorX = lastCursor.desiredX;
                lastCursor.isVirtual = false;
                lastCursor.affinity = false;
                _editorEngine->cursors.push_back(lastCursor);
                _editorEngine->ensureCaretVisible();
                [self.editorView setNeedsDisplay];
                if (self.editorView.inputDelegate) {
                    [self.editorView.inputDelegate selectionDidChange:self.editorView];
                }
            }
        }
    }
}
- (void)closeGoToLine {
    [self.editorView becomeFirstResponder];
    [UIView animateWithDuration:0.08 animations:^{
        self.goToLineContainer.hidden = YES;
        [self.view layoutIfNeeded];
    }];
}
- (void)performGoToLine {
    NSInteger line = [self.goToLineField.text integerValue];
    if (line > 0) {
        [self.editorView jumpToLine:line];
    }
    [self closeGoToLine];
}
- (void)textFieldDidBeginEditing:(UITextField *)textField {
    if (textField.tag == 1001 || textField == self.searchField || textField == self.replaceField) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [textField selectAll:nil];
        });
    }
}
- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    if (textField == self.searchField) {
        [self doFindNext];
    } else if (textField == self.replaceField) {
        [self doReplace];
    } else if (textField == self.goToLineField) {
        [self performGoToLine];
    }
    return YES;
}
- (void)searchTextChanged:(UITextField *)sender {
    if (!_editorEngine) return;
    _editorEngine->searchQuery = sender.text ? sender.text.UTF8String : "";
    [self.editorView setNeedsDisplay];
}
- (void)doFindNext {
    if (!_editorEngine) return;
    if (self.searchField.text.length == 0) {
        if (_editorEngine->searchQuery.empty()) {
            [self showSearchNotif];
            return;
        }
    } else {
        _editorEngine->searchQuery = self.searchField.text.UTF8String;
    }
    [self.editorView.inputDelegate selectionWillChange:self.editorView];
    _editorEngine->findNext(true);
    [self.editorView.inputDelegate selectionDidChange:self.editorView];
    [self.editorView setNeedsDisplay];
}
- (void)doFindPrev {
    if (!_editorEngine) return;
    if (self.searchField.text.length == 0) {
        if (_editorEngine->searchQuery.empty()) {
            [self showSearchNotif];
            return;
        }
    } else {
        _editorEngine->searchQuery = self.searchField.text.UTF8String;
    }
    [self.editorView.inputDelegate selectionWillChange:self.editorView];
    _editorEngine->findNext(false);
    [self.editorView.inputDelegate selectionDidChange:self.editorView];
    [self.editorView setNeedsDisplay];
}
- (void)doReplace {
    if (!_editorEngine || self.searchField.text.length == 0) return;
    [self.editorView.inputDelegate selectionWillChange:self.editorView];
    [self.editorView.inputDelegate textWillChange:self.editorView];
    _editorEngine->searchQuery = self.searchField.text.UTF8String;
    _editorEngine->replaceQuery = self.replaceField.text.UTF8String;
    _editorEngine->replaceNext();
    [self.editorView.inputDelegate textDidChange:self.editorView];
    [self.editorView.inputDelegate selectionDidChange:self.editorView];
    [self.editorView setNeedsDisplay];
}
- (void)doReplaceAll {
    if (!_editorEngine || self.searchField.text.length == 0) return;
    [self.editorView.inputDelegate selectionWillChange:self.editorView];
    [self.editorView.inputDelegate textWillChange:self.editorView];
    _editorEngine->searchQuery = self.searchField.text.UTF8String;
    _editorEngine->replaceQuery = self.replaceField.text.UTF8String;
    _editorEngine->replaceAll();
    [self.editorView.inputDelegate textDidChange:self.editorView];
    [self.editorView.inputDelegate selectionDidChange:self.editorView];
    [self.editorView setNeedsDisplay];
}
@end
