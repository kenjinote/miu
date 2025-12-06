// tests.cpp --- miuのテストプログラム
#include <cstdlib>
#include <cstdio>
#include <io.h>
#include <fcntl.h>

#include <string>
#include <locale>

#include <windows.h>
#include <shlwapi.h> // Path..
#include <strsafe.h> // StringCch...
#pragma comment(lib, "shlwapi.lib")

// ストップウォッチ
class StopWatch {
public:
    LARGE_INTEGER m_freq; // 周波数
    LARGE_INTEGER m_start, m_stop; // 開始時間と終了時間
    BOOL m_qp_supported; // 計測用のQueryPerformanceCounterがサポートされているか？
    StopWatch() : m_qp_supported() {
        m_qp_supported = QueryPerformanceFrequency(&m_freq);
        if (!m_qp_supported) {
            wprintf(L"QueryPerformanceFrequency エラー: %lu\n", GetLastError());
        }
    }
    bool start() {
        if (m_qp_supported) {
            if (QueryPerformanceCounter(&m_start))
                return true;
            wprintf(L"QueryPerformanceCounter エラー: %lu\n", GetLastError());
            m_qp_supported = false;
        }
        m_start.QuadPart = (LONGLONG)GetTickCount64();
        return true;
    }
    double stop() {
        if (m_qp_supported) {
            if (!QueryPerformanceCounter(&m_stop)) {
                wprintf(L"QueryPerformanceCounter エラー: %lu\n", GetLastError());
                return -1.0;
            }
        } else {
            m_stop.QuadPart = (LONGLONG)GetTickCount64();
        }
        LONGLONG delta = m_stop.QuadPart - m_start.QuadPart;
        double milliseconds = m_qp_supported ? ((double)delta * 1000.0 / (double)m_freq.QuadPart) : delta;
        return milliseconds;
    }
};

// ファイルを作成する
bool make_file(LPCWSTR path, size_t mb_size) {
    size_t mega = 1024 * 1024; // 1MiB = 1024 * 1024 バイト
    DWORDLONG size = mb_size * (DWORDLONG)mega;

    // 既存のファイルを確認
    WIN32_FIND_DATAW find;
    HANDLE hFind = FindFirstFileW(path, &find);
    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
        DWORDLONG file_size = find.nFileSizeHigh;
        file_size <<= 32;
        file_size |= find.nFileSizeLow;
        if (file_size == size)
            return true; // 既に存在していてサイズが同じなら作成しない
    }

    wprintf(L"%llu MiBのファイル作成中...\n", (unsigned long long)mb_size);

    // サイズ1MiBの文字列を作成
    std::string str1MiB(mega, 'X');

    // 文字列をファイルを書き込む
    HANDLE hFile = CreateFileW(
        path, GENERIC_WRITE, FILE_SHARE_DELETE | FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        wprintf(L"ファイルオープンエラー\n");
        return false;
    }
    for (size_t i = 0; i < mb_size; ++i) {
        DWORD cbWritten;
        if (!WriteFile(hFile, str1MiB.c_str(), (DWORD)str1MiB.size(), &cbWritten, nullptr) ||
            cbWritten != str1MiB.size())
        {
            wprintf(L"ファイル書き込みエラー\n");
            CloseHandle(hFile);
            return false;
        }
    }
    SetEndOfFile(hFile);
    CloseHandle(hFile);
    Sleep(800); // ファイルシステムの遅延を考慮して少し待つ

    wprintf(L"ファイル作成完了: %ls\n", path);
    return true;
}

// ウィンドウを探すための構造体
struct FIND_WINDOW {
    LPCWSTR class_name; // クラス名
    DWORD pid; // プロセスID
    HWND found; // 見つかったウィンドウ
};

// ウィンドウを探すコールバック関数
BOOL CALLBACK find_window_proc(HWND hwnd, LPARAM lParam) {
    FIND_WINDOW* data = reinterpret_cast<FIND_WINDOW*>(lParam);

    // ウィンドウのプロセスIDを取得
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != data->pid)
        return TRUE; // 継続

    // ウィンドウクラス名を取得
    WCHAR cls_name[128];
    if (!GetClassNameW(hwnd, cls_name, _countof(cls_name)))
        return TRUE; // 継続

    if (lstrcmpiW(cls_name, data->class_name) != 0)
        return TRUE; // 継続

    data->found = hwnd; // 見つかった
    return FALSE; // 中断
}
 
// ウィンドウを閉じる
BOOL close_window(LPCWSTR class_name, DWORD pid) {
    // ウィンドウを探す
    FIND_WINDOW find_window_data = { class_name, pid, nullptr };
    EnumWindows(find_window_proc, (LPARAM)&find_window_data);
    HWND hwnd = find_window_data.found;
    if (!hwnd)
        return TRUE; // ウィンドウが見つからなかった

    // ウィンドウを閉じる
    DWORD_PTR result;
    if (!SendMessageTimeoutW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0, SMTO_ABORTIFHUNG, 3000, &result) &&
        !PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0))
    {
        DestroyWindow(hwnd);
    }
    for (int i = 0; i < 10 && IsWindow(hwnd); ++i)
        Sleep(100);

    if (IsWindow(hwnd)) {
        wprintf(L"%ls ウィンドウが閉じられません。", class_name);
        return FALSE;
    }

    Sleep(800); // アプリ終了後のファイルのロック解除を少し待つ
    return TRUE;
}

// テスト用ファイルのフルパスを取得する
void get_test_file_path(LPWSTR path, DWORD path_max) {
    GetModuleFileNameW(nullptr, path, path_max);
    PathRemoveFileSpecW(path);
    PathAppendW(path, L"file.txt");
}

// 起動時間を計測する
bool measure_startup_time(LPCWSTR program, LPCWSTR class_name, size_t mb_size) {
    // テスト用ファイルのフルパスを取得する
    WCHAR path[MAX_PATH];
    get_test_file_path(path, _countof(path));

    // 必要ならファイルを作成する
    bool use_file = (mb_size > 0);
    if (use_file) {
        if (!make_file(path, mb_size)) {
            wprintf(L"ファイル %ls が作れません。\n", path);
            return false;
        }
    }

    WCHAR params[MAX_PATH + 16];
    HRESULT hr = StringCchPrintfW(params, _countof(params), L"\"%s\"", path);
    if (FAILED(hr)) {
        wprintf(L"StringCchPrintfW失敗: %lu\n", hr);
        return false;
    }

    // 起動時間の測定開始
    StopWatch stop_watch;
    if (!stop_watch.start())
        return false;

    // program を起動する
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = program;
    sei.lpParameters = (mb_size > 0) ? params : nullptr;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) {
        wprintf(L"%ls が起動できません(エラー: %lu)。\n", program, GetLastError());
        return false;
    }
    if (!sei.hProcess) {
        wprintf(L"sei.hProcessが nullptr でした。\n");
        return false;
    }
    DWORD pid = GetProcessId(sei.hProcess);

    // 起動するまで15秒待つ
    DWORD seconds = 15;
    DWORD wait = WaitForInputIdle(sei.hProcess, seconds * 1000);
    CloseHandle(sei.hProcess);
    if (wait == WAIT_FAILED) {
        wprintf(L"WaitForInputIdle のエラー: %lu\n", GetLastError());
        return false;
    }
    if (wait == WAIT_TIMEOUT) {
        wprintf(L"%lu秒以内に起動が完了しませんでした（タイムアウト）\n", seconds);
        return false;
    }

    // 起動時間の測定終了
    double milliseconds = stop_watch.stop();

    WCHAR label[128];
    if (!use_file)
        swprintf_s(label, L"ファイルなし");
    else
        swprintf_s(label, L"%llu MiBのファイル使用", (unsigned long long)mb_size);

    if (milliseconds < 0)
        wprintf(L"%ls (%ls) 失敗\n", class_name, label);
    else
        wprintf(L"%ls (%ls) の起動時間: %.2f [ミリ秒]\n", class_name, label, milliseconds);

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
    wprintf(L"警告：Debug versionで実行しています。\n");
#endif
    // このEXEと同じフォルダの miu.exe のフルパスを取得する
    WCHAR miu_path[MAX_PATH];
    GetModuleFileNameW(nullptr, miu_path, _countof(miu_path));
    PathRemoveFileSpecW(miu_path);
    PathAppendW(miu_path, L"miu.exe");
    wprintf(L"miu のパス: %ls\n", miu_path);

    // Notepad.exe のフルパスを取得する
    WCHAR notepad_path[MAX_PATH];
    GetSystemDirectoryW(notepad_path, _countof(notepad_path));
    PathAppendW(notepad_path, L"Notepad.exe");
    wprintf(L"メモ帳(Notepad)のパス: %ls\n", notepad_path);

    // 計測実行
    // キャッシュを考慮するためそれぞれ2回実行する
    // NOTE: メモ帳(Notepad)は大きいファイル（32MB+ or 1GB+）を開けない
    bool ok = true;
    ok = ok && measure_startup_time(notepad_path, L"Notepad", 0);
    ok = ok && measure_startup_time(notepad_path, L"Notepad", 0);
    ok = ok && measure_startup_time(miu_path, L"miu", 0);
    ok = ok && measure_startup_time(miu_path, L"miu", 0);
    ok = ok && measure_startup_time(notepad_path, L"Notepad", 1);
    ok = ok && measure_startup_time(notepad_path, L"Notepad", 1);
    ok = ok && measure_startup_time(miu_path, L"miu", 1);
    ok = ok && measure_startup_time(miu_path, L"miu", 1);
    ok = ok && measure_startup_time(notepad_path, L"notepad", 8);
    ok = ok && measure_startup_time(notepad_path, L"notepad", 8);
    ok = ok && measure_startup_time(miu_path, L"miu", 8);
    ok = ok && measure_startup_time(miu_path, L"miu", 8);

    // テスト用ファイルを削除する
    WCHAR path[MAX_PATH];
    get_test_file_path(path, _countof(path));
    if (DeleteFileW(path))
        wprintf(L"ファイル削除: %ls\n", path);

    if (!ok) {
        wprintf(L"テスト失敗\n");
        return 1;
    }

    wprintf(L"テスト成功\n");
    return 0;
}
