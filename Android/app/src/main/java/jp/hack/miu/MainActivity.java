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

    private ImeBridgeView imeView;
    private View blurOverlay;

    // ★PopupWindow方式に完全統合（Vulkanの裏に隠れるバグを回避）
    private PopupWindow toolbarPopup;
    private View rootView;
    private boolean isKeyboardOpen = false;
    private int lastKeyboardHeight = 0;

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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            );
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
    }

    private void setupToolbarPopup() {
        LinearLayout rootContainer = new LinearLayout(this);
        rootContainer.setOrientation(LinearLayout.VERTICAL);
        int uiMode = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        boolean isDark = (uiMode == Configuration.UI_MODE_NIGHT_YES);
        rootContainer.setBackgroundColor(isDark ? Color.parseColor("#E62D2D2D") : Color.parseColor("#E6F2F2F7"));

        mainToolbar = new LinearLayout(this);
        HorizontalScrollView scrollView = new HorizontalScrollView(this);
        scrollView.setHorizontalScrollBarEnabled(false);
        LinearLayout buttonStack = new LinearLayout(this);
        buttonStack.setOrientation(LinearLayout.HORIZONTAL);

        buttonStack.addView(createToolbarButton("New", v -> actionNewDocument()));
        buttonStack.addView(createToolbarButton("Open", v -> actionOpenDocument()));
        buttonStack.addView(createToolbarButton("Save", v -> actionSaveDocument(null)));
        buttonStack.addView(createToolbarButton("Find", v -> openSearchUI(false)));
        buttonStack.addView(createToolbarButton("Replace", v -> openSearchUI(true)));
        buttonStack.addView(createToolbarButton("Done", v -> hideSoftwareKeyboard()));

        scrollView.addView(buttonStack);
        mainToolbar.addView(scrollView);

        searchToolbar = new LinearLayout(this);
        searchToolbar.setOrientation(LinearLayout.HORIZONTAL);
        searchToolbar.setGravity(Gravity.CENTER_VERTICAL);
        searchToolbar.setVisibility(View.GONE);

        searchField = new SearchEditText(this);
        searchField.setHint("Search...");
        searchField.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
        searchField.setSingleLine(true);
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
        searchToolbar.addView(createToolbarButton("Cancel", v -> resetToolbarToMain()));

        replaceToolbar = new LinearLayout(this);
        replaceToolbar.setOrientation(LinearLayout.HORIZONTAL);
        replaceToolbar.setGravity(Gravity.CENTER_VERTICAL);
        replaceToolbar.setVisibility(View.GONE);

        replaceField = new SearchEditText(this);
        replaceField.setHint("Replace with...");
        replaceField.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
        replaceField.setSingleLine(true);

        replaceToolbar.addView(replaceField);
        replaceToolbar.addView(createToolbarButton("Rep", v -> actionReplaceNext()));
        replaceToolbar.addView(createToolbarButton("All", v -> actionReplaceAll()));

        LinearLayout optionsToolbar = new LinearLayout(this);
        optionsToolbar.setOrientation(LinearLayout.HORIZONTAL);
        TextView matchCaseBtn = createToolbarButton("Aa", v -> {
            searchMatchCase = !searchMatchCase;
            v.setAlpha(searchMatchCase ? 1.0f : 0.5f);
            updateSearchOptionsToCpp();
        });
        matchCaseBtn.setAlpha(0.5f);
        TextView regexBtn = createToolbarButton(".*", v -> {
            searchRegex = !searchRegex;
            v.setAlpha(searchRegex ? 1.0f : 0.5f);
            updateSearchOptionsToCpp();
        });
        regexBtn.setAlpha(0.5f);
        optionsToolbar.addView(matchCaseBtn);
        optionsToolbar.addView(regexBtn);
        searchToolbar.addView(optionsToolbar);

        int pad = (int) (8 * getResources().getDisplayMetrics().density);
        searchToolbar.setPadding(pad, pad, pad, pad);
        replaceToolbar.setPadding(pad, 0, pad, pad);

        rootContainer.addView(mainToolbar);
        rootContainer.addView(searchToolbar);
        rootContainer.addView(replaceToolbar);

        toolbarPopup = new PopupWindow(rootContainer, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        toolbarPopup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));

        // ★最強のちらつき防止策: OSによる自動Dismissを完全に無効化
        toolbarPopup.setOutsideTouchable(false);
        toolbarPopup.setFocusable(false);
        toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            // ★検索枠にフォーカスがあっても、裏側のエディタ(C++)へのタップを貫通させる
            toolbarPopup.setTouchModal(false);
            toolbarPopup.setIsClippedToScreen(true);
        }

        // 検索枠からフォーカスが外れた場合はメインツールバーに静かに戻す
        View.OnFocusChangeListener focusListener = (v, hasFocus) -> {
            if (!hasFocus) resetToolbarToMain();
        };
        searchField.setOnFocusChangeListener(focusListener);
        replaceField.setOnFocusChangeListener(focusListener);
    }

    private TextView createToolbarButton(String title, View.OnClickListener listener) {
        TextView btn = new TextView(this);
        btn.setText(title);
        btn.setOnClickListener(listener);
        btn.setGravity(Gravity.CENTER);
        btn.setTextSize(16.0f);
        btn.setFocusable(false);
        btn.setFocusableInTouchMode(false);

        int uiMode = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        btn.setTextColor((uiMode == Configuration.UI_MODE_NIGHT_YES) ? Color.parseColor("#4DA8DA") : Color.parseColor("#007AFF"));
        int padH = (int) (16 * getResources().getDisplayMetrics().density);
        int padV = (int) (12 * getResources().getDisplayMetrics().density);
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
        mainToolbar.setVisibility(View.GONE);
        searchToolbar.setVisibility(View.VISIBLE);
        replaceToolbar.setVisibility(withReplace ? View.VISIBLE : View.GONE);

        toolbarPopup.setFocusable(true);
        toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NEEDED);
        toolbarPopup.update();

        searchField.requestFocus();
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.showSoftInput(searchField, InputMethodManager.SHOW_IMPLICIT);
    }

    public void resetToolbarToMain() {
        resetToolbarToMainInternal();

        if (toolbarPopup != null) {
            toolbarPopup.setFocusable(false);
            toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
            toolbarPopup.update(); // 閉じることなくモードだけ変更
        }

        imeView.requestFocus();
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) imm.showSoftInput(imeView, InputMethodManager.SHOW_IMPLICIT);

        commitText("");
    }

    private void resetToolbarToMainInternal() {
        mainToolbar.setVisibility(View.VISIBLE);
        searchToolbar.setVisibility(View.GONE);
        replaceToolbar.setVisibility(View.GONE);
        cmdSetSearchOptions("", "", false, false, false);
    }

    private void updateSearchOptionsToCpp() {
        String q = searchField.getText().toString();
        String r = replaceField.getText().toString();
        cmdSetSearchOptions(q, r, searchMatchCase, searchWholeWord, searchRegex);
        commitText("");
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

    class SearchEditText extends AppCompatEditText {
        public SearchEditText(Context context) { super(context); }
        @Override
        public boolean onKeyPreIme(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
                resetToolbarToMain();
                return true;
            }
            return super.onKeyPreIme(keyCode, event);
        }
    }

    private void confirmSaveIfNeeded(Runnable nextAction) {
        if (!cmdIsDirty()) {
            nextAction.run();
            return;
        }
        new AlertDialog.Builder(this)
                .setTitle("Unsaved Changes")
                .setMessage("Do you want to save the changes before closing?")
                .setPositiveButton("Save", (dialog, which) -> actionSaveDocument(nextAction))
                .setNegativeButton("Don't Save", (dialog, which) -> nextAction.run())
                .setNeutralButton("Cancel", null)
                .show();
    }

    private void actionNewDocument() {
        confirmSaveIfNeeded(() -> {
            cmdNewDocument();
            currentDocumentUri = null;
            hideSoftwareKeyboard();
            Toast.makeText(this, "New Document Created", Toast.LENGTH_SHORT).show();
        });
    }

    private void actionOpenDocument() {
        confirmSaveIfNeeded(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("text/*");
            startActivityForResult(intent, REQUEST_CODE_OPEN_DOCUMENT);
        });
    }

    private void actionSaveDocument(Runnable onSuccess) {
        if (currentDocumentUri == null) {
            actionSaveAsDocument(onSuccess);
        } else {
            saveToUri(currentDocumentUri, onSuccess);
        }
    }

    private void actionSaveAsDocument(Runnable onSuccess) {
        this.pendingAction = onSuccess;
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TITLE, "Untitled.txt");
        startActivityForResult(intent, REQUEST_CODE_SAVE_DOCUMENT);
    }

    private void saveToUri(Uri uri, Runnable onSuccess) {
        try {
            String content = cmdGetTextContent();
            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "wt");
            if (pfd != null) {
                FileOutputStream fos = new FileOutputStream(pfd.getFileDescriptor());
                fos.write(content.getBytes("UTF-8"));
                fos.flush();
                fos.close();
                pfd.close();
            }
            currentDocumentUri = uri;
            cmdMarkSaved();
            Toast.makeText(this, "Document Saved", Toast.LENGTH_SHORT).show();
            if (onSuccess != null) onSuccess.run();
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "Failed to save", Toast.LENGTH_SHORT).show();
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

            if (requestCode == REQUEST_CODE_OPEN_DOCUMENT) {
                loadDocumentFromUri(uri);
            } else if (requestCode == REQUEST_CODE_SAVE_DOCUMENT) {
                saveToUri(uri, pendingAction);
                pendingAction = null;
            }
        } else if (resultCode == RESULT_CANCELED) {
            if (requestCode == REQUEST_CODE_SAVE_DOCUMENT) pendingAction = null;
        }
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
                cmdMarkSaved();
                hideSoftwareKeyboard();
                Toast.makeText(this, "Document Opened", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "Failed to read document", Toast.LENGTH_SHORT).show();
            }
        } catch (Exception e) {
            Toast.makeText(this, "Error reading file", Toast.LENGTH_SHORT).show();
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

    public void showSoftwareKeyboard() {
        runOnUiThread(() -> {
            // ★C++(エディタ本文)からタップ要求が来た際、消去(dismiss)せず静かに中身だけ入れ替える
            if (searchToolbar != null && searchToolbar.getVisibility() == View.VISIBLE) {
                resetToolbarToMainInternal();
                if (toolbarPopup != null) {
                    toolbarPopup.setFocusable(false);
                    toolbarPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
                    toolbarPopup.update(); // 画面にチラつきを出さずに状態だけ更新！
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
        @Override public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            MainActivity.this.deleteSurroundingText(); return true;
        }
        @Override public boolean sendKeyEvent(KeyEvent event) {
            if (event != null && event.getAction() == KeyEvent.ACTION_DOWN) {
                if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) MainActivity.this.deleteSurroundingText();
                else if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER) MainActivity.this.commitText("\n");
            }
            return super.sendKeyEvent(event);
        }
    }
}