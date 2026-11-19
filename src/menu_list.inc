// ==================================================================================
// Menyoo-style list menu (included by menu.cpp, after the classic tabs)
// ==================================================================================
// One option per row under a banner, submenus, the selected option's description
// in a box below, driven by keyboard (arrows / numpad / Enter / Backspace) and
// mouse (hover, click, wheel, right-click = back). Every colour, size, font and
// position is in the UI settings (VCTrainer_ui.ini) and can be changed from
// Settings > Interface. The old tabbed window is still there as "Classic".

// ---------------------------------------------------------------- theme
struct UiTheme {
    int layout = 0;                     // 0 list (Menyoo style), 1 classic window
    float x = 0.025f, y = 0.06f;        // top-left corner, fraction of the screen
    float width = 450.0f;               // pixels at 1080p, before Scale
    float scale = 1.0f;
    int rows = 13;                      // rows shown at once
    float rowHeight = 36.0f, bannerHeight = 104.0f;
    int font = 0, titleFont = 8;        // kFonts
    char title[40] = "VICE CITY";
    char tagline[48] = "Modern Trainer";
    float rounding = 6.0f, opacity = 0.94f;
    bool gradient = true, shimmer = true, descBox = true, wrap = true, smooth = true;
    bool counter = true, outline = true, draggable = true, rowIcons = true, remember = true;
    float banner1[4], banner2[4], bannerText[4], subBg[4], subText[4], rowBg[4], rowText[4];
    float selBg[4], selText[4], accent[4], dimText[4], descBg[4], descText[4];
};
static UiTheme T;

static void SetCol(float* c, int r, int g, int b, int a = 255) { c[0] = r / 255.f; c[1] = g / 255.f; c[2] = b / 255.f; c[3] = a / 255.f; }
static const char* kPresets[] = { "Vice City", "Menyoo", "Midnight", "Emerald", "Crimson", "Gold", "Ocean", "Light" };
static void ApplyPreset(int p) {
    // shared defaults
    SetCol(T.bannerText, 255, 255, 255); SetCol(T.subBg, 8, 8, 12, 245); SetCol(T.subText, 235, 235, 240);
    SetCol(T.rowBg, 12, 12, 16, 215); SetCol(T.rowText, 238, 238, 242); SetCol(T.selText, 255, 255, 255);
    SetCol(T.dimText, 140, 140, 150); SetCol(T.descBg, 8, 8, 12, 235); SetCol(T.descText, 225, 225, 232);
    switch (p) {
    case 0: SetCol(T.banner1, 236, 64, 160); SetCol(T.banner2, 26, 190, 210); SetCol(T.selBg, 236, 64, 160, 235); SetCol(T.accent, 40, 215, 225); break;
    case 1: SetCol(T.banner1, 20, 90, 200); SetCol(T.banner2, 10, 40, 110); SetCol(T.selBg, 245, 245, 245, 240); SetCol(T.selText, 10, 10, 10);
            SetCol(T.accent, 60, 140, 255); SetCol(T.rowBg, 0, 0, 0, 200); break;
    case 2: SetCol(T.banner1, 90, 60, 220); SetCol(T.banner2, 30, 20, 80); SetCol(T.selBg, 110, 80, 240, 235); SetCol(T.accent, 160, 140, 255); break;
    case 3: SetCol(T.banner1, 20, 180, 110); SetCol(T.banner2, 10, 70, 60); SetCol(T.selBg, 25, 160, 100, 235); SetCol(T.accent, 80, 240, 160); break;
    case 4: SetCol(T.banner1, 220, 30, 50); SetCol(T.banner2, 90, 10, 20); SetCol(T.selBg, 200, 30, 45, 235); SetCol(T.accent, 255, 90, 100); break;
    case 5: SetCol(T.banner1, 240, 180, 40); SetCol(T.banner2, 140, 80, 10); SetCol(T.selBg, 230, 170, 40, 240); SetCol(T.selText, 20, 14, 0);
            SetCol(T.accent, 255, 205, 70); break;
    case 6: SetCol(T.banner1, 20, 140, 230); SetCol(T.banner2, 10, 210, 200); SetCol(T.selBg, 20, 120, 210, 235); SetCol(T.accent, 60, 220, 255); break;
    case 7: SetCol(T.banner1, 70, 120, 230); SetCol(T.banner2, 160, 90, 230); SetCol(T.rowBg, 245, 245, 248, 235); SetCol(T.rowText, 20, 20, 28);
            SetCol(T.subBg, 225, 225, 232, 245); SetCol(T.subText, 30, 30, 40); SetCol(T.selBg, 70, 120, 230, 240);
            SetCol(T.dimText, 110, 110, 125); SetCol(T.descBg, 235, 235, 240, 240); SetCol(T.descText, 30, 30, 40); SetCol(T.accent, 70, 120, 230); break;
    }
}

struct FontDef { const char* name; const char* file; const char* bold; };
static const FontDef kFonts[] = {
    { "Segoe UI", "segoeui.ttf", "segoeuib.ttf" }, { "Bahnschrift", "bahnschrift.ttf", "bahnschrift.ttf" },
    { "Arial", "arial.ttf", "arialbd.ttf" },        { "Calibri", "calibri.ttf", "calibrib.ttf" },
    { "Trebuchet", "trebuc.ttf", "trebucbd.ttf" },  { "Verdana", "verdana.ttf", "verdana.ttf" },
    { "Tahoma", "tahoma.ttf", "tahoma.ttf" },       { "Consolas", "consola.ttf", "consola.ttf" },
    { "Impact", "impact.ttf", "impact.ttf" },       { "Pixel (ImGui)", nullptr, nullptr },
};
static const int kFontCount = (int)(sizeof kFonts / sizeof *kFonts);
static ImFont *g_fSmall = nullptr, *g_fBody = nullptr, *g_fTitle = nullptr;
static bool g_fontsDirty = false;

static std::string FontPath(const char* file) {
    if (!file) return "";
    char dir[MAX_PATH] = {};
    GetWindowsDirectoryA(dir, MAX_PATH);
    std::string p = std::string(dir) + "\\Fonts\\" + file;
    return GetFileAttributesA(p.c_str()) == INVALID_FILE_ATTRIBUTES ? "" : p;
}
void LoadUiFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    int bf = T.font < 0 || T.font >= kFontCount ? 0 : T.font;
    int tf = T.titleFont < 0 || T.titleFont >= kFontCount ? 0 : T.titleFont;
    std::string body = FontPath(kFonts[bf].file), bold = FontPath(kFonts[bf].bold), title = FontPath(kFonts[tf].bold);
    ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 1;
    g_fSmall = !body.empty() ? io.Fonts->AddFontFromFileTTF(body.c_str(), 17.0f, &cfg) : nullptr;
    if (!g_fSmall) g_fSmall = io.Fonts->AddFontDefault();
    g_fBody = !body.empty() ? io.Fonts->AddFontFromFileTTF(body.c_str(), 30.0f, &cfg) : nullptr;
    if (!g_fBody) g_fBody = g_fSmall;
    g_fTitle = !title.empty() ? io.Fonts->AddFontFromFileTTF(title.c_str(), 76.0f, &cfg) : nullptr;
    if (!g_fTitle && !bold.empty()) g_fTitle = io.Fonts->AddFontFromFileTTF(bold.c_str(), 76.0f, &cfg);
    if (!g_fTitle) g_fTitle = g_fBody;
    io.FontDefault = g_fSmall;
}
void UiBeforeNewFrame() {
    if (!g_fontsDirty) return;
    g_fontsDirty = false;
    LoadUiFonts();
    ImGui_ImplDX8_InvalidateFontTexture();
}

// ---- UI settings file
static const char* kUiFile = "VCTrainer_ui.ini";
static void SaveUi() {
    char path[MAX_PATH]; GamePath(path, kUiFile);
    FILE* f = fopen(path, "w");
    if (!f) { Log("File: can't write %s - menu look not saved", path); return; }
    fprintf(f, "layout = %d\nx = %.4f\ny = %.4f\nwidth = %.0f\nscale = %.2f\nrows = %d\nrow_height = %.0f\nbanner_height = %.0f\n",
            T.layout, T.x, T.y, T.width, T.scale, T.rows, T.rowHeight, T.bannerHeight);
    fprintf(f, "font = %d\ntitle_font = %d\ntitle = %s\ntagline = %s\nrounding = %.1f\nopacity = %.2f\n",
            T.font, T.titleFont, T.title, T.tagline, T.rounding, T.opacity);
    fprintf(f, "gradient = %d\nshimmer = %d\ndesc_box = %d\nwrap = %d\nsmooth = %d\ncounter = %d\noutline = %d\ndraggable = %d\nrow_icons = %d\nremember = %d\n",
            T.gradient, T.shimmer, T.descBox, T.wrap, T.smooth, T.counter, T.outline, T.draggable, T.rowIcons, T.remember);
    auto col = [&](const char* k, const float* c) { fprintf(f, "%s = %.3f %.3f %.3f %.3f\n", k, c[0], c[1], c[2], c[3]); };
    col("banner1", T.banner1); col("banner2", T.banner2); col("banner_text", T.bannerText); col("sub_bg", T.subBg);
    col("sub_text", T.subText); col("row_bg", T.rowBg); col("row_text", T.rowText); col("sel_bg", T.selBg);
    col("sel_text", T.selText); col("accent", T.accent); col("dim_text", T.dimText); col("desc_bg", T.descBg); col("desc_text", T.descText);
    fclose(f);
}
static void LoadUi() {
    ApplyPreset(0);
    char path[MAX_PATH]; GamePath(path, kUiFile);
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        std::string k = line; while (!k.empty() && k.back() == ' ') k.pop_back();
        std::string v = eq + 1; while (!v.empty() && (v[0] == ' ')) v.erase(0, 1);
        while (!v.empty() && (v.back() == '\n' || v.back() == '\r')) v.pop_back();
        float fv = (float)atof(v.c_str()); int iv = atoi(v.c_str()); bool bv = iv != 0;
        auto col = [&](float* c) { sscanf(v.c_str(), "%f %f %f %f", &c[0], &c[1], &c[2], &c[3]); };
        if (k == "layout") T.layout = iv == 1 ? 1 : 0;
        else if (k == "x") T.x = fminf(fmaxf(fv, 0.0f), 0.95f);
        else if (k == "y") T.y = fminf(fmaxf(fv, 0.0f), 0.95f);
        else if (k == "width") T.width = fminf(fmaxf(fv, 300.0f), 900.0f);
        else if (k == "scale") T.scale = fminf(fmaxf(fv, 0.5f), 2.0f);
        else if (k == "rows") T.rows = iv < 5 ? 5 : iv > 30 ? 30 : iv;
        else if (k == "row_height") T.rowHeight = fminf(fmaxf(fv, 24.0f), 60.0f);
        else if (k == "banner_height") T.bannerHeight = fminf(fmaxf(fv, 0.0f), 220.0f);
        else if (k == "font") T.font = iv;
        else if (k == "title_font") T.titleFont = iv;
        else if (k == "title") snprintf(T.title, sizeof T.title, "%s", v.c_str());
        else if (k == "tagline") snprintf(T.tagline, sizeof T.tagline, "%s", v.c_str());
        else if (k == "rounding") T.rounding = fminf(fmaxf(fv, 0.0f), 20.0f);
        else if (k == "opacity") T.opacity = fminf(fmaxf(fv, 0.2f), 1.0f);
        else if (k == "gradient") T.gradient = bv; else if (k == "shimmer") T.shimmer = bv;
        else if (k == "desc_box") T.descBox = bv; else if (k == "wrap") T.wrap = bv;
        else if (k == "smooth") T.smooth = bv; else if (k == "counter") T.counter = bv;
        else if (k == "outline") T.outline = bv; else if (k == "draggable") T.draggable = bv;
        else if (k == "row_icons") T.rowIcons = bv; else if (k == "remember") T.remember = bv;
        else if (k == "banner1") col(T.banner1); else if (k == "banner2") col(T.banner2);
        else if (k == "banner_text") col(T.bannerText); else if (k == "sub_bg") col(T.subBg);
        else if (k == "sub_text") col(T.subText); else if (k == "row_bg") col(T.rowBg);
        else if (k == "row_text") col(T.rowText); else if (k == "sel_bg") col(T.selBg);
        else if (k == "sel_text") col(T.selText); else if (k == "accent") col(T.accent);
        else if (k == "dim_text") col(T.dimText); else if (k == "desc_bg") col(T.descBg);
        else if (k == "desc_text") col(T.descText);
    }
    fclose(f);
}
static struct UiLoader { UiLoader() { LoadUi(); } } g_uiLoader;   // before the fonts are first built

// ---------------------------------------------------------------- rows
struct MRow {
    enum Kind { Action, Sub, Toggle, Number, Choice, Info, Header, Input, Color, Key } kind = Action;
    std::string label, right, desc;
    bool disabled = false;
    std::function<void()> act;                           // Enter (Action / Sub / Info)
    std::function<void(int)> adjust;                     // Left / Right (Number / Choice)
    std::function<bool()> get;                           // Toggle state
    std::function<void(bool)> set;
    std::function<void()> build;                         // Sub: the submenu
    std::string subTitle;
    // number editing (Enter types a value)
    std::function<double()> num;
    std::function<void(double)> setNum;
    bool isInt = false;
    char* text = nullptr; int textSize = 0;              // Input
    float* color = nullptr;                              // Color
    int keyIndex = -1;                                   // Key (hotkey row)
    std::function<void()> del;                           // Delete key
    std::function<void(ImDrawList*, ImVec2, float)> icon;
    ImU32 swatch = 0;
};
static std::vector<MRow> g_rows;

struct MLevel { std::string title; std::function<void()> build; int sel = 0; float scroll = 0; };
static std::vector<MLevel> g_stack;
static std::unordered_map<std::string, int> g_lastSel;   // "remember position" per submenu

static MRow& Add(MRow::Kind k, const std::string& label, const char* desc) {
    g_rows.emplace_back(); MRow& r = g_rows.back();
    r.kind = k; r.label = label; if (desc) r.desc = desc;
    return r;
}
static void MHeader(const std::string& t) { Add(MRow::Header, t, nullptr); }
static MRow& MSub(const std::string& label, const char* desc, std::function<void()> build, const std::string& title = "") {
    MRow& r = Add(MRow::Sub, label, desc);
    r.build = std::move(build); r.subTitle = title.empty() ? label : title;
    return r;
}
static MRow& MAction(const std::string& label, const char* desc, std::function<void()> fn, const std::string& right = "") {
    MRow& r = Add(MRow::Action, label, desc); r.act = std::move(fn); r.right = right; return r;
}
static MRow& MQueue(const std::string& label, const char* desc, std::function<void()> fn, const std::string& right = "") {
    return MAction(label, desc, [fn] { Queue(fn); }, right);
}
static MRow& MToggle(const std::string& label, bool* b, const char* desc, bool disabled = false) {
    MRow& r = Add(MRow::Toggle, label, desc);
    r.get = [b] { return *b; }; r.set = [b](bool v) { *b = v; }; r.disabled = disabled;
    return r;
}
static MRow& MToggleFn(const std::string& label, std::function<bool()> get, std::function<void(bool)> set, const char* desc) {
    MRow& r = Add(MRow::Toggle, label, desc); r.get = std::move(get); r.set = std::move(set); return r;
}
static std::string Fmt(const char* fmt, double v) { char b[64]; snprintf(b, sizeof b, fmt, v); return b; }
static MRow& MFloatFn(const std::string& label, std::function<float()> get, std::function<void(float)> set,
                      float lo, float hi, float step, const char* fmt, const char* desc) {
    MRow& r = Add(MRow::Number, label, desc);
    float cur = get();
    r.right = Fmt(fmt, cur);
    r.adjust = [get, set, lo, hi, step](int dir) { set(fminf(fmaxf(get() + step * dir, lo), hi)); };
    r.num = [get] { return (double)get(); };
    r.setNum = [set, lo, hi](double v) { set(fminf(fmaxf((float)v, lo), hi)); };
    return r;
}
static MRow& MFloat(const std::string& label, float* f, float lo, float hi, float step, const char* fmt, const char* desc) {
    return MFloatFn(label, [f] { return *f; }, [f](float v) { *f = v; }, lo, hi, step, fmt, desc);
}
static MRow& MIntFn(const std::string& label, std::function<int()> get, std::function<void(int)> set,
                    int lo, int hi, int step, const char* fmt, const char* desc) {
    MRow& r = Add(MRow::Number, label, desc);
    char b[48]; snprintf(b, sizeof b, fmt, get()); r.right = b;
    r.adjust = [get, set, lo, hi, step](int dir) { int v = get() + step * dir; set(v < lo ? lo : v > hi ? hi : v); };
    r.num = [get] { return (double)get(); };
    r.setNum = [set, lo, hi](double v) { int i = (int)lround(v); set(i < lo ? lo : i > hi ? hi : i); };
    r.isInt = true;
    return r;
}
static MRow& MInt(const std::string& label, int* i, int lo, int hi, int step, const char* fmt, const char* desc) {
    return MIntFn(label, [i] { return *i; }, [i](int v) { *i = v; }, lo, hi, step, fmt, desc);
}
static MRow& MChoice(const std::string& label, int* idx, const char* const* names, int n, const char* desc) {
    MRow& r = Add(MRow::Choice, label, desc);
    int i = *idx < 0 || *idx >= n ? 0 : *idx;
    r.right = names[i];
    bool wrap = T.wrap;
    r.adjust = [idx, n, wrap](int dir) { int v = *idx + dir; *idx = v < 0 ? (wrap ? n - 1 : 0) : v >= n ? (wrap ? 0 : n - 1) : v; };
    return r;
}
static MRow& MInfo(const std::string& label, const std::string& right = "", const char* desc = nullptr) {
    MRow& r = Add(MRow::Info, label, desc); r.right = right; return r;
}
static MRow& MInput(const std::string& label, char* buf, int size, const char* desc) {
    MRow& r = Add(MRow::Input, label, desc); r.text = buf; r.textSize = size; return r;
}
static MRow& MColor(const std::string& label, float* c, const char* desc) {
    MRow& r = Add(MRow::Color, label, desc); r.color = c; return r;
}

// ---------------------------------------------------------------- state
static int g_editRow = -1;            // a Number / Input row being typed into
static char g_editBuf[64];
static float* g_colorEdit = nullptr;  // colour picker open for this colour
static std::string g_colorLabel;
static float g_selY = -1.0f;          // smooth highlight
static bool g_uiDirty = false;

// Info panel beside the menu (Menyoo's preview box): a title box, stat bars
// and label / value rows. A menu fills it in while it's built; it's emptied
// before every build, so only the menu on screen shows one.
struct SidePanel {
    struct Bar { std::string label; int pct; std::string text; };   // text: shown on the right (default "NN%")
    bool on = false;
    std::string title, subtitle, badge;
    int picture = -1;                                           // vehicle model whose still picture is shown
    std::vector<Bar> bars;                                      // 0..100
    std::vector<std::pair<std::string, std::string>> info;      // label, value
};
static SidePanel g_side;

static void PushMenu(const std::string& title, std::function<void()> build) {
    MLevel l; l.title = title; l.build = std::move(build);
    if (T.remember) { auto it = g_lastSel.find(title); if (it != g_lastSel.end()) l.sel = it->second; }
    g_stack.push_back(std::move(l));
    g_editRow = -1; g_colorEdit = nullptr; g_selY = -1.0f;
}
static void PopMenu() {
    g_editRow = -1; g_colorEdit = nullptr; g_selY = -1.0f;
    if (g_capturingHotkey >= 0) { g_capturingHotkey = -1; return; }
    if (g_stack.size() > 1) g_stack.pop_back();
    else { g_menuOpen = false; SaveSettings(); SaveUi(); }
}

static bool Selectable(const MRow& r) { return r.kind != MRow::Header; }
static void Activate(MRow& r) {
    if (r.disabled) return;
    switch (r.kind) {
    case MRow::Sub: PushMenu(r.subTitle, r.build); break;
    case MRow::Toggle: r.set(!r.get()); break;
    case MRow::Choice: if (r.adjust) r.adjust(1); break;
    case MRow::Number: g_editRow = (int)(&r - &g_rows[0]);
        if (r.isInt) snprintf(g_editBuf, sizeof g_editBuf, "%d", (int)lround(r.num()));
        else snprintf(g_editBuf, sizeof g_editBuf, "%.2f", r.num());
        break;
    case MRow::Input: g_editRow = (int)(&r - &g_rows[0]); break;
    case MRow::Color: g_colorEdit = r.color; g_colorLabel = r.label; break;
    case MRow::Key: g_capturingHotkey = (g_capturingHotkey == r.keyIndex) ? -1 : r.keyIndex; break;
    default: if (r.act) r.act(); break;
    }
}

// ---------------------------------------------------------------- menus
static ImU32 Col(const float* c, float alphaMul = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * alphaMul));
}
static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool Match(const std::string& s, const char* filter) { return !filter[0] || Lower(s).find(Lower(filter)) != std::string::npos; }

static void ProofsMenu(Proofs* p, bool vehicle) {
    MToggle("Bulletproof", &p->bullet, "Bullets do no damage.");
    MToggle("Fireproof", &p->fire, "Fire and burning do no damage.");
    MToggle("Explosion proof", &p->explosion, "Explosions do no damage.");
    MToggle(vehicle ? "Solid (collision proof)" : "Collision proof", &p->collision, "Crashes and being run over do no damage.");
    MToggle("Melee proof", &p->melee, "Punches and melee weapons do no damage.");
}

static void PedPickerMenu(int* model, const char* title) {
    static char filter[24] = {};
    MInput("Search", filter, sizeof filter, "Type part of a model name to narrow the list.");
    for (auto& m : PedModels()) {
        if (!Match(m.name, filter)) continue;
        char lbl[64]; snprintf(lbl, sizeof lbl, "%s", m.name.c_str());
        int id = m.id;
        MAction(lbl, nullptr, [model, id] { *model = id; PopMenu(); }, *model == id ? "selected" : std::to_string(id));
    }
    (void)title;
}
static std::string PedName(int id) { for (auto& m : PedModels()) if (m.id == id) return m.name; return std::to_string(id); }

static void PlayerMenu() {
    using namespace game;
    void* ped = PlayerPed();
    MHeader("Cheats");
    MToggle("Infinite health", &S.godMode, "Health is kept full: nothing can kill you.");
    MToggle("Infinite armour", &S.infiniteArmour, "Armour is kept full.");
    MToggle("Never wanted", &S.neverWanted, "The police never chase you: stars stay at zero.");
    MToggle("Infinite sprint", &S.neverTired, "Run forever without getting tired.");
    MToggle("No ragdoll", &S.noRagdoll, "You're never knocked down. Cars, explosions, punches and falls can still hurt you.");
    MToggle("Walk on water", &S.walkOnWater, "On foot the sea, rivers and pools are solid ground: walk, run, jump and land on them.");
    MToggle("Invisible", &S.invisible, "You aren't drawn.");
    MToggle("Freeze position", &S.freezePos, "Holds you - or the vehicle you're in - exactly where you are, even in the air.");
    MToggle("Forcefield", &S.forcefield, "Everything near you is pushed away (10 m) or destroyed (5 m). Rockets and grenades are pushed off.");
    static const char* ffModes[] = { "Push away", "Destroy" };
    MChoice("Forcefield mode", &S.forcefieldMode, ffModes, 2, "Push away: people knocked over, cars and objects thrown - the closer, the harder.\nDestroy: cars blow up and people die.");
    MSub("Health & armour", "Set health and armour, and their maximums.", [] {
        using namespace game;
        void* p = PlayerPed();
        if (!p) { MInfo("Player not available"); return; }
        float maxH = (float)*MaxHealth, maxA = (float)*MaxArmour;
        MFloatFn("Health", [] { void* q = PlayerPed(); return q ? at<float>(q, PED_HEALTH) : 0.0f; },
                 [](float v) { Queue([v] { SetHealth(v); }); }, 0.0f, maxH, 5.0f, "%.0f", "Your health right now.");
        MFloatFn("Armour", [] { void* q = PlayerPed(); return q ? at<float>(q, PED_ARMOUR) : 0.0f; },
                 [](float v) { Queue([v] { SetArmour(v); }); }, 0.0f, maxA, 5.0f, "%.0f", "Your armour right now.");
        MQueue("Fill health", nullptr, [maxH] { SetHealth(maxH); });
        MQueue("Fill armour", nullptr, [maxA] { SetArmour(maxA); });
        MIntFn("Max health", [] { return (int)*MaxHealth; }, [](int v) { Queue([v] { SetMaxHealth(v); }); }, 100, 250, 5, "%d", "The most health you can have (kept in your settings).");
        MIntFn("Max armour", [] { return (int)*MaxArmour; }, [](int v) { Queue([v] { SetMaxArmour(v); }); }, 100, 250, 5, "%d", "The most armour you can have.");
    });
    MSub("Wanted level", "Stars, clearing them, and a maximum.", [] {
        MIntFn("Stars", [] { return GetWanted(); }, [](int v) { Queue([v] { SetWanted(v); }); }, 0, 6, 1, "%d", "Your wanted level right now.");
        MQueue("Clear wanted level", nullptr, [] { SetWanted(0); });
        MToggle("Lock wanted level at most", &S.wantedCapOn, "The wanted level can still go up, but never past the maximum below.");
        MInt("Maximum stars", &S.wantedCap, 0, 6, 1, "%d", "Used by the lock above. Never wanted overrides it with zero.");
    });
    MSub("Money", "Add, take or set your cash.", [] {
        MInfo("Cash", "$" + std::to_string(*game::Money));
        const int amounts[] = { 1000, 10000, 100000, 1000000 };
        const char* labels[] = { "Add $1,000", "Add $10,000", "Add $100,000", "Add $1,000,000" };
        for (int i = 0; i < 4; i++) { int a = amounts[i]; MQueue(labels[i], nullptr, [a] { AddMoney(a); }); }
        MQueue("Take $10,000", nullptr, [] { AddMoney(-10000); });
        static int setTo = 1000000;
        MInt("Amount", &setTo, 0, 999999999, 10000, "$%d", "Press Enter to type an exact amount.");
        MQueue("Set cash to amount", nullptr, [] { SetMoney(setTo); });
    });
    MSub("Movement", "Super jump, super run, bunny hop, fly mode.", [] {
        MToggle("Super jump", &S.superJump, "Jump much higher.");
        MFloat("Jump power", &S.superJumpPower, 1.5f, 8.0f, 0.1f, "%.1fx", nullptr);
        MToggle("Super run", &S.superRun, "Run much faster.");
        MFloat("Run speed", &S.superRunSpeed, 1.2f, 5.0f, 0.1f, "%.1fx", nullptr);
        MToggle("No landing roll", &S.noLandingRoll, "Landing from a fall you're straight back on your feet.");
        MSub("Bunny hop", "CS:GO style auto bunny hop and surf.", [] {
            MToggle("Auto bunny hop", &S.autoBhop, "Hold jump: you hop again the moment you land and keep your speed.\nAir strafe with A/D and the mouse.");
            static const char* modes[] = { "Any direction", "AFH (forward)", "ABH (backward)" };
            MChoice("Mode", &S.bhopMode, modes, 3, nullptr);
            MFloat("Speed gain per hop", &S.bhopGain, 1.0f, 1.5f, 0.01f, "x%.2f", nullptr);
            MFloat("Top hop speed", &S.bhopMax, 10.0f, 150.0f, 1.0f, "%.0f m/s", nullptr);
            MFloat("Hop height", &S.bhopHeight, 0.5f, 4.0f, 0.05f, "%.2f m", nullptr);
            MToggle("Instant full speed", &S.bhopInstant, "From the first hop you move at the top hop speed. W/S steer forward/back.");
            MToggle("Keep speed on walls", &S.bhopKeepSpeed, "Hitting a wall never slows you down: you slide along it.");
            MToggle("Surf on steep ramps", &S.bhopSurf, "Land on a ramp steeper than ~45 degrees and slide along it with no friction.");
            MAction("CS:GO feel", "CS:GO's own numbers: speed only from air strafing, 57-unit jump, 3500 u/s top speed, surf on.", [] {
                S.bhopMode = 0; S.bhopGain = 1.0f; S.bhopHeight = 1.45f; S.bhopMax = 89.0f; S.bhopSurf = true;
                S.bhopInstant = false; S.bhopKeepSpeed = false; });
        });
        MSub("Fly mode", "No clip: fly through walls, on foot or in a vehicle.", [] {
            MToggle("Fly", &S.flyMode, "W/S forward/back where the camera looks, A/D sideways, Space up, Ctrl down, Shift 3x speed.");
            MFloat("Fly speed", &S.flySpeed, 5.0f, 150.0f, 5.0f, "%.0f m/s", nullptr);
        });
    });
    MSub("Proofs", "Damage you're immune to.", [] { ProofsMenu(&S.playerProofs, false); });
    (void)ped;
}

static void WeaponsMenu() {
    MHeader("Ammo & firing");
    MToggle("Infinite ammo", &S.infiniteAmmo, "Never run out.");
    MToggle("No reload", &S.noReload, "The clip never empties.");
    MToggle("Fast reload", &S.fastReload, "Reloads are quicker.");
    MToggle("Rapid fire", &S.rapidFire, "Hold fire to burst, about 22 shots a second (a tap is one normal shot). Drive-bys up to ~50.");
    MToggle("No-scope sniper", &S.noScopeSniper, "Sniper / laser sniper / rocket launcher fire from the hip down your crosshair.");
    MToggle("Sniper full auto", &S.sniperAuto, "Hold fire and the sniper keeps firing at full speed.");
    MHeader("Bullets");
    MToggle("Wall bang", &S.wallBang, "Bullets go through walls, cars and wrecks.");
    MToggle("Bullet penetration", &S.bulletPenetrate, "Bullets fly 500 m and pass through everything on the line.");
    MToggle("One-shot vehicles", &S.oneShotVehicles, "Any vehicle you shoot blows up at once.");
    MToggle("Explosive bullets", &S.explosiveBullets, "Your bullets explode where they land.");
    MToggle("Fire bullets", &S.fireBullets, "Your bullets start a fire where they land.");
    MFloat("Damage multiplier", &S.damageMult, 0.1f, 20.0f, 0.1f, "x%.1f", "How hard your bullets hit. Only your shots.");
    MSub("Aimbot", "Aim lock, silent aim, drive-by aimbot, enemy markers.", [] {
        MToggle("Aim lock-on", &S.aimLock, "While you aim (right mouse) the crosshair snaps onto the closest enemy, all round.");
        bool off = !S.aimLock && !S.aimDriveBy;
        MToggle("Silent aim", &S.aimSilent, "The camera is never turned: your shots land on the locked enemy anyway.", off);
        MToggle("Silent aim while hopping", &S.aimSilentBhop, "Silent aim only while auto bunny hopping.", off || S.aimSilent);
        MToggle("Snap camera back", &S.aimSnapBack, "When you stop aiming the camera turns back to where you were looking.", off || S.aimSilent);
        MToggle("Drive-by aimbot", &S.aimDriveBy, "Hold the right mouse button on a drive-by: locks the closest enemy and every bullet lands.");
        MToggle("Show enemies through walls", &S.enemyEsp, "Boxes round every enemy, arrows at the screen edge for ones behind you.");
    });
    MSub("Special guns", "Teleport, delete and vehicle guns, super punch.", [] {
        MToggle("Teleport gun", &S.teleportGun, "You jump to wherever your shot lands.");
        MToggle("Delete gun", &S.deleteGun, "People and vehicles you shoot vanish.");
        MToggle("Vehicle gun", &S.vehicleGun, "Every shot fires a random car out in front of you.");
        MToggle("Super punch", &S.superPunch, "Fists and melee weapons send people flying and shove cars.");
    });
    MSub("Give weapons", "Any weapon, the game's weapon sets, or remove all.", [] {
        static int ammo = 0;
        MInt("Ammo to give", &ammo, 0, 9999, 50, ammo ? "%d" : "default", "0 = each weapon's normal amount.");
        MHeader("Weapon sets");
        MQueue("Set 1 - Thug", nullptr, [] { RunNativeCheat("THUGSTOOLS"); });
        MQueue("Set 2 - Professional", nullptr, [] { RunNativeCheat("PROFESSIONALTOOLS"); });
        MQueue("Set 3 - Nutter", nullptr, [] { RunNativeCheat("NUTTERTOOLS"); });
        MHeader("Weapons");
        for (auto& g : kWeapons) {
            const WeaponGroup* grp = &g;
            MSub(g.group, nullptr, [grp] {
                for (auto& w : grp->w) {
                    if (!w.type) break;
                    int type = w.type, def = w.ammo;
                    MQueue(w.name, nullptr, [type, def] { GiveWeapon(type, ammo ? ammo : def); });
                }
            });
        }
        MQueue("Remove all weapons", nullptr, [] { RemoveAllWeapons(); });
    });
}

static void VehicleMenu() {
    void* veh = PlayerVehicle();
    if (veh) {
        char b[64]; snprintf(b, sizeof b, "%.0f hp   %.0f km/h", game::at<float>(veh, game::VEH_HEALTH), VehicleSpeedKmh());
        MInfo("Current vehicle", b);
    } else MQueue("Get into nearest vehicle", "Into the nearest vehicle within 60 m.", [] { WarpIntoNearestVehicle(); });
    MHeader("Actions");
    MQueue("Repair", nullptr, [] { RepairVehicle(); }).disabled = !veh;
    MQueue("Flip upright", nullptr, [] { FlipVehicle(); }).disabled = !veh;
    MQueue("Boost", nullptr, [] { BoostVehicle(0.6f); }).disabled = !veh;
    MQueue("Jump", nullptr, [] { JumpVehicle(0.35f); }).disabled = !veh;
    MQueue("Stop", nullptr, [] { StopVehicle(); }).disabled = !veh;
    MQueue("Lock doors", "Nobody gets in and nobody can drag you out. Exit still works.", [] { SetDoorLock(true); }).disabled = !veh;
    MQueue("Unlock doors", nullptr, [] { SetDoorLock(false); }).disabled = !veh;
    MQueue(veh ? "Delete this vehicle" : "Delete nearest vehicle", "In a vehicle: you're set down beside it and it disappears. On foot: the nearest within 10 m.", [] { DeleteVehicle(); });
    MHeader("Options");
    MToggle("Car god mode", &S.carGodMode, "Your vehicle's health is kept full.");
    MToggle("Auto repair", &S.autoRepair, "Damage is repaired as it happens.");
    MToggle("Auto lock doors", &S.autoLock, "Locks the doors as soon as you're sitting in a vehicle with doors.");
    MToggle("Never fall off bike", &S.stayOnBike, "Crashes, hits and shots no longer throw you off a motorbike.");
    MToggle("Auto flip car / boat", &S.autoFlip, "Turned back over as soon as it's upside down on the ground or water.");
    MToggle("Kill peds on one touch", &S.touchKill, "A pedestrian your vehicle touches dies at once.");
    MToggle("Hunter aimbot", &S.heliAimbot, "In a Hunter / Sea Sparrow hold fire: the radar-marked targets are taken out one by one.");
    MToggle("Rainbow paint", &S.rainbowCar, "The colours keep changing.");
    MToggle("Bulletproof tyres", &S.bulletproofTyres, "Tyres can't be burst.");
    MToggle("Invisible vehicle", &S.invisibleCar, "Your vehicle isn't drawn.");
    MToggle("Freeze vehicle in place", &S.freezePos, "Holds the vehicle you're in where it is, even in the air.");
    MSub("Heavy car", "Rhino weight, and what touching another car does.", [] {
        MToggle("Heavy car (like the Rhino)", &S.heavyCar, "Your car or bike gets the Rhino tank's weight (not boats, helis or planes).");
        static const char* modes[] = { "Just heavy", "Push it far away", "Blow it up" };
        MChoice("When you touch a car", &S.heavyMode, modes, 3, nullptr);
    });
    MSub("Survive explosions", "Don't die when your vehicle blows up.", [] {
        MToggle("Survive vehicle explosions", &S.surviveBlowUp, "If the vehicle you're in blows up you don't die.");
        static const char* modes[] = { "Stay in the wreck", "Thrown clear beside it" };
        MChoice("What happens", &S.blowUpMode, modes, 2, nullptr);
    });
    MSub("Proofs", "Damage your vehicle is immune to.", [] { ProofsMenu(&S.carProofs, true); });
    MSub("Paint", "Primary and secondary colour from the game's palette.", [] {
        for (int which = 0; which < 2; which++) {
            MSub(which ? "Secondary colour" : "Primary colour", nullptr, [which] {
                int c1 = 0, c2 = 0;
                bool have = GetVehicleColours(c1, c2);
                if (!have) { MInfo("Get in a vehicle first"); return; }
                const auto& pal = CarPalette();
                for (int i = 0; i < (int)pal.size(); i++) {
                    char lbl[64]; snprintf(lbl, sizeof lbl, "%d  %s", i, pal[i].name.c_str());
                    MRow& r = MQueue(lbl, nullptr, [which, i] { int a, b; if (GetVehicleColours(a, b)) SetVehicleColours(which ? a : i, which ? i : b); },
                                     (which ? c2 : c1) == i ? "current" : "");
                    r.swatch = IM_COL32(pal[i].r, pal[i].g, pal[i].b, 255);
                }
            });
        }
    });
    MSub("Performance", "Extra acceleration, grip and brakes.", [] {
        MFloat("Acceleration", &S.carAccel, 1.0f, 5.0f, 0.1f, "x%.1f", nullptr);
        MFloat("Grip", &S.carGrip, 1.0f, 5.0f, 0.1f, "x%.1f", "Less sideways sliding.");
        MFloat("Brakes", &S.carBrakes, 1.0f, 5.0f, 0.1f, "x%.1f", nullptr);
        MAction("Stock", nullptr, [] { S.carAccel = S.carGrip = S.carBrakes = 1.0f; });
    });
    MSub("Garage", "Save the vehicle you're in and spawn it again later.", [] {
        static char name[48] = {};
        MInput("Name (optional)", name, sizeof name, nullptr);
        MQueue("Save this vehicle", "Model and colours, kept in VCTrainer_garage.txt.", [] { GarageSave(name); name[0] = 0; }).disabled = !PlayerVehicle();
        auto& g = Garage();
        if (g.empty()) MInfo("No saved vehicles yet");
        for (int i = 0; i < (int)g.size(); i++) {
            MRow& r = MQueue(g[i].name, "Enter: spawn it. Delete: remove it from the garage.", [i] { GarageSpawn(i); });
            r.del = [i] { Queue([i] { GarageRemove(i); }); };
        }
    });
}

static void WorldMenu() {
    using namespace game;
    MSub("Time", "Clock, freezing it, and how fast it runs.", [] {
        using namespace game;
        char b[16]; snprintf(b, sizeof b, "%02d:%02d", *ClockHours, *ClockMinutes);
        MInfo("Game clock", b);
        // The rows show the game's clock and change it the moment they move.
        MIntFn("Hour", [] { return (int)*ClockHours; },
               [](int v) { Queue([v] { SetTime(v, *ClockMinutes); }); }, 0, 23, 1, "%02d", "Changes the clock straight away.");
        MIntFn("Minute", [] { return (int)*ClockMinutes; },
               [](int v) { Queue([v] { SetTime(*ClockHours, v); }); }, 0, 59, 5, "%02d", "Changes the clock straight away.");
        MToggleFn("Freeze time", [] { return S.freezeTime; },
                  [](bool v) { S.freezeTime = v; if (v) { S.frozenHour = *ClockHours; S.frozenMinute = *ClockMinutes; } }, "The clock stops where it is.");
        MFloat("Clock speed", &S.clockSpeed, 0.1f, 20.0f, 0.1f, "x%.1f", "How fast the in-game clock runs (game speed stays the same).");
    });
    MSub("Weather", "Force any weather.", [] {
        static const char* w[] = { "Sunny", "Cloudy", "Rain", "Fog", "Extra sunny", "Hurricane" };
        for (int i = 0; i < 6; i++) MQueue(w[i], nullptr, [i] { ForceWeather(i); });
        MQueue("Normal weather", "Back to the game's own weather.", [] { ReleaseWeather(); });
    });
    MFloat("Field of view", &S.fov, 40.0f, 120.0f, 1.0f, "%.0f deg", "How much of the world you see. The game's own is 70.");
    MFloatFn("Game speed", [] { return *TimeScale; }, [](float v) { Queue([v] { *TimeScale = v; }); }, 0.1f, 3.0f, 0.05f, "%.2fx", nullptr);
    MFloatFn("Pedestrians", [] { return *PedDensity; }, [](float v) { Queue([v] { *PedDensity = v; }); }, 0.0f, 3.0f, 0.1f, "%.1fx", "How many people walk around.");
    MFloatFn("Traffic", [] { return *CarDensity; }, [](float v) { Queue([v] { *CarDensity = v; }); }, 0.0f, 3.0f, 0.1f, "%.1fx", "How many cars drive around.");
    MFloat("Gravity", &S.gravity, 0.1f, 3.0f, 0.05f, "x%.2f", "Low for moon jumps and floaty cars.");
    MToggle("Bullet time", &S.bulletTime, "Slow motion while you aim (right mouse) on foot.");
    MToggle("Hide radar", &S.hideRadar, nullptr);
    MToggle("Genocide mode", &S.genocide, "Every ped around you dies and every vehicle around you blows up.");
    MToggle("Disable mission timers", &S.freezeMissionTimer, "Countdowns never run out.");
}

static void PedsMenu() {
    MSub("Bodyguards", "Armed people who follow you and fight for you.", [] {
        static int type = 4, weapon = 6, count = 3;
        static const char* tn[IM_ARRAYSIZE(kGuardTypes)]; for (int i = 0; i < IM_ARRAYSIZE(kGuardTypes); i++) tn[i] = kGuardTypes[i].name;
        static const char* wn[IM_ARRAYSIZE(kGuardWeapons)]; for (int i = 0; i < IM_ARRAYSIZE(kGuardWeapons); i++) wn[i] = kGuardWeapons[i].name;
        MChoice("Who", &type, tn, IM_ARRAYSIZE(kGuardTypes), nullptr);
        MChoice("Weapon", &weapon, wn, IM_ARRAYSIZE(kGuardWeapons), nullptr);
        MInt("How many", &count, 1, 8, 1, "%d", nullptr);
        MQueue("Spawn bodyguards", "They follow you, ride with you and go for anyone who attacks you.",
               [] { SpawnBodyguards(kGuardTypes[type].id, count, kGuardWeapons[weapon].id); });
        MQueue("Dismiss bodyguards", nullptr, [] { DismissBodyguards(); });
        MInfo("With you now", std::to_string(BodyguardCount()));
    });
    MSub("Spawn a ped", "Any model, armed, friendly or hostile.", [] {
        static int model = 83, weapon = 1; static bool hostile = false;
        static const char* wn[IM_ARRAYSIZE(kGuardWeapons)]; for (int i = 0; i < IM_ARRAYSIZE(kGuardWeapons); i++) wn[i] = kGuardWeapons[i].name;
        MSub("Model: " + PedName(model), nullptr, [] { PedPickerMenu(&model, "Ped model"); }, "Ped model");
        MChoice("Weapon", &weapon, wn, IM_ARRAYSIZE(kGuardWeapons), nullptr);
        MToggle("Hostile (comes after you)", &hostile, nullptr);
        MQueue("Spawn", nullptr, [] { SpawnPed(model, hostile, kGuardWeapons[weapon].id); });
        MQueue("Remove the ones I spawned", nullptr, [] { RemoveSpawnedPeds(); });
    });
    MSub("Everyone within 40 m", "Do something to every person nearby.", [] {
        MQueue("Set on fire", nullptr, [] { NearbyPeds(NEARBY_FIRE); });
        MQueue("Freeze", nullptr, [] { NearbyPeds(NEARBY_FREEZE); });
        MQueue("Unfreeze", nullptr, [] { NearbyPeds(NEARBY_UNFREEZE); });
        MQueue("Fight each other", nullptr, [] { NearbyPeds(NEARBY_RIOT); });
        MQueue("Follow me", nullptr, [] { NearbyPeds(NEARBY_RECRUIT); });
        MQueue("Delete", "People on foot only - anyone a mission marks on the radar is left alone.", [] { NearbyPeds(NEARBY_DELETE); });
    });
}

// teleport lists
static void BlipRows(std::vector<Blip> list, const float* me, const char* fixedName) {
    if (me) std::stable_sort(list.begin(), list.end(), [me](const Blip& a, const Blip& b) {
        return Dist2D(me, a.pos[0], a.pos[1]) < Dist2D(me, b.pos[0], b.pos[1]); });
    for (auto& b : list) {
        std::string name = fixedName ? fixedName : SpriteKind(b.sprite).name;
        const char* where = DistrictAt(b.pos[0], b.pos[1]);
        Blip c = b;
        MRow& r = MQueue(name, *where ? where : nullptr, [c] { TeleportToBlip(c); }, me ? DistText(Dist2D(me, b.pos[0], b.pos[1])) : "");
        int sprite = b.sprite, type = b.type;
        r.icon = [sprite, type](ImDrawList* dl, ImVec2 c, float s) {
            if (ImTextureID t = RadarIcon(sprite)) dl->AddImage(t, ImVec2(c.x - s * 0.5f, c.y - s * 0.5f), ImVec2(c.x + s * 0.5f, c.y + s * 0.5f));
            else {
                ImU32 col = type == 1 ? IM_COL32(80, 160, 255, 255) : type == 2 ? IM_COL32(255, 80, 80, 255) : type == 3 ? IM_COL32(120, 230, 120, 255)
                          : type == 5 ? IM_COL32(255, 220, 60, 255) : IM_COL32(255, 120, 200, 255);
                dl->AddCircleFilled(c, s * 0.3f, col);
            }
        };
    }
    if (list.empty()) MInfo("Nothing on the radar right now");
}
static void TeleportMenu() {
    float pos[3] = {};
    bool have = GetPlayerPos(pos);
    static float me[3]; memcpy(me, pos, sizeof me);
    const float* mp = have ? me : nullptr;
    if (have) { char b[64]; snprintf(b, sizeof b, "%.0f, %.0f, %.0f", pos[0], pos[1], pos[2]); MInfo(DistrictAt(pos[0], pos[1])[0] ? DistrictAt(pos[0], pos[1]) : "Position", b); }
    MQueue("Teleport to mission marker", "The current mission's marker (patients and drop-off in an ambulance).", [] { TeleportToMarker(); });
    MSub("Coordinates", "Type or adjust X, Y and Z and jump there.", [] {
        static float t[3] = { 0, 0, 20 }; static bool ground = true;
        MFloat("X", &t[0], -3000, 3000, 10, "%.1f", "Left / Right to adjust, Enter to type.");
        MFloat("Y", &t[1], -3000, 3000, 10, "%.1f", nullptr);
        MFloat("Z", &t[2], -100, 1000, 1, "%.1f", nullptr);
        MToggle("Snap to ground", &ground, "Lands on the ground at X / Y whatever Z says.");
        MAction("Use my position", nullptr, [] { GetPlayerPos(t); });
        MQueue("Teleport", nullptr, [] { TeleportTo(t[0], t[1], t[2], ground); });
    });
    enum { M_PERSON, M_VEHICLE, M_OBJECT, M_PLACE, M_COUNT };
    std::vector<Blip> mission[M_COUNT], cats[PC_COUNT], homes, forSale;
    for (auto& b : ActiveBlips()) {
        if (b.sprite >= 1 && b.sprite <= 3) continue;
        if (b.sprite == 0) {
            if (b.type == 5) cats[PC_EVENTS].push_back(b);
            else mission[b.type == 2 ? M_PERSON : b.type == 1 ? M_VEHICLE : b.type == 3 ? M_OBJECT : M_PLACE].push_back(b);
        } else if (b.sprite == 19) homes.push_back(b);
        else if (b.sprite == 25) forSale.push_back(b);
        else cats[SpriteKind(b.sprite).cat].push_back(b);
    }
    int nMission = 0; for (auto& l : mission) nMission += (int)l.size();
    MHeader("Places");
    MSub("Current mission", "What the running mission marks on the radar.", [mission, mp] {
        static const char* names[M_COUNT] = { "Person", "Vehicle", "Object", "Destination" };
        int n = 0; for (auto& l : mission) n += (int)l.size();
        if (!n) { MInfo("Nothing marked right now"); return; }
        for (int i = 0; i < M_COUNT; i++) if (!mission[i].empty()) BlipRows(mission[i], mp, names[i]);
    }).right = std::to_string(nMission);
    MSub("Races & events", "Race starts and other side activities.", [l = cats[PC_EVENTS], mp] { BlipRows(l, mp, "Race / event start"); }).right = std::to_string(cats[PC_EVENTS].size());
    MSub("Mission contacts", "Where story missions start.", [l = cats[PC_CONTACTS], mp] { BlipRows(l, mp, nullptr); }).right = std::to_string(cats[PC_CONTACTS].size());
    MSub("Safehouses", "Yours, and property for sale.", [homes, forSale, mp] {
        MHeader("Your safehouses"); BlipRows(homes, mp, nullptr);
        MHeader("Property for sale"); BlipRows(forSale, mp, nullptr);
    }).right = std::to_string(homes.size() + forSale.size());
    MSub("Businesses", "Asset properties.", [l = cats[PC_BUSINESS], mp] { BlipRows(l, mp, nullptr); }).right = std::to_string(cats[PC_BUSINESS].size());
    MSub("Shops", "Ammu-Nation, Pay 'n' Spray, clothes and more.", [l = cats[PC_SHOPS], o = cats[PC_OTHER], mp] {
        BlipRows(l, mp, nullptr);
        if (!o.empty()) { MHeader("Other map icons"); BlipRows(o, mp, nullptr); }
    }).right = std::to_string(cats[PC_SHOPS].size());
    // pickups
    std::vector<Pickup> kinds[PK_COUNT];
    for (auto& p : ActivePickups()) kinds[PickupKind(p.model)].push_back(p);
    static const char* titles[PK_COUNT] = { "Hidden packages", "Rampages", "Money pickups", "Police bribes", "Health & armour", "Weapons", "Other pickups" };
    MSub("Pickups", "Packages, rampages, money, bribes, health, weapons.", [kinds, mp] {
        for (int k = 0; k < PK_COUNT; k++) {
            auto list = kinds[k];
            MSub(titles[k], nullptr, [list, mp]() mutable {
                if (list.empty()) { MInfo("None left to collect"); return; }
                if (mp) std::stable_sort(list.begin(), list.end(), [mp](const Pickup& a, const Pickup& b) {
                    return Dist2D(mp, a.pos[0], a.pos[1]) < Dist2D(mp, b.pos[0], b.pos[1]); });
                for (auto& p : list) {
                    Pickup c = p;
                    const char* where = DistrictAt(p.pos[0], p.pos[1]);
                    MRow& r = MQueue(PickupName(p.model), *where ? where : nullptr, [c] { TeleportToPickup(c); }, mp ? DistText(Dist2D(mp, p.pos[0], p.pos[1])) : "");
                    int ic = PickupIcon(p.model);
                    r.icon = [ic](ImDrawList* dl, ImVec2 c, float s) { PlaceIconAt(dl, c, s, ic); };
                }
            }).right = std::to_string(list.size());
        }
    });
    MSub("Landmarks", "Famous places, always listed.", [mp] {
        std::vector<const Landmark*> lm; for (auto& l : kLandmarks) lm.push_back(&l);
        if (mp) std::stable_sort(lm.begin(), lm.end(), [mp](const Landmark* a, const Landmark* b) { return Dist2D(mp, a->x, a->y) < Dist2D(mp, b->x, b->y); });
        for (auto* l : lm) {
            Landmark c = *l;
            MRow& r = MQueue(l->name, DistrictAt(l->x, l->y), [c] { TeleportTo(c.x, c.y, c.z != 0.0f ? c.z + 1.0f : 0.0f, c.z == 0.0f); },
                             mp ? DistText(Dist2D(mp, l->x, l->y)) : "");
            r.icon = [](ImDrawList* dl, ImVec2 c, float s) { PlaceIconAt(dl, c, s, IC_LANDMARK); };
        }
    });
    MSub("Saved locations", "Your own saved spots.", [mp] {
        static char name[64] = {};
        MInput("Name for this spot", name, sizeof name, nullptr);
        MAction("Save current position", "Kept in VCTrainer_positions.txt.", [] {
            float p[3]; if (!GetPlayerPos(p)) return;
            auto& list = SavedPositions();
            list.push_back({ name[0] ? name : "Location " + std::to_string(list.size() + 1), p[0], p[1], p[2] });
            SavePositions(); name[0] = 0; });
        auto& list = SavedPositions();
        if (list.empty()) MInfo("Nothing saved yet");
        for (size_t i = 0; i < list.size(); i++) {
            SavedPos p = list[i];
            char lbl[96]; snprintf(lbl, sizeof lbl, "%d. %s", (int)i + 1, p.name.c_str());
            MRow& r = MQueue(lbl, "Enter: go there. Delete: remove it.", [p] { TeleportTo(p.x, p.y, p.z, false); }, mp ? DistText(Dist2D(mp, p.x, p.y)) : "");
            r.del = [i] { auto& l = SavedPositions(); if (i < l.size()) { l.erase(l.begin() + i); SavePositions(); } };
            r.icon = [](ImDrawList* dl, ImVec2 c, float s) { PlaceIconAt(dl, c, s, IC_SAVED); };
        }
    });
}

static void CheatsMenu() {
    bool inGame = PlayerPed() != nullptr;
    for (auto& g : kCheats) {
        const CheatGroup* grp = &g;
        MSub(g.group, nullptr, [grp, inGame] {
            for (auto& c : grp->c) {
                if (!c.code) break;
                const char* code = c.code;
                static char descBuf[128];
                (void)descBuf;
                if (NativeCheatHasState(code))
                    MToggleFn(c.desc, [code, inGame] { return inGame && NativeCheatOn(code); },
                              [code](bool on) { Queue([code, on] { SetNativeCheat(code, on); }); }, code);
                else MQueue(c.desc, NativeCheatIsOneWay(code) ? "Can't be switched off - reload your save to undo it." : code, [code] { RunNativeCheat(code); });
            }
        });
    }
}

static void ActiveMenu() {
    auto items = ActiveCheats();
    if (!S.enabled) { MInfo("The trainer is OFF"); return; }
    if (items.empty()) { MInfo("Nothing is switched on"); return; }
    MQueue("Turn everything off", nullptr, [] { TurnEverythingOff(); });
    MHeader(std::to_string(items.size()) + " active");
    for (auto& a : items) { auto off = a.turnOff; MQueue(a.label, "Enter: turn it off.", off, a.detail); }
}

static void HotkeysMenu() {
    auto& acts = HotkeyActions();
    static char filter[40] = {};
    static bool onlyBound = false;
    MInput("Search", filter, sizeof filter, "Type part of a feature's name.");
    MToggle("Only bound", &onlyBound, "Show only features that have a key.");
    int bound = 0; for (auto& a : acts) if (a.vk) bound++;
    MSub("Clear all hotkeys", "Removes every key binding.", [] {
        MAction("Yes, clear every hotkey", nullptr, [] { for (auto& a : HotkeyActions()) { a.vk = 0; a.mods = 0; } SaveSettings(); PopMenu(); });
        MAction("Cancel", nullptr, [] { PopMenu(); });
    }).right = std::to_string(bound) + " bound";
    std::vector<std::string> cats;
    for (auto& a : acts) if (std::find(cats.begin(), cats.end(), a.category) == cats.end()) cats.push_back(a.category);
    auto rows = [](const std::string& cat, bool all) {
        auto& acts = HotkeyActions();
        for (size_t i = 0; i < acts.size(); i++) {
            HotkeyAction& a = acts[i];
            if ((!all && a.category != cat) || (onlyBound && !a.vk) || !Match(a.label + " " + a.category, filter)) continue;
            MRow& r = Add(MRow::Key, a.label, "Enter: press the new key or combo (Ctrl/Shift/Alt + key). Esc while waiting clears it. Delete removes it.");
            r.keyIndex = (int)i;
            char name[80];
            if (g_capturingHotkey == (int)i) snprintf(name, sizeof name, "press a key...");
            else HotkeyName(a.vk, a.mods, name, sizeof name);
            r.right = name;
            r.del = [i] { auto& l = HotkeyActions(); l[i].vk = 0; l[i].mods = 0; SaveSettings(); };
        }
    };
    if (filter[0] || onlyBound) { MHeader("Results"); rows("", true); return; }
    for (auto& c : cats) MSub(c, nullptr, [c, rows] { rows(c, false); });
}

static void OverlayMenu() {
    MToggle("Coordinates", &S.overlayCoords, nullptr);
    MToggle("Vehicle speed", &S.overlaySpeed, nullptr);
    MToggle("FPS", &S.overlayFps, nullptr);
    MToggle("Speed meter (vel)", &S.overlayShowPos, "Your speed like CS:GO's speed meter: running, driving, hopping, flying.");
    static const char* units[] = { "u/s (CS:GO)", "km/h", "m/s" };
    MChoice("Speed meter units", &S.showPosUnits, units, 3, nullptr);
    MToggle("Active cheats list", &S.overlayActive, nullptr);
}

static void InterfaceMenu() {
    static const char* layouts[] = { "List (Menyoo style)", "Classic window" };
    MChoice("Menu layout", &T.layout, layouts, 2, "List: this menu. Classic: the old tabbed window (switch back in its Settings tab).");
    MSub("Theme presets", "Ready-made colour schemes.", [] {
        for (int i = 0; i < IM_ARRAYSIZE(kPresets); i++) MAction(kPresets[i], nullptr, [i] { ApplyPreset(i); g_uiDirty = true; });
    });
    MSub("Colours", "Every colour of the menu (Enter opens a colour picker).", [] {
        MColor("Banner (left / top)", T.banner1, nullptr);
        MColor("Banner (right / bottom)", T.banner2, "Used when the banner gradient is on.");
        MColor("Banner text", T.bannerText, nullptr);
        MColor("Subtitle bar", T.subBg, nullptr);
        MColor("Subtitle text", T.subText, nullptr);
        MColor("Rows", T.rowBg, nullptr);
        MColor("Row text", T.rowText, nullptr);
        MColor("Selected row", T.selBg, nullptr);
        MColor("Selected text", T.selText, nullptr);
        MColor("Accent (switches, values)", T.accent, nullptr);
        MColor("Dimmed text", T.dimText, nullptr);
        MColor("Description box", T.descBg, nullptr);
        MColor("Description text", T.descText, nullptr);
    });
    MSub("Layout & size", "Position, width, scale, rows, heights.", [] {
        MFloat("Position X", &T.x, 0.0f, 0.95f, 0.005f, "%.3f", "Left edge, as a share of the screen width. You can also drag the banner.");
        MFloat("Position Y", &T.y, 0.0f, 0.95f, 0.005f, "%.3f", "Top edge, as a share of the screen height.");
        MAction("Left side", nullptr, [] { T.x = 0.025f; T.y = 0.06f; });
        MAction("Right side", nullptr, [] { ImVec2 d = ImGui::GetIO().DisplaySize; float s = T.scale * d.y / 1080.0f; T.x = fmaxf(0.0f, 1.0f - (T.width * s + 0.025f * d.x) / d.x); T.y = 0.06f; });
        MAction("Centre", nullptr, [] { ImVec2 d = ImGui::GetIO().DisplaySize; float s = T.scale * d.y / 1080.0f; T.x = fmaxf(0.0f, 0.5f - T.width * s * 0.5f / d.x); T.y = 0.12f; });
        MFloat("Scale", &T.scale, 0.5f, 2.0f, 0.05f, "%.2fx", "Everything bigger or smaller (also follows your screen height).");
        MFloat("Width", &T.width, 300.0f, 900.0f, 10.0f, "%.0f px", nullptr);
        MInt("Visible rows", &T.rows, 5, 30, 1, "%d", "Options shown at once before the list scrolls.");
        MFloat("Row height", &T.rowHeight, 24.0f, 60.0f, 1.0f, "%.0f px", nullptr);
        MFloat("Banner height", &T.bannerHeight, 0.0f, 220.0f, 4.0f, "%.0f px", "0 hides the banner.");
        MFloat("Corner rounding", &T.rounding, 0.0f, 20.0f, 1.0f, "%.0f px", nullptr);
        MFloat("Opacity", &T.opacity, 0.2f, 1.0f, 0.02f, "%.2f", "How see-through the menu background is.");
    });
    MSub("Text & fonts", "Fonts, banner title and tagline.", [] {
        static const char* names[kFontCount];
        for (int i = 0; i < kFontCount; i++) names[i] = kFonts[i].name;
        int f0 = T.font, t0 = T.titleFont;
        MChoice("Menu font", &T.font, names, kFontCount, "Fonts from Windows. Missing ones fall back to the built-in pixel font.");
        MChoice("Banner font", &T.titleFont, names, kFontCount, nullptr);
        if (T.font != f0 || T.titleFont != t0) g_fontsDirty = true;
        MInput("Banner title", T.title, sizeof T.title, nullptr);
        MInput("Banner tagline", T.tagline, sizeof T.tagline, "Small text under the title. Leave empty for none.");
    });
    MSub("Effects & behaviour", "Gradient, shimmer, description box, navigation.", [] {
        MToggle("Banner gradient", &T.gradient, nullptr);
        MToggle("Banner shimmer", &T.shimmer, "A light sweep across the banner.");
        MToggle("Outline", &T.outline, "A thin accent line round the menu.");
        MToggle("Smooth highlight", &T.smooth, "The selection bar glides instead of jumping.");
        MToggle("Description box", &T.descBox, "The selected option's explanation under the menu.");
        MToggle("Position counter", &T.counter, "\"3 / 25\" in the subtitle bar.");
        MToggle("Row icons", &T.rowIcons, "Radar icons and swatches in lists.");
        MToggle("Wrap around", &T.wrap, "Down on the last option goes to the first, and back.");
        MToggle("Remember position", &T.remember, "Each submenu opens where you left it.");
        MToggle("Drag the banner to move", &T.draggable, nullptr);
    });
    MAction("Save interface settings", "Saved to VCTrainer_ui.ini (also saved when you close the menu).", [] { SaveUi(); ShowNative("Interface settings saved"); });
    MAction("Reset interface", "Back to the default look.", [] { T = UiTheme(); ApplyPreset(0); g_fontsDirty = true; });
}

static void SettingsMenu() {
    MToggleFn("Trainer enabled", [] { return S.enabled; }, [](bool v) { Queue([v] { SetTrainerEnabled(v); }); },
              "Off pauses every cheat at once (your choices are kept). Only its hotkey works then.");
    MSub("Interface", "Look and feel: theme, colours, fonts, position, size.", InterfaceMenu);
    MSub("On-screen overlay", "Stats shown while playing.", OverlayMenu);
    MSub("Hotkeys", "A key for any feature.", HotkeysMenu);
    MFloat("Mouse speed", &g_mouseSensitivity, 0.25f, 4.0f, 0.05f, "%.2f", "Cursor speed while the menu is open.");
    MAction("Save settings now", "Toggles, hotkeys and the interface (also saved when you close the menu).", [] { SaveSettings(); SaveUi(); ShowNative("Settings saved"); });
    MInfo("Controls", "", "Up/Down or Numpad 8/2: move.  Left/Right or 4/6: change a value.  Enter or 5: select.\n"
                         "Backspace, Numpad 0 or right-click: back.  Mouse wheel scrolls.  INSERT / F7: open and close.");
    MInfo("About", "v" IMGUI_VERSION, "Modern VC Trainer for GTA Vice City v1.0.");
}

// ---- Vehicle spawner (Menyoo's): a list per category, the highlighted
// vehicle's still picture and its numbers beside it - bars and details from
// data\handling.cfg and data\default.ide. Enter spawns it.
static std::string VehShortName(const std::string& n) { size_t k = n.find("  ("); return k == std::string::npos ? n : n.substr(0, k); }
static std::string VehModelName(const std::string& n) {
    size_t a = n.rfind('('), b = n.rfind(')');
    return a != std::string::npos && b != std::string::npos && b > a + 1 ? n.substr(a + 1, b - a - 1) : n;
}
static std::string Thousands(int v) {
    std::string s = std::to_string(v < 0 ? -v : v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert(i, ",");
    return s;
}
static void VehiclePanel(const VehicleEntry& v) {
    static const char* catNames[] = { "Car", "Bike", "Boat", "Helicopter", "RC" };
    g_side = SidePanel();
    g_side.on = true;
    g_side.picture = v.model;
    g_side.title = VehShortName(v.name);
    const char* cat = v.category >= 0 && v.category < CAT_COUNT ? catNames[v.category] : "";
    g_side.subtitle = cat;
    const VehicleSpec* s = VehicleSpecOf(v.handling);
    if (s) {
        // bars against the fastest / quickest / grippiest in the game
        auto pct = [](float x, float top) { int p = (int)lroundf(x / top * 100.0f); return p < 0 ? 0 : p > 100 ? 100 : p; };
        char sp[24]; snprintf(sp, sizeof sp, "%d mph", (int)lroundf(s->maxKmh * 0.6214f));
        g_side.bars.push_back({ "Speed", pct(s->maxKmh, 240.0f), sp });
        g_side.bars.push_back({ "Acceleration", pct(s->accel, 35.0f), "" });
        g_side.bars.push_back({ "Braking", pct(s->brake, 12.0f), "" });
        g_side.bars.push_back({ "Traction", pct(s->traction, 1.5f), "" });
    }
    static const std::pair<const char*, const char*> classes[] = {
        { "normal", "Normal" }, { "poorfamily", "Poor family" }, { "richfamily", "Rich family" }, { "executive", "Executive" },
        { "worker", "Worker" }, { "big", "Big" }, { "taxi", "Taxi" }, { "moped", "Moped" }, { "motorbike", "Motorbike" },
        { "leisureboat", "Leisure boat" }, { "workerboat", "Work boat" }, { "ignore", "Special" } };
    std::string cls = v.cls;
    for (auto& c : classes) if (!_stricmp(v.cls.c_str(), c.first)) { cls = c.second; break; }
    g_side.info.push_back({ "Class", cls.empty() ? cat : cls });
    g_side.info.push_back({ "Type", cat });
    if (s) {
        const char* drive = s->drive == 'F' ? "Front drive" : s->drive == 'R' ? "Rear drive" : s->drive == '4' ? "4WD" : "-";
        bool landish = v.category == CAT_CARS || v.category == CAT_BIKES || v.category == CAT_RC;
        g_side.info.push_back({ "Layout", landish ? std::to_string(s->gears) + " Gears  |  " + drive : std::to_string(s->gears) + " Gears" });
        const char* eng = s->engine == 'P' ? "Petrol" : s->engine == 'D' ? "Diesel" : s->engine == 'E' ? "Electric" : "-";
        g_side.info.push_back({ "Engine", eng });
        g_side.info.push_back({ "Mass", Thousands((int)lroundf(s->mass)) + " kg" });
        g_side.info.push_back({ "Price", "$" + Thousands(s->value) });
    }
}

static void VehicleRows(const std::vector<const VehicleEntry*>& list) {
    static std::vector<const VehicleEntry*> rowVeh;                // per row: its vehicle
    rowVeh.assign(g_rows.size(), nullptr);
    int sel = g_stack.empty() ? -1 : g_stack.back().sel;
    for (auto* v : list) {
        bool on = sel == (int)g_rows.size();
        int m = v->model;
        MQueue(VehShortName(v->name), v->category == CAT_BOATS ? "Enter: spawn it. Boats spawn in front of you - stand near water."
                                                               : "Enter: spawn it.",
               [m] { SpawnVehicle(m); }, on ? "ModelName: " + VehModelName(v->name) : "");
        rowVeh.push_back(v);
    }
    if (sel >= 0 && sel < (int)rowVeh.size() && rowVeh[sel]) VehiclePanel(*rowVeh[sel]);
}

static void VehicleSpawnerMenu() {
    MToggle("Put me in it", &S.warpIntoSpawned, "You're placed in the driver's seat of what you spawn.");
    static char filter[32] = {};
    MInput("Search", filter, sizeof filter, "Type part of a name to search every category.");
    MQueue("Spawn last vehicle", nullptr, [] { SpawnVehicle(LastSpawnedModel()); });
    if (filter[0]) {
        MHeader("Results");
        std::vector<const VehicleEntry*> list;
        for (auto& v : VehicleList()) if (Match(v.name, filter)) list.push_back(&v);
        VehicleRows(list);
        return;
    }
    static const char* cats[] = { "Cars", "Bikes", "Boats", "Helicopters", "RC" };
    for (int c = 0; c < CAT_COUNT; c++) {
        int count = 0;
        for (auto& v : VehicleList()) if (v.category == c) count++;
        MSub(cats[c], c == CAT_BOATS ? "Boats spawn in front of you - stand near water." : nullptr, [c] {
            std::vector<const VehicleEntry*> list;
            for (auto& v : VehicleList()) if (v.category == c) list.push_back(&v);
            std::sort(list.begin(), list.end(), [](const VehicleEntry* a, const VehicleEntry* b) {
                return _stricmp(a->name.c_str(), b->name.c_str()) < 0; });
            VehicleRows(list);
        }).right = std::to_string(count);
    }
}

// ---- Skins (Menyoo's model changer): moving onto a look puts it on the
// player at once - the live preview - and Enter keeps it. Leaving the menu
// without keeping puts the old look back (DrawTrainerMenu). Beside the list,
// Menyoo's info box with what the game's own files say about that ped:
// data\default.ide (type, walk, cars, radio) and data\pedstats.dat (bars).
static std::string PrettyPedType(const std::string& t) {
    static const std::pair<const char*, const char*> k[] = {
        { "PLAYER1", "Player" }, { "CIVMALE", "Civilian (male)" }, { "CIVFEMALE", "Civilian (female)" },
        { "COP", "Law enforcement" }, { "EMERGENCY", "Paramedic" }, { "FIREMAN", "Fire fighter" },
        { "CRIMINAL", "Criminal" }, { "PROSTITUTE", "Prostitute" }, { "SPECIAL", "Special" },
        { "GANG1", "Gang: Cubans" }, { "GANG2", "Gang: Haitians" }, { "GANG3", "Gang: Streetwannabes" },
        { "GANG4", "Gang: Diaz cartel" }, { "GANG5", "Gang: Security" }, { "GANG6", "Gang: Bikers" },
        { "GANG7", "Gang: Golfers" } };
    for (auto& e : k) if (t == e.first) return e.second;
    return t.empty() ? "-" : t;
}
static std::string PrettyWalk(const std::string& a) {
    static const std::pair<const char*, const char*> k[] = {
        { "man", "Man" }, { "woman", "Woman" }, { "fatwoman", "Heavy woman" }, { "sexywoman", "Sexy woman" },
        { "busywoman", "Busy woman" }, { "oldwoman", "Old woman" }, { "oldman", "Old man" }, { "fatman", "Heavy man" },
        { "gang1", "Gangster" }, { "gang2", "Gangster (cool)" }, { "player", "Player" }, { "cop", "Cop" },
        { "shuffle", "Shuffle" }, { "skate", "Skater" }, { "jogwoman", "Jogger (woman)" }, { "jogger", "Jogger" } };
    for (auto& e : k) if (a == e.first) return e.second;
    if (a.empty()) return "-";
    std::string s = a; s[0] = (char)toupper((unsigned char)s[0]); return s;
}
static const char* RadioName(int r) {
    static const char* k[] = { "Wildstyle", "Flash FM", "K-Chat", "Fever 105", "V-Rock", "VCPR", "Radio Espantoso",
                               "Emotion 98.3", "Wave 103" };
    return r >= 0 && r < 9 ? k[r] : nullptr;
}
static void SkinPanel(const std::string& look, const std::string& shown) {
    g_side = SidePanel();
    g_side.on = true;
    g_side.badge = "LIVE ON TOMMY";
    g_side.title = shown;
    bool outfit = !_strnicmp(look.c_str(), "player", 6);
    const PedModel* pm = nullptr;
    if (!outfit) for (auto& m : PedModels()) if (!_stricmp(m.name.c_str(), look.c_str())) { pm = &m; break; }
    std::string stat = outfit ? "STAT_PLAYER" : pm ? pm->stat : "";
    g_side.subtitle = outfit ? "Tommy Vercetti" : pm ? PrettyPedType(pm->type) : "";
    if (const PedStat* s = PedStatOf(stat)) {
        g_side.bars = { { "Fear", s->fear, "" }, { "Temper", s->temper, "" }, { "Lawfulness", s->law, "" }, { "Sexiness", s->sexy, "" } };
        char b[64];
        snprintf(b, sizeof b, "x%.1f  |  takes x%.1f", s->attack, s->defend);
        g_side.info.push_back({ "Hits", b });
        std::string moves = (s->flags & 1) ? "Punch only" : "Punch";
        if (s->flags & 4) moves += ", kick";
        if (s->flags & 2) moves += ", knee";
        if (s->flags & 8) moves += ", roundhouse";
        g_side.info.push_back({ "Moves", moves });
    }
    if (outfit) {
        g_side.info.push_back({ "Class", "Player" });
        g_side.info.push_back({ "Walk style", "Player" });
        g_side.info.push_back({ "Model", "0  |  " + Lower(look) });
        return;
    }
    if (!pm) return;
    g_side.info.push_back({ "Class", PrettyPedType(pm->type) });
    g_side.info.push_back({ "Walk style", PrettyWalk(pm->anim) });
    if (pm->cars) {
        static const char* groups[] = { "Normal", "Poor family", "Rich family", "Executive", "Worker", "Big",
                                        "Taxi", "Moped", "Motorbike", "Leisure boat", "Work boat" };
        int n = 0; std::string first;
        for (int i = 0; i < 11; i++) if (pm->cars & (1u << i)) { if (!n) first = groups[i]; n++; }
        g_side.info.push_back({ "Drives", n == 1 ? first : first + " +" + std::to_string(n - 1) + " more" });
    } else g_side.info.push_back({ "Drives", "-" });
    const char* r1 = RadioName(pm->radio1), *r2 = RadioName(pm->radio2);
    g_side.info.push_back({ "Radio", r1 ? (r2 && pm->radio2 != pm->radio1 ? std::string(r1) + " / " + r2 : std::string(r1)) : "-" });
    g_side.info.push_back({ "Model", std::to_string(pm->id) + "  |  " + pm->name });
}

static void SkinsMenu() {
    bool entering = g_skinTabFrame < ImGui::GetFrameCount() - 1;
    g_skinTabFrame = ImGui::GetFrameCount();
    static char filter[24] = {};
    static std::vector<std::pair<std::string, std::string>> looks;   // per row: look, shown name ("" = not a look)
    looks.clear();
    auto row = [](const std::string& look, const std::string& shown) {
        looks.resize(g_rows.size());
        bool sel = !g_stack.empty() && g_stack.back().sel == (int)g_rows.size();
        MAction(shown, "Enter: keep this look.  Backspace: put the old one back.", [look] { SkinKeep(look); },
                sel ? "ModelName: " + Lower(look) : "");
        looks.push_back({ look, shown });
    };
    MInput("Search", filter, sizeof filter, "Type part of a name to narrow the list.");
    MHeader("Tommy's outfits");
    for (auto& o : kOutfits) {
        if (!Match(o.name, filter)) continue;
        char look[12];
        if (o.id == 0) strcpy(look, "PLAYER"); else snprintf(look, sizeof look, "PLAYER%d", o.id);
        row(look, o.name);
    }
    MHeader("People");
    for (auto& m : PedModels()) {
        if (!Match(m.name, filter) && !Match(PrettyPedType(m.type), filter)) continue;
        row(m.name, m.name);
    }
    looks.resize(g_rows.size());

    // the highlighted look: worn at once once the highlight rests on it for a
    // moment (holding an arrow doesn't load every model it passes)
    static std::string lastLook;
    static double since = 0;
    int sel = g_stack.empty() ? -1 : g_stack.back().sel;
    const std::string* look = sel >= 0 && sel < (int)looks.size() && !looks[sel].first.empty() ? &looks[sel].first : nullptr;
    if (look) SkinPanel(*look, looks[sel].second);
    std::string cur = look ? *look : "";
    double now = ImGui::GetTime();
    if (entering) { lastLook = cur; since = now; return; }        // opening the menu changes nothing
    if (cur != lastLook) { lastLook = cur; since = now; }
    if (look && _stricmp(g_skinTry.c_str(), cur.c_str()) && now - since > 0.12 && since > 0) {
        SkinTryOn(cur);
        since = -1;                                                 // once per look
    }
}

static void MainMenu() {
    MToggleFn("Trainer enabled", [] { return S.enabled; }, [](bool v) { Queue([v] { SetTrainerEnabled(v); }); }, "Off pauses every cheat at once.");
    MSub("Player", "Health, wanted level, money, movement, proofs.", PlayerMenu);
    MSub("Weapons", "Ammo, bullets, aimbot, special guns, give weapons.", WeaponsMenu);
    MSub("Vehicle", "Repair, options, paint, performance, garage.", VehicleMenu);
    MSub("Vehicle spawner", "Every car, bike, boat and helicopter, with a picture and its numbers.", VehicleSpawnerMenu);
    MSub("Skins", "Every outfit and ped: move onto one to wear it live, Enter to keep it.", SkinsMenu);
    MSub("World", "Time, weather, camera, speed, gravity, population.", WorldMenu);
    MSub("Peds", "Bodyguards, spawning people, everyone nearby.", PedsMenu);
    MSub("Teleport", "Mission markers, coordinates, places, pickups, saved spots.", TeleportMenu);
    MSub("Game cheats", "Vice City's own cheat codes.", CheatsMenu);
    MSub("Active cheats", "Everything switched on right now.", ActiveMenu).right = std::to_string(ActiveCheats().size());
    MSub("Settings", "Interface, overlay, hotkeys, mouse.", SettingsMenu);
}

// ---------------------------------------------------------------- drawing
static void MText(ImDrawList* dl, ImFont* f, float size, ImVec2 p, ImU32 c, const char* t, float maxW = 0) {
    if (maxW > 0) {
        dl->PushClipRect(p, ImVec2(p.x + maxW, p.y + size * 1.5f), true);
        dl->AddText(f, size, p, c, t);
        dl->PopClipRect();
    } else dl->AddText(f, size, p, c, t);
}
static float TextW(ImFont* f, float size, const char* t) { return f->CalcTextSizeA(size, FLT_MAX, 0, t).x; }

static void DrawListMenu() {
    ImGuiIO& io = ImGui::GetIO();
    if (g_stack.empty()) PushMenu("Main menu", MainMenu);
    const size_t depth = g_stack.size();
    auto moved = [&] { return g_stack.size() != depth; };   // went into / out of a submenu
    MLevel& L = g_stack.back();
    g_rows.clear();
    g_side = SidePanel();
    L.build();
    if (moved()) return;
    int n = (int)g_rows.size();

    const float sc = T.scale * io.DisplaySize.y / 1080.0f;
    const float W = T.width * sc, RH = T.rowHeight * sc, BH = T.bannerHeight * sc, SH = 32 * sc, FH = 22 * sc;
    const float fs = RH * 0.50f, fsSmall = RH * 0.40f;
    ImFont* body = g_fBody ? g_fBody : ImGui::GetFont();
    ImFont* title = g_fTitle ? g_fTitle : body;

    // ---- keyboard / wheel
    auto selectable = [&](int i) { return i >= 0 && i < n && Selectable(g_rows[i]); };
    auto step = [&](int from, int dir) {
        for (int k = 0, i = from; k < n; k++) {
            i += dir;
            if (i < 0 || i >= n) { if (!T.wrap) return from; i = i < 0 ? n - 1 : 0; }
            if (selectable(i)) return i;
        }
        return from;
    };
    if (L.sel >= n) L.sel = n - 1;
    if (L.sel < 0) L.sel = 0;
    if (n && !selectable(L.sel)) L.sel = step(L.sel, 1);
    bool editing = g_editRow >= 0 && g_editRow < n;
    bool capturing = g_capturingHotkey >= 0;
    auto pressed = [](ImGuiKey k) { return ImGui::IsKeyPressed(k, true); };
    auto accel = [](ImGuiKey k) { float t = ImGui::GetIO().KeysData[k - ImGuiKey_NamedKey_BEGIN].DownDuration;
                                  return t > 2.0f ? 20 : t > 1.0f ? 5 : 1; };
    if (!editing && !capturing && !g_colorEdit && n) {
        if (pressed(ImGuiKey_UpArrow) || pressed(ImGuiKey_Keypad8)) L.sel = step(L.sel, -1);
        if (pressed(ImGuiKey_DownArrow) || pressed(ImGuiKey_Keypad2)) L.sel = step(L.sel, 1);
        if (pressed(ImGuiKey_PageUp) || pressed(ImGuiKey_Keypad9)) for (int k = 0; k < T.rows; k++) { int s = step(L.sel, -1); if (s > L.sel) break; L.sel = s; }
        if (pressed(ImGuiKey_PageDown) || pressed(ImGuiKey_Keypad3)) for (int k = 0; k < T.rows; k++) { int s = step(L.sel, 1); if (s < L.sel) break; L.sel = s; }
        if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) { L.sel = -1; L.sel = step(-1, 1); }
        if (ImGui::IsKeyPressed(ImGuiKey_End, false)) L.sel = step(n, -1);
        MRow& r = g_rows[L.sel];
        if (!r.disabled && r.adjust) {
            if (pressed(ImGuiKey_LeftArrow)) r.adjust(-accel(ImGuiKey_LeftArrow));
            else if (pressed(ImGuiKey_Keypad4)) r.adjust(-accel(ImGuiKey_Keypad4));
            if (pressed(ImGuiKey_RightArrow)) r.adjust(accel(ImGuiKey_RightArrow));
            else if (pressed(ImGuiKey_Keypad6)) r.adjust(accel(ImGuiKey_Keypad6));
        }
        if (!r.disabled && r.kind == MRow::Toggle &&
            (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) || ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))) r.set(!r.get());
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad5, false)) Activate(r);
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && r.del) r.del();
        else if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad0, false)) { PopMenu(); return; }
    } else if (g_colorEdit && (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) ||
                               ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
        g_colorEdit = nullptr; g_uiDirty = true;
    }
    if (moved()) return;

    // ---- layout
    int visible = n < T.rows ? n : T.rows;
    if (visible < 1) visible = 1;
    float targetScroll = L.scroll;
    if (L.sel < (int)targetScroll) targetScroll = (float)L.sel;
    if (L.sel >= (int)targetScroll + visible) targetScroll = (float)(L.sel - visible + 1);
    if (targetScroll > n - visible) targetScroll = (float)(n - visible < 0 ? 0 : n - visible);
    L.scroll = targetScroll;
    int first = (int)L.scroll;

    const MRow* selRow = n ? &g_rows[L.sel] : nullptr;
    std::string desc = selRow ? selRow->desc : "";
    if (selRow && selRow->disabled && desc.empty()) desc = "Not available right now.";
    float descH = 0;
    if (T.descBox && !desc.empty()) {
        ImVec2 ts = g_fSmall ? g_fSmall->CalcTextSizeA(fsSmall, FLT_MAX, W - 24 * sc, desc.c_str()) : ImVec2(0, fsSmall);
        descH = ts.y + 18 * sc;
    }
    float listH = visible * RH;
    float totalH = BH + SH + listH + FH + (descH > 0 ? descH + 6 * sc : 0);
    ImVec2 pos(T.x * io.DisplaySize.x, T.y * io.DisplaySize.y);
    if (pos.y + totalH > io.DisplaySize.y) pos.y = fmaxf(0, io.DisplaySize.y - totalH);
    if (pos.x + W > io.DisplaySize.x) pos.x = fmaxf(0, io.DisplaySize.x - W);

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(W, totalH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##listmenu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float R = T.rounding * sc, op = T.opacity;
    ImVec2 o = pos;

    // ---- banner
    if (BH > 0) {
        ImVec2 a = o, b(o.x + W, o.y + BH);
        if (T.gradient) {
            dl->AddRectFilled(a, b, Col(T.banner1), R, ImDrawFlags_RoundCornersTop);
            ImU32 c1 = Col(T.banner2, 0.0f), c2 = Col(T.banner2);
            dl->AddRectFilledMultiColor(ImVec2(a.x + R, a.y), ImVec2(b.x - R, b.y), c1, c2, c2, c1);
            dl->AddRectFilled(ImVec2(b.x - R, a.y), b, Col(T.banner2), R, ImDrawFlags_RoundCornersTopRight);
        } else dl->AddRectFilled(a, b, Col(T.banner1), R, ImDrawFlags_RoundCornersTop);
        if (T.shimmer) {
            float t = fmodf((float)ImGui::GetTime() * 0.35f, 1.6f) - 0.3f;
            float cx = a.x + t * W, bw = W * 0.18f;
            dl->PushClipRect(a, b, true);
            dl->AddRectFilledMultiColor(ImVec2(cx - bw, a.y), ImVec2(cx, b.y), IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 40),
                                        IM_COL32(255, 255, 255, 40), IM_COL32(255, 255, 255, 0));
            dl->AddRectFilledMultiColor(ImVec2(cx, a.y), ImVec2(cx + bw, b.y), IM_COL32(255, 255, 255, 40), IM_COL32(255, 255, 255, 0),
                                        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 40));
            dl->PopClipRect();
        }
        float tsz = BH * 0.52f;
        float tw = TextW(title, tsz, T.title);
        if (tw > W - 20 * sc) { tsz *= (W - 20 * sc) / tw; tw = TextW(title, tsz, T.title); }
        float ty = a.y + (T.tagline[0] ? BH * 0.14f : (BH - tsz) * 0.5f);
        dl->AddText(title, tsz, ImVec2(a.x + (W - tw) * 0.5f + 2 * sc, ty + 2 * sc), IM_COL32(0, 0, 0, 110), T.title);
        dl->AddText(title, tsz, ImVec2(a.x + (W - tw) * 0.5f, ty), Col(T.bannerText), T.title);
        if (T.tagline[0]) {
            float gs = BH * 0.17f, gw = TextW(body, gs, T.tagline);
            dl->AddText(body, gs, ImVec2(a.x + (W - gw) * 0.5f, ty + tsz * 1.02f), Col(T.bannerText, 0.85f), T.tagline);
        }
        // drag the banner to move the menu
        static bool dragging = false;
        if (T.draggable && ImGui::IsMouseHoveringRect(a, b) && ImGui::IsMouseClicked(0)) dragging = true;
        if (!ImGui::IsMouseDown(0)) dragging = false;
        if (dragging && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
            T.x = fminf(fmaxf(T.x + io.MouseDelta.x / io.DisplaySize.x, 0.0f), 0.95f);
            T.y = fminf(fmaxf(T.y + io.MouseDelta.y / io.DisplaySize.y, 0.0f), 0.95f);
            g_uiDirty = true;
        }
    }
    // ---- subtitle bar: menu path + counter
    float y = o.y + BH;
    dl->AddRectFilled(ImVec2(o.x, y), ImVec2(o.x + W, y + SH), Col(T.subBg, op), BH > 0 ? 0 : R, BH > 0 ? 0 : ImDrawFlags_RoundCornersTop);
    {
        std::string path = L.title;
        for (auto& c : path) c = (char)toupper((unsigned char)c);
        float fsz = SH * 0.5f;
        char cnt[32] = "";
        if (T.counter && n) {
            int sIdx = 0, sCount = 0;
            for (int i = 0; i < n; i++) if (Selectable(g_rows[i])) { sCount++; if (i <= L.sel) sIdx = sCount; }
            snprintf(cnt, sizeof cnt, "%d / %d", sIdx, sCount);
        }
        float cw = TextW(body, fsz, cnt);
        MText(dl, body, fsz, ImVec2(o.x + 12 * sc, y + (SH - fsz) * 0.5f), Col(T.subText), path.c_str(), W - cw - 30 * sc);
        if (cnt[0]) dl->AddText(body, fsz, ImVec2(o.x + W - cw - 12 * sc, y + (SH - fsz) * 0.5f), Col(T.subText, 0.8f), cnt);
        dl->AddRectFilled(ImVec2(o.x, y + SH - 2 * sc), ImVec2(o.x + W, y + SH), Col(T.accent));
    }
    y += SH;
    // ---- rows
    float listTop = y;
    dl->AddRectFilled(ImVec2(o.x, y), ImVec2(o.x + W, y + listH), Col(T.rowBg, op));
    float targetSelY = listTop + (L.sel - first) * RH;
    if (g_selY < 0 || !T.smooth) g_selY = targetSelY;
    else g_selY += (targetSelY - g_selY) * fminf(1.0f, io.DeltaTime * 18.0f);
    if (n && g_selY >= listTop - RH && g_selY <= listTop + listH) {
        dl->PushClipRect(ImVec2(o.x, listTop), ImVec2(o.x + W, listTop + listH), true);
        dl->AddRectFilled(ImVec2(o.x, g_selY), ImVec2(o.x + W, g_selY + RH), Col(T.selBg));
        dl->AddRectFilled(ImVec2(o.x, g_selY), ImVec2(o.x + 4 * sc, g_selY + RH), Col(T.accent));
        dl->PopClipRect();
    }
    bool mouseMoved = io.MouseDelta.x != 0 || io.MouseDelta.y != 0;
    for (int vi = 0; vi < visible && first + vi < n; vi++) {
        int i = first + vi;
        MRow& r = g_rows[i];
        float ry = listTop + vi * RH;
        ImVec2 ra(o.x, ry), rb(o.x + W, ry + RH);
        bool sel = i == L.sel;
        bool hover = ImGui::IsMouseHoveringRect(ra, rb) && !g_colorEdit;
        if (hover && Selectable(r) && mouseMoved && !editing) L.sel = i, sel = true;
        ImU32 tc = r.disabled ? Col(T.dimText) : sel ? Col(T.selText) : Col(T.rowText);
        float ty = ry + (RH - fs) * 0.5f;
        float lx = o.x + 14 * sc;
        if (r.kind == MRow::Header) {
            std::string h = r.label; for (auto& c : h) c = (char)toupper((unsigned char)c);
            float hs = fs * 0.82f, hw = TextW(body, hs, h.c_str());
            float hy = ry + (RH - hs) * 0.5f;
            dl->AddText(body, hs, ImVec2(o.x + (W - hw) * 0.5f, hy), Col(T.accent), h.c_str());
            dl->AddLine(ImVec2(o.x + 14 * sc, ry + RH * 0.5f), ImVec2(o.x + (W - hw) * 0.5f - 10 * sc, ry + RH * 0.5f), Col(T.dimText, 0.6f));
            dl->AddLine(ImVec2(o.x + (W + hw) * 0.5f + 10 * sc, ry + RH * 0.5f), ImVec2(o.x + W - 14 * sc, ry + RH * 0.5f), Col(T.dimText, 0.6f));
            continue;
        }
        if (T.rowIcons && (r.icon || r.swatch)) {
            float is = RH * 0.62f;
            ImVec2 c(lx + is * 0.5f, ry + RH * 0.5f);
            if (r.swatch) dl->AddRectFilled(ImVec2(c.x - is * 0.42f, c.y - is * 0.42f), ImVec2(c.x + is * 0.42f, c.y + is * 0.42f), r.swatch, 3 * sc);
            else r.icon(dl, c, is);
            lx += is + 8 * sc;
        }
        // right side
        float rx = o.x + W - 14 * sc;
        if (r.kind == MRow::Sub) {
            float ah = fs * 0.32f;
            dl->AddTriangleFilled(ImVec2(rx - ah, ry + RH * 0.5f - ah), ImVec2(rx, ry + RH * 0.5f), ImVec2(rx - ah, ry + RH * 0.5f + ah), tc);
            rx -= ah + 10 * sc;
            if (!r.right.empty()) { float w = TextW(body, fs * 0.85f, r.right.c_str()); dl->AddText(body, fs * 0.85f, ImVec2(rx - w, ry + (RH - fs * 0.85f) * 0.5f), sel ? tc : Col(T.dimText), r.right.c_str()); rx -= w + 8 * sc; }
        } else if (r.kind == MRow::Toggle) {
            bool on = r.get();
            float th = RH * 0.46f, tw = th * 1.9f;
            ImVec2 ta(rx - tw, ry + (RH - th) * 0.5f), tb(rx, ta.y + th);
            dl->AddRectFilled(ta, tb, on ? Col(T.accent) : Col(T.dimText, 0.45f), th * 0.5f);
            float kx = on ? tb.x - th * 0.5f : ta.x + th * 0.5f;
            dl->AddCircleFilled(ImVec2(kx, ta.y + th * 0.5f), th * 0.38f, IM_COL32(255, 255, 255, 255));
            rx -= tw + 10 * sc;
        } else if (r.kind == MRow::Number || r.kind == MRow::Choice || r.kind == MRow::Input || r.kind == MRow::Key ||
                   r.kind == MRow::Action || r.kind == MRow::Info || r.kind == MRow::Color) {
            std::string v = r.right;
            if (r.kind == MRow::Input) v = r.text && r.text[0] ? r.text : "...";
            if (r.kind == MRow::Color) {
                float cs = RH * 0.5f;
                dl->AddRectFilled(ImVec2(rx - cs * 1.6f, ry + (RH - cs) * 0.5f), ImVec2(rx, ry + (RH + cs) * 0.5f), Col(r.color), 3 * sc);
                dl->AddRect(ImVec2(rx - cs * 1.6f, ry + (RH - cs) * 0.5f), ImVec2(rx, ry + (RH + cs) * 0.5f), IM_COL32(255, 255, 255, 120), 3 * sc);
                rx -= cs * 1.6f + 10 * sc;
            }
            bool arrows = sel && !r.disabled && (r.kind == MRow::Number || r.kind == MRow::Choice);
            if (editing && i == g_editRow) v.clear();
            if (!v.empty()) {
                float vs = fs * 0.92f, w = TextW(body, vs, v.c_str());
                float ah = vs * 0.3f;
                if (arrows) {
                    dl->AddTriangleFilled(ImVec2(rx - ah, ry + RH * 0.5f - ah), ImVec2(rx, ry + RH * 0.5f), ImVec2(rx - ah, ry + RH * 0.5f + ah), tc);
                    rx -= ah + 8 * sc;
                }
                ImU32 vc = r.kind == MRow::Number || r.kind == MRow::Choice || r.kind == MRow::Key ? (sel ? tc : Col(T.accent))
                          : sel ? tc : Col(T.dimText);
                if (w > W * 0.5f) w = W * 0.5f;
                MText(dl, body, vs, ImVec2(rx - w, ry + (RH - vs) * 0.5f), vc, v.c_str(), w);
                rx -= w;
                if (arrows) {
                    rx -= 8 * sc;
                    dl->AddTriangleFilled(ImVec2(rx, ry + RH * 0.5f - ah), ImVec2(rx - ah, ry + RH * 0.5f), ImVec2(rx, ry + RH * 0.5f + ah), tc);
                    rx -= ah;
                }
                rx -= 10 * sc;
                // mouse: click the arrows to change the value
                if (hover && ImGui::IsMouseClicked(0) && r.adjust && !r.disabled) {
                    float mx = io.MousePos.x;
                    if (mx > rx && mx < o.x + W) { r.adjust(mx > rx + (o.x + W - rx) * 0.5f ? 1 : -1); continue; }
                }
            }
        }
        // label
        MText(dl, body, fs, ImVec2(lx, ty), tc, r.label.c_str(), rx - lx);
        // inline editor
        if (editing && i == g_editRow) {
            float ex = o.x + W * 0.45f;
            ImGui::SetCursorScreenPos(ImVec2(ex, ry + (RH - ImGui::GetFrameHeight()) * 0.5f));
            ImGui::SetNextItemWidth(o.x + W - ex - 14 * sc);
            ImGui::SetKeyboardFocusHere();
            bool done = false;
            if (r.kind == MRow::Input) done = ImGui::InputText("##edit", r.text, r.textSize, ImGuiInputTextFlags_EnterReturnsTrue);
            else done = ImGui::InputText("##edit", g_editBuf, sizeof g_editBuf, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsScientific);
            if (done) {
                if (r.kind == MRow::Number && r.setNum) r.setNum(atof(g_editBuf));
                g_editRow = -1;
            } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) g_editRow = -1;
        }
        if (hover && Selectable(r) && !editing) {
            if (ImGui::IsMouseClicked(0)) { L.sel = i; Activate(r); }
            else if (ImGui::IsMouseClicked(1)) { PopMenu(); ImGui::End(); ImGui::PopStyleVar(2); return; }
        }
        if (moved()) { ImGui::End(); ImGui::PopStyleVar(2); return; }
    }
    if (n == 0) dl->AddText(body, fs, ImVec2(o.x + 14 * sc, listTop + (RH - fs) * 0.5f), Col(T.dimText), "Nothing here");
    // wheel / right-click on empty space
    if (ImGui::IsMouseHoveringRect(ImVec2(o.x, listTop), ImVec2(o.x + W, listTop + listH)) && io.MouseWheel != 0 && n && !editing) {
        int d = io.MouseWheel > 0 ? -1 : 1;
        L.sel = step(L.sel, d);                                   // one row per wheel notch
    }
    if (ImGui::IsMouseClicked(1) && !ImGui::IsMouseHoveringRect(ImVec2(o.x, listTop), ImVec2(o.x + W, listTop + listH)) &&
        ImGui::IsMouseHoveringRect(o, ImVec2(o.x + W, o.y + totalH)) && !g_colorEdit) { PopMenu(); ImGui::End(); ImGui::PopStyleVar(2); return; }
    y = listTop + listH;
    // ---- footer: scroll indicator
    dl->AddRectFilled(ImVec2(o.x, y), ImVec2(o.x + W, y + FH), Col(T.subBg, op), descH > 0 ? 0 : R, descH > 0 ? 0 : ImDrawFlags_RoundCornersBottom);
    {
        float cx = o.x + W * 0.5f, cy = y + FH * 0.5f, a = FH * 0.22f;
        bool up = first > 0, down = first + visible < n;
        dl->AddTriangleFilled(ImVec2(cx - a * 1.4f, cy - 1.5f * sc), ImVec2(cx + a * 1.4f, cy - 1.5f * sc), ImVec2(cx, cy - a * 1.4f - 1.5f * sc), Col(T.subText, up ? 0.9f : 0.25f));
        dl->AddTriangleFilled(ImVec2(cx - a * 1.4f, cy + 1.5f * sc), ImVec2(cx, cy + a * 1.4f + 1.5f * sc), ImVec2(cx + a * 1.4f, cy + 1.5f * sc), Col(T.subText, down ? 0.9f : 0.25f));
        if (n > visible) {                                       // scrollbar
            float sh = listH * visible / n, sy = listTop + listH * first / n;
            dl->AddRectFilled(ImVec2(o.x + W - 3 * sc, sy), ImVec2(o.x + W, sy + sh), Col(T.accent, 0.8f));
        }
    }
    y += FH;
    // ---- description
    if (descH > 0) {
        y += 6 * sc;
        dl->AddRectFilled(ImVec2(o.x, y), ImVec2(o.x + W, y + descH), Col(T.descBg, op), R);
        dl->AddRectFilled(ImVec2(o.x, y), ImVec2(o.x + 3 * sc, y + descH), Col(T.accent), R, ImDrawFlags_RoundCornersLeft);
        if (g_fSmall) dl->AddText(g_fSmall, fsSmall, ImVec2(o.x + 12 * sc, y + 9 * sc), Col(T.descText), desc.c_str(), nullptr, W - 24 * sc);
    }
    if (T.outline) dl->AddRect(o, ImVec2(o.x + W, o.y + BH + SH + listH + FH), Col(T.accent, 0.55f), R, 0, 1.5f * sc);
    ImGui::End();
    ImGui::PopStyleVar(2);

    // ---- colour picker beside the menu
    if (g_colorEdit) {
        float px = pos.x + W + 12 * sc;
        if (px + 300 > io.DisplaySize.x) px = fmaxf(0, pos.x - 312);
        ImGui::SetNextWindowPos(ImVec2(px, pos.y + BH));
        ImGui::SetNextWindowSize(ImVec2(300, 0));
        if (ImGui::Begin(("Colour: " + g_colorLabel + "###colpick").c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {
            ImGui::ColorPicker4("##pick", g_colorEdit, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview);
            if (ImGui::Button("Done", ImVec2(-FLT_MIN, 0))) { g_colorEdit = nullptr; g_uiDirty = true; }
            ImGui::TextDisabled("Enter or Backspace also closes it.");
        }
        ImGui::End();
    }

    // ---- info panel beside the menu (Menyoo's preview box)
    if (g_side.on && !g_colorEdit) {
        const float PW = W * 0.86f, gap = 8 * sc, pad = 10 * sc;
        float px = pos.x + W + gap;
        if (px + PW > io.DisplaySize.x) px = fmaxf(0, pos.x - PW - gap);
        float py = pos.y + BH + SH;
        const float headH = g_side.picture >= 0 ? PW * 0.62f : PW * 0.36f, barH = RH * 0.62f, infoH = RH * 0.66f;
        float barsH = g_side.bars.empty() ? 0 : g_side.bars.size() * (barH + 4 * sc) + pad * 2 - 4 * sc;
        float infoBoxH = g_side.info.empty() ? 0 : g_side.info.size() * infoH + pad * 2;
        float PH = headH + (barsH > 0 ? gap + barsH : 0) + (infoBoxH > 0 ? gap + infoBoxH : 0);
        ImGui::SetNextWindowPos(ImVec2(px, py));
        ImGui::SetNextWindowSize(ImVec2(PW, PH));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::Begin("##sidepanel", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing);
        ImDrawList* pd = ImGui::GetWindowDrawList();
        ImFont* small = g_fSmall ? g_fSmall : body;
        float rr = T.rounding * sc;
        float cy = py;
        // picture box (a vehicle's still picture, name along the bottom)
        if (g_side.picture >= 0) {
            ImVec2 a(px, cy), b(px + PW, cy + headH);
            pd->AddRectFilledMultiColor(a, b, IM_COL32(58, 62, 80, 255), IM_COL32(58, 62, 80, 255),
                                        IM_COL32(24, 25, 33, 255), IM_COL32(24, 25, 33, 255));
            float side = headH - 4 * sc;
            ImVec2 ia(a.x + (PW - side) * 0.5f, a.y + 2 * sc), ib(ia.x + side, ia.y + side);
            if (void* pic = VehiclePicture(g_side.picture))
                pd->AddImage((ImTextureID)(intptr_t)pic, ia, ib);
            else {
                const char* t = VehiclePictureMissing(g_side.picture) ? "no picture" : "...";
                float tw2 = TextW(small, fsSmall, t);
                pd->AddText(small, fsSmall, ImVec2(a.x + (PW - tw2) * 0.5f, a.y + (headH - fsSmall) * 0.5f), Col(T.dimText), t);
            }
            VehiclePicturesPump();                              // makes the picture if it isn't there yet
            float ns = fs, nw = TextW(title, ns, g_side.title.c_str());
            ImVec2 la(a.x, b.y - ns * 1.5f);
            pd->AddRectFilledMultiColor(la, b, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 170), IM_COL32(0, 0, 0, 170));
            pd->AddText(title, ns, ImVec2(a.x + pad, b.y - ns * 1.25f), Col(T.selText), g_side.title.c_str());
            if (!g_side.subtitle.empty()) {
                float sw = TextW(small, fsSmall, g_side.subtitle.c_str());
                if (pad * 2 + nw + 10 * sc + sw < PW)
                    pd->AddText(small, fsSmall, ImVec2(b.x - pad - sw, b.y - ns * 1.25f + (ns - fsSmall) * 0.6f), Col(T.rowText, 0.8f), g_side.subtitle.c_str());
            }
            pd->AddRect(a, b, Col(T.accent, 0.5f), 0, 0, 1.0f * sc);
            cy += headH;
        } else
        // title box: the name big, its class under it, a "live" badge
        {
            ImVec2 a(px, cy), b(px + PW, cy + headH);
            pd->AddRectFilled(a, b, Col(T.rowBg, op), rr);
            ImU32 t0 = Col(T.accent, 0.35f), t1 = Col(T.accent, 0.0f);
            pd->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + headH * 0.6f), t0, t0, t1, t1);
            float ns = headH * 0.30f, nw = TextW(title, ns, g_side.title.c_str());
            if (nw > PW - pad * 2) { ns *= (PW - pad * 2) / nw; nw = TextW(title, ns, g_side.title.c_str()); }
            float ny = a.y + headH * 0.30f;
            pd->AddText(title, ns, ImVec2(a.x + (PW - nw) * 0.5f + 2 * sc, ny + 2 * sc), IM_COL32(0, 0, 0, 120), g_side.title.c_str());
            pd->AddText(title, ns, ImVec2(a.x + (PW - nw) * 0.5f, ny), Col(T.selText), g_side.title.c_str());
            if (!g_side.subtitle.empty()) {
                float ss = fsSmall, sw = TextW(small, ss, g_side.subtitle.c_str());
                pd->AddText(small, ss, ImVec2(a.x + (PW - sw) * 0.5f, ny + ns * 1.1f), Col(T.rowText, 0.85f), g_side.subtitle.c_str());
            }
            if (!g_side.badge.empty()) {
                float bs = fsSmall * 0.8f, bw = TextW(small, bs, g_side.badge.c_str());
                ImVec2 ba(a.x + pad * 0.6f, a.y + pad * 0.6f), bb(ba.x + bw + 10 * sc + bs, ba.y + bs + 6 * sc);
                pd->AddRectFilled(ba, bb, Col(T.accent), (bb.y - ba.y) * 0.5f);
                pd->AddCircleFilled(ImVec2(ba.x + 5 * sc + bs * 0.3f, (ba.y + bb.y) * 0.5f), bs * 0.22f, IM_COL32(255, 255, 255, 255));
                pd->AddText(small, bs, ImVec2(ba.x + 7 * sc + bs * 0.6f, ba.y + 3 * sc), IM_COL32(255, 255, 255, 255), g_side.badge.c_str());
            }
            cy += headH;
        }
        // stat bars: label inside, value on the right, filled to the value
        if (barsH > 0) {
            cy += gap;
            pd->AddRectFilled(ImVec2(px, cy), ImVec2(px + PW, cy + barsH), Col(T.rowBg, op), rr);
            float by = cy + pad;
            for (auto& br : g_side.bars) {
                ImVec2 a(px + pad, by), b(px + PW - pad, by + barH);
                int v = br.pct < 0 ? 0 : br.pct > 100 ? 100 : br.pct;
                pd->AddRectFilled(a, b, Col(T.accent, 0.22f));
                pd->AddRectFilled(a, ImVec2(a.x + (b.x - a.x) * v / 100.0f, b.y), Col(T.accent, 0.9f));
                float ls = barH * 0.68f;
                pd->AddText(body, ls, ImVec2(a.x + 6 * sc, a.y + (barH - ls) * 0.5f), IM_COL32(255, 255, 255, 255), br.label.c_str());
                char vt[32]; snprintf(vt, sizeof vt, "%d%%", v);
                if (!br.text.empty()) snprintf(vt, sizeof vt, "%s", br.text.c_str());
                float vw = TextW(body, ls, vt);
                pd->AddText(body, ls, ImVec2(b.x - vw - 6 * sc, a.y + (barH - ls) * 0.5f), IM_COL32(255, 255, 255, 255), vt);
                by += barH + 4 * sc;
            }
            cy += barsH;
        }
        // label / value rows
        if (infoBoxH > 0) {
            cy += gap;
            pd->AddRectFilled(ImVec2(px, cy), ImVec2(px + PW, cy + infoBoxH), Col(T.rowBg, op), rr);
            float iy = cy + pad;
            float is = infoH * 0.58f;
            for (auto& in : g_side.info) {
                float ty = iy + (infoH - is) * 0.5f;
                pd->AddText(small, is, ImVec2(px + pad, ty), Col(T.dimText), in.first.c_str());
                float vw = TextW(small, is, in.second.c_str()), maxW = PW * 0.62f;
                if (vw > maxW) vw = maxW;
                MText(pd, small, is, ImVec2(px + PW - pad - vw, ty), Col(T.rowText), in.second.c_str(), vw);
                iy += infoH;
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
}

int& UiLayout() { return T.layout; }
