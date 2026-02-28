#import "ViewController.h"
#import "EditorCore.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

// ===================================================
// MARK: - UITextInput 用のカスタムポジションとレンジ
// ===================================================

@interface iOSTextPosition : UITextPosition <NSCopying>
@property (nonatomic, assign) size_t index;
+ (instancetype)positionWithIndex:(size_t)index;
@end

@implementation iOSTextPosition
+ (instancetype)positionWithIndex:(size_t)index {
    iOSTextPosition *p = [[iOSTextPosition alloc] init];
    p.index = index; return p;
}

// システムがカーソル位置をコピーして記憶できるようにするための必須メソッド
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

// ===================================================
// MARK: - UITextSelectionRect サブクラス (必須)
// ===================================================
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

// ===================================================
// MARK: - iOSEditorView
// ===================================================

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
        // マルチタッチを有効にしないと、ピンチズームなどが正しく動作しない場合があります
        self.multipleTouchEnabled = YES;
        
        _tokenizer = [[UITextInputStringTokenizer alloc] initWithTextInput:self];
        
        // ★【修正】正しいAPIを使用
        if (@available(iOS 13.0, *)) {
            UITextInteraction *interaction = [UITextInteraction textInteractionForMode:UITextInteractionModeEditable];
            interaction.textInput = self;
            [self addInteraction:interaction];
        }

        // 1. トリプルタップ (行選択)
        _tripleTapGR = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onTripleTap:)];
        _tripleTapGR.numberOfTapsRequired = 3;
        [self addGestureRecognizer:_tripleTapGR];
        
        // 2. ダブルタップ (単語選択)
        _doubleTapGR = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onDoubleTap:)];
        _doubleTapGR.numberOfTapsRequired = 2;
        // トリプルタップ成立待ちをする（これがないとダブルタップが先に発火してしまう）
        [_doubleTapGR requireGestureRecognizerToFail:_tripleTapGR];
        [self addGestureRecognizer:_doubleTapGR];
        
        // 3. シングルタップ (カーソル移動)
        _singleTapGR = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(onSingleTap:)];
        _singleTapGR.numberOfTapsRequired = 1;
        // ダブルタップ成立待ちをする（これがないとシングルタップが先に発火してしまう）
        [_singleTapGR requireGestureRecognizerToFail:_doubleTapGR];
        [self addGestureRecognizer:_singleTapGR];
        
        // --- 既存のジェスチャー設定 ---
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

// ---------------------------------------------------
// MARK: - ジェスチャーアクション (新規追加)
// ---------------------------------------------------

- (void)onSingleTap:(UITapGestureRecognizer *)sender {
    if (sender.state == UIGestureRecognizerStateEnded) {
        CGPoint p = [sender locationInView:self];
        // 既存のロジックを呼び出す
        [self handleSingleTapAtPoint:p];
        
        // ★重要: タップでカーソル移動した際、システムにメニュー(Paste等)を出させるため
        // メニューコントローラを表示するなどの処理が必要ならここに書くことができますが、
        // 基本的に selectionDidChange 通知でシステムが判断します。
    }
}

- (void)onDoubleTap:(UITapGestureRecognizer *)sender {
    if (sender.state == UIGestureRecognizerStateEnded) {
        CGPoint p = [sender locationInView:self];
        _selectionGranularity = 1; // Wordモード
        [self handleDoubleTapAtPoint:p];
    }
}

- (void)onTripleTap:(UITapGestureRecognizer *)sender {
    if (sender.state == UIGestureRecognizerStateEnded) {
        CGPoint p = [sender locationInView:self];
        _selectionGranularity = 2; // Lineモード
        [self handleTripleTapAtPoint:p];
    }
}


// システムがレイアウト変更を知るために必要
- (void)layoutSubviews {
    [super layoutSubviews];
}

- (void)drawRect:(CGRect)rect {
    if (!self.editor) return;
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    self.editor->render(ctx, self.bounds.size.width, self.bounds.size.height);
}

// ---------------------------------------------------
// MARK: - ジェスチャー（タップ・スクロール）
// ---------------------------------------------------

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self stopMomentumScroll];
    
    // ★修正: マウス操作(Pencil含む)以外の「指でのタップ」判定ロジックを削除
    // GestureRecognizerに任せるため、ここではドラッグ開始の準備だけにする
    
    UITouch *touch = [touches anyObject];
    if (touch.type == UITouchTypeIndirectPointer || touch.type == UITouchTypePencil) {
        // マウス/Pencilの場合は既存ロジックを生かす（ただし競合するならここもGesture化検討）
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
    
    // [super touchesBegan...] は呼ばない方が安全（UITextInteractionに余計な干渉をさせないため）
    // もしくは呼ぶとしても一番最後
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [super touchesMoved:touches withEvent:event];
    
    if (!_isMouseSelecting || !self.editor) return;
    
    UITouch *touch = [touches anyObject];
    CGPoint p = [touch locationInView:self];
    
    // 1. ドラッグ先の生のテキスト位置
    size_t rawPos = self.editor->getDocPosFromPoint(p.x, p.y);
    
    if (self.editor->cursors.empty()) return;
    Cursor &c = self.editor->cursors.back();
    size_t anchor = c.anchor; // 選択開始地点（固定）
    
    size_t newHead = rawPos;
    
    // ★【修正】選択粒度に応じた Head（終点）の吸着処理
    if (_selectionGranularity == 1) { // Wordモード
        size_t wStart, wEnd;
        // ドラッグ先の単語範囲を取得
        [self getSmartWordRangeAtPos:rawPos outStart:&wStart outEnd:&wEnd];
        
        // アンカーより後ろにいるなら単語の終わりまで、前にいるなら単語の始まりまで含める
        if (rawPos >= anchor) {
            newHead = wEnd;
        } else {
            newHead = wStart;
        }
        
    } else if (_selectionGranularity == 2) { // Lineモード
        int lineIdx = self.editor->getLineIdx(rawPos);
        size_t lStart = self.editor->lineStarts[lineIdx];
        size_t lEnd = (lineIdx + 1 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[lineIdx + 1] : self.editor->pt.length();
        
        if (rawPos >= anchor) {
            newHead = lEnd;
        } else {
            newHead = lStart;
        }
    }
    
    // 3. カーソル更新
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
    // ★修正: 指でのタップ処理（handleSingleTapAtPointなどを呼んでいた部分）を完全に削除
    // すべて onSingleTap: などのジェスチャーメソッドに移行したため。
    
    if (_isMouseSelecting) {
        _isMouseSelecting = NO;
        _selectionGranularity = 0;
        return;
    }
    
    // ここにあったタップ回数判定と handle...TapAtPoint の呼び出しは削除
    
    [super touchesEnded:touches withEvent:event];
}

// ---------------------------------------------------
// MARK: - 選択ロジック本体
// ---------------------------------------------------

// 1回目：即座にカーソル移動
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

    // 行情報を取得
    int lineIdx = self.editor->getLineIdx(pos);
    size_t lineStart = self.editor->lineStarts[lineIdx];
    size_t lineEnd = (lineIdx + 1 < (int)self.editor->lineStarts.size()) ? self.editor->lineStarts[lineIdx + 1] : docLen;
    size_t lineByteLen = lineEnd - lineStart;
    
    if (lineByteLen == 0) return;
    
    // 行テキストをNSString(UTF-16)に変換
    std::string lineBytes = self.editor->pt.getRange(lineStart, lineByteLen);
    NSString *lineStr = [NSString stringWithUTF8String:lineBytes.c_str()];
    if (!lineStr || lineStr.length == 0) return;
    
    // UTF-8位置 -> UTF-16文字インデックス
    size_t relativeTapByte = pos - lineStart;
    if (relativeTapByte > lineBytes.size()) relativeTapByte = lineBytes.size();
    
    NSString *preTapStr = [[NSString alloc] initWithBytes:lineBytes.c_str() length:relativeTapByte encoding:NSUTF8StringEncoding];
    NSUInteger tapCharIdx = preTapStr.length;
    if (tapCharIdx >= lineStr.length && tapCharIdx > 0) tapCharIdx = lineStr.length - 1;

    // トークナイザーによる解析
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
        
        // 英数字の場合、"_" も単語の一部とみなして拡張する処理
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
    
    // UTF-16範囲 -> UTF-8バイト位置に戻す
    NSString *preMatchStr = [lineStr substringToIndex:finalRange.location];
    NSString *matchStr = [lineStr substringWithRange:finalRange];
    size_t preBytes = [preMatchStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    size_t matchBytes = [matchStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    
    *outStart = lineStart + preBytes;
    *outEnd = *outStart + matchBytes;
}

// 2回目：単語選択（ヘルパー利用版）
- (void)handleDoubleTapAtPoint:(CGPoint)p {
    if (!self.editor) return;
    
    size_t pos = self.editor->getDocPosFromPoint(p.x, p.y);
    size_t start, end;
    
    // ヘルパーを使って範囲を決定
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

// 3回目以降：行選択に即座に切り替え
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

// ---------------------------------------------------
// MARK: - フォントサイズ変更（共通処理）
// ---------------------------------------------------

- (void)applyNewFontSize:(CGFloat)newFontSize {
    if (!self.editor) return;

    // フォントサイズの限界値を設定
    newFontSize = std::clamp((float)newFontSize, 8.0f, 120.0f);
    
    // パフォーマンス確保のため、0.5px以上の変化があった時だけ再計算・再描画
    if (fabs(newFontSize - self.editor->currentFontSize) < 0.5f) {
        return;
    }

    self.editor->currentFontSize = newFontSize;
    
    // --- C++コアは触らず、iOS側(ラッパー)でフォントとメトリクスを更新する ---
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
    
    // C++エンジンに行の折り返しやカーソル位置の再計算を指示
    self.editor->updateMaxLineWidth();
    self.editor->ensureCaretVisible();
    
    // ★【修正】ズームポップアップの表示をパーセントからピクセル(px)に変更
    self.editor->zoomPopupText = std::to_string((int)newFontSize) + " px";
    self.editor->zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    
    [self setNeedsDisplay];
    [self setNeedsLayout];
    
    [self.inputDelegate selectionDidChange:self];
}

// ---------------------------------------------------
// MARK: - ジェスチャー処理（パン・スクロール・ホイール）
// ---------------------------------------------------

- (void)handlePan:(UIPanGestureRecognizer *)gesture {
    if (!self.editor) return;
    
    // マウスでのテキスト選択中はスクロール処理をしない
    if (_isMouseSelecting) {
        [gesture setTranslation:CGPointZero inView:self];
        return;
    }

    // --- 1. Cmd + ホイール (ズーム) ---
    if (@available(iOS 13.4, *)) {
        if (gesture.modifierFlags & UIKeyModifierCommand) {
            if (gesture.state == UIGestureRecognizerStateBegan) {
                [self stopMomentumScroll];
            } else if (gesture.state == UIGestureRecognizerStateChanged) {
                CGPoint translation = [gesture translationInView:self];
                
                // ズーム処理（前回の修正：PCライクにするため符号反転）
                CGFloat sensitivity = 0.5;
                CGFloat targetSize = self.editor->currentFontSize - (translation.y * sensitivity);
                
                [self applyNewFontSize:targetSize];
                [gesture setTranslation:CGPointZero inView:self];
            }
            return;
        }
    }

    // --- 2. 通常のスクロール処理 ---
    if (gesture.state == UIGestureRecognizerStateBegan) {
        [self stopMomentumScroll];
        _vScrollAccumulator = 0.0;
        _isScrolling = YES;
        [self.inputDelegate selectionDidChange:self];
    } else if (gesture.state == UIGestureRecognizerStateChanged) {
        CGPoint translation = [gesture translationInView:self];
        
        CGFloat dx = 0;
        CGFloat dy = 0;
        
        // ★【修正】マウス/トラックパッドと、指タッチで処理を分岐する
        if (gesture.numberOfTouches == 0) {
            // [マウス/トラックパッド操作]
            // PCライクな操作感にするため、移動量を反転(-1)させます。
            // 速度(Multiplier)も指より少し速め(5.0倍)に設定
            CGFloat scrollMultiplier = -10.0;
            
            dx = translation.x * scrollMultiplier;
            dy = translation.y * scrollMultiplier;
            
        } else {
            // [指によるタッチ操作]
            // iOS標準の「ナチュラルスクロール（紙を動かす感覚）」のまま
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
            // ★追加: 慣性なしで停止 -> 表示を復元
            _isScrolling = NO;
            [self.inputDelegate selectionDidChange:self];
        } else {
            [self startMomentumScroll];
        }
    }
}

// ピンチ操作も共通メソッドを使うようにシンプル化
- (void)handlePinch:(UIPinchGestureRecognizer *)gesture {
    if (!self.editor) return;
    
    if (gesture.state == UIGestureRecognizerStateBegan) {
        _initialFontSize = self.editor->currentFontSize;
        [self stopMomentumScroll];
    } else if (gesture.state == UIGestureRecognizerStateChanged) {
        // ピンチは倍率計算
        CGFloat targetSize = _initialFontSize * gesture.scale;
        [self applyNewFontSize:targetSize];
    }
}
// スクロール量をC++エンジンに適用する共通メソッド
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
    
    // 端に到達したら速度を殺す
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

// ---------------------------------------------------
// MARK: - 慣性スクロールの物理演算ループ
// ---------------------------------------------------

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
    
    // 摩擦係数による減衰
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

// ---------------------------------------------------
// MARK: - UITextInput プロトコルの実装 (日本語IME連携)
// ---------------------------------------------------

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
    [self.inputDelegate selectionDidChange:self]; // ★追加: カーソル位置更新を通知
    
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
    
    // C++エンジン側のカーソルを同期
    size_t start = ((iOSTextPosition*)selectedTextRange.start).index;
    size_t end = ((iOSTextPosition*)selectedTextRange.end).index;
    
    self.editor->cursors.clear();
    Cursor c;
    c.anchor = start; // anchorとheadの向きはUITextRangeからは不明だが、通常start<=end
    c.head = end;
    
    int li = self.editor->getLineIdx(end);
    c.desiredX = self.editor->getXInLine(li, end);
    c.originalAnchorX = c.desiredX;
    c.isVirtual = false;
    
    self.editor->cursors.push_back(c);
    
    // システムへ変更通知は不要（システムが呼んでいるので）
    // 描画更新だけ行う
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

// --- カーソルや範囲の計算用スタブ群 ---
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
    
    // View上の座標に変換
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
        
        // 行内での開始・終了位置を特定
        size_t segStart = (line == startLine) ? start : lineStartIdx;
        size_t segEnd = (line == endLine) ? end : nextLineStartIdx;
        
        // 改行文字が含まれる場合の調整（通常、選択範囲の見た目は改行を含まない幅にする）
        if (segEnd > segStart && segEnd > 0) {
             // 簡易判定：本当はC++側で文字種判定すべきだが、ここではindexのみで計算
             // 必要であれば segEnd-- 等を行う
        }

        float x1 = self.editor->getXInLine(line, segStart);
        float x2 = self.editor->getXInLine(line, segEnd);
        
        // 空行やカーソルのみの場合でも最低限の幅を持たせる
        if (x2 < x1) x2 = x1;
        
        float w = x2 - x1;
        // 行選択などで行末まで選択されている場合は、見栄えのために少し幅を足す等の調整が可能
        if (line != endLine && w == 0) w = self.editor->charWidth / 2;

        // View座標系へ変換
        float drawX = self.editor->gutterWidth - self.editor->hScrollPos + x1;
        float drawY = (line - self.editor->vScrollPos) * self.editor->lineHeight;
        CGRect r = CGRectMake(drawX, drawY, w, self.editor->lineHeight);
        
        iOSSelectionRect *selRect = [[iOSSelectionRect alloc] init];
        selRect.rectValue = r;
        selRect.writingDirectionValue = NSWritingDirectionNatural;
        
        // ハンドルの位置決定フラグ
        selRect.containsStartValue = (line == startLine && segStart == start);
        selRect.containsEndValue = (line == endLine && segEnd == end);
        selRect.isVerticalValue = NO;
        
        [rects addObject:selRect];
    }
    
    return rects;
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point {
    if (!self.editor) return [self beginningOfDocument];
    
    // スクロール等を考慮した座標を C++エンジンに渡して index を取得
    // ※ pointはViewのローカル座標
    size_t idx = self.editor->getDocPosFromPoint(point.x, point.y);
    return [iOSTextPosition positionWithIndex:idx];
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange *)range { return [self.beginningOfDocument copy]; }
- (UITextRange *)characterRangeAtPoint:(CGPoint)point { return nil; }

- (CGRect)caretRectForPosition:(UITextPosition *)position {
    // スクロール中は非表示にするロジックは維持
    if (_isScrolling) {
        return CGRectZero;
    }
    
    if (!self.editor) return CGRectZero;

    // 1. テキスト位置(index)を取得
    size_t p = ((iOSTextPosition*)position).index;
    
    // 2. C++エンジンから行番号と行内X座標を取得
    int l = self.editor->getLineIdx(p);
    float x = self.editor->getXInLine(l, p);
    
    // 3. View上の描画座標（スクロールとガーター幅を考慮）に変換
    float drawX = self.editor->gutterWidth - self.editor->hScrollPos + x;
    float drawY = (l - self.editor->vScrollPos) * self.editor->lineHeight;
    
    // 4. カーソルの矩形を返す（幅は2.0程度が標準的）
    return CGRectMake(drawX, drawY, 0, 0);
}

// プログラミング用エディタの設定
- (UITextAutocorrectionType)autocorrectionType { return UITextAutocorrectionTypeNo; }
- (UITextSpellCheckingType)spellCheckingType {
    // コードエディタなのでスペルチェック（赤線や修正提案）を無効にする
    return UITextSpellCheckingTypeNo;
}
- (UITextAutocapitalizationType)autocapitalizationType { return UITextAutocapitalizationTypeNone; }
- (UITextSmartQuotesType)smartQuotesType { return UITextSmartQuotesTypeNo; }
- (UITextSmartDashesType)smartDashesType { return UITextSmartDashesTypeNo; }


- (NSUndoManager *)undoManager {
    return nil;
}

// 登録するショートカットキーの一覧
- (NSArray<UIKeyCommand *> *)keyCommands {
    NSArray<UIKeyCommand *> *commands = @[
        // Cmd + C, V, Zなど
        [UIKeyCommand keyCommandWithInput:@"a" modifierFlags:UIKeyModifierCommand action:@selector(selectAll:)],
        [UIKeyCommand keyCommandWithInput:@"c" modifierFlags:UIKeyModifierCommand action:@selector(copy:)],
        [UIKeyCommand keyCommandWithInput:@"x" modifierFlags:UIKeyModifierCommand action:@selector(cut:)],
        [UIKeyCommand keyCommandWithInput:@"v" modifierFlags:UIKeyModifierCommand action:@selector(paste:)],
        [UIKeyCommand keyCommandWithInput:@"z" modifierFlags:UIKeyModifierCommand action:@selector(handleUndo:)],
        [UIKeyCommand keyCommandWithInput:@"z" modifierFlags:UIKeyModifierCommand | UIKeyModifierShift action:@selector(handleRedo:)],
        
        // 矢印キー（単独：カーソル移動）
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:0 action:@selector(moveLeft:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:0 action:@selector(moveRight:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:0 action:@selector(moveUp:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:0 action:@selector(moveDown:)],
        
        // 矢印キー（Shift押下：テキスト選択）
        [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierShift action:@selector(moveLeftAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierShift action:@selector(moveRightAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierShift action:@selector(moveUpAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierShift action:@selector(moveDownAndSelect:)],
        
        // Mac標準: Home / End キー (文頭・文末へのジャンプ)
        [UIKeyCommand keyCommandWithInput:UIKeyInputHome modifierFlags:0 action:@selector(moveToDocStart:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputEnd modifierFlags:0 action:@selector(moveToDocEnd:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputHome modifierFlags:UIKeyModifierShift action:@selector(moveToDocStartAndSelect:)],
        [UIKeyCommand keyCommandWithInput:UIKeyInputEnd modifierFlags:UIKeyModifierShift action:@selector(moveToDocEndAndSelect:)],
        
        // Mac標準: Cmd + 矢印 (行頭/行末, 文頭/文末)
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
        
        // ズーム操作
        [UIKeyCommand keyCommandWithInput:@"+" modifierFlags:UIKeyModifierCommand action:@selector(zoomIn:)],
        [UIKeyCommand keyCommandWithInput:@"=" modifierFlags:UIKeyModifierCommand action:@selector(zoomIn:)],
        [UIKeyCommand keyCommandWithInput:@"-" modifierFlags:UIKeyModifierCommand action:@selector(zoomOut:)],
        [UIKeyCommand keyCommandWithInput:@"0" modifierFlags:UIKeyModifierCommand action:@selector(resetZoom:)]
    ];
    
    // 【最重要】iOSシステムによる矢印キーなどの横取りを禁止し、独自のコマンドを優先させる
    if (@available(iOS 15.0, *)) {
        for (UIKeyCommand *cmd in commands) {
            cmd.wantsPriorityOverSystemBehavior = YES;
        }
    }
    
    return commands;
}

// どのアクションが現在実行可能か（メニューのグレーアウト制御などに使われます）
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
    // 【追加】カーソル移動系のカスタムアクションをシステムに握りつぶさせないための許可リスト
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
    // 【修正】直接C++を呼ばず、ブロックを実行してViewControllerに判断を委ねる
    if (self.onNewDocumentAction) {
        self.onNewDocumentAction();
    }
}

// Cmd + A (すべて選択)
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

// ---------------------------------------------------
// MARK: - キーボードナビゲーション（カーソル移動・選択）
// ---------------------------------------------------

// 共通ヘルパー：指定した位置にカーソルを移動（Shift押下時は keepSelection=YES）
- (void)moveCursorTo:(size_t)newPos keepSelection:(BOOL)keep updateX:(BOOL)updateX {
    if (!self.editor || self.editor->cursors.empty()) return;
    [self.inputDelegate selectionWillChange:self];
    
    Cursor c = self.editor->cursors.back();
    c.head = newPos;
    if (!keep) {
        c.anchor = newPos; // Shiftを押していない場合は選択範囲をリセット
    }
    
    // 上下移動の際は元のX座標（desiredX）を記憶させておくための分岐
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

// 左右移動 (UTF-8を考慮したC++エンジンの moveCaretVisual を活用)
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

// 上下移動 (短い行から長い行へ移動した際もX座標がブレないように updateX:NO とする)
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

// Home / End (行頭・行末)
- (void)moveToHome:(id)sender { [self doMoveHome:NO]; }
- (void)moveToHomeAndSelect:(id)sender { [self doMoveHome:YES]; }
- (void)doMoveHome:(BOOL)keep {
    size_t head = self.editor->cursors.back().head;
    int li = self.editor->getLineIdx(head);
    size_t newPos = self.editor->lineStarts[li]; // その行の開始位置
    [self moveCursorTo:newPos keepSelection:keep updateX:YES];
}

- (void)moveToEnd:(id)sender { [self doMoveEnd:NO]; }
- (void)moveToEndAndSelect:(id)sender { [self doMoveEnd:YES]; }
- (void)doMoveEnd:(BOOL)keep {
    size_t head = self.editor->cursors.back().head;
    int li = self.editor->getLineIdx(head);
    size_t newPos = self.editor->getPosFromLineAndX(li, 999999.0f); // 巨大なX座標を渡して行末を取得
    [self moveCursorTo:newPos keepSelection:keep updateX:YES];
}

// DocStart / DocEnd (文頭・文末)
- (void)moveToDocStart:(id)sender { [self moveCursorTo:0 keepSelection:NO updateX:YES]; }
- (void)moveToDocStartAndSelect:(id)sender { [self moveCursorTo:0 keepSelection:YES updateX:YES]; }
- (void)moveToDocEnd:(id)sender { [self moveCursorTo:self.editor->pt.length() keepSelection:NO updateX:YES]; }
- (void)moveToDocEndAndSelect:(id)sender { [self moveCursorTo:self.editor->pt.length() keepSelection:YES updateX:YES]; }

- (void)openDocument:(id)sender {
    if (self.editor) self.editor->openFile(); // C++の cbOpenFile が呼ばれます
}

- (void)saveDocument:(id)sender {
    if (!self.editor) return;
    if (self.editor->currentFilePath.empty()) {
        self.editor->saveFileAs(); // まだ保存されていなければ「名前を付けて保存」へ
    } else {
        self.editor->saveFile(self.editor->currentFilePath); // 既存ファイルなら上書き保存
    }
}

- (void)saveDocumentAs:(id)sender {
    if (self.editor) self.editor->saveFileAs(); // C++の cbSaveFileAs が呼ばれます
}

// Cmd + C (コピー)
- (void)copy:(id)sender {
    if (!self.editor) return;
    std::string copiedText = "";
    
    // 選択されている文字列を抽出
    for (const auto& c : self.editor->cursors) {
        if (c.hasSelection()) {
            copiedText += self.editor->pt.getRange(c.start(), c.end() - c.start());
        }
    }
    
    // iOSの標準クリップボードに書き込む
    if (!copiedText.empty()) {
        [UIPasteboard generalPasteboard].string = [NSString stringWithUTF8String:copiedText.c_str()];
    }
}

// Cmd + X (カット)
- (void)cut:(id)sender {
    if (!self.editor) return;
    [self copy:sender];
    
    // システムに変更開始を通知
    [self.inputDelegate selectionWillChange:self]; // ★追加
    [self.inputDelegate textWillChange:self];
    
    // C++エンジンで削除（ここでカーソルが「範囲選択」から「一本線」になる）
    self.editor->backspaceAtCursors();
    
    // システムに変更終了を通知
    [self.inputDelegate textDidChange:self];
    [self.inputDelegate selectionDidChange:self]; // ★これが無いと青い範囲が消えません
    
    // 描画更新
    [self setNeedsDisplay];
    [self setNeedsLayout];
}

// Cmd + V (ペースト)
- (void)paste:(id)sender {
    if (!self.editor) return;
    NSString *pasteString = [UIPasteboard generalPasteboard].string;
    
    if (pasteString.length > 0) {
        [self.inputDelegate selectionWillChange:self]; // ★追加
        [self.inputDelegate textWillChange:self];
        
        self.editor->insertAtCursors([pasteString UTF8String]);
        
        [self.inputDelegate textDidChange:self];
        [self.inputDelegate selectionDidChange:self]; // ★追加
        
        [self setNeedsDisplay];
        [self setNeedsLayout];
    }
}

// Cmd + Z (取り消し)
- (void)handleUndo:(id)sender {
    if (!self.editor) return;
    [self.inputDelegate textWillChange:self];
    
    // C++エンジン側に実装されているUndo処理を呼び出す
    self.editor->performUndo();
    
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}

// Cmd + Shift + Z (やり直し)
- (void)handleRedo:(id)sender {
    if (!self.editor) return;
    [self.inputDelegate textWillChange:self];
    
    // C++エンジン側に実装されているRedo処理を呼び出す
    self.editor->performRedo();
    
    self.editor->ensureCaretVisible();
    [self.inputDelegate textDidChange:self];
    [self setNeedsDisplay];
}

- (void)zoomIn:(id)sender {
    if (!self.editor) return;
    // 現在のサイズ + 2px 拡大
    [self applyNewFontSize:self.editor->currentFontSize + 2.0f];
}

- (void)zoomOut:(id)sender {
    if (!self.editor) return;
    // 現在のサイズ - 2px 縮小
    [self applyNewFontSize:self.editor->currentFontSize - 2.0f];
}

- (void)resetZoom:(id)sender {
    if (!self.editor) return;
    // デフォルトサイズ（例: 14px）に戻す
    [self applyNewFontSize:14.0f];
}

// ---------------------------------------------------
// MARK: - 横スライド型 ソフトウェアキーボード用ツールバー (スリム＆タップ反応版)
// ---------------------------------------------------

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
    
    // ボタンを生成するヘルパー関数
    auto createBtn = ^UIButton*(NSString *title, SEL action) {
        UIButton *btn = [UIButton buttonWithType:UIButtonTypeCustom];
        
        // 【重要追加】StackViewの中で勝手に幅を広げたり縮めたりさせないための強力な制約
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
            
            // 【重要追加】ボタンの状態（タップ中かどうか）を監視して色を動的に変えるハンドラ
            btn.configurationUpdateHandler = ^(UIButton *updatedBtn) {
                UIButtonConfiguration *updatedConfig = updatedBtn.configuration;
                if (updatedBtn.isHighlighted) {
                    // タップされた瞬間の色（ライトモード:少し濃いグレー / ダークモード:少し明るい白）
                    updatedConfig.background.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
                        return trait.userInterfaceStyle == UIUserInterfaceStyleDark ? [UIColor colorWithWhite:1.0 alpha:0.3] : [UIColor colorWithWhite:0.0 alpha:0.15];
                    }];
                } else {
                    // 通常時の色
                    updatedConfig.background.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
                        return trait.userInterfaceStyle == UIUserInterfaceStyleDark ? [UIColor colorWithWhite:1.0 alpha:0.1] : [UIColor colorWithWhite:0.0 alpha:0.05];
                    }];
                }
                updatedBtn.configuration = updatedConfig;
            };
        } else {
            // iOS 14以前のフォールバック
            [btn setTitle:title forState:UIControlStateNormal];
            [btn setTitleColor:[UIColor labelColor] forState:UIControlStateNormal];
            btn.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightMedium];
            btn.backgroundColor = [UIColor colorWithWhite:0.5 alpha:0.2];
            btn.layer.cornerRadius = 0;
            btn.contentEdgeInsets = UIEdgeInsetsMake(0, 10, 0, 10);
            btn.showsTouchWhenHighlighted = YES; // タップ時の光るエフェクト
        }
        
        [btn addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
        return btn;
    };
    
    // ボタンの追加（ここで追加した数だけ横に繋がっていきます）
    [stackView addArrangedSubview:createBtn(@"新規", @selector(newDocument:))];
    [stackView addArrangedSubview:createBtn(@"開く", @selector(openDocument:))];
    [stackView addArrangedSubview:createBtn(@"保存", @selector(saveDocument:))];
    [stackView addArrangedSubview:createBtn(@"名前をつけて保存", @selector(saveDocumentAs:))];
    [stackView addArrangedSubview:createBtn(@"検索", @selector(cmdFind:))];
    [stackView addArrangedSubview:createBtn(@"取り消し", @selector(handleUndo:))];
    [stackView addArrangedSubview:createBtn(@"やり直し", @selector(handleRedo:))];
    [stackView addArrangedSubview:createBtn(@"すべて選択", @selector(selectAll:))];
    [stackView addArrangedSubview:createBtn(@"切り取り", @selector(cut:))];
    [stackView addArrangedSubview:createBtn(@"コピー", @selector(copy:))];
    [stackView addArrangedSubview:createBtn(@"ペースト", @selector(paste:))];
    [stackView addArrangedSubview:createBtn(@"完了", @selector(dismissKeyboard))];
    
    return _customAccessoryView;
}

// 完了ボタンが押された時にキーボードをしまう処理
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
    self.titleLabel.text = @"Untitled";
    [self.titleBarView addSubview:self.titleLabel];
    
    // ---------------------------------------------------
    // ★ ステータスバーとタイトルバーを一体化させる背景View
    // ---------------------------------------------------
    UIView *topFillView = [[UIView alloc] init];
    topFillView.translatesAutoresizingMaskIntoConstraints = NO;
    topFillView.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
            return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor blackColor] : [UIColor secondarySystemBackgroundColor];
        }];
    [self.view addSubview:topFillView];
    [self.view insertSubview:topFillView belowSubview:self.headerStack];

    // ---------------------------------------------------
    // 3. 検索バーの構築
    // ---------------------------------------------------
    [self setupSearchUI];
    
    // ---------------------------------------------------
    // 4. エディタビューの作成
    // ---------------------------------------------------
    self.editorView = [[iOSEditorView alloc] initWithFrame:CGRectZero];
    self.editorView.translatesAutoresizingMaskIntoConstraints = NO;
    self.editorView.backgroundColor = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *trait) {
            return (trait.userInterfaceStyle == UIUserInterfaceStyleDark) ? [UIColor blackColor] : [UIColor colorNamed:@"EditorBgColor"]; // Assetsに無い場合は systemBackgroundColor 等へ
        }];
    [self.view addSubview:self.editorView];
    [self.view sendSubviewToBack:self.editorView];
    
    // ---------------------------------------------------
    // 5. AutoLayout の制約設定
    // ---------------------------------------------------
    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    
    _editorBottomConstraint = [self.editorView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor];
    
    [NSLayoutConstraint activateConstraints:@[
        // 背景Viewの制約
        [topFillView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [topFillView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [topFillView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [topFillView.bottomAnchor constraintEqualToAnchor:self.titleBarView.bottomAnchor],
        
        // ---【修正】ヘッダー位置の調整 ---
        // constant: -8 を設定して、セーフエリアよりも少し上に食い込ませる
        [self.headerStack.topAnchor constraintEqualToAnchor:safeArea.topAnchor constant:-12],
        [self.headerStack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.headerStack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        
        // ---【修正】タイトルバーの高さをさらに詰める (20 -> 18) ---
        [self.titleBarView.heightAnchor constraintEqualToConstant:17],
        
        // ラベルを上部揃え（余白なし）
        [self.titleLabel.centerXAnchor constraintEqualToAnchor:self.titleBarView.centerXAnchor],
        [self.titleLabel.topAnchor constraintEqualToAnchor:self.titleBarView.topAnchor],
        
        // エディタビュー
        [self.editorView.topAnchor constraintEqualToAnchor:self.headerStack.bottomAnchor],
        _editorBottomConstraint,
        [self.editorView.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor],
        [self.editorView.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor]
    ]];
    
    // ---------------------------------------------------
    // 6. C++エンジン(EditorCore)の初期化とコールバック登録
    // ---------------------------------------------------
    _editorEngine = std::make_shared<Editor>();
    __weak typeof(self) weakSelf = self;
    
    _editorEngine->cbUpdateTitleBar = [weakSelf]() {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(self) strongSelf = weakSelf;
            if (strongSelf && strongSelf->_editorEngine) {
                NSString *fileName = @"Untitled";
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
    
    // ---------------------------------------------------
    // 7. テーマの反映と描画エンジンの初期起動
    // ---------------------------------------------------
    BOOL isDark = (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
    _editorEngine->isDarkMode = isDark;
    _editorEngine->initGraphics();
    self.editorView.editor = _editorEngine.get();
    
    self.editorView.onNewDocumentAction = ^{
        [weakSelf confirmSaveIfNeededWithAction:^{
            [weakSelf performNewDocument];
        }];
    };
    
    // ---------------------------------------------------
    // 8. 各種通知の登録
    // ---------------------------------------------------
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillShow:) name:UIKeyboardWillShowNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillHide:) name:UIKeyboardWillHideNotification object:nil];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showSearchNotif) name:@"miuShowSearch" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(showReplaceNotif) name:@"miuShowReplace" object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(closeSearch) name:@"miuCloseSearch" object:nil];
    
    if (@available(iOS 17.0, *)) {
        [self registerForTraitChanges:@[[UITraitUserInterfaceStyle class]] withAction:@selector(updateThemeIfNeeded)];
    }
}
// ★【追加】: 未保存なら確認ダイアログを出し、その後 action を実行するメソッド
- (void)confirmSaveIfNeededWithAction:(void(^)(void))action {
    // 未保存でなければ即実行
    if (!_editorEngine || !_editorEngine->isDirty) {
        action();
        return;
    }
    
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"未保存の変更"
                                                                 message:@"変更内容を保存しますか？"
                                                          preferredStyle:UIAlertControllerStyleAlert];
    
    // 「保存する」
    [alert addAction:[UIAlertAction actionWithTitle:@"保存" style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull alertAction) {
        // パスがあれば上書き保存して次へ、なければ「名前をつけて保存」画面へ（その場合actionは中断される）
        if (!self->_editorEngine->currentFilePath.empty()) {
            self->_editorEngine->saveFile(self->_editorEngine->currentFilePath);
            action(); // 保存後に本来の処理を実行
        } else {
            // Untitledの場合は保存ダイアログを出す必要があるため、一旦フローを中断して保存画面へ遷移
            [self presentDocumentPickerForSaving];
            // ユーザーが保存完了後に再度「新規」や「開く」を押してもらう形になります（シンプル化のため）
        }
    }]];
    
    // 「保存しない」
    [alert addAction:[UIAlertAction actionWithTitle:@"保存しない" style:UIAlertActionStyleDestructive handler:^(UIAlertAction * _Nonnull alertAction) {
        action(); // 保存せずに本来の処理を実行
    }]];
    
    // 「キャンセル」
    [alert addAction:[UIAlertAction actionWithTitle:@"キャンセル" style:UIAlertActionStyleCancel handler:nil]];
    
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
    
    // 画面が表示されたら即座にエディタにフォーカスを当てる
    if (self.editorView) {
        [self.editorView becomeFirstResponder];
    }
}

- (void)keyboardWillShow:(NSNotification *)notification {
    NSDictionary *userInfo = notification.userInfo;
    CGRect keyboardFrame = [userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    
    // 【修正】bottomAnchorをself.view.bottomAnchorにしたため、キーボードの高さそのものを引く
    CGFloat shrinkAmount = keyboardFrame.size.height;
    
    // システムのアニメーションを強制的に無効化するブロック
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
    // システムのアニメーションを強制的に無効化するブロック
    [UIView performWithoutAnimation:^{
        _editorBottomConstraint.constant = 0;
        [self.view layoutIfNeeded];
        
        if (_editorEngine) {
            _editorEngine->ensureCaretVisible();
            [self.editorView setNeedsDisplay];
        }
    }];
}

// 共通のテーマ更新処理
- (void)updateThemeIfNeeded {
    if (_editorEngine) {
        BOOL isDark = (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
        _editorEngine->isDarkMode = isDark;
        _editorEngine->updateThemeColors();
        [self.editorView setNeedsDisplay];
    }
}

// ---------------------------------------------------
// MARK: - ファイルピッカー（UIDocumentPickerViewController）連携: iOS 14+ 対応版
// ---------------------------------------------------

// 「ファイルを開く」画面を表示
- (void)presentDocumentPickerForOpening {
    _isExportingDocument = NO; // 「開く」モードとして記録
    
    // public.text や public.source-code を UTType で指定
    NSArray<UTType *> *types = @[UTTypePlainText, UTTypeSourceCode, UTTypeData, UTTypeContent];
    
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:types asCopy:NO];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}

// 「名前を付けて保存」画面を表示
- (void)presentDocumentPickerForSaving {
    if (!_editorEngine) return;
    _isExportingDocument = YES; // 「保存」モードとして記録
    
    // 1. C++エンジンから現在のエディタのテキストをすべて取り出す
    std::string content = _editorEngine->pt.getRange(0, _editorEngine->pt.length());
    NSData *data = [NSData dataWithBytes:content.c_str() length:content.length()];
    
    // 2. iOSの一時フォルダ(tmp)に、ダミーファイルとして書き出す
    std::string utf8Name = WToUTF8(_editorEngine->currentFilePath);
    if (utf8Name.empty()) utf8Name = "untitled.txt"; // 新規作成時のデフォルト名
    NSString *fileName = [[NSString stringWithUTF8String:utf8Name.c_str()] lastPathComponent];
    NSString *tempFilePath = [NSTemporaryDirectory() stringByAppendingPathComponent:fileName];
    [data writeToFile:tempFilePath atomically:YES];
    
    // 3. その一時ファイルを「エクスポート（保存）」対象としてピッカーに渡す
    NSURL *tempURL = [NSURL fileURLWithPath:tempFilePath];
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[tempURL] asCopy:YES];
    picker.delegate = self;
    [self presentViewController:picker animated:YES completion:nil];
}

// ピッカーでファイルが選択された、または保存が完了した時のデリゲートメソッド
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *url = urls.firstObject;
    if (!url || !_editorEngine) return;
    
    // 以前開いていたファイルのアクセス権（通行証）を返却する
    if (self.currentDocumentURL) {
        [self.currentDocumentURL stopAccessingSecurityScopedResource];
    }
    
    // 新しいファイルのアクセス権（通行証）を受け取る（失敗した場合はnilにする）
    BOOL accessGranted = [url startAccessingSecurityScopedResource];
    self.currentDocumentURL = accessGranted ? url : nil;
    
    if (!_isExportingDocument) {
        // --- ファイルを開く ---
        std::string path = url.path.UTF8String;
        bool success = _editorEngine->openFileFromPath(path);
        
        if (success) {
            _editorEngine->vScrollPos = 0;
            _editorEngine->hScrollPos = 0;
            _editorEngine->initGraphics();
            [self.editorView setNeedsDisplay];
        }
        
    } else {
        // --- 保存が完了した ---
        // ユーザーが指定した保存先URLが返ってくるので、それをC++エンジンに登録して上書き保存（確定）させる
        std::string path = url.path.UTF8String;
        std::wstring wpath = UTF8ToW(path);
        _editorEngine->saveFile(wpath);
    }
}

// ---------------------------------------------------
// MARK: - 検索・置換 UIと処理
// ---------------------------------------------------

- (void)setupSearchUI {
    self.searchContainer = [[UIView alloc] init];
    self.searchContainer.backgroundColor = [UIColor secondarySystemBackgroundColor];
    self.searchContainer.hidden = YES; // 初期状態は非表示
    
    // 【変更】影（シャドウ）を廃止し、区切りの境界線を引く
    UIView *bottomLine = [[UIView alloc] init];
    bottomLine.backgroundColor = [UIColor separatorColor];
    bottomLine.translatesAutoresizingMaskIntoConstraints = NO;
    [self.searchContainer addSubview:bottomLine];
    
    // 【重要】親のViewではなく、headerStack に追加して連動させる
    [self.headerStack addArrangedSubview:self.searchContainer];
    
    UIStackView *mainStack = [[UIStackView alloc] init];
    mainStack.axis = UILayoutConstraintAxisVertical;
    mainStack.spacing = 8;
    mainStack.translatesAutoresizingMaskIntoConstraints = NO;
    mainStack.layoutMargins = UIEdgeInsetsMake(8, 12, 8, 12);
    mainStack.layoutMarginsRelativeArrangement = YES;
    [self.searchContainer addSubview:mainStack];
    
    [NSLayoutConstraint activateConstraints:@[
        // 下線の制約
        [bottomLine.bottomAnchor constraintEqualToAnchor:self.searchContainer.bottomAnchor],
        [bottomLine.leadingAnchor constraintEqualToAnchor:self.searchContainer.leadingAnchor],
        [bottomLine.trailingAnchor constraintEqualToAnchor:self.searchContainer.trailingAnchor],
        [bottomLine.heightAnchor constraintEqualToConstant:0.5],
        
        // メインスタックの制約
        [mainStack.topAnchor constraintEqualToAnchor:self.searchContainer.topAnchor],
        [mainStack.bottomAnchor constraintEqualToAnchor:self.searchContainer.bottomAnchor],
        [mainStack.leadingAnchor constraintEqualToAnchor:self.searchContainer.leadingAnchor],
        [mainStack.trailingAnchor constraintEqualToAnchor:self.searchContainer.trailingAnchor]
    ]];
    
    // 共通テキストフィールド生成ブロック
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
    
    // 共通ボタン生成ブロック
    auto createBtn = ^UIButton*(NSString *title, SEL action) {
        UIButton *btn = [UIButton buttonWithType:UIButtonTypeSystem];
        [btn setTitle:title forState:UIControlStateNormal];
        [btn addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
        [btn setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        return btn;
    };
    
    // --- 1段目：検索行 ---
    self.searchField = createField(@"検索...");
    // 【追加】文字が入力・削除されるたびに searchTextChanged: を呼び出す
    [self.searchField addTarget:self action:@selector(searchTextChanged:) forControlEvents:UIControlEventEditingChanged];
    
    UIStackView *searchRow = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.searchField,
        createBtn(@"<", @selector(doFindPrev)),
        createBtn(@">", @selector(doFindNext)),
        createBtn(@"完了", @selector(closeSearch))
    ]];

    searchRow.spacing = 12;
    [mainStack addArrangedSubview:searchRow];
    
    // --- 2段目：置換行 ---
    self.replaceField = createField(@"置換...");
    self.replaceField.returnKeyType = UIReturnKeyDone;
    self.replaceRowStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.replaceField,
        createBtn(@"置換", @selector(doReplace)),
        createBtn(@"すべて", @selector(doReplaceAll))
    ]];
    self.replaceRowStack.spacing = 12;
    [mainStack addArrangedSubview:self.replaceRowStack];
}

// 【変更】UI表示のトリガー（滑らかなアニメーションでエディタを押し出す）
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
        // 【追加】検索バーを閉じる時にハイライトを消す
        _editorEngine->searchQuery = "";
    }
    [self.view endEditing:YES]; // キーボードを閉じる
    
    [UIView animateWithDuration:0.25 animations:^{
        self.searchContainer.hidden = YES;
        self.searchField.text = @""; // 【追加】テキストフィールドも空に戻す
        [self.view layoutIfNeeded];
    } completion:^(BOOL finished) {
        [self.editorView setNeedsDisplay]; // 【追加】ハイライトを消すために再描画
        [self.editorView becomeFirstResponder]; // エディタにフォーカスを戻す
    }];
}

// C++エンジン連携: エンターキー(Return)が押された時
- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    if (textField == self.searchField) {
        [self doFindNext];
    } else if (textField == self.replaceField) {
        [self doReplace];
    }
    return YES;
}

// 検索窓の文字が変更されるたびに呼ばれる
- (void)searchTextChanged:(UITextField *)sender {
    if (!_editorEngine) return;
    
    // 入力された文字をUTF8に変換してC++エンジンに渡し、即座に画面を再描画する
    _editorEngine->searchQuery = sender.text ? sender.text.UTF8String : "";
    [self.editorView setNeedsDisplay];
}

// C++エンジン連携: アクション実行
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
