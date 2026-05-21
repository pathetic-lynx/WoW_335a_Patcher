#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "patches.h"

enum {
    ID_EDIT_PATH = 101,
    ID_BTN_BROWSE,
    ID_BTN_VERIFY,
    ID_BTN_APPLY,
    ID_BTN_ALL,
    ID_BTN_NONE,
    ID_LIST,
    ID_STATIC_STATUS,
};

static std::vector<Patch>       g_patches  = default_patches();
static std::vector<PatchStatus> g_statuses;

struct {
    HWND lbl, path, browse, status, verify, apply, all, none, list;
} g_ctl;

static void refresh_list() {
    for (size_t i = 0; i < g_patches.size(); i++) {
        ListView_SetCheckState(g_ctl.list, (int)i, g_patches[i].enabled ? TRUE : FALSE);

        const char* st = "---";
        if (i < g_statuses.size()) {
            switch (g_statuses[i]) {
                case PatchStatus::Applied:   st = "APPLIED";   break;
                case PatchStatus::Unpatched: st = "UNPATCHED"; break;
                case PatchStatus::Mismatch:  st = "MISMATCH";  break;
            }
        }
        LVITEMA it{};
        it.mask     = LVIF_TEXT;
        it.iItem    = (int)i;
        it.iSubItem = 1;
        it.pszText  = (LPSTR)st;
        ListView_SetItem(g_ctl.list, &it);
    }
}

static void update_status() {
    char buf[MAX_PATH];
    GetWindowTextA(g_ctl.path, buf, MAX_PATH);
    if (!buf[0]) { SetWindowTextA(g_ctl.status, "No file selected."); return; }

    std::error_code ec;
    if (!std::filesystem::exists(buf, ec)) {
        SetWindowTextA(g_ctl.status, "File not found.");
        return;
    }
    auto sz = static_cast<std::streamsize>(std::filesystem::file_size(buf, ec));
    if (ec) { SetWindowTextA(g_ctl.status, "Unreadable."); return; }

    char msg[128];
    if (sz != kExpectedFileSize)
        snprintf(msg, sizeof(msg), "Size mismatch (%lld bytes, expected %lld)",
                 (long long)sz, (long long)kExpectedFileSize);
    else
        snprintf(msg, sizeof(msg), "OK  (%lld bytes)", (long long)sz);
    SetWindowTextA(g_ctl.status, msg);
}

static void action_verify() {
    char buf[MAX_PATH];
    GetWindowTextA(g_ctl.path, buf, MAX_PATH);
    std::string path = buf;
    g_statuses.resize(g_patches.size());
    for (size_t i = 0; i < g_patches.size(); i++) {
        g_statuses[i] = check_patch(path, g_patches[i]);
        g_patches[i].enabled = (g_statuses[i] == PatchStatus::Applied);
    }
    refresh_list();
}

static void action_apply(HWND hwnd) {
    char buf[MAX_PATH];
    GetWindowTextA(g_ctl.path, buf, MAX_PATH);
    std::string path = buf;

    if (MessageBoxA(hwnd, "Write patch state to the selected file?",
                    "Confirm Apply", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    for (size_t i = 0; i < g_patches.size(); i++)
        g_patches[i].enabled = (ListView_GetCheckState(g_ctl.list, (int)i) != 0);

    std::vector<PatchStatus> pre(g_patches.size());
    for (size_t i = 0; i < g_patches.size(); i++)
        if (!g_patches[i].enabled) pre[i] = check_patch(path, g_patches[i]);

    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
        MessageBoxA(hwnd, "Failed to open file for writing.", "Error", MB_ICONERROR);
        return;
    }
    for (size_t i = 0; i < g_patches.size(); i++) {
        const auto& p = g_patches[i];
        for (const auto& bp : p.changes) {
            const std::vector<uint8_t>* bytes = nullptr;
            if (p.enabled)
                bytes = &bp.replacement;
            else if (pre[i] == PatchStatus::Applied)
                bytes = &bp.original;
            if (!bytes) continue;
            f.clear();
            f.seekp(bp.offset);
            if (!f) continue;
            f.write(reinterpret_cast<const char*>(bytes->data()),
                    static_cast<std::streamsize>(bytes->size()));
        }
    }
    f.close();
    action_verify();
    MessageBoxA(hwnd, "Patches applied.", "Done", MB_ICONINFORMATION);
}

static void browse(HWND hwnd) {
    char buf[MAX_PATH] = {};
    GetWindowTextA(g_ctl.path, buf, MAX_PATH);
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter = "Executable Files\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = sizeof(buf);
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = "Select WoW.exe";
    if (!GetOpenFileNameA(&ofn)) return;
    SetWindowTextA(g_ctl.path, buf);
    update_status();
    action_verify();
}

static void layout(HWND hwnd) {
    RECT r;
    GetClientRect(hwnd, &r);
    int w = r.right, h = r.bottom;

    SetWindowPos(g_ctl.lbl,    nullptr, 8,       10,      32,       20,  SWP_NOZORDER);
    SetWindowPos(g_ctl.path,   nullptr, 44,       8, w-44-90,       22,  SWP_NOZORDER);
    SetWindowPos(g_ctl.browse, nullptr, w-82,     7,      74,       24,  SWP_NOZORDER);
    SetWindowPos(g_ctl.status, nullptr, 8,       36,    w-16,       20,  SWP_NOZORDER);
    SetWindowPos(g_ctl.verify, nullptr, 8,       62,      76,       24,  SWP_NOZORDER);
    SetWindowPos(g_ctl.apply,  nullptr, 92,      62,      76,       24,  SWP_NOZORDER);
    SetWindowPos(g_ctl.all,    nullptr, w-162,   62,      74,       24,  SWP_NOZORDER);
    SetWindowPos(g_ctl.none,   nullptr, w-82,    62,      74,       24,  SWP_NOZORDER);
    SetWindowPos(g_ctl.list,   nullptr, 8,       94,    w-16, h-102,     SWP_NOZORDER);

    RECT lr;
    GetClientRect(g_ctl.list, &lr);
    ListView_SetColumnWidth(g_ctl.list, 0, lr.right - 100);
    ListView_SetColumnWidth(g_ctl.list, 1, 95);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        auto mk = [&](LPCSTR cls, LPCSTR txt, DWORD sty, HMENU id) -> HWND {
            HWND h = CreateWindowExA(0, cls, txt, WS_CHILD | WS_VISIBLE | sty,
                                     0, 0, 0, 0, hwnd, id, nullptr, nullptr);
            SendMessageA(h, WM_SETFONT, (WPARAM)hf, FALSE);
            return h;
        };
        g_ctl.lbl    = mk("STATIC", "File:",       SS_LEFT,       (HMENU)0);
        g_ctl.path   = [&] {
            HWND h = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                         WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                         0, 0, 0, 0, hwnd, (HMENU)ID_EDIT_PATH, nullptr, nullptr);
            SendMessageA(h, WM_SETFONT, (WPARAM)hf, FALSE);
            return h;
        }();
        g_ctl.browse = mk("BUTTON", "Browse...",   BS_PUSHBUTTON, (HMENU)ID_BTN_BROWSE);
        g_ctl.status = mk("STATIC", "No file selected.", SS_LEFT, (HMENU)ID_STATIC_STATUS);
        g_ctl.verify = mk("BUTTON", "Verify",      BS_PUSHBUTTON, (HMENU)ID_BTN_VERIFY);
        g_ctl.apply  = mk("BUTTON", "Apply",       BS_PUSHBUTTON, (HMENU)ID_BTN_APPLY);
        g_ctl.all    = mk("BUTTON", "All",         BS_PUSHBUTTON, (HMENU)ID_BTN_ALL);
        g_ctl.none   = mk("BUTTON", "None",        BS_PUSHBUTTON, (HMENU)ID_BTN_NONE);

        g_ctl.list = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, (HMENU)ID_LIST, nullptr, nullptr);
        SendMessageA(g_ctl.list, WM_SETFONT, (WPARAM)hf, FALSE);
        ListView_SetExtendedListViewStyle(g_ctl.list,
            LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNA col{};
        col.mask    = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt     = LVCFMT_LEFT;
        col.cx      = 500; col.pszText = (LPSTR)"Patch";
        ListView_InsertColumn(g_ctl.list, 0, &col);
        col.cx      = 90;  col.pszText = (LPSTR)"Status";
        ListView_InsertColumn(g_ctl.list, 1, &col);

        for (size_t i = 0; i < g_patches.size(); i++) {
            LVITEMA it{};
            it.mask    = LVIF_TEXT;
            it.iItem   = (int)i;
            it.pszText = (LPSTR)g_patches[i].name.c_str();
            ListView_InsertItem(g_ctl.list, &it);
            ListView_SetCheckState(g_ctl.list, (int)i, TRUE);
            it.iSubItem = 1; it.pszText = (LPSTR)"---";
            ListView_SetItem(g_ctl.list, &it);
        }
        layout(hwnd);
        break;
    }
    case WM_SIZE:
        layout(hwnd);
        break;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lp)->ptMinTrackSize = {500, 400};
        break;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BTN_BROWSE: browse(hwnd); break;
        case ID_BTN_VERIFY: update_status(); action_verify(); break;
        case ID_BTN_APPLY:  action_apply(hwnd); break;
        case ID_BTN_ALL:
            for (size_t i = 0; i < g_patches.size(); i++)
                ListView_SetCheckState(g_ctl.list, (int)i, TRUE);
            break;
        case ID_BTN_NONE:
            for (size_t i = 0; i < g_patches.size(); i++)
                ListView_SetCheckState(g_ctl.list, (int)i, FALSE);
            break;
        case ID_EDIT_PATH:
            if (HIWORD(wp) == EN_KILLFOCUS) update_status();
            break;
        }
        break;
    case WM_DROPFILES: {
        char buf[MAX_PATH] = {};
        DragQueryFileA((HDROP)wp, 0, buf, MAX_PATH);
        DragFinish((HDROP)wp);
        SetWindowTextA(g_ctl.path, buf);
        update_status();
        action_verify();
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmd, int nShow) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "WoWPatcher";
    wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconA(nullptr, IDI_APPLICATION);
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(WS_EX_ACCEPTFILES, "WoWPatcher",
        "WoW 3.3.5a Patcher", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 560,
        nullptr, nullptr, hInst, nullptr);

    if (lpCmd && lpCmd[0]) {
        std::string p = lpCmd;
        if (p.size() >= 2 && p.front() == '"' && p.back() == '"')
            p = p.substr(1, p.size() - 2);
        SetWindowTextA(g_ctl.path, p.c_str());
        update_status();
        action_verify();
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
