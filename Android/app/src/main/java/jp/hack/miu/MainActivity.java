package jp.hack.miu;
import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
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
public class MainActivity extends NativeActivity {
    static {
        System.loadLibrary("miu");
    }
    public native void commitText(String text);
    public native void setComposingText(String text);
    public native void deleteSurroundingText();
    public native void updateVisibleHeight(int bottomInset);
    public native void updateTopMargin(int topMargin);
    // C++連携
    public native void cmdNewDocument();
    public native boolean cmdOpenDocument(String filePath);
    public native boolean cmdIsDirty();
    public native String cmdGetTextContent();
    public native void cmdMarkSaved();
    public native void cmdSetSearchOptions(String query, String replace, boolean matchCase, boolean wholeWord, boolean regex);
    public native void cmdFindNext(boolean forward);
    public native void cmdReplaceNext();
    public native void cmdReplaceAll();
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
    private ImeBridgeView imeView;
    private View blurOverlay;
    private PopupWindow toolbarPopup;
    private View rootView;
    private boolean isKeyboardOpen = false;
    private int lastKeyboardHeight = 0;
    private boolean isSwitchingUI = false;
    private LinearLayout mainToolbar;
    private LinearLayout searchToolbar;
    private LinearLayout replaceToolbar;
    private SearchEditText searchField;
    private SearchEditText replaceField;
    private boolean searchMatchCase = false;
    private boolean searchWholeWord = false;
    private boolean searchRegex = false;

    private static final int REQUEST_CODE_OPEN_DOCUMENT = 1001;
    private static final int REQUEST_CODE_SAVE_DOCUMENT = 1002;
    private Uri currentDocumentUri = null;
    private Runnable pendingAction = null;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final Runnable hideToolbarRunnable = new Runnable() {
        @Override
        public void run() {
            if (toolbarPopup != null && toolbarPopup.isShowing()) {
                toolbarPopup.dismiss();
                resetToolbarToMainInternal();
            }
            isKeyboardOpen = false;
        }
    };
    public String getStringResourceByName(String resName) {
        int resId = getResources().getIdentifier(resName, "string", getPackageName());
        if (resId != 0) {
            return getString(resId);
        }
        return resName;
    }
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int uiMode = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        boolean isDark = (uiMode == Configuration.UI_MODE_NIGHT_YES);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            android.view.WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                if (!isDark) {
                    controller.setSystemBarsAppearance(
                            android.view.WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS,
                            android.view.WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS
                    );
                } else {
                    controller.setSystemBarsAppearance(
                            0,
                            android.view.WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS
                    );
                }
            }
        } else {
            int flags = View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
            if (!isDark && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                flags |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
            }
            getWindow().getDecorView().setSystemUiVisibility(flags);
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        getWindow().setStatusBarColor(Color.TRANSPARENT);
        imeView = new ImeBridgeView(this);
        addContentView(imeView, new FrameLayout.LayoutParams(1, 1));
        blurOverlay = new View(this);
        blurOverlay.setClickable(false);
        blurOverlay.setFocusable(false);
        blurOverlay.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        addContentView(blurOverlay, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0));
        rootView = getWindow().getDecorView().findViewById(android.R.id.content);
        setupToolbarPopup();
        rootView.setOnApplyWindowInsetsListener((v, insets) -> {
            if (blurOverlay != null) blurOverlay.bringToFront();
            int bottomInset = 0;
            int topInset = 0;
            int imeHeight = 0;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                imeHeight = insets.getInsets(WindowInsets.Type.ime()).bottom;
                int systemBars = insets.getInsets(WindowInsets.Type.systemBars()).bottom;
                bottomInset = Math.max(imeHeight, systemBars);
                topInset = insets.getInsets(WindowInsets.Type.statusBars()).top;
            } else {
                bottomInset = insets.getSystemWindowInsetBottom();
                topInset = insets.getSystemWindowInsetTop();
                imeHeight = bottomInset > (rootView.getHeight() * 0.15) ? bottomInset : 0;
            }
            boolean currentlyOpen = imeHeight > 0;
            if (toolbarPopup != null && toolbarPopup.isFocusable() && toolbarPopup.isShowing()) {
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
                int toolbarHeight = toolbarPopup != null && toolbarPopup.getContentView() != null ?
                        toolbarPopup.getContentView().getHeight() : (int) (48 * getResources().getDisplayMetrics().density);
                updateVisibleHeight(bottomInset + toolbarHeight);
            } else {
                if (isSwitchingUI) {
                    return insets;
                }
                if (isKeyboardOpen) {
                    mainHandler.removeCallbacks(hideToolbarRunnable);
                    mainHandler.postDelayed(hideToolbarRunnable, 100);
                }
                updateVisibleHeight(bottomInset);
            }
            int topMargin = topInset + 20;
            updateTopMargin(topMargin);
            buildProgressiveFade(topInset, topMargin);
            return insets;
        });
        Intent intent = getIntent();
        if (intent != null) {
            handleIncomingIntent(intent);
        }
        imeView.postDelayed(this::showSoftwareKeyboardIfNeeded, 300);
    }
    @Override
    protected void onNewIntent(Intent intent) {
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
    @Override
    public boolean onKeyShortcut(int keyCode, KeyEvent event) {
        boolean isModifierPressed = event.isCtrlPressed() || event.isMetaPressed();
        boolean isShiftPressed = event.isShiftPressed();
        if (isModifierPressed) {
            switch (keyCode) {
                case KeyEvent.KEYCODE_N: actionNewDocument(); return true;
                case KeyEvent.KEYCODE_O: actionOpenDocument(); return true;
                case KeyEvent.KEYCODE_S:
                    if (isShiftPressed) actionSaveAsDocument(null);
                    else actionSaveDocument(null);
                    return true;
                case KeyEvent.KEYCODE_F: openSearchUI(false); return true;
                case KeyEvent.KEYCODE_R: openSearchUI(true); return true;
                case KeyEvent.KEYCODE_G:
                case KeyEvent.KEYCODE_J: actionGoTo(); return true;
                case KeyEvent.KEYCODE_Z:
                    if (isShiftPressed) cmdRedo();
                    else cmdUndo();
                    return true;
                case KeyEvent.KEYCODE_A: cmdSelectAll(); return true;
                case KeyEvent.KEYCODE_X: actionCut(); return true;
                case KeyEvent.KEYCODE_C: actionCopy(); return true;
                case KeyEvent.KEYCODE_V: actionPaste(); return true;
                case KeyEvent.KEYCODE_EQUALS:
                case KeyEvent.KEYCODE_PLUS:
                case KeyEvent.KEYCODE_MINUS:
                case KeyEvent.KEYCODE_0:
                    // ズーム関連は必要に応じてC++側へブリッジ
                    return true;
            }
        }
        if (keyCode == KeyEvent.KEYCODE_F1) {
            // ヘルプ表示のトグル
            return true;
        }
        return super.onKeyShortcut(keyCode, event);
    }
    private void setupToolbarPopup() {
        LinearLayout rootContainer = new LinearLayout(this);
        rootContainer.setOrientation(LinearLayout.VERTICAL);
        int uiMode = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        boolean isDark = (uiMode == Configuration.UI_MODE_NIGHT_YES);
        int bgColor = isDark ? Color.parseColor("#F21E1E1E") : Color.parseColor("#F2F8F9FA");
        rootContainer.setBackgroundColor(bgColor);
        View borderLine = new View(this);
        int borderColor = isDark ? Color.parseColor("#40FFFFFF") : Color.parseColor("#20000000");
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
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_save"), v -> actionSaveDocument(null)));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_save_as"), v -> actionSaveAsDocument(null)));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_find"), v -> openSearchUI(false)));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_replace"), v -> openSearchUI(true)));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_top"), v -> cmdTop()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_bottom"), v -> cmdBottom()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_goto"), v -> actionGoTo()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_undo"), v -> cmdUndo()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_redo"), v -> cmdRedo()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_select_all"), v -> cmdSelectAll()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_cut"), v -> actionCut()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_copy"), v -> actionCopy()));
        buttonStack.addView(createToolbarButton(getStringResourceByName("toolbar_paste"), v -> actionPaste()));
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
        setEditTextStyle(searchField, isDark);
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
        setEditTextStyle(replaceField, isDark);
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
        TextView regexBtn = createToolbarButton(".*", v -> {
            searchRegex = !searchRegex;
            v.setAlpha(searchRegex ? 1.0f : 0.4f);
            updateSearchOptionsToCpp();
        });
        regexBtn.setAlpha(0.4f);
        optionsToolbar.addView(matchCaseBtn);
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
            if (!hasFocus) {
                mainHandler.postDelayed(() -> {
                    if (searchField != null && replaceField != null) {
                        if (!searchField.hasFocus() && !replaceField.hasFocus()) {
                            resetToolbarToMain();
                        }
                    }
                }, 50);
            }
        };
        searchField.setOnFocusChangeListener(focusListener);
        replaceField.setOnFocusChangeListener(focusListener);
    }
    private void setEditTextStyle(AppCompatEditText editText, boolean isDark) {
        GradientDrawable bg = new GradientDrawable();
        bg.setShape(GradientDrawable.RECTANGLE);
        bg.setCornerRadius(16 * getResources().getDisplayMetrics().density);
        bg.setColor(isDark ? Color.parseColor("#33FFFFFF") : Color.parseColor("#1A000000"));
        editText.setBackground(bg);
        int padH = (int) (12 * getResources().getDisplayMetrics().density);
        int padV = (int) (8 * getResources().getDisplayMetrics().density);
        editText.setPadding(padH, padV, padH, padV);
        editText.setTextColor(isDark ? Color.WHITE : Color.BLACK);
        editText.setHintTextColor(isDark ? Color.parseColor("#80FFFFFF") : Color.parseColor("#80000000"));
        editText.setBackgroundTintList(android.content.res.ColorStateList.valueOf(Color.TRANSPARENT));
    }
    private TextView createToolbarButton(String title, View.OnClickListener listener) {
        TextView btn = new TextView(this);
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
        int uiMode = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        boolean isDark = (uiMode == Configuration.UI_MODE_NIGHT_YES);
        int defaultColor = isDark ? Color.parseColor("#F2F2F2") : Color.parseColor("#333333");
        btn.setTextColor(defaultColor);
        int padH = (int) (16 * getResources().getDisplayMetrics().density);
        int padV = (int) (14 * getResources().getDisplayMetrics().density);
        btn.setPadding(padH, padV, padH, padV);
        return btn;
    }
    private void showToolbarPopup(int keyboardHeight) {
        if (toolbarPopup != null && rootView != null && rootView.getWindowToken() != null) {
            toolbarPopup.showAtLocation(rootView, Gravity.BOTTOM | Gravity.LEFT, 0, keyboardHeight);
        }
    }
    private void updateToolbarPopupPosition(int keyboardHeight) {
        if (toolbarPopup != null && toolbarPopup.isShowing()) {
            toolbarPopup.update(0, keyboardHeight, -1, -1);
        }
    }
    private void openSearchUI(boolean withReplace) {
        isSwitchingUI = true;
        mainToolbar.setVisibility(View.GONE);
        searchToolbar.setVisibility(View.VISIBLE);
        replaceToolbar.setVisibility(withReplace ? View.VISIBLE : View.GONE);
        String autoText = cmdGetAutoSearchText();
        if (autoText != null) {
            searchField.setText(autoText);
        }
        searchField.selectAll();
        toolbarPopup.setFocusable(true);
        toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NEEDED);
        toolbarPopup.update();
        Runnable requestFocusTask = new Runnable() {
            int retry = 5;
            @Override
            public void run() {
                searchField.requestFocus();
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                boolean success = false;
                if (imm != null) {
                    success = imm.showSoftInput(searchField, 0);
                }
                if (!success && retry > 0) {
                    retry--;
                    searchField.postDelayed(this, 20);
                } else {
                    mainHandler.postDelayed(() -> isSwitchingUI = false, 300);
                }
            }
        };
        searchField.post(requestFocusTask);
    }
    public void resetToolbarToMain() {
        isSwitchingUI = true;
        resetToolbarToMainInternal();
        if (toolbarPopup != null) {
            toolbarPopup.setFocusable(false);
            toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
            toolbarPopup.update();
        }
        Runnable requestFocusTask = new Runnable() {
            int retry = 5;
            @Override
            public void run() {
                imeView.requestFocus();
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                boolean success = false;
                if (imm != null) {
                    success = imm.showSoftInput(imeView, 0);
                }
                if (!success && retry > 0) {
                    retry--;
                    imeView.postDelayed(this, 20);
                } else {
                    mainHandler.postDelayed(() -> isSwitchingUI = false, 300);
                }
            }
        };
        imeView.post(requestFocusTask);
    }
    private void resetToolbarToMainInternal() {
        mainToolbar.setVisibility(View.VISIBLE);
        searchToolbar.setVisibility(View.GONE);
        replaceToolbar.setVisibility(View.GONE);
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
        cmdReplaceAll();
    }
    private void actionCut() {
        String text = cmdCut();
        if (text != null && !text.isEmpty()) {
            setClipboardText(text);
        }
    }
    private void actionCopy() {
        String text = cmdCopy();
        if (text != null && !text.isEmpty()) {
            setClipboardText(text);
        }
    }
    private void actionPaste() {
        String text = getClipboardText();
        if (text != null && !text.isEmpty()) {
            cmdPaste(text);
        }
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
        int currentLine = cmdGetCurrentLine();
        input.setText(String.valueOf(currentLine));
        input.selectAll();

        AlertDialog dialog = new AlertDialog.Builder(this)
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
            input.requestFocus();
            input.postDelayed(() -> {
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) imm.showSoftInput(input, InputMethodManager.SHOW_IMPLICIT);
            }, 100);
        });
        dialog.show();
    }
    class SearchEditText extends AppCompatEditText {
        public SearchEditText(Context context) { super(context); }
        @Override
        public boolean onKeyPreIme(int keyCode, KeyEvent event) {
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
        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_ENTER) {
                if (this == searchField) { actionFind(true); return true; }
                else if (this == replaceField) { actionReplaceNext(); return true; }
            }
            return super.onKeyDown(keyCode, event);
        }
    }
    private void confirmSaveIfNeeded(Runnable nextAction) {
        if (!cmdIsDirty()) {
            nextAction.run();
            return;
        }
        new AlertDialog.Builder(this)
                .setTitle(getStringResourceByName("dialog_unsaved_title"))
                .setMessage(getStringResourceByName("dialog_unsaved_msg"))
                .setPositiveButton(getStringResourceByName("dialog_btn_save"), (dialog, which) -> actionSaveDocument(nextAction))
                .setNegativeButton(getStringResourceByName("dialog_btn_dont_save"), (dialog, which) -> nextAction.run())
                .setNeutralButton(getStringResourceByName("toolbar_cancel"), null)
                .show();
    }
    private void actionNewDocument() {
        confirmSaveIfNeeded(() -> {
            cmdNewDocument();
            currentDocumentUri = null;
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
    private void actionSaveDocument(Runnable onSuccess) {
        if (currentDocumentUri == null) actionSaveAsDocument(onSuccess);
        else saveToUri(currentDocumentUri, onSuccess);
    }
    private void actionSaveAsDocument(Runnable onSuccess) {
        this.pendingAction = onSuccess;
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_TITLE, getStringResourceByName("untitled") + ".txt");
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
                cmdSetDisplayFileName(getFileName(uri));
                cmdMarkSaved();
                Toast.makeText(this, getStringResourceByName("msg_doc_saved"), Toast.LENGTH_SHORT).show();
                if (onSuccess != null) onSuccess.run();
            } else {
                Toast.makeText(this, getStringResourceByName("msg_encode_fail"), Toast.LENGTH_SHORT).show();
            }
        } catch (Exception e) {
            Toast.makeText(this, getStringResourceByName("msg_save_fail"), Toast.LENGTH_SHORT).show();
        }
    }
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
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
        if (uri.getScheme().equals("content")) {
            try (android.database.Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int idx = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                    if (idx >= 0) result = cursor.getString(idx);
                }
            }
        }
        if (result == null) {
            result = uri.getPath();
            int cut = result.lastIndexOf('/');
            if (cut != -1) result = result.substring(cut + 1);
        }
        return result != null ? result : "Unknown";
    }
    private void loadDocumentFromUri(Uri uri) {
        try {
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
                cmdSetDisplayFileName(getFileName(uri));
                cmdMarkSaved();
                hideSoftwareKeyboard();
                Toast.makeText(this, getStringResourceByName("msg_doc_opened"), Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, getStringResourceByName("msg_open_fail"), Toast.LENGTH_SHORT).show();
            }
        } catch (Exception e) {
            Toast.makeText(this, getStringResourceByName("msg_read_error"), Toast.LENGTH_SHORT).show();
        }
    }
    private void buildProgressiveFade(int topInset, int topMargin) {
        if (blurOverlay == null || topInset <= 0) return;
        int fadeHeight = topMargin + 80;
        ViewGroup.LayoutParams params = blurOverlay.getLayoutParams();
        if (params.height != fadeHeight) {
            params.height = fadeHeight;
            blurOverlay.setLayoutParams(params);
        }
        int uiMode = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        boolean isDark = (uiMode == Configuration.UI_MODE_NIGHT_YES);
        int r = isDark ? 0 : 255;
        int g = isDark ? 0 : 255;
        int b = isDark ? 0 : 255;
        int steps = 20;
        int[] colors = new int[steps];
        for (int i = 0; i < steps; i++) {
            float currentY = (float) i / (steps - 1) * fadeHeight;
            float fraction = (currentY <= topInset * 0.7f) ? 1.0f : (float) Math.pow(Math.max(0.0, 1.0f - ((currentY - topInset * 0.7f) / (fadeHeight - topInset * 0.7f))), 1.5);
            colors[i] = Color.argb((int) (fraction * 255), r, g, b);
        }
        blurOverlay.setBackground(new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM, colors));
    }
    private boolean isHardwareKeyboardConnected() {
        return getResources().getConfiguration().keyboard == Configuration.KEYBOARD_QWERTY;
    }
    public void showSoftwareKeyboardIfNeeded() {
        if (!isHardwareKeyboardConnected()) showSoftwareKeyboard();
    }
    public void showSoftwareKeyboard() {
        runOnUiThread(() -> {
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
            if (imm != null) imm.hideSoftInputFromWindow(getWindow().getDecorView().getWindowToken(), 0);
        });
    }
    class ImeBridgeView extends View {
        public ImeBridgeView(Context context) { super(context); setFocusable(true); setFocusableInTouchMode(true); }
        @Override public boolean onCheckIsTextEditor() { return true; }
        @Override public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE;
            outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_ACTION_NONE;
            return new MiuInputConnection(this, true);
        }
    }
    class MiuInputConnection extends BaseInputConnection {
        public MiuInputConnection(View targetView, boolean fullEditor) { super(targetView, fullEditor); }
        @Override public boolean commitText(CharSequence text, int newCursorPosition) {
            if (text != null) MainActivity.this.commitText(text.toString()); return true;
        }
        @Override public boolean setComposingText(CharSequence text, int newCursorPosition) {
            if (text != null) MainActivity.this.setComposingText(text.toString()); return true;
        }
        @Override public boolean finishComposingText() {
            MainActivity.this.finishComposingTextNative();
            return super.finishComposingText();
        }
        @Override public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            MainActivity.this.deleteSurroundingText(); return true;
        }
        @Override public boolean sendKeyEvent(KeyEvent event) {
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                boolean isCtrl = event.isCtrlPressed() || event.isMetaPressed();
                int keyCode = event.getKeyCode();
                if (isCtrl && keyCode == KeyEvent.KEYCODE_MOVE_HOME) { cmdTop(); return true; }
                if (isCtrl && keyCode == KeyEvent.KEYCODE_MOVE_END) { cmdBottom(); return true; }
                if (keyCode == KeyEvent.KEYCODE_DEL) MainActivity.this.deleteSurroundingText();
                else if (keyCode == KeyEvent.KEYCODE_ENTER) MainActivity.this.commitText("\n");
            }
            return super.sendKeyEvent(event);
        }
    }
}