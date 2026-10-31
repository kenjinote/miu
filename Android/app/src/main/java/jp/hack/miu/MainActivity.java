package jp.hack.miu;
import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.widget.AppCompatEditText;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public class MainActivity extends NativeActivity {
    static { System.loadLibrary("miu"); }
    private int sysBgColor, sysTextColor, sysGutterBgColor, sysGutterTextColor, sysAccentColor;
    private TextView overlayTitleView;
    public native void commitText(String text);
    public native void setComposingText(String text);
    public native void deleteSurroundingText();
    public native void updateVisibleHeight(int bottomInset);
    public native void updateTopMargin(int topMargin);
    public native void cmdNewDocument();
    public native boolean cmdOpenDocument(String filePath);
    public native String cmdGetCharset();
    public native boolean cmdIsDirty();
    public native String cmdGetTextContent();
    public native void cmdMarkSaved();
    public native void cmdSetSearchOptions(String query, String replace, boolean matchCase, boolean wholeWord, boolean regex);
    public native void cmdFindNext(boolean forward);
    public native void cmdReplaceNext();
    public native int cmdReplaceAll();
    public native void finishComposingTextNative();
    public native void cmdSetDisplayFileName(String name);
    public native void cmdTop();
    public native void cmdBottom();
    public native void cmdGoToLine(int line);
    public native void cmdUndo();
    public native void cmdRedo();
    public native void cmdSelectAll();
    public native String cmdCut();
    public native String cmdCopy();
    public native void cmdPaste(String text);
    public native int cmdGetCurrentLine();
    public native byte[] cmdGetSaveData();
    public native String cmdGetAutoSearchText();
    public native void cmdMoveCursor(int direction, boolean isCtrl, boolean keepAnchor);
    public native void deleteForwardText();
    public native void cmdMoveHomeEnd(boolean isHome, boolean isCtrl, boolean keepAnchor);
    public native void cmdSelectNextOccurrence();
    public native void cmdClearSelectionAndMultiCursor();
    public native void cmdPageMove(boolean isUp, boolean keepAnchor);
    public native void cmdIndentLines(boolean isUnindent);
    public native void cmdZoom(int mode);
    public native void cmdToggleComment();
    public native boolean cmdToggleWordWrap();
    public native boolean cmdIsWordWrapEnabled();
    public native void cmdSetWordWrapEnabled(boolean enabled);
    public native void cmdDeleteLine();
    public native void cmdMoveLines(boolean up);
    public native void cmdDuplicateLines(boolean up);
    public native void cmdJumpToBracket(boolean select);
    public native void cmdConvertCase(boolean toUpper);
    public native void cmdAddCursor(boolean up);
    public native void cmdMoveRectSelection(int dLine, int dCol);
    public native void cmdSetEditorViewportSize(int widthPx, int heightPx);
    public native void cmdSetHelpVisible(boolean visible);
    public native String cmdGetPreviewHtml(boolean isDark);
    public native String cmdGetPreviewDomUpdateJs(boolean isDark);
    public native void cmdTogglePreviewTask(int lineIdx);
    public native float[] cmdGetViewState();
    public native void cmdSetViewState(float scrollX, float scrollY, int head, int anchor, boolean wordWrap);
    public native String cmdGetTextBeforeCursor(int length);
    public native String cmdGetTextAfterCursor(int length);

    private ImeBridgeView imeView;
    private PopupWindow toolbarPopup;
    private PopupWindow titleOverlayPopup;
    private PopupWindow helpPopup;
    private View rootView;
    private boolean isKeyboardOpen = false;
    private int lastKeyboardHeight = 0;
    private int currentEffectiveBottomInset = 0;
    private int currentCppTopMargin = 0;
    private boolean isSwitchingUI = false;
    private LinearLayout mainToolbar;
    private LinearLayout searchToolbar;
    private LinearLayout replaceToolbar;
    private SearchEditText searchField;
    private SearchEditText replaceField;
    private TextView previewToolbarBtn;
    private TextView wordWrapToolbarBtn;
    private boolean searchMatchCase = false;
    private boolean searchWholeWord = false;
    private boolean searchRegex = false;
    private static final int REQUEST_CODE_OPEN_DOCUMENT = 1001;
    private static final int REQUEST_CODE_SAVE_DOCUMENT = 1002;
    private static final String PREFS_NAME = "miu_prefs";
    private static final String KEY_RECENT_URIS = "recent_uris";
    private static final int MAX_RECENT_FILES = 10;
    private Uri currentDocumentUri = null;
    private long lastKnownModifiedTime = 0L;
    private boolean isShowingReloadPrompt = false;
    private Runnable pendingAction = null;

    // Live Preview state
    private PopupWindow previewPopup;
    private FrameLayout previewOverlayContainer;
    private PreviewWebView previewWebView;
    private SplitterView splitterView;
    private boolean isLivePreviewVisible = false;
    private boolean previewShellLoaded = false;
    private boolean pendingPreviewShellIsMdOrMermaid = false;
    private boolean pendingPreviewDomRefresh = false;
    private float previewSplitRatio = 0.5f;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final Runnable hideToolbarRunnable = new Runnable() {
        @Override public void run() {
            if (toolbarPopup != null && toolbarPopup.isShowing()) {
                toolbarPopup.dismiss();
                resetToolbarToMainInternal();
            }
            isKeyboardOpen = false;
            currentEffectiveBottomInset = 0;
            updatePreviewLayout();
        }
    };
    private final Runnable debouncedPreviewUpdateRunnable = new Runnable() {
        @Override public void run() {
            if (isLivePreviewVisible) {
                updateLivePreviewContent(false);
            }
        }
    };

    public String getStringResourceByName(String resName) {
        int resId = getResources().getIdentifier(resName, "string", getPackageName());
        if (resId != 0) return getString(resId);
        return resName;
    }

    private boolean isDarkThemeActive() {
        int uiMode = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        return uiMode == Configuration.UI_MODE_NIGHT_YES;
    }

    private void updateSystemUI(boolean isDark) {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        getWindow().setStatusBarColor(Color.TRANSPARENT);
        getWindow().setNavigationBarColor(sysGutterBgColor);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            android.view.WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                int appearance = 0;
                if (!isDark) {
                    appearance = android.view.WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS | android.view.WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
                }
                controller.setSystemBarsAppearance(appearance, android.view.WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS | android.view.WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS);
            }
        } else {
            int flags = View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
            if (!isDark) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) flags |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) flags |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
            }
            getWindow().getDecorView().setSystemUiVisibility(flags);
        }
    }

    @Override protected void onCreate(Bundle savedInstanceState) {
        boolean initIsDark = isDarkThemeActive();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) setTheme(android.R.style.Theme_DeviceDefault_DayNight);
        else setTheme(initIsDark ? android.R.style.Theme_DeviceDefault : android.R.style.Theme_DeviceDefault_Light);
        requestWindowFeature(android.view.Window.FEATURE_NO_TITLE);
        super.onCreate(savedInstanceState);
        updateSystemColors();
        boolean isDark = isDarkThemeActive();
        updateSystemUI(isDark);
        imeView = new ImeBridgeView(this);
        addContentView(imeView, new ViewGroup.LayoutParams(1, 1));
        setupPreviewOverlay();
        FrameLayout uiOverlayContainer = new FrameLayout(this);
        uiOverlayContainer.setClickable(false);
        uiOverlayContainer.setFocusable(false);
        overlayTitleView = new TextView(this);
        overlayTitleView.setTextSize(13.0f);
        overlayTitleView.setSingleLine(true);
        overlayTitleView.setMaxLines(1);
        overlayTitleView.setEllipsize(android.text.TextUtils.TruncateAt.END);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) overlayTitleView.setAutoSizeTextTypeUniformWithConfiguration(4, 13, 1, TypedValue.COMPLEX_UNIT_DIP);
        overlayTitleView.setTextColor(sysAccentColor);
        overlayTitleView.setTypeface(null, android.graphics.Typeface.BOLD);
        overlayTitleView.setGravity(Gravity.CENTER);
        overlayTitleView.setText(getStringResourceByName("untitled"));
        FrameLayout.LayoutParams titleParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, (int)(24 * getResources().getDisplayMetrics().density));
        titleParams.gravity = Gravity.TOP;
        uiOverlayContainer.addView(overlayTitleView, titleParams);
        titleOverlayPopup = new PopupWindow(uiOverlayContainer,
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
        titleOverlayPopup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        titleOverlayPopup.setTouchable(false);
        titleOverlayPopup.setFocusable(false);
        titleOverlayPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        titleOverlayPopup.setClippingEnabled(false);
        rootView = getWindow().getDecorView().findViewById(android.R.id.content);
        rootView.post(() -> {
            if (!isFinishing() && !isDestroyed()) {
                Configuration config = getResources().getConfiguration();
                boolean isDeskMode = (config.uiMode & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_DESK;
                boolean isMultiWindow = false;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    isMultiWindow = isInMultiWindowMode();
                }

                if (!isDeskMode && !isMultiWindow) {
                    titleOverlayPopup.showAtLocation(rootView, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 0);
                }

                if (isHardwareKeyboardConnected()) {
                    showHelpUI();
                }
            }
        });
        setupToolbarPopup();
        rootView.setOnApplyWindowInsetsListener((v, insets) -> {
            int bottomInset = 0;
            int topInset = 0;
            int imeHeight = 0;
            int captionBarHeight = 0;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                imeHeight = insets.getInsets(WindowInsets.Type.ime()).bottom;
                int systemBars = insets.getInsets(WindowInsets.Type.systemBars()).bottom;
                bottomInset = Math.max(imeHeight, systemBars);
                topInset = insets.getInsets(WindowInsets.Type.statusBars()).top;
                captionBarHeight = insets.getInsets(WindowInsets.Type.captionBar()).top;
            } else {
                bottomInset = insets.getSystemWindowInsetBottom();
                topInset = insets.getSystemWindowInsetTop();
                imeHeight = bottomInset > (rootView.getHeight() * 0.15) ? bottomInset : 0;
            }
            Configuration config = getResources().getConfiguration();
            boolean isDeskMode = (config.uiMode & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_DESK;
            boolean isMultiWindow = false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                isMultiWindow = isInMultiWindowMode();
            }
            boolean isDesktopOrWindowed = isDeskMode || isMultiWindow || captionBarHeight > 0;
            int cppTopMargin = topInset;
            if (isDesktopOrWindowed) {
                cppTopMargin = captionBarHeight > 0 ? captionBarHeight : (int)(48 * getResources().getDisplayMetrics().density);
            }
            if (overlayTitleView != null) {
                if (isDesktopOrWindowed) {
                    if (titleOverlayPopup != null && titleOverlayPopup.isShowing()) {
                        titleOverlayPopup.dismiss();
                    }
                } else {
                    FrameLayout.LayoutParams tp = (FrameLayout.LayoutParams) overlayTitleView.getLayoutParams();
                    tp.topMargin = Math.max(10, topInset + (int)(4 * getResources().getDisplayMetrics().density) - 64);
                    overlayTitleView.setLayoutParams(tp);
                    if (titleOverlayPopup != null) {
                        if (!titleOverlayPopup.isShowing() && !isFinishing() && !isDestroyed()) {
                            titleOverlayPopup.showAtLocation(rootView, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 0);
                        }
                        titleOverlayPopup.update(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                    }
                }
            }
            boolean helpVisible = (helpPopup != null && helpPopup.isShowing());
            boolean currentlyOpen = imeHeight > 0 && !helpVisible;
            if (!helpVisible && toolbarPopup != null && toolbarPopup.isFocusable() && toolbarPopup.isShowing()) {
                currentlyOpen = true;
                bottomInset = lastKeyboardHeight;
            }
            if (currentlyOpen) {
                lastKeyboardHeight = bottomInset;
                mainHandler.removeCallbacks(hideToolbarRunnable);
                if (!isKeyboardOpen) {
                    showToolbarPopup(bottomInset);
                    isKeyboardOpen = true;
                } else {
                    updateToolbarPopupPosition(bottomInset);
                }
                int toolbarHeight = toolbarPopup != null && toolbarPopup.getContentView() != null && toolbarPopup.getContentView().getHeight() > 0
                        ? toolbarPopup.getContentView().getHeight()
                        : (int) (48 * getResources().getDisplayMetrics().density);
                currentEffectiveBottomInset = bottomInset + toolbarHeight;
                updateVisibleHeight(currentEffectiveBottomInset);
            } else {
                if (isSwitchingUI) return insets;
                if (isKeyboardOpen) {
                    mainHandler.removeCallbacks(hideToolbarRunnable);
                    mainHandler.postDelayed(hideToolbarRunnable, 100);
                }
                int effectiveBottom = helpVisible ? 0 : bottomInset;
                currentEffectiveBottomInset = effectiveBottom;
                updateVisibleHeight(effectiveBottom);
            }
            currentCppTopMargin = cppTopMargin;
            updateTopMargin(cppTopMargin);
            updatePreviewLayout();
            return insets;
        });
        rootView.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
            if ((right - left) != (oldRight - oldLeft) || (bottom - top) != (oldBottom - oldTop)) {
                if (isLivePreviewVisible) {
                    updatePreviewLayout();
                }
            }
        });
        Intent intent = getIntent();
        if (intent != null) handleIncomingIntent(intent);
        imeView.postDelayed(this::showSoftwareKeyboardIfNeeded, 300);
    }

    @Override protected void onResume() {
        super.onResume();
        checkExternalDocumentModification();
    }

    @Override protected void onPause() {
        saveCurrentViewState();
        super.onPause();
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIncomingIntent(intent);
    }

    private void handleIncomingIntent(Intent intent) {
        String action = intent.getAction();
        Uri uri = intent.getData();
        if ((Intent.ACTION_VIEW.equals(action) || Intent.ACTION_EDIT.equals(action)) && uri != null) {
            confirmSaveIfNeeded(() -> loadDocumentFromUri(uri));
        }
    }

    public class MiuPreviewBridge {
        @JavascriptInterface
        public void postMessage(String msg) {
            if (msg != null && msg.startsWith("toggle_task:")) {
                try {
                    int lineIdx = Integer.parseInt(msg.substring("toggle_task:".length()).trim());
                    runOnUiThread(() -> cmdTogglePreviewTask(lineIdx));
                } catch (NumberFormatException ignored) {}
            }
        }
    }

    private int getWindowFullWidth() {
        View decor = getWindow() != null ? getWindow().getDecorView() : null;
        if (decor != null && decor.getWidth() > 0) return decor.getWidth();
        if (rootView != null && rootView.getWidth() > 0) return rootView.getWidth();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return getWindowManager().getCurrentWindowMetrics().getBounds().width();
        }
        return getResources().getDisplayMetrics().widthPixels;
    }

    private int getWindowFullHeight() {
        View decor = getWindow() != null ? getWindow().getDecorView() : null;
        if (decor != null && decor.getHeight() > 0) return decor.getHeight();
        if (rootView != null && rootView.getHeight() > 0) return rootView.getHeight();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return getWindowManager().getCurrentWindowMetrics().getBounds().height();
        }
        return getResources().getDisplayMetrics().heightPixels;
    }

    private class SplitterView extends View {
        private final Paint linePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint pillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF pillRect = new RectF();
        private boolean isDraggingSplitter = false;
        private boolean isVerticalSplit = false;

        public SplitterView(Context context) {
            super(context);
        }

        public void setVerticalSplit(boolean verticalSplit) {
            if (this.isVerticalSplit != verticalSplit) {
                this.isVerticalSplit = verticalSplit;
                invalidate();
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            boolean isDark = isDarkThemeActive();
            float density = getResources().getDisplayMetrics().density;
            float cx = getWidth() * 0.5f;
            float cy = getHeight() * 0.5f;

            if (isDraggingSplitter) {
                linePaint.setColor((sysAccentColor & 0x00FFFFFF) | 0x99000000);
                linePaint.setStrokeWidth(Math.max(2f, 1.5f * density));
                pillPaint.setColor((sysAccentColor & 0x00FFFFFF) | 0xCC000000);
            } else {
                linePaint.setColor(isDark ? 0x24FFFFFF : 0x24000000);
                linePaint.setStrokeWidth(Math.max(1f, 1.0f * density));
                pillPaint.setColor(isDark ? 0x45FFFFFF : 0x45000000);
            }

            if (isVerticalSplit) {
                canvas.drawLine(0, cy, getWidth(), cy, linePaint);
                float pillW = 28.0f * density;
                float pillH = 3.5f * density;
                pillRect.set(cx - pillW * 0.5f, cy - pillH * 0.5f, cx + pillW * 0.5f, cy + pillH * 0.5f);
                canvas.drawRoundRect(pillRect, pillH * 0.5f, pillH * 0.5f, pillPaint);
            } else {
                canvas.drawLine(cx, 0, cx, getHeight(), linePaint);
                float pillW = 3.5f * density;
                float pillH = 28.0f * density;
                pillRect.set(cx - pillW * 0.5f, cy - pillH * 0.5f, cx + pillW * 0.5f, cy + pillH * 0.5f);
                canvas.drawRoundRect(pillRect, pillW * 0.5f, pillW * 0.5f, pillPaint);
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (!isLivePreviewVisible || previewOverlayContainer == null) return false;
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_DOWN) {
                isDraggingSplitter = true;
                invalidate();
                return true;
            } else if (action == MotionEvent.ACTION_MOVE && isDraggingSplitter) {
                if (isVerticalSplit) {
                    int totalH = getWindowFullHeight();
                    int topMargin = currentCppTopMargin;
                    if (topMargin <= 0) {
                        int resId = getResources().getIdentifier("status_bar_height", "dimen", "android");
                        topMargin = resId > 0 ? getResources().getDimensionPixelSize(resId) : Math.round(24 * getResources().getDisplayMetrics().density);
                    }
                    int visibleBottom = Math.max(topMargin + 80, totalH - currentEffectiveBottomInset);
                    int availH = visibleBottom - topMargin;
                    if (availH > 0) {
                        float rawY = event.getRawY();
                        previewSplitRatio = Math.max(0.20f, Math.min(0.80f, (rawY - topMargin) / (float) availH));
                        updatePreviewLayout();
                    }
                } else {
                    int totalW = getWindowFullWidth();
                    if (totalW > 0) {
                        float rawX = event.getRawX();
                        previewSplitRatio = Math.max(0.20f, Math.min(0.80f, rawX / (float) totalW));
                        updatePreviewLayout();
                    }
                }
                return true;
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                isDraggingSplitter = false;
                invalidate();
                return true;
            }
            return super.onTouchEvent(event);
        }
    }

    private class PreviewWebView extends WebView {
        private final Paint sbPaint = new Paint();
        private final ScaleGestureDetector scaleDetector;
        private float minWebScale = 0.0f;
        private float currentWebScale = 0.0f;
        private float cssZoom = 1.0f;

        public PreviewWebView(Context context) {
            super(context);
            setVerticalScrollBarEnabled(false);
            setHorizontalScrollBarEnabled(false);
            setOverScrollMode(View.OVER_SCROLL_NEVER);
            scaleDetector = new ScaleGestureDetector(context, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
                @Override
                public boolean onScale(ScaleGestureDetector detector) {
                    float factor = detector.getScaleFactor();
                    if (Float.isNaN(factor) || Float.isInfinite(factor) || factor <= 0.0f) return false;
                    float baseScale = minWebScale > 0.0f ? minWebScale : getResources().getDisplayMetrics().density;
                    float curScale = currentWebScale > 0.0f ? currentWebScale : getScale();
                    boolean isNativeZoomedIn = curScale > baseScale * 1.03f;
                    if (!isNativeZoomedIn && (factor < 1.0f || cssZoom < 1.0f)) {
                        float nextZoom = Math.max(0.25f, Math.min(1.0f, cssZoom * factor));
                        if (Math.abs(nextZoom - cssZoom) > 0.002f) {
                            cssZoom = nextZoom;
                            applyCssZoom();
                        }
                        return true;
                    }
                    return false;
                }
            });
        }

        public void onPageLoaded() {
            float sc = getScale();
            if (sc > 0.0f) {
                minWebScale = sc;
                currentWebScale = sc;
            }
            reapplyZoomIfNeeded();
            postInvalidateOnAnimation();
        }

        public void onWebScaleChanged(float oldScale, float newScale) {
            currentWebScale = newScale;
            if (minWebScale <= 0.0f || newScale < minWebScale) {
                minWebScale = newScale;
            }
            postInvalidateOnAnimation();
        }

        public void applyCssZoom() {
            if (Math.abs(cssZoom - 1.0f) < 0.005f) {
                cssZoom = 1.0f;
                evaluateJavascript("if(document.documentElement)document.documentElement.style.zoom='';", null);
            } else {
                String z = String.format(Locale.US, "%.4f", cssZoom);
                evaluateJavascript("if(document.documentElement)document.documentElement.style.zoom='" + z + "';", null);
            }
            postInvalidateOnAnimation();
        }

        public void reapplyZoomIfNeeded() {
            if (cssZoom < 0.995f) {
                applyCssZoom();
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent ev) {
            boolean wasSubOneZoom = (cssZoom < 0.995f);
            scaleDetector.onTouchEvent(ev);
            boolean isSubOneZoom = (cssZoom < 0.995f);
            if (ev.getPointerCount() >= 2 && (wasSubOneZoom || isSubOneZoom) && ev.getActionMasked() == MotionEvent.ACTION_MOVE) {
                postInvalidateOnAnimation();
                return true;
            }
            boolean handled = super.onTouchEvent(ev);
            postInvalidateOnAnimation();
            return handled;
        }

        @Override
        protected void onScrollChanged(int l, int t, int oldl, int oldt) {
            super.onScrollChanged(l, t, oldl, oldt);
            postInvalidateOnAnimation();
        }

        @Override
        protected void dispatchDraw(Canvas canvas) {
            super.dispatchDraw(canvas);
            int viewW = getWidth();
            int viewH = getHeight();
            if (viewW <= 0 || viewH <= 0) return;

            sbPaint.setColor(isDarkThemeActive() ? 0x40FFFFFF : 0x40000000);
            float sbThickness = 12.0f;
            float minBarLen = 40.0f;
            float margin = 4.0f;
            int sx = getScrollX();
            int sy = getScrollY();

            int vRange = computeVerticalScrollRange();
            int vExtent = computeVerticalScrollExtent();
            int vOffset = computeVerticalScrollOffset();
            if (vRange > vExtent + 1 && vExtent > 0) {
                float displayAreaH = (float) viewH;
                float viewportRatio = Math.min(1.0f, (float) vExtent / (float) vRange);
                float barHeight = Math.min(displayAreaH, Math.max(minBarLen, displayAreaH * viewportRatio));
                float maxScrollY = Math.max(1.0f, (float) (vRange - vExtent));
                float scrollRatio = Math.max(0.0f, Math.min(1.0f, (float) vOffset / maxScrollY));
                float barX = sx + viewW - sbThickness - margin;
                float barY = sy + scrollRatio * (displayAreaH - barHeight);
                canvas.drawRect(barX, barY, barX + sbThickness, barY + barHeight, sbPaint);
            }

            int hRange = computeHorizontalScrollRange();
            int hExtent = computeHorizontalScrollExtent();
            int hOffset = computeHorizontalScrollOffset();
            if (hRange > hExtent + 1 && hExtent > 0) {
                float displayAreaW = (float) viewW;
                float viewportRatio = Math.min(1.0f, (float) hExtent / (float) hRange);
                float barWidth = Math.min(displayAreaW, Math.max(minBarLen, displayAreaW * viewportRatio));
                float maxScrollX = Math.max(1.0f, (float) (hRange - hExtent));
                float scrollRatio = Math.max(0.0f, Math.min(1.0f, (float) hOffset / maxScrollX));
                float barX = sx + scrollRatio * (displayAreaW - barWidth);
                float barY = sy + viewH - sbThickness - margin;
                canvas.drawRect(barX, barY, barX + barWidth, barY + sbThickness, sbPaint);
            }
        }
    }

    private void setupPreviewOverlay() {
        previewOverlayContainer = new FrameLayout(this) {
            @Override
            public boolean dispatchTouchEvent(MotionEvent ev) {
                if (helpPopup != null && helpPopup.isShowing()) {
                    if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
                        closeHelpUI();
                    }
                    return true;
                }
                return super.dispatchTouchEvent(ev);
            }
        };
        previewOverlayContainer.setBackgroundColor(Color.TRANSPARENT);

        previewWebView = new PreviewWebView(this);
        WebSettings settings = previewWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(false);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        settings.setSupportZoom(true);
        settings.setBuiltInZoomControls(true);
        settings.setDisplayZoomControls(false);
        previewWebView.setBackgroundColor(isDarkThemeActive() ? Color.BLACK : Color.WHITE);
        previewWebView.addJavascriptInterface(new MiuPreviewBridge(), "miuPreview");
        previewWebView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return true;
            }
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                return true;
            }
            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                previewShellLoaded = pendingPreviewShellIsMdOrMermaid;
                previewWebView.onPageLoaded();
                if (pendingPreviewDomRefresh && previewShellLoaded && isLivePreviewVisible) {
                    pendingPreviewDomRefresh = false;
                    updateLivePreviewContent(false);
                }
            }
            @Override
            public void onScaleChanged(WebView view, float oldScale, float newScale) {
                super.onScaleChanged(view, oldScale, newScale);
                previewWebView.onWebScaleChanged(oldScale, newScale);
            }
        });

        splitterView = new SplitterView(this);

        previewOverlayContainer.addView(previewWebView);
        previewOverlayContainer.addView(splitterView);

        previewPopup = new PopupWindow(previewOverlayContainer, 100, 100);
        previewPopup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        previewPopup.setOutsideTouchable(false);
        previewPopup.setFocusable(false);
        previewPopup.setTouchable(true);
        previewPopup.setClippingEnabled(false);
        previewPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            previewPopup.setTouchModal(false);
        }
    }

    private void updatePreviewLayout() {
        if (previewPopup == null || previewOverlayContainer == null || previewWebView == null || splitterView == null) return;
        int totalW = getWindowFullWidth();
        int totalH = getWindowFullHeight();
        if (!isLivePreviewVisible || totalW <= 0 || totalH <= 0) {
            if (previewPopup.isShowing()) {
                previewPopup.dismiss();
            }
            cmdSetEditorViewportSize(0, 0);
            return;
        }
        float density = getResources().getDisplayMetrics().density;
        int topMargin = currentCppTopMargin;
        if (topMargin <= 0) {
            int resId = getResources().getIdentifier("status_bar_height", "dimen", "android");
            topMargin = resId > 0 ? getResources().getDimensionPixelSize(resId) : Math.round(24 * density);
        }
        int visibleBottom = Math.max(topMargin + 80, totalH - currentEffectiveBottomInset);
        int availH = Math.max(1, visibleBottom - topMargin);
        boolean isPortrait = (totalH > totalW);
        splitterView.setVerticalSplit(isPortrait);

        int popupX, popupY, popupW, popupH;
        if (isPortrait) {
            int minPaneH = Math.min(availH / 3, Math.round(60 * density));
            int topPaneH = Math.max(minPaneH, Math.min(availH - minPaneH, Math.round(availH * previewSplitRatio)));
            int splitY = topMargin + topPaneH;
            int splitterHitH = Math.round(24 * density);
            int halfSplitter = splitterHitH / 2;

            popupX = 0;
            popupY = Math.max(topMargin, splitY - halfSplitter);
            popupW = totalW;
            popupH = Math.max(1, visibleBottom - popupY);
            int webTop = Math.max(0, splitY - popupY);
            int webH = Math.max(1, visibleBottom - splitY);

            FrameLayout.LayoutParams webParams = new FrameLayout.LayoutParams(totalW, webH);
            webParams.gravity = Gravity.TOP | Gravity.LEFT;
            webParams.leftMargin = 0;
            webParams.topMargin = webTop;
            previewWebView.setLayoutParams(webParams);
            previewWebView.setVisibility(View.VISIBLE);

            FrameLayout.LayoutParams splitParams = new FrameLayout.LayoutParams(totalW, splitterHitH);
            splitParams.gravity = Gravity.TOP | Gravity.LEFT;
            splitParams.leftMargin = 0;
            splitParams.topMargin = 0;
            splitterView.setLayoutParams(splitParams);
            splitterView.setVisibility(View.VISIBLE);
            splitterView.invalidate();
            previewOverlayContainer.requestLayout();

            cmdSetEditorViewportSize(totalW, splitY);
        } else {
            int minPaneW = Math.min(totalW / 3, Math.round(80 * density));
            int splitX = Math.max(minPaneW, Math.min(totalW - minPaneW, Math.round(totalW * previewSplitRatio)));
            int splitterHitW = Math.round(24 * density);
            int halfSplitter = splitterHitW / 2;

            popupX = Math.max(0, splitX - halfSplitter);
            popupY = topMargin;
            popupW = Math.max(1, totalW - popupX);
            popupH = availH;
            int webLeft = Math.max(0, splitX - popupX);
            int webW = Math.max(1, totalW - splitX);

            FrameLayout.LayoutParams webParams = new FrameLayout.LayoutParams(webW, availH);
            webParams.gravity = Gravity.TOP | Gravity.LEFT;
            webParams.leftMargin = webLeft;
            webParams.topMargin = 0;
            previewWebView.setLayoutParams(webParams);
            previewWebView.setVisibility(View.VISIBLE);

            FrameLayout.LayoutParams splitParams = new FrameLayout.LayoutParams(splitterHitW, availH);
            splitParams.gravity = Gravity.TOP | Gravity.LEFT;
            splitParams.leftMargin = 0;
            splitParams.topMargin = 0;
            splitterView.setLayoutParams(splitParams);
            splitterView.setVisibility(View.VISIBLE);
            splitterView.invalidate();
            previewOverlayContainer.requestLayout();

            cmdSetEditorViewportSize(splitX, visibleBottom);
        }

        View anchor = rootView != null ? rootView : (getWindow() != null ? getWindow().getDecorView() : null);
        if (anchor != null && anchor.getWindowToken() != null && !isFinishing() && !isDestroyed()) {
            if (!previewPopup.isShowing()) {
                previewPopup.setWidth(popupW);
                previewPopup.setHeight(popupH);
                previewPopup.showAtLocation(anchor, Gravity.TOP | Gravity.LEFT, popupX, popupY);
            } else {
                previewPopup.update(popupX, popupY, popupW, popupH);
            }
        } else if (anchor != null) {
            anchor.post(this::updatePreviewLayout);
        }
    }

    public void toggleLivePreview() {
        isLivePreviewVisible = !isLivePreviewVisible;
        previewShellLoaded = false;
        pendingPreviewShellIsMdOrMermaid = false;
        pendingPreviewDomRefresh = false;
        updateToolbarToggleStates();
        if (isLivePreviewVisible) {
            updatePreviewLayout();
            updateLivePreviewContent(true);
        } else {
            updatePreviewLayout();
        }
    }

    private void updateLivePreviewContent(boolean forceFullReload) {
        if (!isLivePreviewVisible || previewWebView == null) return;
        boolean isDark = isDarkThemeActive();
        previewWebView.setBackgroundColor(isDark ? Color.BLACK : Color.WHITE);
        if (!forceFullReload) {
            if (previewShellLoaded) {
                String domJs = cmdGetPreviewDomUpdateJs(isDark);
                if (domJs != null && !domJs.isEmpty()) {
                    previewWebView.evaluateJavascript(domJs, v -> {
                        if (previewWebView != null) {
                            previewWebView.reapplyZoomIfNeeded();
                            previewWebView.postInvalidateOnAnimation();
                        }
                    });
                    return;
                }
            } else if (pendingPreviewShellIsMdOrMermaid) {
                pendingPreviewDomRefresh = true;
                return;
            }
        }
        String fullHtml = cmdGetPreviewHtml(isDark);
        String domJsCheck = cmdGetPreviewDomUpdateJs(isDark);
        previewShellLoaded = false;
        pendingPreviewShellIsMdOrMermaid = (domJsCheck != null && !domJsCheck.isEmpty());
        pendingPreviewDomRefresh = false;
        previewWebView.loadDataWithBaseURL("https://miu.local/", fullHtml != null ? fullHtml : "", "text/html", "UTF-8", null);
    }

    // Called from C++ via JNI when document content changes
    public void onNativeTextChanged() {
        if (!isLivePreviewVisible) return;
        mainHandler.removeCallbacks(debouncedPreviewUpdateRunnable);
        mainHandler.postDelayed(debouncedPreviewUpdateRunnable, 60);
    }

    private void actionToggleWordWrap() {
        cmdToggleWordWrap();
        updateToolbarToggleStates();
    }

    private void updateToolbarToggleStates() {
        if (previewToolbarBtn != null) {
            previewToolbarBtn.setTextColor(isLivePreviewVisible ? sysAccentColor : sysTextColor);
        }
        if (wordWrapToolbarBtn != null) {
            boolean ww = cmdIsWordWrapEnabled();
            wordWrapToolbarBtn.setTextColor(ww ? sysAccentColor : sysTextColor);
        }
    }

    public boolean handleEditorKeyCommand(int keyCode, KeyEvent event) {
        if (helpPopup != null && helpPopup.isShowing()) {
            closeHelpUI();
            if (keyCode == KeyEvent.KEYCODE_F1 || keyCode == KeyEvent.KEYCODE_ESCAPE) return true;
        }
        if (keyCode == KeyEvent.KEYCODE_F1) {
            showHelpUI();
            return true;
        }
        boolean isCtrl = event.isCtrlPressed() || event.isMetaPressed();
        boolean isShift = event.isShiftPressed();
        boolean isAlt = event.isAltPressed();

        if (keyCode == KeyEvent.KEYCODE_ESCAPE) {
            if (searchToolbar != null && searchToolbar.getVisibility() == View.VISIBLE) resetToolbarToMain();
            else cmdClearSelectionAndMultiCursor();
            return true;
        }

        // Alt + Z (Word Wrap) / Alt + P (Live Preview)
        if (isAlt && !isCtrl && !isShift) {
            if (keyCode == KeyEvent.KEYCODE_Z) { actionToggleWordWrap(); return true; }
            if (keyCode == KeyEvent.KEYCODE_P) { toggleLivePreview(); return true; }
        }

        // Rectangular selection / Multi-cursor / Line move / Line duplicate with Alt+Arrow
        if (isAlt) {
            if (isCtrl && !isShift) {
                if (keyCode == KeyEvent.KEYCODE_DPAD_UP) { cmdAddCursor(true); return true; }
                if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) { cmdAddCursor(false); return true; }
            } else if (isShift && !isCtrl) {
                if (keyCode == KeyEvent.KEYCODE_DPAD_UP) { cmdDuplicateLines(true); return true; }
                if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) { cmdDuplicateLines(false); return true; }
                if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT) { cmdMoveRectSelection(0, -1); return true; }
                if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) { cmdMoveRectSelection(0, 1); return true; }
            } else if (!isCtrl && !isShift) {
                if (keyCode == KeyEvent.KEYCODE_DPAD_UP) { cmdMoveLines(true); return true; }
                if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) { cmdMoveLines(false); return true; }
            }
            if (isCtrl && isShift) {
                if (keyCode == KeyEvent.KEYCODE_DPAD_UP) { cmdMoveRectSelection(-1, 0); return true; }
                if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) { cmdMoveRectSelection(1, 0); return true; }
                if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT) { cmdMoveRectSelection(0, -1); return true; }
                if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) { cmdMoveRectSelection(0, 1); return true; }
            }
        }

        if (isCtrl) {
            switch (keyCode) {
                case KeyEvent.KEYCODE_N: actionNewDocument(); return true;
                case KeyEvent.KEYCODE_O: actionOpenDocument(); return true;
                case KeyEvent.KEYCODE_S:
                    if (isShift) actionSaveAsDocument(null);
                    else actionSaveDocument(null);
                    return true;
                case KeyEvent.KEYCODE_F: openSearchUI(false); return true;
                case KeyEvent.KEYCODE_H:
                case KeyEvent.KEYCODE_R: openSearchUI(true); return true;
                case KeyEvent.KEYCODE_G:
                case KeyEvent.KEYCODE_J: actionGoTo(); return true;
                case KeyEvent.KEYCODE_Z:
                    if (isShift) cmdRedo();
                    else cmdUndo();
                    return true;
                case KeyEvent.KEYCODE_Y:
                    cmdRedo();
                    return true;
                case KeyEvent.KEYCODE_A: cmdSelectAll(); return true;
                case KeyEvent.KEYCODE_X: actionCut(); return true;
                case KeyEvent.KEYCODE_C: actionCopy(); return true;
                case KeyEvent.KEYCODE_V: actionPaste(); return true;
                case KeyEvent.KEYCODE_D: cmdSelectNextOccurrence(); return true;
                case KeyEvent.KEYCODE_B:
                case KeyEvent.KEYCODE_RIGHT_BRACKET:
                    cmdJumpToBracket(isShift);
                    return true;
                case KeyEvent.KEYCODE_K:
                    if (isShift) { cmdDeleteLine(); return true; }
                    break;
                case KeyEvent.KEYCODE_U:
                    cmdConvertCase(!isShift);
                    return true;
                case KeyEvent.KEYCODE_SLASH:
                    cmdToggleComment();
                    return true;
                case KeyEvent.KEYCODE_PLUS:
                case KeyEvent.KEYCODE_EQUALS:
                case KeyEvent.KEYCODE_NUMPAD_ADD:
                    cmdZoom(1);
                    return true;
                case KeyEvent.KEYCODE_MINUS:
                case KeyEvent.KEYCODE_NUMPAD_SUBTRACT:
                    cmdZoom(-1);
                    return true;
                case KeyEvent.KEYCODE_0:
                case KeyEvent.KEYCODE_NUMPAD_0:
                    cmdZoom(0);
                    return true;
            }
        }

        if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT) { cmdMoveCursor(0, isCtrl, isShift); return true; }
        if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) { cmdMoveCursor(1, isCtrl, isShift); return true; }
        if (keyCode == KeyEvent.KEYCODE_DPAD_UP) { cmdMoveCursor(2, isCtrl, isShift); return true; }
        if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) { cmdMoveCursor(3, isCtrl, isShift); return true; }
        if (keyCode == KeyEvent.KEYCODE_PAGE_UP) { cmdPageMove(true, isShift); return true; }
        if (keyCode == KeyEvent.KEYCODE_PAGE_DOWN) { cmdPageMove(false, isShift); return true; }
        if (keyCode == KeyEvent.KEYCODE_TAB) { cmdIndentLines(isShift); return true; }
        if (keyCode == KeyEvent.KEYCODE_FORWARD_DEL) { deleteForwardText(); return true; }
        if (keyCode == KeyEvent.KEYCODE_DEL) { deleteSurroundingText(); return true; }
        if (keyCode == KeyEvent.KEYCODE_MOVE_HOME) { cmdMoveHomeEnd(true, isCtrl, isShift); return true; }
        if (keyCode == KeyEvent.KEYCODE_MOVE_END) { cmdMoveHomeEnd(false, isCtrl, isShift); return true; }
        if (isShift && keyCode == KeyEvent.KEYCODE_INSERT) { actionPaste(); return true; }
        if (keyCode == KeyEvent.KEYCODE_F3) {
            if (searchField == null || searchField.getText().toString().isEmpty()) openSearchUI(false);
            else actionFind(!isShift);
            return true;
        }
        if (!isCtrl && (keyCode == KeyEvent.KEYCODE_ENTER || keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER)) {
            commitText("\n");
            return true;
        }
        return false;
    }

    @Override public boolean onKeyShortcut(int keyCode, KeyEvent event) {
        if (handleEditorKeyCommand(keyCode, event)) return true;
        return super.onKeyShortcut(keyCode, event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(android.view.MotionEvent event) {
        if (event.getAction() == android.view.MotionEvent.ACTION_SCROLL) {
            int metaState = event.getMetaState();
            boolean isCtrlPressed = (metaState & KeyEvent.META_CTRL_ON) != 0;
            boolean isMetaPressed = (metaState & KeyEvent.META_META_ON) != 0;
            if (isCtrlPressed || isMetaPressed) {
                float vScroll = event.getAxisValue(android.view.MotionEvent.AXIS_VSCROLL);
                if (vScroll > 0) {
                    cmdZoom(1);
                } else if (vScroll < 0) {
                    cmdZoom(-1);
                }
                return true;
            }
        }
        return super.dispatchGenericMotionEvent(event);
    }

    private void applyFocusListenerToAll(View view, View.OnFocusChangeListener listener) {
        if (view.isFocusable()) {
            view.setOnFocusChangeListener(listener);
        }
        if (view instanceof ViewGroup) {
            ViewGroup vg = (ViewGroup) view;
            for (int i = 0; i < vg.getChildCount(); i++) {
                applyFocusListenerToAll(vg.getChildAt(i), listener);
            }
        }
    }

    private void setupToolbarPopup() {
        LinearLayout rootContainer = new LinearLayout(this);
        rootContainer.setOrientation(LinearLayout.VERTICAL);
        int bgColor = (sysGutterBgColor & 0x00FFFFFF) | 0xF2000000;
        rootContainer.setBackgroundColor(bgColor);
        View borderLine = new View(this);
        borderLine.setTag("borderLine");
        int borderColor = (sysGutterTextColor & 0x00FFFFFF) | 0x40000000;
        borderLine.setBackgroundColor(borderColor);
        borderLine.setLayoutParams(new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 1));
        rootContainer.addView(borderLine);
        mainToolbar = new LinearLayout(this);
        HorizontalScrollView scrollView = new HorizontalScrollView(this);
        scrollView.setHorizontalScrollBarEnabled(false);
        scrollView.setOverScrollMode(View.OVER_SCROLL_NEVER);
        LinearLayout buttonStack = new LinearLayout(this);
        buttonStack.setOrientation(LinearLayout.HORIZONTAL);
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_new"), v -> actionNewDocument()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_open"), v -> actionOpenDocument()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("menu_open_recent"), v -> actionOpenRecent()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_save"), v -> actionSaveDocument(null)));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_save_as"), v -> actionSaveAsDocument(null)));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_find"), v -> openSearchUI(false)));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_replace"), v -> openSearchUI(true)));
        previewToolbarBtn = createToolbarButton(getStringResourceByName("menu_live_preview"), v -> toggleLivePreview());
        wordWrapToolbarBtn = createToolbarButton(getStringResourceByName("menu_word_wrap"), v -> actionToggleWordWrap());
        buttonStack.addView(previewToolbarBtn);
        buttonStack.addView(wordWrapToolbarBtn);
        buttonStack.addView(createToolbarButton(getStringResourceByName("menu_toggle_comment"), v -> cmdToggleComment()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("menu_tab"), v -> cmdIndentLines(false)));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_top"), v -> cmdTop()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_bottom"), v -> cmdBottom()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_goto"), v -> actionGoTo()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_undo"), v -> cmdUndo()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_redo"), v -> cmdRedo()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_select_all"), v -> cmdSelectAll()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_cut"), v -> actionCut()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_copy"), v -> actionCopy()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_paste"), v -> actionPaste()));
        buttonStack.addView(createToolbarButton("?", v -> {
            if (helpPopup != null && helpPopup.isShowing()) closeHelpUI();
            else showHelpUI();
        }));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_done"), v -> hideSoftwareKeyboard()));
        scrollView.addView(buttonStack);
        mainToolbar.addView(scrollView);
        searchToolbar = new LinearLayout(this);
        searchToolbar.setOrientation(LinearLayout.HORIZONTAL);
        searchToolbar.setGravity(Gravity.CENTER_VERTICAL);
        searchToolbar.setVisibility(View.GONE);
        searchField = new SearchEditText(this);
        searchField.setHint(getStringResourceByName("search_hint"));
        searchField.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
        searchField.setSingleLine(true);
        setEditTextStyle(searchField);
        searchField.setImeOptions(EditorInfo.IME_ACTION_SEARCH);
        searchField.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}
            @Override public void afterTextChanged(Editable s) { updateSearchOptionsToCpp(); }
        });
        searchField.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_SEARCH || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                actionFind(true);
                return true;
            }
            return false;
        });
        searchToolbar.addView(searchField);
        searchToolbar.addView(createToolbarButton("<", v -> actionFind(false)));
        searchToolbar.addView(createToolbarButton(">", v -> actionFind(true)));
        replaceToolbar = new LinearLayout(this);
        replaceToolbar.setOrientation(LinearLayout.HORIZONTAL);
        replaceToolbar.setGravity(Gravity.CENTER_VERTICAL);
        replaceToolbar.setVisibility(View.GONE);
        replaceField = new SearchEditText(this);
        replaceField.setHint(getStringResourceByName("replace_hint"));
        replaceField.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
        replaceField.setSingleLine(true);
        setEditTextStyle(replaceField);
        replaceToolbar.addView(replaceField);
        replaceToolbar.addView(createToolbarButton(getStringResourceByName("replace_btn"), v -> actionReplaceNext()));
        replaceToolbar.addView(createToolbarButton(getStringResourceByName("replace_all_btn"), v -> actionReplaceAll()));
        LinearLayout optionsToolbar = new LinearLayout(this);
        optionsToolbar.setOrientation(LinearLayout.HORIZONTAL);
        TextView matchCaseBtn = createToolbarButton("Aa", v -> {
            searchMatchCase = !searchMatchCase;
            v.setAlpha(searchMatchCase ? 1.0f : 0.4f);
            updateSearchOptionsToCpp();
        });
        matchCaseBtn.setAlpha(0.4f);
        TextView wholeWordBtn = createToolbarButton("\\b", v -> {
            searchWholeWord = !searchWholeWord;
            v.setAlpha(searchWholeWord ? 1.0f : 0.4f);
            updateSearchOptionsToCpp();
        });
        wholeWordBtn.setAlpha(0.4f);
        TextView regexBtn = createToolbarButton(".*", v -> {
            searchRegex = !searchRegex;
            v.setAlpha(searchRegex ? 1.0f : 0.4f);
            updateSearchOptionsToCpp();
        });
        regexBtn.setAlpha(0.4f);
        optionsToolbar.addView(matchCaseBtn);
        optionsToolbar.addView(wholeWordBtn);
        optionsToolbar.addView(regexBtn);
        searchToolbar.addView(optionsToolbar);
        searchToolbar.addView(createToolbarButton(getStringResourceByName("toolbar_done"), v -> resetToolbarToMain()));
        int padH = (int) (12 * getResources().getDisplayMetrics().density);
        int padV = (int) (8 * getResources().getDisplayMetrics().density);
        searchToolbar.setPadding(padH, padV, padH, padV);
        replaceToolbar.setPadding(padH, 0, padH, padV);
        rootContainer.addView(mainToolbar);
        rootContainer.addView(searchToolbar);
        rootContainer.addView(replaceToolbar);
        rootContainer.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
            int newH = bottom - top;
            int oldH = oldBottom - oldTop;
            if (newH != oldH && isKeyboardOpen && newH > 0) {
                currentEffectiveBottomInset = lastKeyboardHeight + newH;
                updateVisibleHeight(currentEffectiveBottomInset);
                updatePreviewLayout();
            }
        });
        toolbarPopup = new PopupWindow(rootContainer, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        toolbarPopup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        toolbarPopup.setOutsideTouchable(false);
        toolbarPopup.setFocusable(false);
        toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            toolbarPopup.setTouchModal(false);
            toolbarPopup.setIsClippedToScreen(true);
        }
        View.OnFocusChangeListener focusListener = (v, hasFocus) -> {
            if (v instanceof AppCompatEditText) {
                GradientDrawable bg = (GradientDrawable) v.getBackground();
                if (hasFocus) {
                    bg.setStroke((int)(2 * getResources().getDisplayMetrics().density), sysAccentColor);
                } else {
                    bg.setStroke(0, Color.TRANSPARENT);
                }
            } else if (v instanceof TextView) {
                if (hasFocus) {
                    GradientDrawable focusBg = new GradientDrawable();
                    focusBg.setColor((sysTextColor & 0x00FFFFFF) | 0x20000000);
                    focusBg.setCornerRadius(8 * getResources().getDisplayMetrics().density);
                    v.setBackground(focusBg);
                } else {
                    android.util.TypedValue outValue = new android.util.TypedValue();
                    getTheme().resolveAttribute(android.R.attr.selectableItemBackgroundBorderless, outValue, true);
                    v.setBackgroundResource(outValue.resourceId);
                }
            }
            if (!hasFocus) {
                mainHandler.postDelayed(() -> {
                    if (toolbarPopup != null && toolbarPopup.getContentView() != null) {
                        View focusedView = toolbarPopup.getContentView().findFocus();
                        if (focusedView == null && searchToolbar.getVisibility() == View.VISIBLE) {
                            resetToolbarToMain();
                        }
                    }
                }, 100);
            }
        };
        applyFocusListenerToAll(rootContainer, focusListener);
    }

    private void setEditTextStyle(AppCompatEditText editText) {
        GradientDrawable bg = new GradientDrawable();
        bg.setShape(GradientDrawable.RECTANGLE);
        bg.setCornerRadius(16 * getResources().getDisplayMetrics().density);
        int edBg = (sysBgColor & 0x00FFFFFF) | 0x80000000;
        bg.setColor(edBg);
        int padH = (int) (12 * getResources().getDisplayMetrics().density);
        int padV = (int) (8 * getResources().getDisplayMetrics().density);
        editText.setPadding(padH, padV, padH, padV);
        editText.setBackground(bg);
        editText.setTextColor(sysTextColor);
        editText.setHintTextColor((sysTextColor & 0x00FFFFFF) | 0x80000000);
        editText.setBackgroundTintList(android.content.res.ColorStateList.valueOf(Color.TRANSPARENT));
    }

    private TextView createToolbarButton(String title, View.OnClickListener listener) {
        TextView btn = new TextView(this);
        btn.setTextColor(sysTextColor);
        btn.setText(title);
        btn.setOnClickListener(listener);
        btn.setGravity(Gravity.CENTER);
        btn.setTextSize(15.0f);
        android.util.TypedValue outValue = new android.util.TypedValue();
        getTheme().resolveAttribute(android.R.attr.selectableItemBackgroundBorderless, outValue, true);
        btn.setBackgroundResource(outValue.resourceId);
        btn.setFocusable(true);
        btn.setClickable(true);
        btn.setFocusableInTouchMode(false);
        int padH = (int) (16 * getResources().getDisplayMetrics().density);
        int padV = (int) (14 * getResources().getDisplayMetrics().density);
        btn.setPadding(padH, padV, padH, padV);
        btn.setOnKeyListener((v, keyCode, event) -> {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                boolean isCtrl = event.isCtrlPressed() || event.isMetaPressed();
                if (isCtrl) {
                    if (keyCode == KeyEvent.KEYCODE_F) { openSearchUI(false); return true; }
                    if (keyCode == KeyEvent.KEYCODE_H || keyCode == KeyEvent.KEYCODE_R) { openSearchUI(true); return true; }
                }
                if (keyCode == KeyEvent.KEYCODE_ENTER || keyCode == KeyEvent.KEYCODE_SPACE || keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER) {
                    v.performClick();
                    return true;
                }
            }
            return false;
        });
        return btn;
    }

    private void showToolbarPopup(int keyboardHeight) {
        if (toolbarPopup != null && rootView != null && rootView.getWindowToken() != null) {
            updateToolbarToggleStates();
            toolbarPopup.showAtLocation(rootView, Gravity.BOTTOM | Gravity.LEFT, 0, keyboardHeight);
        }
    }

    private void updateToolbarPopupPosition(int keyboardHeight) {
        if (toolbarPopup != null && toolbarPopup.isShowing()) toolbarPopup.update(0, keyboardHeight, -1, -1);
    }

    private void openSearchUI(boolean withReplace) {
        boolean wasAlreadyOpen = searchToolbar != null && searchToolbar.getVisibility() == View.VISIBLE;
        isSwitchingUI = true;
        mainToolbar.setVisibility(View.GONE);
        searchToolbar.setVisibility(View.VISIBLE);
        replaceToolbar.setVisibility(withReplace ? View.VISIBLE : View.GONE);
        if (!wasAlreadyOpen) {
            String autoText = cmdGetAutoSearchText();
            if (autoText != null) searchField.setText(autoText);
            searchField.selectAll();
        }
        if (toolbarPopup != null && !toolbarPopup.isShowing()) {
            showToolbarPopup(lastKeyboardHeight);
            isKeyboardOpen = true;
        }
        toolbarPopup.setFocusable(true);
        toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NEEDED);
        toolbarPopup.update();
        Runnable requestFocusTask = new Runnable() {
            int retry = 5;
            @Override public void run() {
                AppCompatEditText targetField = (wasAlreadyOpen && withReplace) ? replaceField : searchField;
                targetField.requestFocus();
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                boolean success = false;
                if (imm != null) success = imm.showSoftInput(targetField, 0);
                if (!success && retry > 0) {
                    retry--;
                    targetField.postDelayed(this, 20);
                } else mainHandler.postDelayed(() -> isSwitchingUI = false, 300);
            }
        };
        searchField.post(requestFocusTask);
    }

    public void resetToolbarToMain() {
        isSwitchingUI = true;
        resetToolbarToMainInternal();
        if (isHardwareKeyboardConnected()) {
            if (toolbarPopup != null && toolbarPopup.isShowing()) {
                toolbarPopup.dismiss();
            }
            isKeyboardOpen = false;
        } else {
            if (toolbarPopup != null) {
                toolbarPopup.setFocusable(false);
                toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
                toolbarPopup.update();
            }
        }
        Runnable requestFocusTask = new Runnable() {
            int retry = 5;
            @Override public void run() {
                imeView.requestFocus();
                if (!isHardwareKeyboardConnected()) {
                    InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                    boolean success = false;
                    if (imm != null) success = imm.showSoftInput(imeView, 0);
                    if (!success && retry > 0) {
                        retry--;
                        imeView.postDelayed(this, 20);
                        return;
                    }
                }
                mainHandler.postDelayed(() -> isSwitchingUI = false, 300);
            }
        };
        imeView.post(requestFocusTask);
        if (rootView != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            rootView.requestApplyInsets();
        }
    }

    private void resetToolbarToMainInternal() {
        mainToolbar.setVisibility(View.VISIBLE);
        searchToolbar.setVisibility(View.GONE);
        replaceToolbar.setVisibility(View.GONE);
        updateToolbarToggleStates();
    }

    private void updateSearchOptionsToCpp() {
        String q = searchField.getText().toString();
        String r = replaceField.getText().toString();
        cmdSetSearchOptions(q, r, searchMatchCase, searchWholeWord, searchRegex);
    }

    private void actionFind(boolean forward) {
        updateSearchOptionsToCpp();
        cmdFindNext(forward);
    }

    private void actionReplaceNext() {
        updateSearchOptionsToCpp();
        cmdReplaceNext();
    }

    private void actionReplaceAll() {
        updateSearchOptionsToCpp();
        int count = cmdReplaceAll();
        String fmt = getStringResourceByName("replace_all_result");
        String msg;
        try {
            msg = String.format(fmt, count);
        } catch (Exception e) {
            msg = count + " occurrence(s) replaced.";
        }
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    private void actionCut() {
        String text = cmdCut();
        if (text != null && !text.isEmpty()) setClipboardText(text);
    }

    private void actionCopy() {
        String text = cmdCopy();
        if (text != null && !text.isEmpty()) setClipboardText(text);
    }

    private void actionPaste() {
        String text = getClipboardText();
        if (text != null && !text.isEmpty()) cmdPaste(text);
    }

    private void setClipboardText(String text) {
        android.content.ClipboardManager clipboard = (android.content.ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null) {
            android.content.ClipData clip = android.content.ClipData.newPlainText("miu", text);
            clipboard.setPrimaryClip(clip);
            Toast.makeText(this, getStringResourceByName("msg_copied"), Toast.LENGTH_SHORT).show();
        }
    }

    private String getClipboardText() {
        android.content.ClipboardManager clipboard = (android.content.ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null && clipboard.hasPrimaryClip()) {
            android.content.ClipData.Item item = clipboard.getPrimaryClip().getItemAt(0);
            CharSequence text = item.getText();
            if (text != null) return text.toString();
        }
        return null;
    }

    private void actionGoTo() {
        AppCompatEditText input = new AppCompatEditText(this);
        input.setInputType(InputType.TYPE_CLASS_NUMBER);
        input.setImeOptions(EditorInfo.IME_ACTION_DONE);
        input.setTextColor(sysTextColor);
        input.setHintTextColor((sysTextColor & 0x00FFFFFF) | 0x80000000);
        input.setBackgroundTintList(android.content.res.ColorStateList.valueOf(sysAccentColor));
        int currentLine = cmdGetCurrentLine();
        input.setText(String.valueOf(currentLine));
        input.selectAll();
        boolean isDark = isDarkThemeActive();
        int dialogTheme = isDark ? android.R.style.Theme_DeviceDefault_Dialog_Alert : android.R.style.Theme_DeviceDefault_Light_Dialog_Alert;
        AlertDialog dialog = new AlertDialog.Builder(this, dialogTheme)
                .setTitle(getStringResourceByName("dialog_goto_title"))
                .setView(input)
                .setPositiveButton(getStringResourceByName("dialog_goto_btn"), (d, which) -> {
                    try {
                        int line = Integer.parseInt(input.getText().toString());
                        cmdGoToLine(line);
                    } catch (NumberFormatException e) {}
                })
                .setNegativeButton(getStringResourceByName("toolbar_cancel"), null)
                .create();
        input.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_DOWN)) {
                try {
                    int line = Integer.parseInt(input.getText().toString());
                    cmdGoToLine(line);
                } catch (NumberFormatException e) {}
                dialog.dismiss();
                return true;
            }
            return false;
        });
        dialog.setOnShowListener(d -> {
            android.widget.Button posBtn = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
            if (posBtn != null) posBtn.setTextColor(sysAccentColor);
            android.widget.Button negBtn = dialog.getButton(AlertDialog.BUTTON_NEGATIVE);
            if (negBtn != null) negBtn.setTextColor(sysAccentColor);
            input.requestFocus();
            input.postDelayed(() -> {
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) imm.showSoftInput(input, InputMethodManager.SHOW_IMPLICIT);
            }, 100);
        });
        dialog.show();
    }

    private void confirmSaveIfNeeded(Runnable nextAction) {
        if (!cmdIsDirty()) {
            nextAction.run();
            return;
        }
        boolean isDark = isDarkThemeActive();
        int dialogTheme = isDark ? android.R.style.Theme_DeviceDefault_Dialog_Alert : android.R.style.Theme_DeviceDefault_Light_Dialog_Alert;
        AlertDialog dialog = new AlertDialog.Builder(this, dialogTheme)
                .setTitle(getStringResourceByName("dialog_unsaved_title"))
                .setMessage(getStringResourceByName("dialog_unsaved_msg"))
                .setPositiveButton(getStringResourceByName("dialog_btn_save"), (d, which) -> actionSaveDocument(nextAction))
                .setNegativeButton(getStringResourceByName("dialog_btn_dont_save"), (d, which) -> nextAction.run())
                .setNeutralButton(getStringResourceByName("toolbar_cancel"), null)
                .create();
        dialog.setOnShowListener(d -> {
            android.widget.Button posBtn = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
            if (posBtn != null) posBtn.setTextColor(sysAccentColor);
            android.widget.Button negBtn = dialog.getButton(AlertDialog.BUTTON_NEGATIVE);
            if (negBtn != null) negBtn.setTextColor(sysAccentColor);
            android.widget.Button neuBtn = dialog.getButton(AlertDialog.BUTTON_NEUTRAL);
            if (neuBtn != null) neuBtn.setTextColor(sysAccentColor);
        });
        dialog.show();
    }

    private void actionNewDocument() {
        confirmSaveIfNeeded(() -> {
            saveCurrentViewState();
            cmdNewDocument();
            currentDocumentUri = null;
            lastKnownModifiedTime = 0L;
            previewShellLoaded = false;
            if (isLivePreviewVisible) updateLivePreviewContent(true);
            showSoftwareKeyboardIfNeeded();
            Toast.makeText(this, getStringResourceByName("msg_new_doc"), Toast.LENGTH_SHORT).show();
        });
    }

    private void actionOpenDocument() {
        confirmSaveIfNeeded(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, REQUEST_CODE_OPEN_DOCUMENT);
        });
    }

    private List<Uri> getRecentUris() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String raw = prefs.getString(KEY_RECENT_URIS, "");
        List<Uri> list = new ArrayList<>();
        if (raw != null && !raw.isEmpty()) {
            String[] parts = raw.split("\n");
            for (String p : parts) {
                if (!p.trim().isEmpty()) {
                    try {
                        list.add(Uri.parse(p.trim()));
                    } catch (Exception ignored) {}
                }
            }
        }
        return list;
    }

    private void addRecentUri(Uri uri) {
        if (uri == null) return;
        List<Uri> list = getRecentUris();
        String target = uri.toString();
        for (int i = list.size() - 1; i >= 0; i--) {
            if (list.get(i).toString().equals(target)) {
                list.remove(i);
            }
        }
        list.add(0, uri);
        while (list.size() > MAX_RECENT_FILES) {
            list.remove(list.size() - 1);
        }
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < list.size(); i++) {
            if (i > 0) sb.append("\n");
            sb.append(list.get(i).toString());
        }
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit().putString(KEY_RECENT_URIS, sb.toString()).apply();
    }

    private void clearRecentUris() {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit().remove(KEY_RECENT_URIS).apply();
    }

    private void actionOpenRecent() {
        List<Uri> recents = getRecentUris();
        boolean isDark = isDarkThemeActive();
        int dialogTheme = isDark ? android.R.style.Theme_DeviceDefault_Dialog_Alert : android.R.style.Theme_DeviceDefault_Light_Dialog_Alert;
        if (recents.isEmpty()) {
            Toast.makeText(this, getStringResourceByName("menu_no_recent"), Toast.LENGTH_SHORT).show();
            return;
        }
        String[] labels = new String[recents.size()];
        for (int i = 0; i < recents.size(); i++) {
            labels[i] = getFileName(recents.get(i));
        }
        AlertDialog dialog = new AlertDialog.Builder(this, dialogTheme)
                .setTitle(getStringResourceByName("menu_open_recent"))
                .setItems(labels, (d, which) -> {
                    if (which >= 0 && which < recents.size()) {
                        Uri selected = recents.get(which);
                        confirmSaveIfNeeded(() -> loadDocumentFromUri(selected));
                    }
                })
                .setNeutralButton(getStringResourceByName("menu_clear_recent"), (d, which) -> clearRecentUris())
                .setNegativeButton(getStringResourceByName("toolbar_cancel"), null)
                .create();
        dialog.setOnShowListener(d -> {
            android.widget.Button negBtn = dialog.getButton(AlertDialog.BUTTON_NEGATIVE);
            if (negBtn != null) negBtn.setTextColor(sysAccentColor);
            android.widget.Button neuBtn = dialog.getButton(AlertDialog.BUTTON_NEUTRAL);
            if (neuBtn != null) neuBtn.setTextColor(sysAccentColor);
        });
        dialog.show();
    }

    private String getViewStateKey(Uri uri) {
        return uri != null ? "vs_" + uri.toString() : "vs___untitled__";
    }

    private void saveCurrentViewState() {
        float[] state = cmdGetViewState();
        if (state == null || state.length < 5) return;
        String key = getViewStateKey(currentDocumentUri);
        String val = state[0] + "," + state[1] + "," + ((int) state[2]) + "," + ((int) state[3]) + "," + (state[4] > 0.5f ? "1" : "0");
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit().putString(key, val).apply();
    }

    private void restoreViewStateForUri(Uri uri) {
        String key = getViewStateKey(uri);
        String val = getSharedPreferences(PREFS_NAME, MODE_PRIVATE).getString(key, null);
        if (val == null || val.isEmpty()) return;
        try {
            String[] parts = val.split(",");
            if (parts.length >= 5) {
                float sx = Float.parseFloat(parts[0]);
                float sy = Float.parseFloat(parts[1]);
                int head = Integer.parseInt(parts[2]);
                int anchor = Integer.parseInt(parts[3]);
                boolean ww = "1".equals(parts[4]);
                cmdSetViewState(sx, sy, head, anchor, ww);
                updateToolbarToggleStates();
            }
        } catch (Exception ignored) {}
    }

    private long queryLastModified(Uri uri) {
        if (uri == null) return 0L;
        if ("content".equals(uri.getScheme())) {
            try (android.database.Cursor cursor = getContentResolver().query(uri, new String[]{DocumentsContract.Document.COLUMN_LAST_MODIFIED}, null, null, null)) {
                if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) {
                    return cursor.getLong(0);
                }
            } catch (Exception ignored) {}
        } else if ("file".equals(uri.getScheme()) && uri.getPath() != null) {
            File f = new File(uri.getPath());
            if (f.exists()) return f.lastModified();
        }
        return 0L;
    }

    private void checkExternalDocumentModification() {
        if (currentDocumentUri == null || lastKnownModifiedTime <= 0L || isShowingReloadPrompt) return;
        long currentMod = queryLastModified(currentDocumentUri);
        if (currentMod > lastKnownModifiedTime) {
            lastKnownModifiedTime = currentMod;
            if (!cmdIsDirty()) {
                loadDocumentFromUri(currentDocumentUri);
            } else {
                isShowingReloadPrompt = true;
                boolean isDark = isDarkThemeActive();
                int dialogTheme = isDark ? android.R.style.Theme_DeviceDefault_Dialog_Alert : android.R.style.Theme_DeviceDefault_Light_Dialog_Alert;
                AlertDialog dialog = new AlertDialog.Builder(this, dialogTheme)
                        .setMessage(getStringResourceByName("reload_prompt"))
                        .setPositiveButton(android.R.string.ok, (d, which) -> {
                            isShowingReloadPrompt = false;
                            loadDocumentFromUri(currentDocumentUri);
                        })
                        .setNegativeButton(getStringResourceByName("toolbar_cancel"), (d, which) -> isShowingReloadPrompt = false)
                        .setOnCancelListener(d -> isShowingReloadPrompt = false)
                        .create();
                dialog.setOnShowListener(d -> {
                    android.widget.Button posBtn = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
                    if (posBtn != null) posBtn.setTextColor(sysAccentColor);
                    android.widget.Button negBtn = dialog.getButton(AlertDialog.BUTTON_NEGATIVE);
                    if (negBtn != null) negBtn.setTextColor(sysAccentColor);
                });
                dialog.show();
            }
        }
    }

    private void actionSaveDocument(Runnable onSuccess) {
        if (currentDocumentUri == null) actionSaveAsDocument(onSuccess);
        else saveToUri(currentDocumentUri, onSuccess);
    }

    private void actionSaveAsDocument(Runnable onSuccess) {
        this.pendingAction = onSuccess;
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        String defaultTitle = (currentDocumentUri != null) ? getFileName(currentDocumentUri) : (getStringResourceByName("untitled") + ".txt");
        intent.putExtra(Intent.EXTRA_TITLE, defaultTitle);
        startActivityForResult(intent, REQUEST_CODE_SAVE_DOCUMENT);
    }

    private void saveToUri(Uri uri, Runnable onSuccess) {
        try {
            byte[] saveData = cmdGetSaveData();
            if (saveData != null) {
                ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "wt");
                if (pfd != null) {
                    FileOutputStream fos = new FileOutputStream(pfd.getFileDescriptor());
                    fos.write(saveData);
                    fos.flush();
                    fos.close();
                    pfd.close();
                }
                currentDocumentUri = uri;
                lastKnownModifiedTime = queryLastModified(uri);
                addRecentUri(uri);
                cmdSetDisplayFileName(getFileName(uri));
                cmdMarkSaved();
                saveCurrentViewState();
                if (isLivePreviewVisible) updateLivePreviewContent(true);
                Toast.makeText(this, getStringResourceByName("msg_doc_saved"), Toast.LENGTH_SHORT).show();
                if (onSuccess != null) onSuccess.run();
            } else Toast.makeText(this, getStringResourceByName("msg_encode_fail"), Toast.LENGTH_SHORT).show();
        } catch (Exception e) {
            Toast.makeText(this, getStringResourceByName("msg_save_fail"), Toast.LENGTH_SHORT).show();
        }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode == RESULT_OK && data != null && data.getData() != null) {
            Uri uri = data.getData();
            try {
                getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            } catch (SecurityException e) {}
            if (requestCode == REQUEST_CODE_OPEN_DOCUMENT) loadDocumentFromUri(uri);
            else if (requestCode == REQUEST_CODE_SAVE_DOCUMENT) {
                saveToUri(uri, pendingAction);
                pendingAction = null;
            }
        } else if (resultCode == RESULT_CANCELED) {
            if (requestCode == REQUEST_CODE_SAVE_DOCUMENT) pendingAction = null;
        }
    }

    private String getFileName(Uri uri) {
        String result = null;
        if ("content".equals(uri.getScheme())) {
            try (android.database.Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int idx = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                    if (idx >= 0) result = cursor.getString(idx);
                }
            } catch (Exception ignored) {}
        }
        if (result == null) {
            result = uri.getPath();
            if (result != null) {
                int cut = result.lastIndexOf('/');
                if (cut != -1) result = result.substring(cut + 1);
            }
        }
        return result != null ? result : "Unknown";
    }

    private void loadDocumentFromUri(Uri uri) {
        try {
            saveCurrentViewState();
            InputStream inputStream = getContentResolver().openInputStream(uri);
            if (inputStream == null) return;
            File tempFile = new File(getCacheDir(), "miu_temp_open.txt");
            FileOutputStream outputStream = new FileOutputStream(tempFile);
            byte[] buffer = new byte[8192];
            int length;
            while ((length = inputStream.read(buffer)) > 0) outputStream.write(buffer, 0, length);
            outputStream.flush(); outputStream.close(); inputStream.close();
            if (cmdOpenDocument(tempFile.getAbsolutePath())) {
                currentDocumentUri = uri;
                lastKnownModifiedTime = queryLastModified(uri);
                addRecentUri(uri);
                cmdSetDisplayFileName(getFileName(uri));
                cmdMarkSaved();
                restoreViewStateForUri(uri);
                previewShellLoaded = false;
                if (isLivePreviewVisible) updateLivePreviewContent(true);
                hideSoftwareKeyboard();
                Toast.makeText(this, getStringResourceByName("msg_doc_opened"), Toast.LENGTH_SHORT).show();
            } else Toast.makeText(this, getStringResourceByName("msg_open_fail"), Toast.LENGTH_SHORT).show();
        } catch (Exception e) {
            Toast.makeText(this, getStringResourceByName("msg_read_error"), Toast.LENGTH_SHORT).show();
        }
    }

    private boolean isHardwareKeyboardConnected() { return getResources().getConfiguration().keyboard == Configuration.KEYBOARD_QWERTY; }
    public void showSoftwareKeyboardIfNeeded() {
        if (!isHardwareKeyboardConnected() && (helpPopup == null || !helpPopup.isShowing())) {
            showSoftwareKeyboard();
        }
    }
    public void showSoftwareKeyboard() {
        runOnUiThread(() -> {
            if (helpPopup != null && helpPopup.isShowing()) {
                closeHelpUI();
            }
            if (searchToolbar != null && searchToolbar.getVisibility() == View.VISIBLE) {
                resetToolbarToMainInternal();
                if (toolbarPopup != null) {
                    toolbarPopup.setFocusable(false);
                    toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
                    toolbarPopup.update();
                }
            }
            imeView.requestFocus();
            InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) imm.showSoftInput(imeView, InputMethodManager.SHOW_IMPLICIT);
        });
    }
    public void hideSoftwareKeyboard() {
        runOnUiThread(() -> {
            InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                if (imeView != null && imeView.getWindowToken() != null) {
                    imm.hideSoftInputFromWindow(imeView.getWindowToken(), 0);
                }
                if (getWindow() != null && getWindow().getDecorView() != null && getWindow().getDecorView().getWindowToken() != null) {
                    imm.hideSoftInputFromWindow(getWindow().getDecorView().getWindowToken(), 0);
                }
            }
        });
    }

    class ImeBridgeView extends View {
        public ImeBridgeView(Context context) { super(context); setFocusable(true); setFocusableInTouchMode(true); }
        @Override public boolean onCheckIsTextEditor() { return true; }
        @Override public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT |
                    InputType.TYPE_TEXT_FLAG_MULTI_LINE |
                    InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS |
                    InputType.TYPE_TEXT_VARIATION_FILTER;
            outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_ACTION_NONE | 0x1000000;
            outAttrs.privateImeOptions = "disableSticker=true;disableGifKeyboard=true;disableAutoCorrect=true;disablePredictiveText=true;";
            outAttrs.initialCapsMode = 0;
            return new MiuInputConnection(this, false);
        }
        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                if (handleEditorKeyCommand(keyCode, event)) return true;
            }
            return super.onKeyDown(keyCode, event);
        }
    }

    class MiuInputConnection extends BaseInputConnection {
        public MiuInputConnection(View targetView, boolean fullEditor) { super(targetView, fullEditor); }
        @Override
        public int getCursorCapsMode(int reqModes) {
            return 0;
        }
        @Override
        public CharSequence getTextBeforeCursor(int n, int flags) {
            String text = MainActivity.this.cmdGetTextBeforeCursor(n);
            return text != null ? text : "";
        }
        @Override
        public CharSequence getTextAfterCursor(int n, int flags) {
            String text = MainActivity.this.cmdGetTextAfterCursor(n);
            return text != null ? text : "";
        }
        @Override
        public android.view.inputmethod.ExtractedText getExtractedText(android.view.inputmethod.ExtractedTextRequest request, int flags) {
            android.view.inputmethod.ExtractedText et = new android.view.inputmethod.ExtractedText();
            String before = MainActivity.this.cmdGetTextBeforeCursor(100);
            String after = MainActivity.this.cmdGetTextAfterCursor(100);
            if (before == null) before = "";
            if (after == null) after = "";
            et.text = before + after;
            et.partialStartOffset = -1;
            et.partialEndOffset = -1;
            et.selectionStart = before.length();
            et.selectionEnd = before.length();
            return et;
        }
        @Override public boolean commitText(CharSequence text, int newCursorPosition) {
            if (text != null && text.length() > 0) {
                MainActivity.this.closeHelpUI();
            }
            if (text != null) MainActivity.this.commitText(text.toString());
            return true;
        }
        @Override public boolean setComposingText(CharSequence text, int newCursorPosition) {
            if (text != null && text.length() > 0) {
                MainActivity.this.closeHelpUI();
            }
            if (text != null) MainActivity.this.setComposingText(text.toString());
            return true;
        }
        @Override public boolean finishComposingText() {
            MainActivity.this.finishComposingTextNative();
            return super.finishComposingText();
        }
        @Override public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            if (beforeLength > 0 || afterLength > 0) {
                MainActivity.this.closeHelpUI();
            }
            for (int i = 0; i < beforeLength; i++) {
                MainActivity.this.deleteSurroundingText();
            }
            return true;
        }
        @Override public boolean sendKeyEvent(KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                if (handleEditorKeyCommand(event.getKeyCode(), event)) return true;
            }
            return super.sendKeyEvent(event);
        }
    }

    class SearchEditText extends AppCompatEditText {
        public SearchEditText(Context context) { super(context); }
        @Override public boolean onKeyPreIme(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
                resetToolbarToMain();
                return true;
            }
            if (keyCode == KeyEvent.KEYCODE_ESCAPE && event.getAction() == KeyEvent.ACTION_UP) {
                resetToolbarToMain();
                return true;
            }
            return super.onKeyPreIme(keyCode, event);
        }
        @Override public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                if (MainActivity.this.helpPopup != null && MainActivity.this.helpPopup.isShowing()) {
                    MainActivity.this.closeHelpUI();
                    if (keyCode == KeyEvent.KEYCODE_F1 || keyCode == KeyEvent.KEYCODE_ESCAPE) {
                        return true;
                    }
                }
                if (keyCode == KeyEvent.KEYCODE_F1) {
                    MainActivity.this.showHelpUI();
                    return true;
                }
                boolean isCtrl = event.isCtrlPressed() || event.isMetaPressed();
                if (isCtrl) {
                    if (keyCode == KeyEvent.KEYCODE_F) { MainActivity.this.openSearchUI(false); return true; }
                    if (keyCode == KeyEvent.KEYCODE_H || keyCode == KeyEvent.KEYCODE_R) { MainActivity.this.openSearchUI(true); return true; }
                }
                if (keyCode == KeyEvent.KEYCODE_ENTER || keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER) {
                    if (this == searchField) { actionFind(true); return true; }
                    else if (this == replaceField) { actionReplaceNext(); return true; }
                }
                if (keyCode == KeyEvent.KEYCODE_TAB) {
                    View next = focusSearch(event.isShiftPressed() ? View.FOCUS_BACKWARD : View.FOCUS_FORWARD);
                    if (next != null) {
                        next.requestFocus();
                        return true;
                    }
                }
            }
            return super.onKeyDown(keyCode, event);
        }
    }

    private void updateSystemColors() {
        int isDarkInt = isDarkThemeActive() ? 1 : 0;
        sysBgColor = getThemeColor(0, isDarkInt);
        sysTextColor = getThemeColor(1, isDarkInt);
        sysGutterBgColor = getThemeColor(2, isDarkInt);
        sysGutterTextColor = getThemeColor(3, isDarkInt);
        sysAccentColor = getThemeColor(4, isDarkInt);
    }

    public int getThemeColor(int colorType, int isDarkModeInt) {
        boolean isDark = (isDarkModeInt != 0);
        int defaultBg = isDark ? 0xFF1C1C1C : 0xFFFFFFFF;
        int defaultText = isDark ? 0xFFF2F2F2 : 0xFF1E1E1E;
        int defaultGutterBg = isDark ? 0xFF1C1C1C : 0xFFF7F7F7;
        int defaultGutterText = isDark ? 0xFF737373 : 0xFF999999;
        int defaultAccent = isDark ? 0xFF8AB4F8 : 0xFF1A73E8;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            try {
                int resId = 0;
                switch (colorType) {
                    case 0: resId = isDark ? android.R.color.system_neutral1_900 : android.R.color.system_neutral1_10; break;
                    case 1: resId = isDark ? android.R.color.system_neutral1_10 : android.R.color.system_neutral1_900; break;
                    case 2: resId = isDark ? android.R.color.system_neutral2_900 : android.R.color.system_neutral2_50; break;
                    case 3: resId = isDark ? android.R.color.system_neutral2_400 : android.R.color.system_neutral2_600; break;
                    case 4: resId = isDark ? android.R.color.system_accent1_100 : android.R.color.system_accent1_300; break;
                }
                if (resId != 0) return getResources().getColor(resId, getTheme());
            } catch (Exception e) {}
        }
        switch (colorType) {
            case 0: return defaultBg; case 1: return defaultText;
            case 2: return defaultGutterBg; case 3: return defaultGutterText;
            case 4: return defaultAccent; default: return 0xFF000000;
        }
    }

    @Override public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        boolean isDark = (newConfig.uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) getTheme().applyStyle(android.R.style.Theme_DeviceDefault_DayNight, true);
        else getTheme().applyStyle(isDark ? android.R.style.Theme_DeviceDefault : android.R.style.Theme_DeviceDefault_Light, true);
        updateSystemColors();
        updateSystemUI(isDark);
        if (toolbarPopup != null && toolbarPopup.getContentView() != null) {
            View root = toolbarPopup.getContentView();
            int bgColor = (sysGutterBgColor & 0x00FFFFFF) | 0xF2000000;
            root.setBackgroundColor(bgColor);
            updateViewColors(root);
            updateToolbarToggleStates();
        }
        if (isLivePreviewVisible) {
            previewShellLoaded = false;
            updateLivePreviewContent(true);
            updatePreviewLayout();
        }
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null && imeView != null) imm.restartInput(imeView);
        if (rootView != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            rootView.requestApplyInsets();
        }
    }

    private void updateViewColors(View view) {
        if ("borderLine".equals(view.getTag())) {
            view.setBackgroundColor((sysGutterTextColor & 0x00FFFFFF) | 0x40000000);
            return;
        }
        if (view instanceof AppCompatEditText) {
            AppCompatEditText et = (AppCompatEditText) view;
            et.setTextColor(sysTextColor);
            et.setHintTextColor((sysTextColor & 0x00FFFFFF) | 0x80000000);
            if (et.getBackground() instanceof GradientDrawable) {
                ((GradientDrawable) et.getBackground()).setColor((sysBgColor & 0x00FFFFFF) | 0x80000000);
            }
        } else if (view instanceof TextView) {
            ((TextView) view).setTextColor(sysTextColor);
        } else if (view instanceof ViewGroup) {
            ViewGroup vg = (ViewGroup) view;
            for (int i = 0; i < vg.getChildCount(); i++) updateViewColors(vg.getChildAt(i));
        }
    }

    public void setOverlayTitle(String title) {
        runOnUiThread(() -> {
            setTitle(title);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                try {
                    setTaskDescription(new android.app.ActivityManager.TaskDescription(title));
                } catch (Exception e) {
                }
            }
            if (overlayTitleView != null) overlayTitleView.setText(title);
        });
    }

    @Override
    protected void onDestroy() {
        saveCurrentViewState();
        if (helpPopup != null && helpPopup.isShowing()) {
            helpPopup.dismiss();
        }
        if (previewPopup != null && previewPopup.isShowing()) {
            previewPopup.dismiss();
        }
        if (titleOverlayPopup != null && titleOverlayPopup.isShowing()) {
            titleOverlayPopup.dismiss();
        }
        super.onDestroy();
    }

    private void showHelpUI() {
        if (helpPopup != null && helpPopup.isShowing()) return;
        cmdSetHelpVisible(true);
        hideSoftwareKeyboard();
        if (toolbarPopup != null) {
            if (toolbarPopup.isFocusable()) {
                toolbarPopup.setFocusable(false);
                toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
            }
            if (toolbarPopup.isShowing()) {
                toolbarPopup.dismiss();
            }
            resetToolbarToMainInternal();
        }
        isKeyboardOpen = false;
        currentEffectiveBottomInset = 0;
        updateVisibleHeight(0);
        updatePreviewLayout();

        TextView helpText = new TextView(this);
        String versionName = "0.0.0";
        try {
            versionName = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (android.content.pm.PackageManager.NameNotFoundException e) {
        }
        String helpStr = getString(R.string.help_text, versionName);
        helpText.setText(helpStr);
        helpText.setTextColor(sysTextColor);
        helpText.setTextSize(14.0f);
        float density = getResources().getDisplayMetrics().density;
        int pad = (int) (24 * density);
        helpText.setPadding(pad, pad, pad, pad);
        helpText.setLineSpacing(0, 1.3f);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor((sysBgColor & 0x00FFFFFF) | 0xF0000000);
        bg.setCornerRadius(16 * density);
        bg.setStroke((int) (1 * density), (sysGutterTextColor & 0x00FFFFFF) | 0x80000000);
        helpText.setBackground(bg);

        FrameLayout helpOverlay = new FrameLayout(this);
        helpOverlay.setBackgroundColor(Color.TRANSPARENT);
        FrameLayout.LayoutParams cardParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER);
        int margin = (int) (16 * density);
        cardParams.setMargins(margin, margin, margin, margin);
        helpOverlay.addView(helpText, cardParams);

        View.OnTouchListener dismissTouchListener = (v, event) -> {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                closeHelpUI();
            }
            return true;
        };
        helpOverlay.setOnTouchListener(dismissTouchListener);
        helpText.setOnTouchListener(dismissTouchListener);

        helpPopup = new PopupWindow(helpOverlay, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        helpPopup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        helpPopup.setFocusable(false);
        helpPopup.setTouchable(true);
        helpPopup.setOutsideTouchable(true);
        helpPopup.setClippingEnabled(false);
        helpPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        helpPopup.setAnimationStyle(android.R.style.Animation_Dialog);
        helpPopup.setTouchInterceptor((v, event) -> {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN || event.getActionMasked() == MotionEvent.ACTION_OUTSIDE) {
                closeHelpUI();
                return true;
            }
            return false;
        });
        if (rootView != null) {
            helpPopup.showAtLocation(rootView, Gravity.CENTER, 0, 0);
        }
    }

    public void closeHelpUI() {
        cmdSetHelpVisible(false);
        if (Looper.myLooper() == Looper.getMainLooper()) {
            if (helpPopup != null) {
                if (helpPopup.isShowing()) {
                    helpPopup.dismiss();
                }
                helpPopup = null;
            }
        } else {
            runOnUiThread(() -> {
                if (helpPopup != null) {
                    if (helpPopup.isShowing()) {
                        helpPopup.dismiss();
                    }
                    helpPopup = null;
                }
            });
        }
    }
}
