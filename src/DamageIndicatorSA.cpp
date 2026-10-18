#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <math.h>
#include <algorithm>

// =======================================================================
// GTA San Andreas (v1.0 US) Minimal Directional Damage Indicator
// Standalone ASI plugin - Zero dependencies.
// =======================================================================

// --- RenderWare State Enums ---
#define rwRENDERSTATETEXTURERASTER      1
#define rwRENDERSTATEZTESTENABLE        6
#define rwRENDERSTATESHADEMODE          7
#define rwRENDERSTATEZWRITEENABLE       8
#define rwRENDERSTATESRCBLEND           10
#define rwRENDERSTATEDESTBLEND          11
#define rwRENDERSTATEVERTEXALPHAENABLE  12

#define rwBLENDSRCALPHA                 5
#define rwBLENDINVSRCALPHA              6
#define rwSHADEMODEFLAT                 1
#define rwSHADEMODEGOURAUD              2
#define rwPRIMTYPETRIFAN                5

#pragma pack(push, 1)
struct RwIm2DVertex {
    float x;
    float y;
    float z;
    float rhw;
    uint32_t color;
    float u;
    float v;
};
#pragma pack(pop)

// --- Engine & RenderWare Calls ---
static inline int RwRenderStateSet(int state, void* value)
{
    uintptr_t rwEngine = *(uintptr_t*)0x00C97B24; // RwEngineInstance
    if (!rwEngine) return 0;
    typedef int (__cdecl *tRwRenderStateSet)(int state, void *value);
    tRwRenderStateSet fn = *(tRwRenderStateSet*)(rwEngine + 0x20);
    if (!fn) return 0;
    return fn(state, value);
}

static inline int RwIm2DRenderPrimitive(int primType, RwIm2DVertex *vertices, int numVertices)
{
    typedef int (__cdecl *tRwIm2DRenderPrimitive)(int primType, RwIm2DVertex *vertices, int numVertices);
    tRwIm2DRenderPrimitive fn = (tRwIm2DRenderPrimitive)0x00734E90;
    return fn(primType, vertices, numVertices);
}

typedef void      (*HudDrawFn)();
typedef uintptr_t (__cdecl *FindPlayerPedFn)(int playerId);
typedef uintptr_t (__cdecl *FindPlayerVehicleFn)(int playerId, bool bIncludeRemote);

static const FindPlayerPedFn     FindPlayerPed     = (FindPlayerPedFn)0x0056E210;
static const FindPlayerVehicleFn FindPlayerVehicle = (FindPlayerVehicleFn)0x0056E0D0;

static HudDrawFn g_OrigHudDraw = nullptr;
static uintptr_t g_RetComputeDamage = 0x004B5AC6;

// --- Mod Configuration ---
struct ModConfig {
    bool     enabled;
    uint8_t  red;
    uint8_t  green;
    uint8_t  blue;
    float    maxAlpha;
    DWORD    duration;
    float    widthPercent;
    float    heightPercent;
    bool     smoothFade;
    int      reloadKey;
};

static ModConfig g_Config = {
    true,        // enabled
    215, 18, 18, // red, green, blue
    210.0f,      // maxAlpha
    800,         // duration (ms)
    16.0f,       // widthPercent (%)
    18.0f,       // heightPercent (%)
    true,        // smoothFade
    0            // reloadKey
};

static HMODULE g_hModule = NULL;

static void LoadConfig(HMODULE hModule) {
    if (hModule) {
        g_hModule = hModule;
    }
    char iniPath[MAX_PATH] = { 0 };
    if (GetModuleFileNameA(g_hModule, iniPath, MAX_PATH)) {
        char* dot = strrchr(iniPath, '.');
        if (dot) {
            dot[1] = 'i';
            dot[2] = 'n';
            dot[3] = 'i';
            dot[4] = '\0';
        }
    }

    if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) {
        if (GetFileAttributesA(".\\DamageIndicator.ini") != INVALID_FILE_ATTRIBUTES) {
            strncpy_s(iniPath, ".\\DamageIndicator.ini", MAX_PATH);
        } else {
            HANDLE hFile = CreateFileA(iniPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                const char* defaultIni =
                    "; ==============================================================================\r\n"
                    "; GTA San Andreas - Damage Indicator Configuration by Caner Karaca\r\n"
                    "; ==============================================================================\r\n\r\n"
                    "[General]\r\n"
                    "; Enable or disable the damage indicator (1 = Enabled, 0 = Disabled)\r\n"
                    "Enabled = 1\r\n\r\n"
                    "; Optional hotkey to reload settings without restarting the game\r\n"
                    "; (0 = Disabled, 121 = F10, 122 = F11, 123 = F12)\r\n"
                    "ReloadKey = 0\r\n\r\n"
                    "[Color]\r\n"
                    "; Indicator color in RGB format (0 - 255)\r\n"
                    "Red = 215\r\n"
                    "Green = 18\r\n"
                    "Blue = 18\r\n\r\n"
                    "[Appearance]\r\n"
                    "; Maximum edge opacity at peak damage (0 - 255, default 210)\r\n"
                    "MaxAlpha = 210\r\n\r\n"
                    "; Duration of the indicator fade in milliseconds (default 800)\r\n"
                    "Duration = 800\r\n\r\n"
                    "; Side edge thickness (% of screen width, 1 - 50, default 16)\r\n"
                    "WidthPercent = 16\r\n\r\n"
                    "; Top/Bottom edge thickness (% of screen height, 1 - 50, default 18)\r\n"
                    "HeightPercent = 18\r\n\r\n"
                    "; Fade-out animation curve (1 = Smooth quadratic, 0 = Linear)\r\n"
                    "SmoothFade = 1\r\n";
                DWORD written = 0;
                WriteFile(hFile, defaultIni, (DWORD)strlen(defaultIni), &written, NULL);
                CloseHandle(hFile);
            }
        }
    }

    g_Config.enabled       = GetPrivateProfileIntA("General", "Enabled", 1, iniPath) != 0;
    g_Config.reloadKey     = GetPrivateProfileIntA("General", "ReloadKey", 0, iniPath);

    int r = GetPrivateProfileIntA("Color", "Red", 215, iniPath);
    int g = GetPrivateProfileIntA("Color", "Green", 18, iniPath);
    int b = GetPrivateProfileIntA("Color", "Blue", 18, iniPath);
    g_Config.red   = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
    g_Config.green = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
    g_Config.blue  = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));

    int maxA = GetPrivateProfileIntA("Appearance", "MaxAlpha", 210, iniPath);
    g_Config.maxAlpha = (float)(maxA < 1 ? 1 : (maxA > 255 ? 255 : maxA));

    int dur = GetPrivateProfileIntA("Appearance", "Duration", 800, iniPath);
    g_Config.duration = (DWORD)(dur < 50 ? 50 : (dur > 10000 ? 10000 : dur));

    int wp = GetPrivateProfileIntA("Appearance", "WidthPercent", 16, iniPath);
    g_Config.widthPercent = (float)(wp < 1 ? 1 : (wp > 50 ? 50 : wp));

    int hp = GetPrivateProfileIntA("Appearance", "HeightPercent", 18, iniPath);
    g_Config.heightPercent = (float)(hp < 1 ? 1 : (hp > 50 ? 50 : hp));

    g_Config.smoothFade = GetPrivateProfileIntA("Appearance", "SmoothFade", 1, iniPath) != 0;
}

static inline uint32_t MakeARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline void SetVertex(RwIm2DVertex& v, float x, float y, uint32_t argb, float z, float rhw) {
    v.x = x;
    v.y = y;
    v.z = z;
    v.rhw = rhw;
    v.color = argb;
    v.u = 0.0f;
    v.v = 0.0f;
}

// --- Damage Hit Tracking ---
struct DamageHit {
    DWORD startTime;
    DWORD duration;
    float sourceX;
    float sourceY;
    bool  active;
};

static const int MAX_HITS = 16;
static DamageHit g_Hits[MAX_HITS];

static inline bool IsValidPtr(uintptr_t p)
{
    return (p >= 0x00010000 && p <= 0x7FFFFFFF);
}

static bool GetEntityPos(uintptr_t pEntity, float& outX, float& outY)
{
    if (!IsValidPtr(pEntity)) return false;
    uintptr_t pMatrix = *(uintptr_t*)(pEntity + 0x14); // CPlaceable::m_matrix
    if (IsValidPtr(pMatrix)) {
        outX = *(float*)(pMatrix + 0x30); // CMatrix::pos.x
        outY = *(float*)(pMatrix + 0x34); // CMatrix::pos.y
    } else {
        outX = *(float*)(pEntity + 0x04); // CSimpleTransform::m_vPosn.x
        outY = *(float*)(pEntity + 0x08); // CSimpleTransform::m_vPosn.y
    }
    return true;
}

static void RecordHit(float atkX, float atkY)
{
    DWORD now = GetTickCount();
    for (int i = 0; i < MAX_HITS; ++i) {
        if (!g_Hits[i].active || (now - g_Hits[i].startTime >= g_Hits[i].duration)) {
            g_Hits[i].startTime = now;
            g_Hits[i].duration  = g_Config.duration;
            g_Hits[i].sourceX   = atkX;
            g_Hits[i].sourceY   = atkY;
            g_Hits[i].active    = true;
            return;
        }
    }
    // Shift oldest if full
    for (int i = 0; i < MAX_HITS - 1; ++i) {
        g_Hits[i] = g_Hits[i + 1];
    }
    g_Hits[MAX_HITS - 1].startTime = now;
    g_Hits[MAX_HITS - 1].duration  = g_Config.duration;
    g_Hits[MAX_HITS - 1].sourceX   = atkX;
    g_Hits[MAX_HITS - 1].sourceY   = atkY;
    g_Hits[MAX_HITS - 1].active    = true;
}

// --- Damage Hook Handler ---
void __stdcall OnPedDamage(uintptr_t pVictim, uintptr_t pCalculator)
{
    if (!IsValidPtr(pVictim) || !IsValidPtr(pCalculator)) return;

    uintptr_t pPlayer = FindPlayerPed(-1);
    if (!IsValidPtr(pPlayer) || pVictim != pPlayer) return;

    // Attacker entity is at offset 0x00 of CPedDamageResponseCalculator
    uintptr_t pAttacker = *(uintptr_t*)(pCalculator + 0x00);
    if (!IsValidPtr(pAttacker)) return;

    // If attacker is a vehicle, use driver position if available
    uint8_t entityType = *(uint8_t*)(pAttacker + 0x36) & 7;
    if (entityType == 2) {
        uintptr_t pDriver = *(uintptr_t*)(pAttacker + 0x460); // CVehicle::m_pDriver
        if (IsValidPtr(pDriver)) {
            pAttacker = pDriver;
        }
    }

    float atkX = 0.0f, atkY = 0.0f;
    if (!GetEntityPos(pAttacker, atkX, atkY)) return;

    float plrX = 0.0f, plrY = 0.0f;
    uintptr_t pVeh = FindPlayerVehicle(-1, false);
    if (IsValidPtr(pVeh)) {
        GetEntityPos(pVeh, plrX, plrY);
    } else {
        GetEntityPos(pPlayer, plrX, plrY);
    }

    float dx = atkX - plrX;
    float dy = atkY - plrY;
    if ((dx * dx + dy * dy) < 0.001f) return;

    RecordHit(atkX, atkY);
}

// Naked detour for CPedDamageResponseCalculator::ComputeDamageResponse (0x004B5AC0)
__declspec(naked) void Hook_ComputeDamageResponse()
{
    __asm {
        pushad
        pushfd

        // Stack layout after pushad (32 bytes) + pushfd (4 bytes) = 36 bytes:
        // [esp + 36] = caller return address
        // [esp + 40] = victim CPed* (first argument pushed by caller)
        // ecx = this (CPedDamageResponseCalculator*)
        push ecx
        push dword ptr [esp + 44] // victim CPed*
        call OnPedDamage

        popfd
        popad

        // Replaced instruction at 0x004B5AC0 (mov eax, dword ptr fs:[0])
        mov eax, dword ptr fs:[0]
        jmp dword ptr [g_RetComputeDamage]
    }
}

// Render full rectangular screen-edge gradients (smooth corners, identical to Vice City)
static void DrawEdgeGlow(float screenW, float screenH, float edgeW, float edgeH,
                         float aLeft, float aRight, float aTop, float aBottom,
                         float nearZ, float recipNearClip) {
    RwIm2DVertex quad[4];
    const uint8_t R = g_Config.red, G = g_Config.green, B = g_Config.blue;

    // Top Edge: spans full screen width, fades downwards
    if (aTop > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)aTop, R, G, B);
        uint32_t colTrans  = MakeARGB(0, R, G, B);
        SetVertex(quad[0], 0.0f,    0.0f,  colOpaque, nearZ, recipNearClip);
        SetVertex(quad[1], screenW, 0.0f,  colOpaque, nearZ, recipNearClip);
        SetVertex(quad[2], screenW, edgeH, colTrans,  nearZ, recipNearClip);
        SetVertex(quad[3], 0.0f,    edgeH, colTrans,  nearZ, recipNearClip);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Bottom Edge: spans full screen width, fades upwards
    if (aBottom > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)aBottom, R, G, B);
        uint32_t colTrans  = MakeARGB(0, R, G, B);
        SetVertex(quad[0], 0.0f,    screenH - edgeH, colTrans,  nearZ, recipNearClip);
        SetVertex(quad[1], screenW, screenH - edgeH, colTrans,  nearZ, recipNearClip);
        SetVertex(quad[2], screenW, screenH,         colOpaque, nearZ, recipNearClip);
        SetVertex(quad[3], 0.0f,    screenH,         colOpaque, nearZ, recipNearClip);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Left Edge: spans full screen height, fades rightwards
    if (aLeft > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)aLeft, R, G, B);
        uint32_t colTrans  = MakeARGB(0, R, G, B);
        SetVertex(quad[0], 0.0f,  0.0f,    colOpaque, nearZ, recipNearClip);
        SetVertex(quad[1], edgeW, 0.0f,    colTrans,  nearZ, recipNearClip);
        SetVertex(quad[2], edgeW, screenH, colTrans,  nearZ, recipNearClip);
        SetVertex(quad[3], 0.0f,  screenH, colOpaque, nearZ, recipNearClip);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Right Edge: spans full screen height, fades leftwards
    if (aRight > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)aRight, R, G, B);
        uint32_t colTrans  = MakeARGB(0, R, G, B);
        SetVertex(quad[0], screenW - edgeW, 0.0f,    colTrans,  nearZ, recipNearClip);
        SetVertex(quad[1], screenW,         0.0f,    colOpaque, nearZ, recipNearClip);
        SetVertex(quad[2], screenW,         screenH, colOpaque, nearZ, recipNearClip);
        SetVertex(quad[3], screenW - edgeW, screenH, colTrans,  nearZ, recipNearClip);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }
}

// --- Render Indicators ---
static void DrawDamageIndicators()
{
    __try {
        if (g_Config.reloadKey != 0 && (GetAsyncKeyState(g_Config.reloadKey) & 1)) {
            LoadConfig(NULL);
        }

        if (!g_Config.enabled) return;

        // Check if there are active hits
        DWORD now = GetTickCount();
        bool anyActive = false;
        for (int i = 0; i < MAX_HITS; ++i) {
            if (g_Hits[i].active) {
                if (now - g_Hits[i].startTime < g_Hits[i].duration) {
                    anyActive = true;
                    break;
                } else {
                    g_Hits[i].active = false;
                }
            }
        }
        if (!anyActive) return;

        // Player coordinates
        uintptr_t pPlayer = FindPlayerPed(-1);
        if (!IsValidPtr(pPlayer)) return;

        float plrX = 0.0f, plrY = 0.0f;
        uintptr_t pVeh = FindPlayerVehicle(-1, false);
        if (IsValidPtr(pVeh)) {
            GetEntityPos(pVeh, plrX, plrY);
        } else {
            GetEntityPos(pPlayer, plrX, plrY);
        }

        int width  = *(int*)0x00C17044; // RsGlobal.maximumWidth
        int height = *(int*)0x00C17048; // RsGlobal.maximumHeight
        if (width <= 0 || height <= 0) return;

        float screenW = (float)width;
        float screenH = (float)height;
        float edgeW   = screenW * (g_Config.widthPercent / 100.0f);
        float edgeH   = screenH * (g_Config.heightPercent / 100.0f);

        float nearZ = *(float*)0x00C80460;        // CSprite2d::NearScreenZ
        float recipNearClip = *(float*)0x00C80464; // CSprite2d::RecipNearClip
        if (recipNearClip <= 0.00001f) recipNearClip = 1.0f;

        // Active Camera 2D Forward Vector
        uint8_t activeCam = *(uint8_t*)(0x00B6F028 + 0x59);
        float camLookX = 0.0f, camLookY = 0.0f;
        if (activeCam <= 2) {
            uintptr_t pCam = 0x00B6F028 + 0x174 + activeCam * 0x238;
            camLookX = *(float*)(pCam + 0x190); // CCam::m_vecFront.x
            camLookY = *(float*)(pCam + 0x194); // CCam::m_vecFront.y
        }
        if ((camLookX * camLookX + camLookY * camLookY) < 0.0001f) {
            // Fallback: TheCamera.m_mCameraMatrix up vector at +0x974 + 0x10
            camLookX = *(float*)(0x00B6F028 + 0x974 + 0x10);
            camLookY = *(float*)(0x00B6F028 + 0x974 + 0x14);
        }

        float camLen = sqrtf(camLookX * camLookX + camLookY * camLookY);
        if (camLen > 0.0001f) {
            camLookX /= camLen;
            camLookY /= camLen;
        } else {
            camLookX = 0.0f;
            camLookY = 1.0f;
        }

        // 2D Camera Right Vector (90 degrees clockwise in world XY)
        float camRightX = camLookY;
        float camRightY = -camLookX;

        float edgeLeft = 0.0f, edgeRight = 0.0f, edgeTop = 0.0f, edgeBottom = 0.0f;

        for (int i = 0; i < MAX_HITS; ++i) {
            if (!g_Hits[i].active) continue;

            DWORD elapsed = now - g_Hits[i].startTime;
            if (elapsed >= g_Hits[i].duration) {
                g_Hits[i].active = false;
                continue;
            }

            float t = 1.0f - ((float)elapsed / (float)g_Hits[i].duration);
            float fade = g_Config.smoothFade ? (t * t) : t;
            float alpha = fade * g_Config.maxAlpha;

            float dx = g_Hits[i].sourceX - plrX;
            float dy = g_Hits[i].sourceY - plrY;

            // Project attacker onto Camera Forward and Right
            float fwd   = dx * camLookX  + dy * camLookY;
            float right = dx * camRightX + dy * camRightY;

            float len = sqrtf(fwd * fwd + right * right);
            if (len > 0.0001f) {
                fwd   /= len;
                right /= len;
            }

            edgeTop    += (std::max)(0.0f, fwd)    * alpha;
            edgeBottom += (std::max)(0.0f, -fwd)   * alpha;
            edgeRight  += (std::max)(0.0f, right)  * alpha;
            edgeLeft   += (std::max)(0.0f, -right) * alpha;
        }

        edgeLeft   = (std::min)(g_Config.maxAlpha, edgeLeft);
        edgeRight  = (std::min)(g_Config.maxAlpha, edgeRight);
        edgeTop    = (std::min)(g_Config.maxAlpha, edgeTop);
        edgeBottom = (std::min)(g_Config.maxAlpha, edgeBottom);

        if (edgeLeft <= 1.0f && edgeRight <= 1.0f && edgeTop <= 1.0f && edgeBottom <= 1.0f) {
            return;
        }

        // Set RenderWare states
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, 0);
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, 0);
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, 0);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
        RwRenderStateSet(rwRENDERSTATESHADEMODE, (void*)rwSHADEMODEGOURAUD);

        DrawEdgeGlow(screenW, screenH, edgeW, edgeH, edgeLeft, edgeRight, edgeTop, edgeBottom, nearZ, recipNearClip);

        // Fully restore RenderWare states cleanly so crosshair, fonts, and HUD render properly
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, 0);
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, 0);
        RwRenderStateSet(rwRENDERSTATESHADEMODE, (void*)rwSHADEMODEFLAT);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Hooked HUD Draw callback called inside Render2DStuff (0x0053E4FF)
static void HookedHudDraw()
{
    if (g_OrigHudDraw) {
        g_OrigHudDraw();
    }
    DrawDamageIndicators();
}

// --- Initialization & Hook Installation ---
static void InstallHooks()
{
    // Sanity check: GTA San Andreas v1.0 US (Hoodlum)
    // 0x0053E4FF: call CHud::Draw (0xE8)
    // 0x004B5AC0: CPedDamageResponseCalculator::ComputeDamageResponse (0x64 = mov eax, fs:[0])
    if (*(uint8_t*)0x0053E4FF != 0xE8 || *(uint8_t*)0x004B5AC0 != 0x64) {
        return;
    }

    DWORD oldProt;

    // 1. Hook Damage Response (0x004B5AC0)
    VirtualProtect((void*)0x004B5AC0, 6, PAGE_EXECUTE_READWRITE, &oldProt);
    *(uint8_t*)0x004B5AC0 = 0xE9; // JMP Hook_ComputeDamageResponse
    *(int32_t*)0x004B5AC1 = (int32_t)((uintptr_t)Hook_ComputeDamageResponse - (0x004B5AC0 + 5));
    *(uint8_t*)0x004B5AC5 = 0x90; // NOP
    VirtualProtect((void*)0x004B5AC0, 6, oldProt, &oldProt);

    // 2. Hook HUD Draw call site inside Render2DStuff (0x0053E4FF)
    VirtualProtect((void*)0x0053E4FF, 5, PAGE_EXECUTE_READWRITE, &oldProt);
    int32_t origRel = *(int32_t*)0x0053E500;
    g_OrigHudDraw = (HudDrawFn)(0x0053E504 + origRel);
    *(int32_t*)0x0053E500 = (int32_t)((uintptr_t)HookedHudDraw - 0x0053E504);
    VirtualProtect((void*)0x0053E4FF, 5, oldProt, &oldProt);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        LoadConfig(hinstDLL);
        InstallHooks();
    }
    return TRUE;
}
