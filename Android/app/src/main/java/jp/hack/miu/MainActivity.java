package jp.hack.miu;

import android.app.NativeActivity;
import android.content.Context;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

public class MainActivity extends NativeActivity {

    static {
        System.loadLibrary("miu");
    }

    public native void commitText(String text);
    public native void setComposingText(String text);
    public native void deleteSurroundingText();
    public native void updateVisibleHeight(int bottomInset);
    public native void updateTopMargin(int topMargin);

    private ImeBridgeView imeView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 画面をステータスバーの裏側まで広げる (Edge-to-Edge)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            );
        }

        // ステータスバー（時計エリア）自体の背景を完全に透明にする
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        getWindow().setStatusBarColor(Color.TRANSPARENT);

        imeView = new ImeBridgeView(this);
        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(1, 1);
        addContentView(imeView, layoutParams);

        FrameLayout rootView = (FrameLayout) getWindow().getDecorView().findViewById(android.R.id.content);

        // キーボードとステータスバーの高さを検知してC++に伝える
        rootView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                int bottomInset = 0;
                int topInset = 0;

                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
                    bottomInset = insets.getInsets(WindowInsets.Type.ime() | WindowInsets.Type.systemBars()).bottom;
                    topInset = insets.getInsets(WindowInsets.Type.statusBars()).top;
                } else {
                    bottomInset = insets.getSystemWindowInsetBottom();
                    topInset = insets.getSystemWindowInsetTop();
                }

                updateVisibleHeight(bottomInset);

                // ステータスバーの高さ + 少しの遊び(20px) をC++へ伝える
                // C++側(Vulkan)でこの値をもとにフェードアウト効果を描画します
                updateTopMargin(topInset + 20);

                return insets;
            }
        });
    }

    // C++側から画面がタッチされた時に呼ばれるメソッド
    public void showSoftwareKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                imeView.requestFocus();
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.showSoftInput(imeView, InputMethodManager.SHOW_IMPLICIT);
                }
            }
        });
    }

    public void hideSoftwareKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.hideSoftInputFromWindow(getWindow().getDecorView().getWindowToken(), 0);
                }
            }
        });
    }

    // =========================================================
    // IMEと通信するためのカスタムビュー
    // =========================================================
    class ImeBridgeView extends View {
        public ImeBridgeView(Context context) {
            super(context);
            setFocusable(true);
            setFocusableInTouchMode(true);
        }

        @Override
        public boolean onCheckIsTextEditor() {
            return true;
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE;
            outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_ACTION_NONE;
            return new MiuInputConnection(this, true);
        }
    }

    // =========================================================
    // キーボードの入力イベントをC++へ中継するコネクション
    // =========================================================
    class MiuInputConnection extends BaseInputConnection {
        public MiuInputConnection(View targetView, boolean fullEditor) {
            super(targetView, fullEditor);
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition) {
            if (text != null) {
                MainActivity.this.commitText(text.toString());
            }
            return true;
        }

        @Override
        public boolean setComposingText(CharSequence text, int newCursorPosition) {
            if (text != null) {
                MainActivity.this.setComposingText(text.toString());
            }
            return true;
        }

        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            MainActivity.this.deleteSurroundingText();
            return true;
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            if (event != null && event.getAction() == KeyEvent.ACTION_DOWN) {
                if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) {
                    MainActivity.this.deleteSurroundingText();
                } else if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
                    MainActivity.this.commitText("\n");
                }
            }
            return super.sendKeyEvent(event);
        }
    }
}