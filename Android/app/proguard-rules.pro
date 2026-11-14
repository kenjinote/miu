# Project-specific ProGuard / R8 rules for miu Android (NativeActivity + JNI + WebView Bridge)

-keep class jp.hack.miu.MainActivity {
    public *;
    native <methods>;
}

-keep class jp.hack.miu.MainActivity$MiuPreviewBridge {
    <init>(...);
    @android.webkit.JavascriptInterface <methods>;
    public void postMessage(java.lang.String);
}

-keepattributes *Annotation*,InnerClasses,EnclosingMethod,SourceFile,LineNumberTable
