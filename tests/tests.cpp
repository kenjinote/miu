// tests.cpp --- miuのテストプログラム
#include <cstdlib>
#include <cstdio>
#include <io.h>
#include <fcntl.h>

#include <string>
#include <locale>

#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

// ファイルを作成する
bool make_file(LPCWSTR path, DWORD mb_size) {
    DWORDLONG mega = 1024 * 1024; // 1MiB = 1024 * 1024 バイト
    DWORDLONG size = mb_size * mega;

    // 既存のファイルを確認
    WIN32_FIND_DATAW find;
    HANDLE hFind = FindFirstFileW(path, &find);
    DWORDLONG file_size = find.nFileSizeHigh;
    file_size <<= 32;
    file_size |= find.nFileSizeLow;
    if (hFind != INVALID_HANDLE_VALUE && file_size == size)
        return true; // 既に存在していてサイズが同じなら作成しない

    wprintf(L"%lu MiBのファイル作成中...\n", mb_size);

    // 指定サイズの文字列を作成
#ifdef _WIN64
    // 64-bitの場合は一気に書き込むため、1つの文字列を使う
    std::string strTotal(size, ' ');
#else
    // 32-bitの場合は複数回に分けて書き込むため、1MiBの文字列を使う
    std::string str1MiB(mega, ' ');
#endif

    // 文字列をファイルを書き込む
    FILE* fout;
    _wfopen_s(&fout, path, L"wb");
    if (!fout) {
        wprintf(L"ファイルオープンエラー\n");
        return false;
    }
#ifdef _WIN64
    // 64-bitの場合は一気に書き込む
    if (!fwrite(strTotal.c_str(), strTotal.size(), 1, fout)) {
        wprintf(L"ファイル書き込みエラー\n");
        fclose(fout);
        return false;
    }
#else
    // 32-bitの場合は複数回に分けて書き込む
    for (DWORD i = 0; i < mb_size; ++i) {
        if (!fwrite(str1MiB.c_str(), str1MiB.size(), 1, fout)) {
            wprintf(L"ファイル書き込みエラー\n");
            fclose(fout);
            return false;
        }
    }
#endif
    fclose(fout);

    wprintf(L"ファイル作成完了: %ls\n", path);
    return true;
}

// ウィンドウを閉じる
BOOL close_window(LPCWSTR class_name, DWORD dwProcessID) {
    // ウィンドウを探す
    HWND hWnd = FindWindow(class_name, nullptr);
    if (!hWnd) return TRUE;
    DWORD pid;
    GetWindowThreadProcessId(hWnd, &pid);
    if (pid != dwProcessID) { // プロセスIDが不一致
        // さらにウィンドウを探す
        WCHAR cls_name[128];
        hWnd = GetTopWindow(nullptr);
        while (hWnd) {
            GetClassNameW(hWnd, cls_name, _countof(cls_name));
            GetWindowThreadProcessId(hWnd, &pid);
            if (lstrcmpiW(cls_name, class_name) == 0 && pid == dwProcessID) { // クラス名とプロセスIDが一致
                break;
            }
            hWnd = GetNextWindow(hWnd, GW_HWNDNEXT); // 次のウィンドウ
        }
        if (!hWnd) {
            wprintf(L"%ls ウィンドウが見つかりません。", class_name);
            return FALSE;
        }
    }

    // ウィンドウを閉じる
    DWORD_PTR result;
    if (!SendMessageTimeoutW(hWnd, WM_SYSCOMMAND, SC_CLOSE, 0, SMTO_ABORTIFHUNG, 3000, &result) &&
        !PostMessageW(hWnd, WM_SYSCOMMAND, SC_CLOSE, 0))
    {
        DestroyWindow(hWnd);
    }
    for (int i = 0; i < 10 && IsWindow(hWnd); ++i)
        Sleep(100);

    if (IsWindow(hWnd)) {
        wprintf(L"%ls ウィンドウが閉じられません。", class_name);
        return FALSE;
    }

    return TRUE;
}

// テスト用ファイルのフルパスを取得する
void get_test_file_path(LPWSTR path, DWORD path_max) {
    GetModuleFileNameW(nullptr, path, path_max);
    PathRemoveFileSpecW(path);
    PathAppendW(path, L"\\file.txt");
}

// 起動時間を計測する
bool measure_startup_time(LPCWSTR program, LPCWSTR class_name, long mb_size) {
    // テスト用ファイルのフルパスを取得する
    WCHAR path[MAX_PATH];
    get_test_file_path(path, _countof(path));

    // 必要ならファイルを作成する
    bool use_file = (mb_size >= 0);
    if (use_file) {
        if (!make_file(path, mb_size)) {
            wprintf(L"ファイル %ls が作れません。\n", path);
            return false;
        }
    }

    // 起動時間の測定開始
    DWORD dwTick0 = GetTickCount();

    // program を起動する
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = program;
    sei.lpParameters = (mb_size >= 0) ? path : nullptr;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        wprintf(L"%ls が起動できません。\n", program);
        return false;
    }
    DWORD pid = GetProcessId(sei.hProcess);
    WaitForInputIdle(sei.hProcess, INFINITE); // 起動するまで待つ
    CloseHandle(sei.hProcess);

    // 起動時間の測定終了
    DWORD dwTick1 = GetTickCount();

    WCHAR label[128];
    if (!use_file)
        swprintf_s(label, L"ファイルなし");
    else
        swprintf_s(label, L"%lu MiBのファイル使用", mb_size);

    wprintf(L"%ls (%ls) の起動時間: %lu [ミリ秒]\n", class_name, label, dwTick1 - dwTick0);

    // ウィンドウを閉じる
    close_window(class_name, pid);

    return true;
}

int main(void) {
    // wprintfの出力を正しく行うためのおまじない
    _setmode(_fileno(stdout), _O_U8TEXT);
    SetConsoleOutputCP(CP_UTF8);
    std::setlocale(LC_ALL, "");

#ifndef NDEBUG // Debug versionのとき
    wprintf(L"Release versionで実行してください。\n");
    return 0;
#else
    // このEXEと同じフォルダの miu.exe を取得する
    WCHAR miu_path[MAX_PATH];
    GetModuleFileNameW(nullptr, miu_path, _countof(miu_path));
    PathRemoveFileSpecW(miu_path);
    PathAppendW(miu_path, L"miu.exe");
    wprintf(L"miu のパス: %ls\n", miu_path);

    // Notepad.exe のフルパスを取得する
    WCHAR notepad_path[MAX_PATH];
    GetSystemDirectoryW(notepad_path, _countof(notepad_path));
    PathAppend(notepad_path, L"Notepad.exe");
    wprintf(L"メモ帳(Notepad)のパス: %ls\n", notepad_path);

    // 計測実行
    // キャッシュを考慮するためそれぞれ2回実行する
    // メモ帳(Notepad)は大きいファイルを開けない
    bool ok = true;
    ok = ok && measure_startup_time(notepad_path, L"Notepad", -1);
    ok = ok && measure_startup_time(notepad_path, L"Notepad", -1);
    ok = ok && measure_startup_time(miu_path, L"miu", -1);
    ok = ok && measure_startup_time(miu_path, L"miu", -1);
    ok = ok && measure_startup_time(notepad_path, L"Notepad", 1);
    ok = ok && measure_startup_time(notepad_path, L"Notepad", 1);
    ok = ok && measure_startup_time(miu_path, L"miu", 1);
    ok = ok && measure_startup_time(miu_path, L"miu", 1);
    ok = ok && measure_startup_time(miu_path, L"miu", 1024);
    ok = ok && measure_startup_time(miu_path, L"miu", 1024);

    // テスト用ファイルを削除する
    WCHAR path[MAX_PATH];
    get_test_file_path(path, _countof(path));
    DeleteFileW(path);
    wprintf(L"ファイル削除: %ls\n", path);

    if (!ok) {
        wprintf(L"テスト失敗\n");
        return 1;
    }

    wprintf(L"テスト成功\n");
    return 0;
#endif
}
