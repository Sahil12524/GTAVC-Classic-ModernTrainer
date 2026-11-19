// Vehicle model preview for the spawner: the game's own 3D model of the
// selected vehicle, with its own textures, drawn in the menu - no image files.
//
// How: on the game's update (Queue) the model is streamed in and its visible
// geometry (positions, texture coordinates, material colours and the
// Direct3D texture of each material) is copied into the trainer's own arrays.
// The model stays loaded while it's previewed (its textures belong to the
// game), and is released when another vehicle is picked. Each frame the copy
// is drawn with ImGui as shaded, textured triangles sorted back to front,
// turning slowly on its own and by dragging with the mouse. If the game drops
// the model anyway (loading a save), it's simply loaded again.
//
// RenderWare 3.x layouts used (checked against gta-vc.exe where noted):
//   RwFrame: parent +4, modelling matrix +0x10, child +0x98, next +0x9C (0x644840)
//   RwMatrix: right +0, up +0x10, at +0x20, pos +0x30
//   RpAtomic: frame +4, flags byte +2 (4 = render; the game clears it for
//             "_dam" parts, 0x57A1AA), geometry +0x18
//   RpGeometry: numTriangles +0x10, numVertices +0x14, numTexCoordSets +0x1C,
//               materials** +0x20, numMaterials +0x24, triangles +0x2C,
//               texCoords[0] +0x34, morphTarget +0x5C
//   RpMorphTarget: verts +0x14;  RpTriangle: u16 v[3], u16 material
//   RpMaterial: texture +0, colour RGBA +4;  RwTexture: raster +0
//   Direct3D texture: *(raster + *(int*)0xA0D8B0)  (RwD3D8SetTexture, 0x659370)
//   GetFrameNodeName 0x580600 (frame plugin name), RpClumpForAllAtomics 0x640D00
#include "trainer.h"
#include "game.h"
#include "features.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx8.h"
#include <windows.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <string>
#include <math.h>

using namespace game;

namespace {

struct Tri { int a, b, c; unsigned col; void* tex; bool cull; };   // cull: faces outward, hidden when turned away
struct Mesh {
    int model = -1;
    void* clump = nullptr;         // the model's clump when copied: still the same = still loaded
    std::vector<float> v;          // x, y, z
    std::vector<float> uv;         // u, v
    std::vector<Tri> t;
    float cx = 0, cy = 0, cz = 0, radius = 1;
    bool failed = false;
};

using NodeName_t = const char*(__cdecl*)(void*);
using ForAllAtomics_t = void*(__cdecl*)(void*, void*(__cdecl*)(void*, void*), void*);
auto GetFrameNodeName = (NodeName_t)0x580600;
auto ClumpForAllAtomics = (ForAllAtomics_t)0x640D00;
int* const RasterExtOffset = (int*)0xA0D8B0;

bool Readable(const void* p, size_t n) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return (const char*)p + n <= (const char*)mbi.BaseAddress + mbi.RegionSize;
}

// The frame's matrix in the vehicle's space: its own matrix times every
// parent's up to the clump's root (the root's own placement is left out).
struct M34 { float r[3], u[3], a[3], p[3]; };
M34 FrameMatrix(void* frame) {
    M34 m = { {1,0,0}, {0,1,0}, {0,0,1}, {0,0,0} };
    for (int depth = 0; frame && depth < 32; depth++) {
        void* parent = at<void*>(frame, 4);
        if (!parent) break;                                   // root: the vehicle's own placement
        const float* f = (const float*)((char*)frame + 0x10);
        const float* R = f, *U = f + 4, *A = f + 8, *P = f + 12;
        auto xf = [&](const float* v, float* out) {
            out[0] = v[0] * R[0] + v[1] * U[0] + v[2] * A[0];
            out[1] = v[0] * R[1] + v[1] * U[1] + v[2] * A[1];
            out[2] = v[0] * R[2] + v[1] * U[2] + v[2] * A[2];
        };
        M34 n;
        xf(m.r, n.r); xf(m.u, n.u); xf(m.a, n.a); xf(m.p, n.p);
        for (int i = 0; i < 3; i++) n.p[i] += P[i];
        m = n;
        frame = parent;
    }
    return m;
}
void Apply(const M34& m, const float* v, float* o) {
    for (int i = 0; i < 3; i++) o[i] = v[0] * m.r[i] + v[1] * m.u[i] + v[2] * m.a[i] + m.p[i];
}

// Material colour as the game shows it: the paint placeholders become paint.
// The vehicle's first colour pair from data\carcols.dat ("car" section:
// "name, primary,secondary, ..."), as palette indices into CarPalette().
// -1 = none listed. The file (from the game folder, not the working
// directory) is read once; each vehicle's pair is kept by lower-case name.
void DefaultColours(const char* name, int& c1, int& c2) {
    static std::unordered_map<std::string, std::pair<int, int>> pairs;
    static bool loaded = false;
    c1 = c2 = -1;
    if (!loaded) {
        loaded = true;
        char path[MAX_PATH];
        GamePath(path, "data\\carcols.dat");
        FILE* f = fopen(path, "r");
        if (!f) { Log("File: can't open %s - spawner pictures use the default paint", path); return; }
        char line[512]; bool inCar = false;
        while (fgets(line, sizeof line, f)) {
            char* l = line; while (*l == ' ' || *l == '\t') l++;
            if (!strncmp(l, "car", 3) && (l[3] == '\n' || l[3] == '\r' || !l[3])) { inCar = true; continue; }
            if (!inCar) continue;
            if (!strncmp(l, "end", 3)) break;
            char* comma = strchr(l, ',');
            if (!comma || comma == l) continue;
            std::string key(l, comma - l);
            for (auto& ch : key) ch = (char)tolower((unsigned char)ch);
            int a = -1, b = -1;
            sscanf(comma + 1, " %d , %d", &a, &b);
            pairs.emplace(key, std::make_pair(a, b));             // the first line for a name wins, as before
        }
        fclose(f);
    }
    std::string key = name;
    for (auto& ch : key) ch = (char)tolower((unsigned char)ch);
    auto it = pairs.find(key);
    if (it != pairs.end()) { c1 = it->second.first; c2 = it->second.second; }
}
unsigned g_paint1 = IM_COL32(190, 30, 40, 255), g_paint2 = IM_COL32(225, 225, 230, 255);

unsigned PaintColour(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (r == 60 && g == 255 && b == 0)       return (g_paint1 & ~IM_COL32_A_MASK) | ((unsigned)a << IM_COL32_A_SHIFT);   // primary
    else if (r == 255 && g == 0 && b == 175) return (g_paint2 & ~IM_COL32_A_MASK) | ((unsigned)a << IM_COL32_A_SHIFT);   // secondary
    else if (r == 0 && g == 255 && b == 255) { r = 255; g = 220; b = 120; }   // lights placeholders
    else if (r == 255 && g == 0 && b == 255) { r = 255; g = 60;  b = 40;  }
    return IM_COL32(r, g, b, a);
}

void* D3DTextureOf(void* rwTexture) {
    if (!Readable(rwTexture, 4)) return nullptr;
    char* raster = at<char*>(rwTexture, 0);
    int ext = *RasterExtOffset;
    if (ext <= 0 || ext > 0x400 || !Readable(raster, ext + 4)) return nullptr;
    void* tex = at<void*>(raster, ext);
    return ImGui_ImplDX8_IsTexture(tex) ? tex : nullptr;
}

struct Ctx { Mesh* m; int budget; bool noWheels; };

// Helicopters (and RC ones) have a rotor somewhere in their frame tree.
bool HasRotor(void* frame, int depth = 0) {
    for (int n = 0; frame && depth < 32 && n < 256; n++) {
        if (!Readable(frame, 0xA4)) return false;
        const char* name = GetFrameNodeName(frame);
        if (name && Readable(name, 1) && strstr(name, "rotor")) return true;
        if (HasRotor(at<void*>(frame, 0x98), depth + 1)) return true;
        frame = at<void*>(frame, 0x9C);
    }
    return false;
}

void* __cdecl CollectAtomic(void* atomic, void* data) {
    Ctx* c = (Ctx*)data;
    if (!Readable(atomic, 0x1C)) return atomic;
    if (!(at<unsigned char>(atomic, 2) & 4)) return atomic;            // hidden (damaged parts)
    void* frame = at<void*>(atomic, 4);
    if (!Readable(frame, 0xA4)) return atomic;
    const char* name = GetFrameNodeName(frame);
    if (name && Readable(name, 1) &&
        (strstr(name, "_vlo") || strstr(name, "moving_rotor"))) return atomic;   // far LOD, blurred spinning rotor
    // Wheels: when the game sets a vehicle model up it attaches the wheel
    // model to each wheel marker (bikes' wheel_front / wheel_rear too), placed
    // and sized. Helicopters get them as well but the game hides them, and
    // the left-side ones only get turned round to face out while driving.
    bool wheel = name && Readable(name, 1) && !_strnicmp(name, "wheel", 5);
    if (wheel && c->noWheels) return atomic;
    bool leftWheel = wheel && !_strnicmp(name, "wheel_l", 7);
    Mesh* dst = c->m;
    char* g = at<char*>(atomic, 0x18);
    if (!Readable(g, 0x60)) return atomic;
    int nt = at<int>(g, 0x10), nv = at<int>(g, 0x14), nuv = at<int>(g, 0x1C), nm = at<int>(g, 0x24);
    void** mats = at<void**>(g, 0x20);
    unsigned short* tris = at<unsigned short*>(g, 0x2C);
    const float* uvs = nuv > 0 ? at<const float*>(g, 0x34) : nullptr;
    char* mt = at<char*>(g, 0x5C);
    if (nt <= 0 || nt > 20000 || nv <= 0 || nv > 65535 || nm < 0 || nm > 256) return atomic;
    if (!Readable(tris, nt * 8) || !Readable(mt, 0x1C)) return atomic;
    const float* verts = at<const float*>(mt, 0x14);
    if (!Readable(verts, nv * 12)) return atomic;
    const float* norms = at<const float*>(mt, 0x18);                   // vertex normals (point outward)
    if (norms && !Readable(norms, nv * 12)) norms = nullptr;
    if (nm && !Readable(mats, nm * 4)) return atomic;
    if (uvs && !Readable(uvs, nv * 8)) uvs = nullptr;
    if (c->budget < nt) return atomic;
    c->budget -= nt;

    // per material: colour and texture
    std::vector<unsigned> mcol(nm, IM_COL32(170, 170, 175, 255));
    std::vector<void*> mtex(nm, nullptr);
    for (int i = 0; i < nm; i++) {
        char* mat = (char*)mats[i];
        if (!Readable(mat, 8)) continue;
        const unsigned char* rgba = (const unsigned char*)(mat + 4);
        mcol[i] = PaintColour(rgba[0], rgba[1], rgba[2], rgba[3]);
        if (uvs) mtex[i] = D3DTextureOf(at<void*>(mat, 0));
    }

    M34 fm = FrameMatrix(frame);
    if (leftWheel)                                                     // half a turn about the vertical: rim faces out
        for (int j = 0; j < 3; j++) { fm.r[j] = -fm.r[j]; fm.u[j] = -fm.u[j]; }
    int base = (int)dst->v.size() / 3;
    for (int i = 0; i < nv; i++) {
        float o[3]; Apply(fm, verts + i * 3, o);
        dst->v.insert(dst->v.end(), o, o + 3);
        dst->uv.push_back(uvs ? uvs[i * 2] : 0.0f);
        dst->uv.push_back(uvs ? uvs[i * 2 + 1] : 0.0f);
    }
    // Which side of each triangle is its front: the vertex normals say
    // which way is out. Triangles are stored with that side by the winding
    // (swapped if needed), so the preview can hide the ones turned away -
    // without a depth buffer those (car insides, far sides) flickered
    // through. No normals: drawn from both sides.
    const float* V = &dst->v[base * 3];
    for (int i = 0; i < nt; i++) {
        const unsigned short* t = tris + i * 4;
        if (t[0] >= nv || t[1] >= nv || t[2] >= nv) continue;
        int a = t[0], b = t[1], d = t[2];
        bool cull = false;
        if (norms) {
            float ux = V[b*3] - V[a*3], uy = V[b*3+1] - V[a*3+1], uz = V[b*3+2] - V[a*3+2];
            float vx = V[d*3] - V[a*3], vy = V[d*3+1] - V[a*3+1], vz = V[d*3+2] - V[a*3+2];
            float gx = uy * vz - uz * vy, gy = uz * vx - ux * vz, gz = ux * vy - uy * vx;
            float n[3] = { 0, 0, 0 };
            for (int k : { a, b, d }) for (int j = 0; j < 3; j++) n[j] += norms[k * 3 + j];
            float wn[3];                                                // vertex normal turned like the part
            for (int j = 0; j < 3; j++) wn[j] = n[0] * fm.r[j] + n[1] * fm.u[j] + n[2] * fm.a[j];
            float dot = gx * wn[0] + gy * wn[1] + gz * wn[2];
            float gl = sqrtf(gx * gx + gy * gy + gz * gz), nl = sqrtf(wn[0] * wn[0] + wn[1] * wn[1] + wn[2] * wn[2]);
            if (gl > 1e-12f && nl > 1e-6f && fabsf(dot) > 0.2f * gl * nl) {   // normals clearly agree on a side
                if (dot < 0) { int tmp = b; b = d; d = tmp; }
                cull = true;
            }
        }
        bool ok = t[3] < nm;
        dst->t.push_back({ base + a, base + b, base + d,
                            ok ? mcol[t[3]] : IM_COL32(170, 170, 175, 255), ok ? mtex[t[3]] : nullptr, cull });
    }
    return atomic;
}

void Release(int model, bool byUs) {
    if (model <= 0 || !byUs) return;
    unsigned char* si = StreamInfo + model * 20;
    if (si[0] == 1) { SetModelDeletable(model); SetModelTxdDeletable(model); }
}

// Load the model (kept loaded until its picture is taken) and copy what's
// needed to draw it. Runs on the game's update.
void Extract(int model, Mesh& out, bool& heldByUs) {
    Mesh m; m.model = model;
    heldByUs = false;
    if (model <= 0 || model >= MODEL_COUNT || !ModelInfoPtrs[model]) { m.failed = true; out = std::move(m); return; }
    unsigned char* si = StreamInfo + model * 20;
    bool alreadyKept = (si[1] & 1) != 0;                    // the game (or a spawn) keeps it anyway
    RequestModel(model, 1);
    LoadAllRequested(false);
    heldByUs = !alreadyKept;
    void* clump = si[0] == 1 ? at<void*>(ModelInfoPtrs[model], 0x28) : nullptr;
    if (clump && Readable(clump, 0x30)) {
        m.clump = clump;
        // the vehicle's own default paint (first pair in carcols.dat)
        g_paint1 = IM_COL32(190, 30, 40, 255); g_paint2 = IM_COL32(225, 225, 230, 255);
        for (auto& e : VehicleList())
            if (e.model == model) {
                size_t a = e.name.rfind('('), b = e.name.rfind(')');
                if (a != std::string::npos && b != std::string::npos && b > a + 1) {
                    int c1, c2; DefaultColours(e.name.substr(a + 1, b - a - 1).c_str(), c1, c2);
                    const auto& pal = CarPalette();
                    if (c1 >= 0 && c1 < (int)pal.size()) g_paint1 = IM_COL32(pal[c1].r, pal[c1].g, pal[c1].b, 255);
                    if (c2 >= 0 && c2 < (int)pal.size()) g_paint2 = IM_COL32(pal[c2].r, pal[c2].g, pal[c2].b, 255);
                }
                break;
            }
        Ctx c{ &m, 16000, HasRotor(at<void*>(clump, 4)) };
        ClumpForAllAtomics(clump, CollectAtomic, &c);
    }
    if (m.t.empty()) { m.failed = true; out = std::move(m); return; }
    float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
    for (size_t i = 0; i < m.v.size(); i += 3)
        for (int k = 0; k < 3; k++) { lo[k] = fminf(lo[k], m.v[i + k]); hi[k] = fmaxf(hi[k], m.v[i + k]); }
    m.cx = (lo[0] + hi[0]) * 0.5f; m.cy = (lo[1] + hi[1]) * 0.5f; m.cz = (lo[2] + hi[2]) * 0.5f;
    float dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2];
    m.radius = fmaxf(0.5f, 0.5f * sqrtf(dx * dx + dy * dy + dz * dz));
    out = std::move(m);
}

// Still the same loaded model the copy (and its textures) came from?
bool StillLoaded(const Mesh& m) {
    int mi = m.model;
    if (mi <= 0 || !m.clump) return false;
    if (StreamInfo[mi * 20] != 1 || !ModelInfoPtrs[mi]) return false;
    return at<void*>(ModelInfoPtrs[mi], 0x28) == m.clump;
}

// The mesh seen from a three-quarter front angle, fitted into size x size
// pixels, as vertices + texture batches for the depth-buffered draw.
void BuildVerts(const Mesh& m, float size, std::vector<ImGuiDX8MeshVertex>& verts, std::vector<ImGuiDX8MeshBatch>& batches) {
    const float yaw = 2.35f, pitch = 0.30f;
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
    float scale = 0.5f * size / m.radius * 0.98f;
    float camDist = m.radius * 3.2f;
    size_t nv = m.v.size() / 3;
    std::vector<float> sx(nv), syv(nv), wx(nv), wy(nv), wz(nv);
    for (size_t i = 0; i < nv; i++) {
        float x = m.v[i * 3] - m.cx, y = m.v[i * 3 + 1] - m.cy, z = m.v[i * 3 + 2] - m.cz;
        float x1 = x * cy - y * sy, y1 = x * sy + y * cy;                 // turn around the vertical
        float y2 = y1 * cp - z * sp, z2 = y1 * sp + z * cp;               // tilt towards the viewer
        wx[i] = x1; wy[i] = y2; wz[i] = z2;
        float persp = camDist / (camDist + y2);
        sx[i] = size * 0.5f + x1 * scale * persp;
        syv[i] = size * 0.5f - z2 * scale * persp;
    }
    float zNear = camDist - m.radius * 1.05f, zSpan = m.radius * 2.1f;
    struct Pending { int tri; float d; };
    std::vector<Pending> solid, glass;
    for (size_t i = 0; i < m.t.size(); i++) {
        const Tri& t = m.t[i];
        bool see = (t.col >> IM_COL32_A_SHIFT & 0xFF) < 250;
        (see ? glass : solid).push_back({ (int)i, wy[t.a] + wy[t.b] + wy[t.c] });
    }
    std::sort(solid.begin(), solid.end(), [&](const Pending& a, const Pending& b) { return m.t[a.tri].tex < m.t[b.tri].tex; });
    std::sort(glass.begin(), glass.end(), [](const Pending& a, const Pending& b) { return a.d > b.d; });
    verts.clear(); batches.clear();
    const float L[3] = { -0.35f, -0.55f, 0.76f };                            // light: upper left, towards the viewer
    auto emit = [&](const Pending& pd, bool see) {
        const Tri& t = m.t[pd.tri];
        float ux = wx[t.b] - wx[t.a], uy = wy[t.b] - wy[t.a], uz = wz[t.b] - wz[t.a];
        float vx = wx[t.c] - wx[t.a], vy = wy[t.c] - wy[t.a], vz = wz[t.c] - wz[t.a];
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        float nl = sqrtf(nx * nx + ny * ny + nz * nz);
        float lit = 0.6f;
        if (nl > 1e-9f) lit = 0.45f + 0.65f * fabsf((nx * L[0] + ny * L[1] + nz * L[2]) / nl);
        unsigned col = t.col;
        auto ch = [&](int shift) { float v = (col >> shift & 0xFF) * lit; return (unsigned)(v > 255 ? 255 : v); };
        unsigned shaded = IM_COL32(ch(IM_COL32_R_SHIFT), ch(IM_COL32_G_SHIFT), ch(IM_COL32_B_SHIFT), col >> IM_COL32_A_SHIFT & 0xFF);
        if (batches.empty() || batches.back().texture != t.tex || batches.back().seeThrough != see)
            batches.push_back({ t.tex, (int)verts.size(), 0, see });
        for (int k : { t.a, t.b, t.c }) {
            float z = (camDist + wy[k] - zNear) / zSpan;
            z = z < 0.0f ? 0.0f : z > 1.0f ? 1.0f : z;
            verts.push_back({ sx[k], syv[k], z, shaded, m.uv[k * 2], m.uv[k * 2 + 1] });
        }
        batches.back().count += 3;
    };
    for (auto& pd : solid) emit(pd, false);
    for (auto& pd : glass) emit(pd, true);
}

// ---- pictures: one vehicle at a time is loaded, copied, drawn once into a
// picture and released; the pictures are kept for the session.
constexpr int kPicSize = 192;
std::unordered_map<int, void*> g_pics;      // model -> picture (managed IDirect3DTexture8*)
std::unordered_set<int> g_noPic;            // models with nothing to draw
std::vector<int> g_wantPics;                // tiles on screen this frame without a picture
enum { JOB_NONE, JOB_LOADING, JOB_READY, JOB_RENDERING } g_jobState = JOB_NONE;
int g_job = -1;
bool g_jobHeld = false;
Mesh g_jobMesh;
std::vector<ImGuiDX8MeshVertex> g_jobVerts;
std::vector<ImGuiDX8MeshBatch> g_jobBatches;

// retry: the picture couldn't be drawn for a passing reason (the device was
// being reset - alt-tab, a refresh-rate change): it is asked for again when
// its tile next shows, instead of being marked "nothing to draw" for good.
void FinishJob(void* pic, bool retry = false) {
    if (pic) g_pics[g_job] = pic; else if (!retry) g_noPic.insert(g_job);
    int m = g_job; bool held = g_jobHeld;
    Queue([m, held] { Release(m, held); });                 // the model can go now
    g_jobMesh = Mesh(); g_jobVerts.clear(); g_jobBatches.clear();
    g_job = -1; g_jobState = JOB_NONE;
}
} // namespace

void* VehiclePicture(int model) {
    auto it = g_pics.find(model);
    if (it != g_pics.end()) return it->second;
    if (!g_noPic.count(model) && model != g_job) g_wantPics.push_back(model);
    return nullptr;
}
bool VehiclePictureMissing(int model) { return g_noPic.count(model) != 0; }

// Once per frame, after the tiles asked for their pictures: starts loading
// the first one that's missing, or draws the loaded one into its picture
// (from a draw-list callback, so it happens while the menu is rendered).
void VehiclePicturesPump() {
    if (g_jobState == JOB_NONE && !g_wantPics.empty()) {
        int m = g_wantPics.front();
        g_job = m; g_jobState = JOB_LOADING;
        Queue([m] { Extract(m, g_jobMesh, g_jobHeld); g_jobState = JOB_READY; });
    } else if (g_jobState == JOB_READY) {
        if (g_jobMesh.failed || !StillLoaded(g_jobMesh)) {
            FinishJob(nullptr);
        } else {
            BuildVerts(g_jobMesh, (float)kPicSize, g_jobVerts, g_jobBatches);
            g_jobState = JOB_RENDERING;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddCallback([](const ImDrawList*, const ImDrawCmd*) {
                if (g_jobState != JOB_RENDERING) return;
                bool retry = false;
                void* pic = StillLoaded(g_jobMesh)
                    ? ImGui_ImplDX8_RenderMeshToTexture(kPicSize, g_jobVerts.data(), g_jobBatches.data(), (int)g_jobBatches.size(), &retry)
                    : nullptr;
                if (!pic && !retry) Log("Spawner: no picture for vehicle model %d (nothing drawable)", g_job);
                FinishJob(pic, retry);
            }, nullptr);
            dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
        }
    }
    g_wantPics.clear();
}
