#define MIU_MACOS_TESTING 1
#import "../miu_macOS/main.mm"
#include <iostream>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

        AppDelegate *appDelegate = [AppDelegate new];
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

        // 1. Verify initial state (preview off, help popup on, no NSVisualEffectView gray overlay)
        if ([win.contentView isKindOfClass:[NSVisualEffectView class]]) {
            std::cerr << "[FAIL] win.contentView should not be NSVisualEffectView (causes semi-transparent gray overlay)" << std::endl;
            return 1;
        }
        if (!win.isOpaque || !win.contentView.layer || !win.contentView.layer.opaque || !ev.layer || !ev.layer.opaque) {
            std::cerr << "[FAIL] Window, contentView layer, or EditorView layer is not opaque" << std::endl;
            return 1;
        }
        if (ev->editor->showPreview || !ev->editor->showHelpPopup) {
            std::cerr << "[FAIL] Unexpected initial state before togglePreviewAction" << std::endl;
            return 1;
        }

        // 2. Test Dark Mode pure black background (#000000) and Preview Mode toggle
        ev->editor->isDarkMode = true;
        ev->editor->updateThemeColors();
        [ev updatePreviewBackground];
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
        if (!ev->previewWebView.layer || !ev->previewWebView.layer.opaque) {
            std::cerr << "[FAIL] previewWebView layer is not opaque" << std::endl;
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

        std::cout << "[PASS] macOS WKWebView split preview window rendered and verified: "
                  << [renderedHtml UTF8String] << std::endl;
    }
    return 0;
}
