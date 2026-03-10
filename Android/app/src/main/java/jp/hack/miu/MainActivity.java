package jp.hack.miu;

import android.app.NativeActivity;
import android.content.Context;
import android.os.Bundle;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;

public class MainActivity extends NativeActivity {

    static {
        System.loadLibrary("miu");
    }
    // C++ (main.cpp) で定義した JNI関数の宣言
    public native void commitText(String text);
    public native void setComposingText(String text);
    public native void deleteSurroundingText();

    private ImeBridgeView imeView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 画面の隅に1x1ピクセルの「見えない入力ビュー」を配置する
        imeView = new ImeBridgeView(this);
        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(1, 1);
        addContentView(imeView, layoutParams);
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