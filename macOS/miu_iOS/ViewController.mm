#import "ViewController.h"
#import "EditorCore.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
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
@interface iOSEditorView : UIView <UITextInput, UIGestureRecognizerDelegate>
@property (nonatomic) Editor *editor;
@property (nonatomic, copy) void (^onNewDocumentAction)(void);
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
    UIPanGestureRecognizer *_panRecognizer;
    NSInteger _selectionGranularity;
    NSMutableArray<id<UIInteraction>> *_textInteractions;
    BOOL _isScrolling;
    UITapGestureRecognizer *_singleTapGR;
    UITapGestureRecognizer *_doubleTapGR;
    UITapGestureRecognizer *_tripleTapGR;
}
@synthesize inputDelegate = _inputDelegate;
@synthesize markedTextStyle = _markedTextStyle;
@synthesize selectedTextRange = _selectedTextRange;
- (instancetype)initWithFrame:(CGRect)frame {
    if (self = [super initWithFrame:frame]) {
        self.contentMode = UIViewContentModeTopLeft;
        self.multipleTouchEnabled = YES;
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
        [_doubleTapGR requireGestureRecognizerToFail:_tripleTapGR];
        [self addGestureRecognizer:_doubleTapGR];
        _singleTapGR = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onSingleTap:)];
        _singleTapGR.numberOfTapsRequired = 1;
        [_singleTapGR requireGestureRecognizerToFail:_doubleTapGR];
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
    }
    return self;
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
- (void)layoutSubviews {
    [super layoutSubviews];
}
- (void)drawRect:(CGRect)rect {
    if (!self.editor) return;
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    self.editor->render(ctx, self.bounds.size.width, self.bounds.size.height);
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self stopMomentumScroll];
    UITouch *touch = [touches anyObject];
    if (touch.type == UITouchTypeIndirectPointer || touch.type == UITouchTypePencil) {
        _isMouseSelecting = YES;
        CGPoint p = [touch locationInView:self];
        if (touch.tapCount == 2) {
            _selectionGranularity = 1;
            [self handleDoubleTapAtPoint:p];
        } else if (touch.tapCount >= 3) {
            _selectionGranularity = 2;
            [self handleTripleTapAtPoint:p];
        } else {
            _selectionGranularity = 0;
            [self handleSingleTapAtPoint:p];
        }
    } else {
        _isMouseSelecting = NO;
        _selectionGranularity = 0;
    }
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [super touchesMoved:touches withEvent:event];
    if (!_isMouseSelecting || !self.editor) return;
    UITouch *touch = [touches anyObject];
    CGPoint p = [touch locationInView:self];
    size_t rawPos = self.editor->getDocPosFromPoint(p.x, p.y);
    if (self.editor->cursors.empty()) return;
    Cursor &c = self.editor->cursors.back();
    size_t anchor = c.anchor;
    size_t newHead = rawPos;
    if (_selectionGranularity == 1) {
        size_t wStart, wEnd;
        [self getSmartWordRangeAtPos:rawPos outStart:&wStart outEnd:&wEnd];
        if (rawPos >= anchor) {
            newHead = wEnd;
        } else {
            newHead = wStart;
        }
    } else if (_selectionGranularity == 2) {
        int lineIdx = self.editor->getLineIdx(rawPos);
        size_t lStart = self.editor->lineStarts[lineIdx];
        size_t lEnd = (lineIdx + 1 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[lineIdx + 1] : self.editor->pt.length();
        if (rawPos >= anchor) {
            newHead = lEnd;
        } else {
            newHead = lStart;
        }
    }
    if (c.head != newHead) {
        [self.inputDelegate selectionWillChange:self];
        c.head = newHead;
        int li = self.editor->getLineIdx(newHead);
        c.desiredX = self.editor->getXInLine(li, newHead);
        [self.inputDelegate selectionDidChange:self];
        [self setNeedsDisplay];
    }
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (_isMouseSelecting) {
        _isMouseSelecting = NO;
        _selectionGranularity = 0;
        return;
    }
    [super touchesEnded:touches withEvent:event];
}
- (void)handleSingleTapAtPoint:(CGPoint)p {
    if (!self.editor) return;
    size_t pos = self.editor->getDocPosFromPoint(p.x, p.y);
    [self.inputDelegate selectionWillChange:self];
    self.editor->cursors.clear();
    Cursor c; c.head = pos; c.anchor = pos;
    int li = self.editor->getLineIdx(pos);
    c.desiredX = self.editor->getXInLine(li, pos);
    c.originalAnchorX = c.desiredX; c.isVirtual = false;
    self.editor->cursors.push_back(c);
    [self.inputDelegate selectionDidChange:self];
    self.editor->ensureCaretVisible();
    [self setNeedsDisplay];
    [self becomeFirstResponder];
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
    size_t pos = self.editor->getDocPosFromPoint(p.x, p.y);
    int lineIdx = self.editor->getLineIdx(pos);
    size_t start = self.editor->lineStarts[lineIdx];
    size_t end = (lineIdx + 1 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[lineIdx + 1] : self.editor->pt.length();
    [self.inputDelegate selectionWillChange:self];
    self.editor->cursors.clear();
    Cursor c; c.anchor = start; c.head = end;
    c.desiredX = self.editor->getXInLine(lineIdx, end);
    c.originalAnchorX = c.desiredX; c.isVirtual = false;
    self.editor->cursors.push_back(c);
    [self.inputDelegate selectionDidChange:self];
    self.editor->ensureCaretVisible();
    [self setNeedsDisplay];
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch {
    return YES;
}
- (void)applyNewFontSize:(CGFloat)newFontSize {
    if (!self.editor) return;
    newFontSize = std::clamp((float)newFontSize, 8.0f, 120.0f);
    if (fabs(newFontSize - self.editor->currentFontSize) < 0.5f) {
        return;
    }
    self.editor->currentFontSize = newFontSize;
    if (self.editor->fontRef) {
        CFRelease(self.editor->fontRef);
        self.editor->fontRef = NULL;
    }
    self.editor->fontRef = CTFontCreateWithName(CFSTR("Menlo"), newFontSize, NULL);
    if (self.editor->fontRef) {
        CGFloat ascent = CTFontGetAscent(self.editor->fontRef);
        CGFloat descent = CTFontGetDescent(self.editor->fontRef);
        CGFloat leading = CTFontGetLeading(self.editor->fontRef);
        self.editor->lineHeight = std::ceil(ascent + descent + leading);
        if (self.editor->lineHeight < 1.0f) self.editor->lineHeight = 1.0f;
        UniChar ch = 'A';
        CGGlyph glyph;
        if (CTFontGetGlyphsForCharacters(self.editor->fontRef, &ch, &glyph, 1)) {
            CGSize advance;
            CTFontGetAdvancesForGlyphs(self.editor->fontRef, kCTFontOrientationHorizontal, &glyph, &advance, 1);
            self.editor->charWidth = advance.width;
        }
    }
    self.editor->updateGutterWidth();
    self.editor->updateMaxLineWidth();
    self.editor->ensureCaretVisible();
    self.editor->zoomPopupText = std::to_string((int)newFontSize) + " px";
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
    if (fabs(_vScrollAccumulator) >= self.editor->lineHeight) {
        int linesToScroll = (int)(_vScrollAccumulator / self.editor->lineHeight);
        self.editor->vScrollPos += linesToScroll;
        _vScrollAccumulator -= linesToScroll * self.editor->lineHeight;
    }
    self.editor->hScrollPos -= dx;
    int totalLines = (int)self.editor->lineStarts.size();
    self.editor->vScrollPos = std::clamp(self.editor->vScrollPos, 0, std::max(0, totalLines - 1));
    float vw = self.bounds.size.width - self.editor->gutterWidth - self.editor->visibleVScrollWidth;
    int maxH = std::max(0, (int)(self.editor->maxLineWidth - vw + self.editor->charWidth * 4));
    self.editor->hScrollPos = std::clamp(self.editor->hScrollPos, 0, maxH);
    if (self.editor->vScrollPos == 0 || self.editor->vScrollPos == std::max(0, totalLines - 1)) {
        _scrollVelocity.y = 0;
    }
    if (self.editor->hScrollPos == 0 || self.editor->hScrollPos == maxH) {
        _scrollVelocity.x = 0;
    }
    [self setNeedsDisplay];
    [self setNeedsLayout];
    [self.inputDelegate selectionDidChange:self];
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
- (BOOL)hasText { return self.editor && self.editor->pt.length() > 0; }
- (void)setMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange {
    if (!self.editor) return;
    [self.inputDelegate textWillChange:self];
    self.editor->imeComp = markedText ? [markedText UTF8String] : "";
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (void)unmarkText {
    if (!self.editor) return;
    [self.inputDelegate textWillChange:self];
    if (!self.editor->imeComp.empty()) {
        self.editor->insertAtCursors(self.editor->imeComp);
        self.editor->imeComp = "";
    }
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (void)insertText:(NSString *)text {
    if (!self.editor) return;
    [self.inputDelegate textWillChange:self];
    self.editor->imeComp = "";
    if ([text isEqualToString:@"\n"]) {
        self.editor->insertNewlineWithAutoIndent();
    } else {
        self.editor->insertAtCursors([text UTF8String]);
    }
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (void)deleteBackward {
    if (!self.editor) return;
    [self.inputDelegate selectionWillChange:self]; // ★追加
    [self.inputDelegate textWillChange:self];
    self.editor->backspaceAtCursors();
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
    self.editor->cursors.clear();
    Cursor c; c.head = end; c.anchor = start;
    self.editor->cursors.push_back(c);
    self.editor->insertAtCursors([text UTF8String]);
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (UITextPosition *)beginningOfDocument { return [iOSTextPosition positionWithIndex:0]; }
- (UITextPosition *)endOfDocument { return [iOSTextPosition positionWithIndex:(self.editor ? self.editor->pt.length() : 0)]; }
- (UITextRange *)textRangeFromPosition:(UITextPosition *)f toPosition:(UITextPosition *)t { return [iOSTextRange rangeWithStart:((iOSTextPosition*)f).index end:((iOSTextPosition*)t).index]; }
- (UITextPosition *)positionFromPosition:(UITextPosition *)position offset:(NSInteger)offset {
    NSInteger newIndex = ((iOSTextPosition*)position).index + offset;
    if (newIndex < 0) newIndex = 0;
    return [iOSTextPosition positionWithIndex:newIndex];
}
- (UITextPosition *)positionFromPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction offset:(NSInteger)offset {
    return position;
}
- (NSComparisonResult)comparePosition:(UITextPosition *)p1 toPosition:(UITextPosition *)p2 {
    size_t i1 = ((iOSTextPosition*)p1).index; size_t i2 = ((iOSTextPosition*)p2).index;
    if (i1 < i2) return NSOrderedAscending; if (i1 > i2) return NSOrderedDescending; return NSOrderedSame;
}
- (NSInteger)offsetFromPosition:(UITextPosition *)f toPosition:(UITextPosition *)t { return ((iOSTextPosition*)t).index - ((iOSTextPosition*)f).index; }
- (id<UITextInputTokenizer>)tokenizer { return _tokenizer; }
- (UITextPosition *)positionWithinRange:(UITextRange *)r farthestInDirection:(UITextLayoutDirection)d { return r.start; }
- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)p inDirection:(UITextLayoutDirection)d { return nil; }
- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition *)p inDirection:(UITextStorageDirection)d { return NSWritingDirectionNatural; }
- (void)setBaseWritingDirection:(NSWritingDirection)w forRange:(UITextRange *)r {}
- (CGRect)firstRectForRange:(UITextRange *)range {
    if (!self.editor || !range) return CGRectZero;
    size_t start = ((iOSTextPosition*)range.start).index;
    int li = self.editor->getLineIdx(start);
    float x = self.editor->getXInLine(li, start);
    float drawX = self.editor->gutterWidth - self.editor->hScrollPos + x;
    float drawY = (li - self.editor->vScrollPos) * self.editor->lineHeight;
    return CGRectMake(drawX, drawY, 2, self.editor->lineHeight);
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
    for (int line = startLine; line <= endLine; line++) {
        size_t lineStartIdx = self.editor->lineStarts[line];
        size_t nextLineStartIdx = (line + 1 < self.editor->lineStarts.size())
                                  ? self.editor->lineStarts[line + 1]
                                  : self.editor->pt.length();
        size_t segStart = (line == startLine) ? start : lineStartIdx;
        size_t segEnd = (line == endLine) ? end : nextLineStartIdx;
        float x1 = self.editor->getXInLine(line, segStart);
        float x2 = self.editor->getXInLine(line, segEnd);
        if (x2 < x1) x2 = x1;
        float w = x2 - x1;
        if (line != endLine && w == 0) w = self.editor->charWidth / 2;
        float drawX = self.editor->gutterWidth - self.editor->hScrollPos + x1;
        float drawY = (line - self.editor->vScrollPos) * self.editor->lineHeight;
        CGRect r = CGRectMake(drawX, drawY, w, self.editor->lineHeight);
        iOSSelectionRect *selRect = [[iOSSelectionRect alloc] init];
        selRect.rectValue = r;
        selRect.writingDirectionValue = NSWritingDirectionNatural;
        selRect.containsStartValue = (line == startLine && segStart == start);
        selRect.containsEndValue = (line == endLine && segEnd == end);
        selRect.isVerticalValue = NO;
        [rects addObject:selRect];
    }
    return rects;
}
- (UITextPosition *)closestPositionToPoint:(CGPoint)point {
    if (!self.editor) return [self beginningOfDocument];
    size_t idx = self.editor->getDocPosFromPoint(point.x, point.y);
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
    int l = self.editor->getLineIdx(p);
    float x = self.editor->getXInLine(l, p);
    float drawX = self.editor->gutterWidth - self.editor->hScrollPos + x;
    float drawY = (l - self.editor->vScrollPos) * self.editor->lineHeight;
    return CGRectMake(drawX, drawY, 0, 0);
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
- (NSArray<UIKeyCommand *> *)keyCommands {
    NSArray<UIKeyCommand *> *commands = @[
        [UIKeyCommand keyCommandWithInput:@"a" modifierFlags:UIKeyModifierCommand action:@selector(selectAll:)],
        [UIKeyCommand keyCommandWithInput:@"c" modifierFlags:UIKeyModifierCommand action:@selector(copy:)],
        [UIKeyCommand keyCommandWithInput:@"x" modifierFlags:UIKeyModifierCommand action:@selector(cut:)],
        [UIKeyCommand keyCommandWithInput:@"v" modifierFlags:UIKeyModifierCommand action:@selector(paste:)],
        [UIKeyCommand keyCommandWithInput:@"z" modifierFlags:UIKeyModifierCommand action:@selector(handleUndo:)],
        [UIKeyCommand keyCommandWithInput:@"z" modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(handleRedo:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:0 action:@selector(moveLeft:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:0 action:@selector(moveRight:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:0 action:@selector(moveUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:0 action:@selector(moveDown:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierShift action:@selector(moveLeftAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierShift action:@selector(moveRightAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierShift action:@selector(moveUpAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierShift action:@selector(moveDownAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputHome modifierFlags:0 action:@selector(moveToDocStart:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputEnd modifierFlags:0 action:@selector(moveToDocEnd:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputHome modifierFlags:UIKeyModifierShift action:@selector(moveToDocStartAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputEnd modifierFlags:UIKeyModifierShift action:@selector(moveToDocEndAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierCommand action:@selector(moveToHome:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierCommand action:@selector(moveToEnd:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(moveToHomeAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(moveToEndAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierCommand action:@selector(moveToDocStart:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierCommand action:@selector(moveToDocEnd:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(moveToDocStartAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(moveToDocEndAndSelect:)],
        [UIKeyCommand keyCommandWithInput:@"o" modifierFlags:UIKeyModifierCommand action:@selector(openDocument:)],
        [UIKeyCommand keyCommandWithInput:@"s" modifierFlags:UIKeyModifierCommand action:@selector(saveDocument:)],
        [UIKeyCommand keyCommandWithInput:@"s" modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(saveDocumentAs:)],
        [UIKeyCommand keyCommandWithInput:@"f" modifierFlags:UIKeyModifierCommand action:@selector(cmdFind:)],
        [UIKeyCommand keyCommandWithInput:@"f" modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate action:@selector(cmdReplace:)],
        [UIKeyCommand keyCommandWithInput:@"+" modifierFlags:UIKeyModifierCommand action:@selector(zoomIn:)],
        [UIKeyCommand keyCommandWithInput:@"=" modifierFlags:UIKeyModifierCommand action:@selector(zoomIn:)],
        [UIKeyCommand keyCommandWithInput:@"-" modifierFlags:UIKeyModifierCommand action:@selector(zoomOut:)],
        [UIKeyCommand keyCommandWithInput:@"0" modifierFlags:UIKeyModifierCommand action:@selector(resetZoom:)]
    ];
    if (@available(iOS 15.0, *)) {
        for (UIKeyCommand *cmd in commands) {
            cmd.wantsPriorityOverSystemBehavior = YES;
        }
    }
    return commands;
}
- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
    if (action == @selector(newDocument:)) {
        return YES;
    }
    if (action == @selector(copy:) || action == @selector(cut:)) {
        return self.editor && !self.editor->cursors.empty() && self.editor->cursors.back().hasSelection();
    }
    if (action == @selector(paste:)) {
        return [UIPasteboard generalPasteboard].hasStrings;
    }
    if (action == @selector(selectAll:)) {
        return self.editor && self.editor->pt.length() > 0;
    }
    if (action == @selector(handleUndo:) || action == @selector(handleRedo:)) {
        return YES;
    }
    if (action == @selector(openDocument:) || action == @selector(saveDocument:) || action == @selector(saveDocumentAs:)) {
        return YES;
    }
    if (action == @selector(cmdFind:) || action == @selector(cmdReplace:)) {
        return YES;
    }
    if (action == @selector(zoomIn:) ||
        action == @selector(zoomOut:) ||
        action == @selector(resetZoom:)) {
        return YES;
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
    if (self.onNewDocumentAction) {
        self.onNewDocumentAction();
    }
}
- (void)selectAll:(id)sender {
    if (!self.editor) return;
    [self.inputDelegate selectionWillChange:self];
    self.editor->cursors.clear();
    Cursor c;
    c.anchor = 0;
    c.head = self.editor->pt.length();
    c.desiredX = 0;
    c.originalAnchorX = 0;
    c.isVirtual = false;
    self.editor->cursors.push_back(c);
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
}
- (void)moveCursorTo:(size_t)newPos keepSelection:(BOOL)keep updateX:(BOOL)updateX {
    if (!self.editor || self.editor->cursors.empty()) return;
    [self.inputDelegate selectionWillChange:self];
    Cursor c = self.editor->cursors.back();
    c.head = newPos;
    if (!keep) {
        c.anchor = newPos;
    }
    if (updateX) {
        int li = self.editor->getLineIdx(newPos);
        c.desiredX = self.editor->getXInLine(li, newPos);
        c.originalAnchorX = c.desiredX;
    }
    self.editor->cursors.clear();
    self.editor->cursors.push_back(c);
    [self.inputDelegate selectionDidChange:self];
    self.editor->ensureCaretVisible();
    [self setNeedsDisplay];
}
- (void)moveLeft:(id)sender { [self doMoveLeft:NO]; }
- (void)moveLeftAndSelect:(id)sender { [self doMoveLeft:YES]; }
- (void)doMoveLeft:(BOOL)keep {
    size_t head = self.editor->cursors.back().head;
    size_t newPos = self.editor->moveCaretVisual(head, false);
    [self moveCursorTo:newPos keepSelection:keep updateX:YES];
}
- (void)moveRight:(id)sender { [self doMoveRight:NO]; }
- (void)moveRightAndSelect:(id)sender { [self doMoveRight:YES]; }
- (void)doMoveRight:(BOOL)keep {
    size_t head = self.editor->cursors.back().head;
    size_t newPos = self.editor->moveCaretVisual(head, true);
    [self moveCursorTo:newPos keepSelection:keep updateX:YES];
}
- (void)moveUp:(id)sender { [self doMoveUp:NO]; }
- (void)moveUpAndSelect:(id)sender { [self doMoveUp:YES]; }
- (void)doMoveUp:(BOOL)keep {
    Cursor c = self.editor->cursors.back();
    int li = self.editor->getLineIdx(c.head);
    size_t newPos = (li > 0) ? self.editor->getPosFromLineAndX(li - 1, c.desiredX) : 0;
    [self moveCursorTo:newPos keepSelection:keep updateX:NO];
}
- (void)moveDown:(id)sender { [self doMoveDown:NO]; }
- (void)moveDownAndSelect:(id)sender { [self doMoveDown:YES]; }
- (void)doMoveDown:(BOOL)keep {
    Cursor c = self.editor->cursors.back();
    int li = self.editor->getLineIdx(c.head);
    size_t newPos = (li + 1 < self.editor->lineStarts.size()) ? self.editor->getPosFromLineAndX(li + 1, c.desiredX) : self.editor->pt.length();
    [self moveCursorTo:newPos keepSelection:keep updateX:NO];
}
- (void)moveToHome:(id)sender { [self doMoveHome:NO]; }
- (void)moveToHomeAndSelect:(id)sender { [self doMoveHome:YES]; }
- (void)doMoveHome:(BOOL)keep {
    size_t head = self.editor->cursors.back().head;
    int li = self.editor->getLineIdx(head);
    size_t newPos = self.editor->lineStarts[li];
    [self moveCursorTo:newPos keepSelection:keep updateX:YES];
}
- (void)moveToEnd:(id)sender { [self doMoveEnd:NO]; }
- (void)moveToEndAndSelect:(id)sender { [self doMoveEnd:YES]; }
- (void)doMoveEnd:(BOOL)keep {
    size_t head = self.editor->cursors.back().head;
    int li = self.editor->getLineIdx(head);
    size_t newPos = self.editor->getPosFromLineAndX(li, 999999.0f);
    [self moveCursorTo:newPos keepSelection:keep updateX:YES];
}
- (void)moveToDocStart:(id)sender { [self moveCursorTo:0 keepSelection:NO updateX:YES]; }
- (void)moveToDocStartAndSelect:(id)sender { [self moveCursorTo:0 keepSelection:YES updateX:YES]; }
- (void)moveToDocEnd:(id)sender { [self moveCursorTo:self.editor->pt.length() keepSelection:NO updateX:YES]; }
- (void)moveToDocEndAndSelect:(id)sender { [self moveCursorTo:self.editor->pt.length() keepSelection:YES updateX:YES]; }
- (void)openDocument:(id)sender {
    if (self.editor) self.editor->openFile();
}
- (void)saveDocument:(id)sender {
    if (!self.editor) return;
    if (self.editor->currentFilePath.empty()) {
        self.editor->saveFileAs();
    } else {
        self.editor->saveFile(self.editor->currentFilePath);
    }
}
- (void)saveDocumentAs:(id)sender {
    if (self.editor) self.editor->saveFileAs();
}
- (void)copy:(id)sender {
    if (!self.editor) return;
    std::string copiedText = "";
    for (const auto& c : self.editor->cursors) {
        if (c.hasSelection()) {
            copiedText += self.editor->pt.getRange(c.start(), c.end() - c.start());
        }
    }
    if (!copiedText.empty()) {
        [UIPasteboard generalPasteboard].string = [NSString stringWithUTF8String:copiedText.c_str()];
    }
}
- (void)cut:(id)sender {
    if (!self.editor) return;
    [self copy:sender];
    [self.inputDelegate selectionWillChange:self];
    [self.inputDelegate textWillChange:self];
    self.editor->backspaceAtCursors();
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self];
    [self setNeedsDisplay];
    [self setNeedsLayout];
}
- (void)paste:(id)sender {
    if (!self.editor) return;
    NSString *pasteString = [UIPasteboard generalPasteboard].string;
    if (pasteString.length > 0) {
        [self.inputDelegate selectionWillChange:self];
        [self.inputDelegate textWillChange:self];
        self.editor->insertAtCursors([pasteString UTF8String]);
        [self.inputDelegate textDidChange:self];
        [self.inputDelegate selectionDidChange:self];
        [self setNeedsDisplay];
        [self setNeedsLayout];
    }
}
- (void)handleUndo:(id)sender {
    if (!self.editor) return;
    [self.inputDelegate textWillChange:self];
    self.editor->performUndo();
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (void)handleRedo:(id)sender {
    if (!self.editor) return;
    [self.inputDelegate textWillChange:self];
    self.editor->performRedo();
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}
- (void)zoomIn:(id)sender {
    if (!self.editor) return;
    [self applyNewFontSize:self.editor->currentFontSize + 2.0f];
}
- (void)zoomOut:(id)sender {
    if (!self.editor) return;
    [self applyNewFontSize:self.editor->currentFontSize - 2.0f];
}
- (void)resetZoom:(id)sender {
    if (!self.editor) return;
    [self applyNewFontSize:14.0f];
}
- (UIView *)inputAccessoryView {
    if (_customAccessoryView) return _customAccessoryView;
    CGFloat barHeight = 36.0;
    UIBlurEffect *blurEffect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterial];
    UIVisualEffectView *effectView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    effectView.frame = CGRectMake(0, 0, UIScreen.mainScreen.bounds.size.width, barHeight);
    effectView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _customAccessoryView = [[UIView alloc] initWithFrame:effectView.bounds];
    _customAccessoryView.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    [_customAccessoryView addSubview:effectView];
    UIScrollView *scrollView = [[UIScrollView alloc] initWithFrame:_customAccessoryView.bounds];
    scrollView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    scrollView.showsHorizontalScrollIndicator = NO;
    scrollView.alwaysBounceHorizontal = YES;
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
            btn.contentEdgeInsets = UIEdgeInsetsMake(0, 10, 0, 10);
            btn.showsTouchWhenHighlighted = YES;
        }
        [btn addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
        return btn;
    };
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"New", nil), @selector(newDocument:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Open", nil), @selector(openDocument:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Save", nil), @selector(saveDocument:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Save As", nil), @selector(saveDocumentAs:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Find", nil), @selector(cmdFind:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Undo", nil), @selector(handleUndo:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Redo", nil), @selector(handleRedo:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Select All", nil), @selector(selectAll:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Cut", nil), @selector(cut:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Copy", nil), @selector(copy:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Paste", nil), @selector(paste:))];
    [stackView addArrangedSubview:createBtn(NSLocalizedString(@"Done", nil), @selector(dismissKeyboard))];
    return _customAccessoryView;
}
- (void)dismissKeyboard {
    [self resignFirstResponder];
}
- (void)cmdFind:(id)sender {
    [[NSNotificationCenter defaultCenter] postNotificationName:@"miuShowSearch" object:nil];
}
- (void)cmdReplace:(id)sender {
    [[NSNotificationCenter defaultCenter] postNotificationName:@"miuShowReplace" object:nil];
}
@end
@interface ViewController () <UIDocumentPickerDelegate, UITextFieldDelegate>
@property (nonatomic, strong) iOSEditorView *editorView;
@property (nonatomic, strong) NSURL *currentDocumentURL;
@property (nonatomic, strong) UIStackView *headerStack;
@property (nonatomic, strong) UIView *titleBarView;
@property (nonatomic, strong) UILabel *titleLabel;
@property (nonatomic, strong) UIView *searchContainer;
@property (nonatomic, strong) UITextField *searchField;
@property (nonatomic, strong) UITextField *replaceField;
@property (nonatomic, strong) UIStackView *replaceRowStack;
@end
@implementation ViewController {
    std::shared_ptr<Editor> _editorEngine;
    BOOL _isFirstLayoutDone;
    NSLayoutConstraint *_editorBottomConstraint;
    BOOL _isExportingDocument;
}
- (void)performNewDocument {
    if (!_editorEngine) return;
    _editorEngine->newFile();
    _editorEngine->vScrollPos = 0;
    _editorEngine->hScrollPos = 0;
    _editorEngine->updateMaxLineWidth();
    _editorEngine->ensureCaretVisible();
    [self.editorView setNeedsDisplay];
    if (_editorEngine->cbUpdateTitleBar) {
        _editorEngine->cbUpdateTitleBar();
    }
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
            return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor blackColor] : [UIColor systemBackgroundColor];
    }];
    self.headerStack = [[UIStackView alloc] init];
    self.headerStack.axis = UILayoutConstraintAxisVertical;
    self.headerStack.translatesAutoresizingMaskIntoConstraints = NO;
    self.headerStack.layer.zPosition = 10;
    [self.view addSubview:self.headerStack];
    self.titleBarView = [[UIView alloc] init];
    self.titleBarView.backgroundColor = [UIColor clearColor];
    [self.headerStack addArrangedSubview:self.titleBarView];
    self.titleLabel = [[UILabel alloc] init];
    self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.titleLabel.textColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
            return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor colorWithWhite:0.7 alpha:1.0] : [UIColor secondaryLabelColor];
        }];
    self.titleLabel.font = [UIFont boldSystemFontOfSize:13];
    self.titleLabel.textAlignment = NSTextAlignmentCenter;
    self.titleLabel.text = NSLocalizedString(@"Untitled", @"新規ファイル名");
    [self.titleBarView addSubview:self.titleLabel];
    UIView *topFillView = [[UIView alloc] init];
    topFillView.translatesAutoresizingMaskIntoConstraints = NO;
    topFillView.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
            return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor blackColor] : [UIColor secondarySystemBackgroundColor];
        }];
    [self.view addSubview:topFillView];
    [self.view insertSubview:topFillView belowSubview:self.headerStack];
    [self setupSearchUI];
    self.editorView = [[iOSEditorView alloc] initWithFrame:CGRectZero];
    self.editorView.translatesAutoresizingMaskIntoConstraints = NO;
    self.editorView.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
            return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor blackColor] : [UIColor colorNamed:@"EditorBgColor"];
        }];
    [self.view addSubview:self.editorView];
    [self.view sendSubviewToBack:self.editorView];
    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    _editorBottomConstraint = [self.editorView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
        [topFillView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [topFillView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [topFillView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [topFillView.bottomAnchor constraintEqualToAnchor:self.titleBarView.bottomAnchor],
        [self.headerStack.topAnchor constraintEqualToAnchor:safeArea.topAnchor constant:-12],
        [self.headerStack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.headerStack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.titleBarView.heightAnchor constraintEqualToConstant:17],
        [self.titleLabel.centerXAnchor constraintEqualToAnchor:self.titleBarView.centerXAnchor],
        [self.titleLabel.topAnchor constraintEqualToAnchor:self.titleBarView.topAnchor],
        [self.editorView.topAnchor constraintEqualToAnchor:self.headerStack.bottomAnchor],
        _editorBottomConstraint,
        [self.editorView.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor],
        [self.editorView.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor]
    ]];
    _editorEngine = std::make_shared<Editor>();
    __weak typeof(self) weakSelf = self;
    _editorEngine->cbUpdateTitleBar = [weakSelf]() {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(self) strongSelf = weakSelf;
            if (strongSelf && strongSelf->_editorEngine) {
                NSString *fileName = NSLocalizedString(@"Untitled", @"新規ファイル名");
                if (!strongSelf->_editorEngine->currentFilePath.empty()) {
                    std::string utf8Name = WToUTF8(strongSelf->_editorEngine->currentFilePath);
                    fileName = [[NSString stringWithUTF8String:utf8Name.c_str()] lastPathComponent];
                }
                NSString *dirtyMark = strongSelf->_editorEngine->isDirty ? @"*" : @"";
                strongSelf.titleLabel.text = [NSString stringWithFormat:@"%@%@", dirtyMark, fileName];
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
    _editorEngine->cbGetViewSize = [weakSelf](float& w, float& h) {
        __strong typeof(self) strongSelf = weakSelf;
        if (strongSelf) {
            w = strongSelf.editorView.bounds.size.width;
            h = strongSelf.editorView.bounds.size.height;
        }
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
    BOOL isDark = (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
    _editorEngine->isDarkMode = isDark;
    _editorEngine->initGraphics();
    self.editorView.editor = _editorEngine.get();
    self.editorView.onNewDocumentAction = ^{
        [weakSelf confirmSaveIfNeededWithAction:^{
            [weakSelf performNewDocument];
        }];
    };
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showSearchNotif) name:@"miuShowSearch" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showReplaceNotif) name:@"miuShowReplace" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(closeSearch) name:@"miuCloseSearch" object:nil];
    if (@available(iOS 17.0, *)) {
        [self registerForTraitChanges:@[[UITraitUserInterfaceStyle class]] withAction:@selector(updateThemeIfNeeded)];
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
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    
    if (_editorEngine) {
        if (!_isFirstLayoutDone) {
            _isFirstLayoutDone = YES;
            _editorEngine->vScrollPos = 0;
            _editorEngine->hScrollPos = 0;
        } else {
            _editorEngine->updateMaxLineWidth();
            _editorEngine->ensureCaretVisible();
        }
        [self.editorView setNeedsDisplay];
    }
}
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (self.editorView) {
        [self.editorView becomeFirstResponder];
    }
}
- (void)keyboardWillShow:(NSNotification *)notification {
    NSDictionary *userInfo = notification.userInfo;
    CGRect keyboardFrame = [userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGFloat shrinkAmount = keyboardFrame.size.height;
    [UIView performWithoutAnimation:^{
        _editorBottomConstraint.constant = -shrinkAmount;
        [self.view layoutIfNeeded];
        if (_editorEngine) {
            _editorEngine->ensureCaretVisible();
            [self.editorView setNeedsDisplay];
        }
    }];
}
- (void)keyboardWillHide:(NSNotification *)notification {
    [UIView performWithoutAnimation:^{
        _editorBottomConstraint.constant = 0;
        [self.view layoutIfNeeded];
        if (_editorEngine) {
            _editorEngine->ensureCaretVisible();
            [self.editorView setNeedsDisplay];
        }
    }];
}
- (void)updateThemeIfNeeded {
    if (_editorEngine) {
        BOOL isDark = (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
        _editorEngine->isDarkMode = isDark;
        _editorEngine->updateThemeColors();
        [self.editorView setNeedsDisplay];
    }
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
    if (utf8Name.empty()) {
        NSString *defaultName = NSLocalizedString(@"Untitled", @"新規ファイル名");
        utf8Name = [defaultName stringByAppendingString:@".txt"].UTF8String;
    }
    NSString *fileName = [[NSString stringWithUTF8String:utf8Name.c_str()] lastPathComponent];
    NSString *tempFilePath = [NSTemporaryDirectory() stringByAppendingPathComponent:fileName];
    [data writeToFile:tempFilePath atomically:YES];
    NSURL *tempURL = [NSURL fileURLWithPath:tempFilePath];
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[tempURL] asCopy:YES];
    picker.delegate = self;
    [self presentViewController:picker animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *url = urls.firstObject;
    if (!url || !_editorEngine) return;
    if (self.currentDocumentURL) {
        [self.currentDocumentURL stopAccessingSecurityScopedResource];
    }
    BOOL accessGranted = [url startAccessingSecurityScopedResource];
    self.currentDocumentURL = accessGranted ? url : nil;
    if (!_isExportingDocument) {
        std::string path = url.path.UTF8String;
        bool success = _editorEngine->openFileFromPath(path);
        if (success) {
            _editorEngine->vScrollPos = 0;
            _editorEngine->hScrollPos = 0;
            _editorEngine->initGraphics();
            [self.editorView setNeedsDisplay];
        }
    } else {
        std::string path = url.path.UTF8String;
        std::wstring wpath = UTF8ToW(path);
        _editorEngine->saveFile(wpath);
    }
}
- (void)setupSearchUI {
    self.searchContainer = [[UIView alloc] init];
    self.searchContainer.backgroundColor = [UIColor secondarySystemBackgroundColor];
    self.searchContainer.hidden = YES;
    UIView *bottomLine = [[UIView alloc] init];
    bottomLine.backgroundColor = [UIColor separatorColor];
    bottomLine.translatesAutoresizingMaskIntoConstraints = NO;
    [self.searchContainer addSubview:bottomLine];
    [self.headerStack addArrangedSubview:self.searchContainer];
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
        [tf setContentHuggingPriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
        return tf;
    };
    auto createBtn = ^UIButton*(NSString *title, SEL action) {
        UIButton *btn = [UIButton buttonWithType:UIButtonTypeSystem];
        [btn setTitle:title forState:UIControlStateNormal];
        [btn addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
        [btn setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        return btn;
    };
    self.searchField = createField(NSLocalizedString(@"Search...", nil));
    [self.searchField addTarget:self action:@selector(searchTextChanged:) forControlEvents:UIControlEventEditingChanged];
    UIStackView *searchRow = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.searchField,
        createBtn(@"<", @selector(doFindPrev)),
        createBtn(@">", @selector(doFindNext)),
        createBtn(NSLocalizedString(@"Done", nil), @selector(closeSearch))
    ]];
    searchRow.spacing = 12;
    [mainStack addArrangedSubview:searchRow];
    self.replaceField = createField(NSLocalizedString(@"Replace...", nil));
    self.replaceField.returnKeyType = UIReturnKeyDone;
    self.replaceRowStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.replaceField,
        createBtn(NSLocalizedString(@"Replace", nil), @selector(doReplace)),
        createBtn(NSLocalizedString(@"All", nil), @selector(doReplaceAll))
    ]];
    self.replaceRowStack.spacing = 12;
    [mainStack addArrangedSubview:self.replaceRowStack];
}
- (void)showSearchNotif {
    if (_editorEngine) _editorEngine->isReplaceMode = false;
    [UIView animateWithDuration:0.25 animations:^{
        self.searchContainer.hidden = NO;
        self.replaceRowStack.hidden = YES;
        [self.view layoutIfNeeded];
    } completion:^(BOOL finished) {
        [self.searchField becomeFirstResponder];
    }];
}
- (void)showReplaceNotif {
    if (_editorEngine) _editorEngine->isReplaceMode = true;
    [UIView animateWithDuration:0.25 animations:^{
        self.searchContainer.hidden = NO;
        self.replaceRowStack.hidden = NO;
        [self.view layoutIfNeeded];
    } completion:^(BOOL finished) {
        [self.searchField becomeFirstResponder];
    }];
}
- (BOOL)canBecomeFirstResponder {
    return YES;
}
- (NSArray<UIKeyCommand *> *)keyCommands {
    return @[
        [UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:0 action:@selector(closeSearch)]
    ];
}
- (void)closeSearch {
    if (_editorEngine) {
        _editorEngine->isReplaceMode = false;
        _editorEngine->searchQuery = "";
    }
    [self.view endEditing:YES];
    [UIView animateWithDuration:0.25 animations:^{
        self.searchContainer.hidden = YES;
        self.searchField.text = @"";
        [self.view layoutIfNeeded];
    } completion:^(BOOL finished) {
        [self.editorView setNeedsDisplay];
        [self.editorView becomeFirstResponder];
    }];
}
- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    if (textField == self.searchField) {
        [self doFindNext];
    } else if (textField == self.replaceField) {
        [self doReplace];
    }
    return YES;
}
- (void)searchTextChanged:(UITextField *)sender {
    if (!_editorEngine) return;
    _editorEngine->searchQuery = sender.text ? sender.text.UTF8String : "";
    [self.editorView setNeedsDisplay];
}
- (void)doFindNext {
    if (!_editorEngine || self.searchField.text.length == 0) return;
    _editorEngine->searchQuery = self.searchField.text.UTF8String;
    _editorEngine->findNext(true); // forward = true
    [self.editorView setNeedsDisplay];
}
- (void)doFindPrev {
    if (!_editorEngine || self.searchField.text.length == 0) return;
    _editorEngine->searchQuery = self.searchField.text.UTF8String;
    _editorEngine->findNext(false); // forward = false
    [self.editorView setNeedsDisplay];
}
- (void)doReplace {
    if (!_editorEngine || self.searchField.text.length == 0) return;
    _editorEngine->searchQuery = self.searchField.text.UTF8String;
    _editorEngine->replaceQuery = self.replaceField.text.UTF8String;
    _editorEngine->replaceNext();
    [self.editorView setNeedsDisplay];
}
- (void)doReplaceAll {
    if (!_editorEngine || self.searchField.text.length == 0) return;
    _editorEngine->searchQuery = self.searchField.text.UTF8String;
    _editorEngine->replaceQuery = self.replaceField.text.UTF8String;
    _editorEngine->replaceAll();
    [self.editorView setNeedsDisplay];
}
@end
