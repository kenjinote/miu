#define MIU_MACOS_TESTING 1
#import "../miu_macOS/main.mm"
#import <QuartzCore/QuartzCore.h>
#include <iostream>

struct PixelRGBA {
    int r = -1, g = -1, b = -1, a = -1;
};

static std::string colorToStr(CGColorRef c) {
    if (!c) return "nil";
    NSColor *nsc = [NSColor colorWithCGColor:c];
    if (!nsc) return "unknown";
    NSColor *srgb = [nsc colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!srgb) srgb = nsc;
    char buf[128];
    snprintf(buf, sizeof(buf), "rgba(%.3f, %.3f, %.3f, %.3f)",
             srgb.redComponent, srgb.greenComponent, srgb.blueComponent, srgb.alphaComponent);
    return buf;
}

static void dumpLayerHierarchy(CALayer *l, int indent = 0) {
    if (!l) return;
    std::string pad(indent * 2, ' ');
    CGRect f = l.frame;
    CGRect b = l.bounds;
    std::cout << pad << "[LAYER] " << [NSStringFromClass([l class]) UTF8String]
              << " name=" << (l.name ? [l.name UTF8String] : "nil")
              << " delegate=" << (l.delegate ? [NSStringFromClass([l.delegate class]) UTF8String] : "nil")
              << " frame=(" << f.origin.x << "," << f.origin.y << "," << f.size.width << "," << f.size.height << ")"
              << " bounds=(" << b.origin.x << "," << b.origin.y << "," << b.size.width << "," << b.size.height << ")"
              << " hidden=" << (l.isHidden ? "YES" : "NO")
              << " opaque=" << (l.isOpaque ? "YES" : "NO")
              << " masksToBounds=" << (l.masksToBounds ? "YES" : "NO")
              << " bg=" << colorToStr(l.backgroundColor)
              << std::endl;
    for (CALayer *sub in l.sublayers) {
        dumpLayerHierarchy(sub, indent + 1);
    }
}

static PixelRGBA sampleViewPixel(NSView *targetView, int x, int y) {
    PixelRGBA out;
    if (!targetView) return out;
    [targetView layoutSubtreeIfNeeded];
    [targetView displayIfNeeded];
    NSBitmapImageRep *rep = [targetView bitmapImageRepForCachingDisplayInRect:targetView.bounds];
    if (!rep) return out;
    [targetView cacheDisplayInRect:targetView.bounds toBitmapImageRep:rep];
    NSColor *c = [rep colorAtX:x y:y];
    if (!c) return out;
    NSColor *srgb = [c colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!srgb) srgb = c;
    out.r = (int)std::round(srgb.redComponent * 255.0);
    out.g = (int)std::round(srgb.greenComponent * 255.0);
    out.b = (int)std::round(srgb.blueComponent * 255.0);
    out.a = (int)std::round(srgb.alphaComponent * 255.0);
    return out;
}

static PixelRGBA sampleLayerPixel(CALayer *layer, int x, int y) {
    PixelRGBA out;
    if (!layer) return out;
    int w = (int)layer.bounds.size.width;
    int h = (int)layer.bounds.size.height;
    if (w <= 0 || h <= 0) return out;
    std::vector<uint8_t> pixels(w * h * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(pixels.data(), w, h, 8, w * 4, cs, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    if (!ctx) return out;
    [layer renderInContext:ctx];
    CGContextRelease(ctx);
    int cx = std::clamp(x, 0, w - 1);
    int cy = std::clamp(y, 0, h - 1);
    size_t idx = ((size_t)cy * w + cx) * 4;
    out.r = pixels[idx];
    out.g = pixels[idx + 1];
    out.b = pixels[idx + 2];
    out.a = pixels[idx + 3];
    return out;
}

static PixelRGBA sampleWindowPixel(NSWindow *win, int x, int y) {
    PixelRGBA out;
    if (!win) return out;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CGImageRef img = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, (CGWindowID)[win windowNumber], kCGWindowImageBoundsIgnoreFraming);
#pragma clang diagnostic pop
    if (!img) return out;
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:img];
    CGImageRelease(img);
    NSColor *c = [rep colorAtX:x y:y];
    if (!c) return out;
    NSColor *srgb = [c colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!srgb) srgb = c;
    out.r = (int)std::round(srgb.redComponent * 255.0);
    out.g = (int)std::round(srgb.greenComponent * 255.0);
    out.b = (int)std::round(srgb.blueComponent * 255.0);
    out.a = (int)std::round(srgb.alphaComponent * 255.0);
    return out;
}

static void printPixel(const char *label, int x, int y, const PixelRGBA &p) {
    std::cout << "[" << label << "] (" << x << "," << y << ") -> R=" << p.r << " G=" << p.g << " B=" << p.b << " A=" << p.a << std::endl;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        AppDelegate *appDelegate = [AppDelegate new];
        [appDelegate setupMenuBar];
        [appDelegate createWindowWithPath:nullptr];
        if (appDelegate.windows.count == 0) {
            std::cerr << "[FAIL] Failed to create CustomWindow" << std::endl;
            return 1;
        }
        CustomWindow *win = appDelegate.windows.firstObject;
        EditorView *ev = [appDelegate findEditorViewInWindow:win];
        if (!ev || !ev->editor) {
            std::cerr << "[FAIL] Failed to find EditorView in CustomWindow" << std::endl;
            return 1;
        }

        [win setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
        ev->editor->isDarkMode = true;
        ev->editor->updateThemeColors();
        [ev updatePreviewBackground];

        // 1. Verify initial state (preview off, help popup on, frosted-glass NSVisualEffectView active)
        if (![win.contentView isKindOfClass:[NSVisualEffectView class]]) {
            std::cerr << "[FAIL] win.contentView should be NSVisualEffectView for frosted-glass background" << std::endl;
            return 1;
        }
        NSVisualEffectView *vev = (NSVisualEffectView *)win.contentView;
        if (vev.material != NSVisualEffectMaterialSidebar || vev.blendingMode != NSVisualEffectBlendingModeBehindWindow) {
            std::cerr << "[FAIL] NSVisualEffectView material/blendingMode does not match frosted-glass configuration" << std::endl;
            return 1;
        }
        if (ev.isOpaque || CGColorGetAlpha(ev->editor->colBackground) != 0.0 || CGColorGetAlpha(ev->editor->colGutterBg) != 0.0) {
            std::cerr << "[FAIL] EditorView and colBackground/colGutterBg must be transparent on macOS so frosted glass shows through" << std::endl;
            return 1;
        }
        if (ev->editor->showPreview || !ev->editor->showHelpPopup) {
            std::cerr << "[FAIL] Unexpected initial state before togglePreviewAction" << std::endl;
            return 1;
        }

        // 2. Toggle Preview Mode ON in Dark Mode
        [ev togglePreviewAction:nil];
        if (!ev->editor->showPreview || ev->editor->showHelpPopup) {
            std::cerr << "[FAIL] togglePreviewAction did not enable showPreview or hide showHelpPopup" << std::endl;
            return 1;
        }
        if (!ev->previewWebView || ev->previewWebView.isHidden || !ev->splitterView || ev->splitterView.isHidden) {
            std::cerr << "[FAIL] previewWebView or splitterView is nil or hidden after togglePreviewAction" << std::endl;
            return 1;
        }
        if (ev.frame.size.width < 100.0 || ev->previewWebView.frame.size.width < 100.0) {
            std::cerr << "[FAIL] Split frames have invalid widths: editor=" << ev.frame.size.width
                      << " preview=" << ev->previewWebView.frame.size.width << std::endl;
            return 1;
        }
        id drawsBg = [ev->previewWebView valueForKey:@"drawsBackground"];
        if (drawsBg && ![drawsBg boolValue]) {
            std::cerr << "[FAIL] previewWebView drawsBackground is NO (causes gray window on macOS)" << std::endl;
            return 1;
        }

        // 3. Insert Markdown content while initial loadHTMLString is in flight (tests race condition fix)
        ev->editor->insertAtCursors("# Hello macOS Preview\n\n- [x] Fixed gray window\n");

        // 4. Pump runloop until WKWebView navigation completes and DOM + dark background are updated
        NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:10.0];
        __block NSString *renderedHtml = nil;
        __block NSString *bodyBg = nil;
        while ([deadline timeIntervalSinceNow] > 0) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
            if (ev->previewMsgHandler && ev->previewMsgHandler.previewPageLoaded) {
                __block BOOL evalDone = NO;
                [ev->previewWebView evaluateJavaScript:@"(document.getElementById('__miu_root') ? document.getElementById('__miu_root').innerHTML : '') + '|||' + window.getComputedStyle(document.body).backgroundColor"
                                     completionHandler:^(id result, NSError *error) {
                    if ([result isKindOfClass:[NSString class]]) {
                        NSArray<NSString *> *parts = [(NSString *)result componentsSeparatedByString:@"|||"];
                        if (parts.count >= 2) {
                            renderedHtml = parts[0];
                            bodyBg = parts[1];
                        }
                    }
                    evalDone = YES;
                }];
                NSDate *evalDeadline = [NSDate dateWithTimeIntervalSinceNow:2.0];
                while (!evalDone && [evalDeadline timeIntervalSinceNow] > 0) {
                    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
                }
                if (renderedHtml && [renderedHtml containsString:@"Hello macOS Preview"] &&
                    bodyBg && [bodyBg isEqualToString:@"rgb(0, 0, 0)"]) {
                    break;
                }
            }
        }

        // Wait an extra 0.3s for compositor/layers to settle
        NSDate *postWait = [NSDate dateWithTimeIntervalSinceNow:0.3];
        while ([postWait timeIntervalSinceNow] > 0) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        }

        NSView *themeFrame = win.contentView.superview ? win.contentView.superview : win.contentView;
        std::cout << "=== AFTER PREVIEW TOGGLE (DARK MODE) ===" << std::endl;
        dumpLayerHierarchy(themeFrame.layer);

        if (ev.isOpaque || ev.layer.opaque || ev->splitterView.isOpaque || ev->splitterView.layer.opaque) {
            std::cerr << "[FAIL] EditorView and MiuSplitterView must be non-opaque so frosted glass shows through" << std::endl;
            return 1;
        }
        if (!ev->splitterView.layer.masksToBounds || !ev.layer.masksToBounds || !ev->previewWebView.layer.masksToBounds) {
            std::cerr << "[FAIL] splitterView, EditorView, or previewWebView layer masksToBounds is NO (causes full-window ContentLayer bleed)" << std::endl;
            return 1;
        }
        if ((int)std::round(ev->splitterView.bounds.size.width) != 4) {
            std::cerr << "[FAIL] splitterView bounds width should be 4pt matching subtle Windows splitter" << std::endl;
            return 1;
        }
        for (CALayer *sub in ev->splitterView.layer.sublayers) {
            if (sub.frame.size.width > ev->splitterView.bounds.size.width + 1.0) {
                std::cerr << "[FAIL] splitterView sublayer width (" << sub.frame.size.width << ") exceeds splitter bounds (" << ev->splitterView.bounds.size.width << ")" << std::endl;
                return 1;
            }
        }

        // 5. Verify actual rendered pixels in Dark Mode:
        //    - EditorView background pixel is transparent (A=0) for frosted glass and has no gray overlay
        //    - MiuSplitterView draws only a subtle 1pt hairline at x=3 (A ~ 13) and is transparent at x=1 (A=0)
        //    - Preview pane pixel is pure black R=0,G=0,B=0,A=255
        PixelRGBA evDirectDark = sampleViewPixel(ev, 200, 300);
        PixelRGBA splitPadDark = sampleViewPixel(ev->splitterView, 1, 300);
        PixelRGBA splitLineDark = sampleViewPixel(ev->splitterView, 3, 300);
        PixelRGBA cvPreview = sampleViewPixel(win.contentView, 650, 300);
        PixelRGBA lyPreview = sampleLayerPixel(themeFrame.layer, 650, 300);
        PixelRGBA winPreview = sampleWindowPixel(win, 650, 300);

        printPixel("DARK_EDITOR_VIEW_TRANSPARENT_BG", 200, 300, evDirectDark);
        printPixel("DARK_SPLITTER_IDLE_PAD", 1, 300, splitPadDark);
        printPixel("DARK_SPLITTER_1PT_LINE", 3, 300, splitLineDark);
        printPixel("DARK_CONTENT_VIEW_PREVIEW", 650, 300, cvPreview);
        printPixel("DARK_LAYER_PREVIEW", 650, 300, lyPreview);
        printPixel("DARK_WIN_PREVIEW", 650, 300, winPreview);

        auto isPureBlack = [](const PixelRGBA &p) {
            return p.r == 0 && p.g == 0 && p.b == 0 && p.a == 255;
        };
        if (evDirectDark.a != 0) {
            std::cerr << "[FAIL] Dark mode EditorView background pixel should be transparent (A=0) for frosted glass!" << std::endl;
            return 1;
        }
        if (splitPadDark.a != 0 || splitLineDark.a < 8 || splitLineDark.a > 22) {
            std::cerr << "[FAIL] Dark mode MiuSplitterView did not match subtle 1pt hairline (rgba(255,255,255,0.05))!" << std::endl;
            return 1;
        }
        if (!isPureBlack(cvPreview) || !isPureBlack(lyPreview) || (winPreview.a != -1 && !isPureBlack(winPreview))) {
            std::cerr << "[FAIL] Dark mode Preview pixels are not pure black (gray overlay detected)!" << std::endl;
            return 1;
        }

        // 6. Verify Light Mode rendered pixels as well:
        //    - EditorView background pixel is transparent (A=0) for frosted glass
        //    - MiuSplitterView draws only a subtle 1pt hairline at x=3 (A ~ 15) and is transparent at x=1 (A=0)
        //    - Preview pane pixel is pure white R=255,G=255,B=255,A=255
        [win setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
        ev->editor->isDarkMode = false;
        ev->editor->updateThemeColors();
        [ev updatePreviewBackground];
        ev->editor->previewInitialLoaded = false;
        [ev updatePreviewPane];
        NSDate *lightDeadline = [NSDate dateWithTimeIntervalSinceNow:5.0];
        while ([lightDeadline timeIntervalSinceNow] > 0) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
            if (ev->previewMsgHandler && ev->previewMsgHandler.previewPageLoaded) break;
        }
        NSDate *lightWait = [NSDate dateWithTimeIntervalSinceNow:0.3];
        while ([lightWait timeIntervalSinceNow] > 0) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        }
        PixelRGBA evDirectLight = sampleViewPixel(ev, 200, 300);
        PixelRGBA splitPadLight = sampleViewPixel(ev->splitterView, 1, 300);
        PixelRGBA splitLineLight = sampleViewPixel(ev->splitterView, 3, 300);
        PixelRGBA lightCvPreview = sampleViewPixel(win.contentView, 650, 300);
        PixelRGBA lightLyPreview = sampleLayerPixel(themeFrame.layer, 650, 300);
        printPixel("LIGHT_EDITOR_VIEW_TRANSPARENT_BG", 200, 300, evDirectLight);
        printPixel("LIGHT_SPLITTER_IDLE_PAD", 1, 300, splitPadLight);
        printPixel("LIGHT_SPLITTER_1PT_LINE", 3, 300, splitLineLight);
        printPixel("LIGHT_CONTENT_VIEW_PREVIEW", 650, 300, lightCvPreview);
        printPixel("LIGHT_LAYER_PREVIEW", 650, 300, lightLyPreview);
        auto isPureWhite = [](const PixelRGBA &p) {
            return p.r == 255 && p.g == 255 && p.b == 255 && p.a == 255;
        };
        if (evDirectLight.a != 0) {
            std::cerr << "[FAIL] Light mode EditorView background pixel should be transparent (A=0) for frosted glass!" << std::endl;
            return 1;
        }
        if (splitPadLight.a != 0 || splitLineLight.a < 10 || splitLineLight.a > 24) {
            std::cerr << "[FAIL] Light mode MiuSplitterView did not match subtle 1pt hairline (rgba(0,0,0,0.06))!" << std::endl;
            return 1;
        }
        if (!isPureWhite(lightCvPreview) || !isPureWhite(lightLyPreview)) {
            std::cerr << "[FAIL] Light mode Preview pixels are not pure white (gray overlay detected)!" << std::endl;
            return 1;
        }

        if (!ev->previewMsgHandler || !ev->previewMsgHandler.previewPageLoaded) {
            std::cerr << "[FAIL] WKWebView did not finish navigation within timeout" << std::endl;
            return 1;
        }
        if (!renderedHtml || ![renderedHtml containsString:@"Hello macOS Preview"]) {
            std::cerr << "[FAIL] WKWebView DOM did not contain expected Markdown heading. Got: "
                      << (renderedHtml ? [renderedHtml UTF8String] : "(null)") << std::endl;
            return 1;
        }
        if (!bodyBg || ![bodyBg isEqualToString:@"rgb(0, 0, 0)"]) {
            std::cerr << "[FAIL] Dark mode WKWebView body background is not pure black rgb(0, 0, 0). Got: "
                      << (bodyBg ? [bodyBg UTF8String] : "(null)") << std::endl;
            return 1;
        }

        // 7. Verify scrollbars are HIDDEN when content fits within the window
        if (!ev->vScroller.isHidden || !ev->hScroller.isHidden) {
            std::cerr << "[FAIL] Editor scrollbars should be hidden when short content fits in window" << std::endl;
            return 1;
        }
        __block NSString *shortSbState = nil;
        {
            __block BOOL evalDone = NO;
            [ev->previewWebView evaluateJavaScript:@"(function(){var v=document.querySelector('.__ag_sb_v'),h=document.querySelector('.__ag_sb_h');return (v?v.getAttribute('data-visible'):'none')+':'+(h?h.getAttribute('data-visible'):'none');})()"
                                 completionHandler:^(id result, NSError *error) {
                if ([result isKindOfClass:[NSString class]]) shortSbState = (NSString *)result;
                evalDone = YES;
            }];
            NSDate *evalWait = [NSDate dateWithTimeIntervalSinceNow:2.0];
            while (!evalDone && [evalWait timeIntervalSinceNow] > 0) {
                [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
            }
        }
        std::cout << "[SHORT_CONTENT_PREVIEW_SCROLLBAR] state=" << (shortSbState ? [shortSbState UTF8String] : "nil") << std::endl;
        if (!shortSbState || ![shortSbState isEqualToString:@"false:false"]) {
            std::cerr << "[FAIL] Preview scrollbars should be hidden (false:false) when content fits in window. Got: "
                      << (shortSbState ? [shortSbState UTF8String] : "nil") << std::endl;
            return 1;
        }

        // 8. Insert long content that overflows vertically & horizontally and verify BOTH Editor and Preview show matching scrollbars
        std::string longMd;
        for (int i = 1; i <= 60; ++i) {
            longMd += "## Section Heading " + std::to_string(i) + " with extra wide text to test scrolling behavior in both panes\n\n";
            longMd += "- Item A for section " + std::to_string(i) + "\n- Item B for section " + std::to_string(i) + "\n\n";
        }
        ev->editor->insertAtCursors(longMd);
        [ev updateScrollers];
        [ev updatePreviewPane];

        __block NSString *longSbInfo = nil;
        NSDate *longDeadline = [NSDate dateWithTimeIntervalSinceNow:5.0];
        while ([longDeadline timeIntervalSinceNow] > 0) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
            __block BOOL evalDone = NO;
            [ev->previewWebView evaluateJavaScript:@"(function(){if(window.__miuSbUpd)window.__miuSbUpd();var v=document.querySelector('.__ag_sb_v'),vt=document.querySelector('.__ag_sb_vt');if(!v||!vt)return '';var r=vt.getBoundingClientRect(),cs=window.getComputedStyle(vt);return v.getAttribute('data-visible')+'|'+Math.round(r.width)+'|'+Math.round(r.height)+'|'+cs.backgroundColor;})()"
                                 completionHandler:^(id result, NSError *error) {
                if ([result isKindOfClass:[NSString class]]) longSbInfo = (NSString *)result;
                evalDone = YES;
            }];
            NSDate *evalWait = [NSDate dateWithTimeIntervalSinceNow:1.0];
            while (!evalDone && [evalWait timeIntervalSinceNow] > 0) {
                [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
            }
            if (longSbInfo && [longSbInfo hasPrefix:@"true|6|"]) break;
        }
        std::cout << "[LONG_CONTENT_PREVIEW_SCROLLBAR_LIGHT] info=" << (longSbInfo ? [longSbInfo UTF8String] : "nil") << std::endl;
        if (ev->vScroller.isHidden) {
            std::cerr << "[FAIL] Editor vertical MiuScroller should be visible when 60 sections overflow the window" << std::endl;
            return 1;
        }
        if (!longSbInfo || ![longSbInfo hasPrefix:@"true|6|"]) {
            std::cerr << "[FAIL] Preview vertical scrollbar should be visible with 6px pill width when 60 sections overflow. Got: "
                      << (longSbInfo ? [longSbInfo UTF8String] : "nil") << std::endl;
            return 1;
        }
        NSRect editorKnobRect = [ev->vScroller knobRect];
        if ((int)std::round(editorKnobRect.size.width) != 6 || editorKnobRect.size.height < 28.0) {
            std::cerr << "[FAIL] Editor MiuScroller knobRect width should be 6px matching preview scrollbar. Got: "
                      << editorKnobRect.size.width << "x" << editorKnobRect.size.height << std::endl;
            return 1;
        }
        PixelRGBA lightEditorTrackPx = sampleViewPixel(ev->vScroller, 1, 10);
        PixelRGBA lightEditorKnobPx;
        int scrollerH = (int)ev->vScroller.bounds.size.height;
        int midX = (int)std::round(NSMidX(editorKnobRect));
        for (int sy = 4; sy < scrollerH - 4; sy += 4) {
            PixelRGBA px = sampleViewPixel(ev->vScroller, midX, sy);
            if (px.a > 50) {
                lightEditorKnobPx = px;
                printPixel("LIGHT_EDITOR_SCROLLER_KNOB", midX, sy, lightEditorKnobPx);
                break;
            }
        }
        if (lightEditorTrackPx.a != 0) {
            std::cerr << "[FAIL] Editor MiuScroller track pixel should be transparent (A=0) over frosted glass" << std::endl;
            return 1;
        }
        if (lightEditorKnobPx.a < 85 || lightEditorKnobPx.a > 110 || lightEditorKnobPx.r != 0 || lightEditorKnobPx.g != 0 || lightEditorKnobPx.b != 0) {
            std::cerr << "[FAIL] Light mode Editor MiuScroller knob pixel did not match expected rgba(0,0,0,0.38)" << std::endl;
            return 1;
        }

        std::cout << "[PASS] macOS frosted-glass NSVisualEffectView, subtle 1pt splitter, WKWebView split preview (Dark=0,0,0 / Light=255,255,255), and unified scrollbars verified: "
                  << [renderedHtml UTF8String] << std::endl;
    }
    return 0;
}
