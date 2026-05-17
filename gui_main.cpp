#include "patches.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include <SDL.h>
#include <SDL_opengl.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <string>

// ── Native file dialog ────────────────────────────────────────────────────────
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <commdlg.h>

static std::string native_open_dialog() {
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn   = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.lpstrFilter     = "Executable Files\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile       = buf;
    ofn.nMaxFile        = sizeof(buf);
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle      = "Select WoW.exe";
    return GetOpenFileNameA(&ofn) ? buf : "";
}

#elif defined(__APPLE__)
static std::string native_open_dialog() {
    FILE* f = popen(
        "osascript -e 'POSIX path of (choose file with prompt \"Select WoW.exe\")' 2>/dev/null",
        "r");
    if (!f) return "";
    char buf[512] = {};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return buf;
}

#else // Linux / BSD
static std::string try_cmd(const char* cmd) {
    FILE* f = popen(cmd, "r");
    if (!f) return "";
    char buf[512] = {};
    fgets(buf, sizeof(buf), f);
    int rc = pclose(f);
    if (rc != 0) return "";            // user cancelled or tool missing
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return buf;
}

static std::string native_open_dialog() {
    std::string r;
    // zenity (GTK / GNOME)
    r = try_cmd("zenity --file-selection --title='Select WoW.exe' 2>/dev/null");
    if (!r.empty()) return r;
    // kdialog (KDE)
    r = try_cmd("kdialog --getopenfilename '$HOME' '*.exe *' 2>/dev/null");
    if (!r.empty()) return r;
    // yad (Yet Another Dialog)
    r = try_cmd("yad --file-selection --title='Select WoW.exe' 2>/dev/null");
    return r;
}
#endif

// ── State ─────────────────────────────────────────────────────────────────────
static std::vector<Patch>       g_patches  = default_patches();
static std::vector<PatchStatus> g_statuses;
static char                     g_path[512] = {};
static std::deque<std::string>  g_log;
static bool                     g_open_confirm = false;
static bool                     g_dragging     = false; // file being dragged over window

// ── Helpers ───────────────────────────────────────────────────────────────────
static void append_log(const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char ts[10];
    std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&t));
    g_log.push_front(std::string("[") + ts + "] " + msg);
    if (g_log.size() > 300) g_log.pop_back();
}

static void set_path(const char* p) {
    std::strncpy(g_path, p, sizeof(g_path) - 1);
    g_path[sizeof(g_path) - 1] = '\0';
    g_statuses.clear();
}

static ImVec4 status_col(PatchStatus s) {
    switch (s) {
        case PatchStatus::Applied:   return {0.20f, 0.85f, 0.35f, 1.f};
        case PatchStatus::Unpatched: return {1.00f, 0.78f, 0.10f, 1.f};
        case PatchStatus::Mismatch:  return {1.00f, 0.30f, 0.30f, 1.f};
    }
    return {0.5f, 0.5f, 0.5f, 1.f};
}

static const char* status_str(PatchStatus s) {
    switch (s) {
        case PatchStatus::Applied:   return "APPLIED";
        case PatchStatus::Unpatched: return "UNPATCHED";
        case PatchStatus::Mismatch:  return "MISMATCH";
    }
    return "---";
}

// ── Actions ───────────────────────────────────────────────────────────────────
static void action_verify() {
    std::string path = g_path;
    g_statuses.resize(g_patches.size());
    int applied = 0, unpatched = 0, mismatch = 0;
    for (size_t i = 0; i < g_patches.size(); i++) {
        g_statuses[i] = check_patch(path, g_patches[i]);
        switch (g_statuses[i]) {
            case PatchStatus::Applied:   applied++;   break;
            case PatchStatus::Unpatched: unpatched++; break;
            case PatchStatus::Mismatch:  mismatch++;  break;
        }
    }
    append_log("Verify: " + std::to_string(applied) + " applied, "
             + std::to_string(unpatched) + " unpatched, "
             + std::to_string(mismatch) + " mismatch");
}

static void action_backup() {
    std::string path = g_path;
    std::string bk   = path + ".backup";
    try {
        std::filesystem::copy(path, bk, std::filesystem::copy_options::overwrite_existing);
        append_log("Backup created: " + bk);
    } catch (const std::exception& e) {
        append_log("Backup failed: " + std::string(e.what()));
    }
}

static void action_restore() {
    std::string path = g_path;
    std::string bk   = path + ".backup";
    try {
        std::filesystem::copy(bk, path, std::filesystem::copy_options::overwrite_existing);
        g_statuses.clear();
        append_log("Restored from: " + bk);
    } catch (const std::exception& e) {
        append_log("Restore failed: " + std::string(e.what()));
    }
}

static void action_apply() {
    std::string path = g_path;
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) { append_log("Error: could not open file for writing."); return; }

    int ok = 0, fail = 0;
    for (const auto& p : g_patches) {
        if (!p.enabled) continue;
        bool all_ok = true;
        for (const auto& bp : p.changes) {
            f.clear();
            f.seekp(bp.offset);
            if (!f) { all_ok = false; break; }
            f.write(reinterpret_cast<const char*>(bp.replacement.data()),
                    static_cast<std::streamsize>(bp.replacement.size()));
            if (!f) { all_ok = false; break; }
        }
        if (all_ok) ok++;
        else { fail++; append_log("Write failed: " + p.name); }
    }
    f.close();
    g_statuses.clear();
    append_log("Applied " + std::to_string(ok) + " patch(es)"
             + (fail ? (", " + std::to_string(fail) + " failed") : "."));
}

// ── Theme ─────────────────────────────────────────────────────────────────────
static void apply_theme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding     = {12, 10};
    s.FramePadding      = {8, 4};
    s.ItemSpacing       = {8, 6};
    s.WindowRounding    = 6;
    s.FrameRounding     = 4;
    s.ChildRounding     = 4;
    s.ScrollbarRounding = 4;
    s.GrabRounding      = 4;
    s.PopupRounding     = 5;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = {0.10f, 0.10f, 0.14f, 1.f};
    c[ImGuiCol_ChildBg]          = {0.12f, 0.12f, 0.17f, 1.f};
    c[ImGuiCol_PopupBg]          = {0.12f, 0.12f, 0.18f, 1.f};
    c[ImGuiCol_Border]           = {0.28f, 0.28f, 0.40f, 0.7f};
    c[ImGuiCol_FrameBg]          = {0.16f, 0.16f, 0.22f, 1.f};
    c[ImGuiCol_FrameBgHovered]   = {0.20f, 0.20f, 0.30f, 1.f};
    c[ImGuiCol_FrameBgActive]    = {0.24f, 0.28f, 0.38f, 1.f};
    c[ImGuiCol_TitleBgActive]    = {0.12f, 0.16f, 0.26f, 1.f};
    c[ImGuiCol_Button]           = {0.20f, 0.22f, 0.32f, 1.f};
    c[ImGuiCol_ButtonHovered]    = {0.28f, 0.44f, 0.68f, 1.f};
    c[ImGuiCol_ButtonActive]     = {0.20f, 0.38f, 0.65f, 1.f};
    c[ImGuiCol_Header]           = {0.20f, 0.34f, 0.55f, 1.f};
    c[ImGuiCol_HeaderHovered]    = {0.26f, 0.44f, 0.66f, 1.f};
    c[ImGuiCol_HeaderActive]     = {0.20f, 0.38f, 0.65f, 1.f};
    c[ImGuiCol_CheckMark]        = {0.28f, 0.72f, 1.00f, 1.f};
    c[ImGuiCol_Separator]        = {0.28f, 0.28f, 0.42f, 1.f};
    c[ImGuiCol_SeparatorHovered] = {0.40f, 0.50f, 0.68f, 1.f};
    c[ImGuiCol_ScrollbarBg]      = {0.08f, 0.08f, 0.12f, 1.f};
    c[ImGuiCol_ScrollbarGrab]    = {0.28f, 0.28f, 0.40f, 1.f};
}

// ── UI render ─────────────────────────────────────────────────────────────────
static void render(int win_w, int win_h) {
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({(float)win_w, (float)win_h});
    constexpr ImGuiWindowFlags root_flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##root", nullptr, root_flags);

    // Title
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.80f, 1.0f, 1.f));
    ImGui::SetWindowFontScale(1.25f);
    ImGui::Text("WoW 3.3.5a Patcher");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // ── File path row ─────────────────────────────────────────────────────────
    ImGui::Text("File:");
    ImGui::SameLine();

    // Reserve space for the browse button on the right
    float browse_w = ImGui::CalcTextSize("Browse...").x
                   + ImGui::GetStyle().FramePadding.x * 2.f
                   + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(-browse_w);
    if (ImGui::InputText("##path", g_path, sizeof(g_path)))
        g_statuses.clear();

    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        std::string chosen = native_open_dialog();
        if (!chosen.empty()) {
            set_path(chosen.c_str());
            append_log("Selected: " + chosen);
        }
    }

    // Hint shown only when path is empty
    if (g_path[0] == '\0')
        ImGui::TextDisabled("  Tip: type a path, click Browse, or drag & drop a file here");

    // File status line
    std::string path     = g_path;
    bool        f_exists = !path.empty() && std::filesystem::exists(path);

    if (path.empty()) {
        ImGui::TextDisabled("Status: no file selected");
    } else if (!f_exists) {
        ImGui::TextColored({1.f, 0.3f, 0.3f, 1.f}, "Status: file not found");
    } else {
        std::error_code ec;
        auto sz = static_cast<std::streamsize>(std::filesystem::file_size(path, ec));
        if (ec) {
            ImGui::TextColored({1.f, 0.3f, 0.3f, 1.f}, "Status: unreadable");
        } else if (sz != kExpectedFileSize) {
            ImGui::TextColored({1.f, 0.78f, 0.1f, 1.f},
                               "Status: size mismatch (%lld bytes, expected %lld)",
                               (long long)sz, (long long)kExpectedFileSize);
        } else {
            ImGui::TextColored({0.2f, 0.85f, 0.35f, 1.f},
                               "Status: OK  (%lld bytes)", (long long)sz);
        }
    }
    if (f_exists && std::filesystem::exists(path + ".backup")) {
        ImGui::SameLine();
        ImGui::TextDisabled("  [backup exists]");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Action buttons ────────────────────────────────────────────────────────
    int n_sel = 0;
    for (const auto& p : g_patches) if (p.enabled) n_sel++;

    if (!f_exists) ImGui::BeginDisabled();

    if (ImGui::Button("Verify"))         action_verify();
    ImGui::SameLine();
    if (ImGui::Button("Create Backup"))  action_backup();
    ImGui::SameLine();
    if (ImGui::Button("Restore Backup")) {
        if (!std::filesystem::exists(path + ".backup"))
            append_log("No backup found for: " + path);
        else
            action_restore();
    }
    ImGui::SameLine();
    {
        char lbl[48];
        std::snprintf(lbl, sizeof(lbl), "Apply  (%d selected)", n_sel);
        if (n_sel == 0) ImGui::BeginDisabled();
        if (ImGui::Button(lbl)) g_open_confirm = true;
        if (n_sel == 0) ImGui::EndDisabled();
    }

    if (!f_exists) ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Confirm-apply modal ───────────────────────────────────────────────────
    if (g_open_confirm) {
        ImGui::OpenPopup("Confirm##apply");
        g_open_confirm = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Confirm##apply", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Apply %d selected patch(es) to:", n_sel);
        ImGui::TextColored({0.45f, 0.80f, 1.f, 1.f}, "%s", g_path);
        ImGui::Spacing();
        ImGui::TextColored({1.f, 0.78f, 0.1f, 1.f},
                           "Create a backup first if you haven't already.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Apply", {120, 0})) {
            action_apply();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {120, 0}))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Two-column content: patches | log ─────────────────────────────────────
    float left_w = ImGui::GetContentRegionAvail().x * 0.60f;

    ImGui::BeginChild("##patches", {left_w, 0}, ImGuiChildFlags_Borders);
    ImGui::TextDisabled("Patches");
    ImGui::Separator();

    if (ImGui::BeginTable("##ptable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("##name",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##status", ImGuiTableColumnFlags_WidthFixed, 86.f);

        for (size_t i = 0; i < g_patches.size(); i++) {
            auto& p = g_patches[i];
            ImGui::PushID((int)i);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            bool en = p.enabled;
            if (ImGui::Checkbox(p.name.c_str(), &en)) p.enabled = en;

            ImGui::TableSetColumnIndex(1);
            if (i < g_statuses.size())
                ImGui::TextColored(status_col(g_statuses[i]),
                                   "[%s]", status_str(g_statuses[i]));
            else
                ImGui::TextDisabled("[---]");

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::SmallButton("All"))  for (auto& p : g_patches) p.enabled = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) for (auto& p : g_patches) p.enabled = false;
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##log", {0, 0}, ImGuiChildFlags_Borders);
    ImGui::TextDisabled("Log");
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) g_log.clear();
    ImGui::Separator();
    for (const auto& line : g_log)
        ImGui::TextUnformatted(line.c_str());
    ImGui::EndChild();

    ImGui::End();

    // ── Drag-over overlay (drawn above everything) ────────────────────────────
    if (g_dragging) {
        auto* dl = ImGui::GetForegroundDrawList();
        dl->AddRectFilled({0, 0}, {(float)win_w, (float)win_h},
                          IM_COL32(30, 100, 220, 140));
        // rounded border
        dl->AddRect({8, 8}, {(float)win_w - 8, (float)win_h - 8},
                    IM_COL32(120, 190, 255, 200), 10.f, 0, 3.f);

        const char* txt = "Drop file here";
        ImGui::SetWindowFontScale(2.0f);
        ImVec2 tsz = ImGui::CalcTextSize(txt);
        ImGui::SetWindowFontScale(1.0f);
        // draw at 2× scale via DrawList
        dl->AddText(ImGui::GetFont(),
                    ImGui::GetFontSize() * 2.f,
                    {(win_w - tsz.x * 2.f) * 0.5f,
                     (win_h - tsz.y * 2.f) * 0.5f},
                    IM_COL32(255, 255, 255, 240), txt);
    }
}

// ── Entry point ───────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    if (argc >= 2)
        std::strncpy(g_path, argv[1], sizeof(g_path) - 1);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "WoW 3.3.5a Patcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        960, 640,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) { SDL_Log("CreateWindow: %s", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_SetWindowMinimumSize(window, 700, 420);

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1);

    // SDL2 enables drop events by default; be explicit
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImFontConfig fc;
    fc.SizePixels = 15.f;
    io.Fonts->AddFontDefault(&fc);

    apply_theme();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);

            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    if (ev.window.event == SDL_WINDOWEVENT_CLOSE &&
                        ev.window.windowID == SDL_GetWindowID(window))
                        running = false;
                    break;

                // drag & drop
                case SDL_DROPBEGIN:
                    g_dragging = true;
                    break;
                case SDL_DROPCOMPLETE:
                    g_dragging = false;
                    break;
                case SDL_DROPFILE: {
                    g_dragging = false;
                    set_path(ev.drop.file);
                    append_log("Dropped: " + std::string(ev.drop.file));
                    SDL_free(ev.drop.file);
                    break;
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        render(w, h);

        ImGui::Render();
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.10f, 0.14f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
