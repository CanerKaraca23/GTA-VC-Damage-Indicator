#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cmath>
#include <algorithm>

// GTA Vice City v1.0 Addresses
#define ADDR_PLAYERS_ARRAY      0x0094AD28  // CWorld::Players[0].m_pPed
#define ADDR_CAM_SOURCE_X       0x007E4DB8  // Active Camera Source Pos X (World space)
#define ADDR_CAM_TARGET_X       0x007E4DC4  // Active Camera Target Pos X (World space)
#define ADDR_RS_GLOBAL_WIDTH    0x009B48E4  // RsGlobal.maximumWidth
#define ADDR_RS_GLOBAL_HEIGHT   0x009B48E8  // RsGlobal.maximumHeight
#define ADDR_NEAR_SCREEN_Z      0x0097F05C  // RwIm2DGetNearScreenZ()
#define ADDR_RECIP_NEAR_CLIP    0x0094DB54  // CSprite2d::RecipNearClip
#define ADDR_INFLICT_DAMAGE     0x00525B20  // CPed::InflictDamage()
#define ADDR_HUD_DRAW_CALL      0x004A64D0  // call CHud::Draw inside Render2DStuff

// RenderWare Addresses
#define ADDR_RW_RENDER_STATE_SET 0x00649BA0
#define ADDR_RW_IM2D_RENDER_PRIM 0x00649C10

// RenderWare Enums & Constants
#define rwRENDERSTATETEXTURERASTER      1
#define rwRENDERSTATEZTESTENABLE        6
#define rwRENDERSTATESHADEMODE          7
#define rwRENDERSTATEZWRITEENABLE       8
#define rwRENDERSTATESRCBLEND           10
#define rwRENDERSTATEDESTBLEND          11
#define rwRENDERSTATEVERTEXALPHAENABLE  12

#define rwBLENDSRCALPHA                 5
#define rwBLENDINVSRCALPHA              6
#define rwSHADEMODEGOURAUD              2

#define rwPRIMTYPETRILIST               4
#define rwPRIMTYPETRIFAN                5
#define rwPRIMTYPETRISTRIP              6

#define M_PI_F                          3.14159265358979323846f
#define M_2PI_F                         6.28318530717958647692f

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

typedef int(__cdecl* tRwRenderStateSet)(int state, void* value);
typedef int(__cdecl* tRwIm2DRenderPrimitive)(int primType, RwIm2DVertex* vertices, int numVertices);

tRwRenderStateSet RwRenderStateSet = (tRwRenderStateSet)ADDR_RW_RENDER_STATE_SET;
tRwIm2DRenderPrimitive RwIm2DRenderPrimitive = (tRwIm2DRenderPrimitive)ADDR_RW_IM2D_RENDER_PRIM;

static inline uint32_t MakeARGB(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline void Set2DVertex(RwIm2DVertex& v, float x, float y, uint32_t argb) {
    v.x = x;
    v.y = y;
    v.z = *(float*)ADDR_NEAR_SCREEN_Z;
    v.rhw = *(float*)ADDR_RECIP_NEAR_CLIP;
    v.color = argb;
    v.u = 0.0f;
    v.v = 0.0f;
}

// Damage Hit Event
struct DamageHit {
    DWORD startTime;
    DWORD duration;
    float relAngle;    // Screen relative angle: 0 = Top, +PI/2 = Right, -PI/2 = Left, PI = Bottom
    bool  hasDirection;
    float intensity;   // 0.0 to 1.0
    bool  active;
};

static const int MAX_HITS = 16;
static DamageHit g_Hits[MAX_HITS];
static int g_NextHitIdx = 0;

static inline void GetEntityPos(uintptr_t pEntity, float& x, float& y, float& z) {
    if (!pEntity || pEntity < 0x100000) {
        x = y = z = 0.0f;
        return;
    }
    // Check if ped in vehicle
    uintptr_t pVeh = *(uintptr_t*)(pEntity + 0x3A8);
    if (pVeh && pVeh >= 0x100000) {
        x = *(float*)(pVeh + 0x34);
        y = *(float*)(pVeh + 0x38);
        z = *(float*)(pVeh + 0x3C);
        return;
    }
    x = *(float*)(pEntity + 0x34);
    y = *(float*)(pEntity + 0x38);
    z = *(float*)(pEntity + 0x3C);
}

static void AddDamageHit(void* pPlayer, void* pInflictor, float damage) {
    DWORD now = GetTickCount();

    bool hasDir = false;
    float relAngle = 0.0f;

    if (pInflictor && (uintptr_t)pInflictor >= 0x100000 && pInflictor != pPlayer) {
        float px, py, pz;
        GetEntityPos((uintptr_t)pPlayer, px, py, pz);

        float ix, iy, iz;
        GetEntityPos((uintptr_t)pInflictor, ix, iy, iz);

        float dx = ix - px;
        float dy = iy - py;

        if (dx * dx + dy * dy > 0.01f) {
            // Camera line of sight from TheCamera.m_vecSource to TheCamera.m_vecTarget
            float camSourceX = *(float*)(ADDR_CAM_SOURCE_X);
            float camSourceY = *(float*)(ADDR_CAM_SOURCE_X + 4);
            float camTargetX = *(float*)(ADDR_CAM_TARGET_X);
            float camTargetY = *(float*)(ADDR_CAM_TARGET_X + 4);

            float camLookX = camTargetX - camSourceX;
            float camLookY = camTargetY - camSourceY;

            float camLen = sqrtf(camLookX * camLookX + camLookY * camLookY);
            if (camLen > 0.0001f) {
                camLookX /= camLen;
                camLookY /= camLen;
            } else {
                camLookX = 0.0f;
                camLookY = 1.0f;
            }

            // 2D Camera Right Vector (90 degrees clockwise from camLook)
            float camRightX = camLookY;
            float camRightY = -camLookX;

            // Project attacker vector onto Camera Forward and Right vectors
            float forwardDot = dx * camLookX + dy * camLookY;
            float rightDot   = dx * camRightX + dy * camRightY;

            // 2D screen angle:
            // 0       = Top (In front of camera)
            // +PI/2   = Right (To the right of camera)
            // -PI/2   = Left (To the left of camera)
            // +/-PI   = Bottom (Behind camera)
            relAngle = atan2f(rightDot, forwardDot);
            hasDir = true;
        }
    }

    float intensity = (std::min)(1.0f, (std::max)(0.4f, damage / 20.0f));

    DamageHit& hit = g_Hits[g_NextHitIdx];
    hit.startTime = now;
    hit.duration = 750; // 750ms duration
    hit.relAngle = relAngle;
    hit.hasDirection = hasDir;
    hit.intensity = intensity;
    hit.active = true;

    g_NextHitIdx = (g_NextHitIdx + 1) % MAX_HITS;
}

// Hooked CPed::InflictDamage
static uint8_t g_InflictDamageTrampoline[16];
typedef bool(__thiscall* tInflictDamage)(void* pThis, void* pInflictor, int weapon, float damage, int pedPiece, uint8_t unk);
tInflictDamage Orig_InflictDamage = (tInflictDamage)(void*)g_InflictDamageTrampoline;

static bool __fastcall Hooked_InflictDamage(void* pThis, void* edx, void* pInflictor, int weapon, float damage, int pedPiece, uint8_t unk) {
    __try {
        uintptr_t pPlayer = *(uintptr_t*)ADDR_PLAYERS_ARRAY;
        if (pThis && (uintptr_t)pThis == pPlayer && damage > 0.05f) {
            AddDamageHit(pThis, pInflictor, damage);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    return Orig_InflictDamage(pThis, pInflictor, weapon, damage, pedPiece, unk);
}

// Render the GTA V Center Curved Reticle Damage Arc
static void DrawDamageArc(float cx, float cy, float relAngle, float alpha, float radius, float thickness) {
    const int NUM_SEGS = 16;
    const int NUM_VERTS = (NUM_SEGS + 1) * 2;
    RwIm2DVertex verts[NUM_VERTS];

    float halfSpan = 28.0f * (M_PI_F / 180.0f); // 28 degrees half-span
    float rIn = radius - thickness * 0.5f;
    float rOut = radius + thickness * 0.5f;

    for (int i = 0; i <= NUM_SEGS; ++i) {
        float u = (float)i / (float)NUM_SEGS; // 0.0 to 1.0
        float angle = (relAngle - halfSpan) + (halfSpan * 2.0f * u);

        // Smooth bell curve: 0 at tips, 1 at center
        float tipFade = sinf(u * M_PI_F);
        tipFade = tipFade * tipFade;

        uint8_t vAlpha = (uint8_t)(std::min)(240.0f, alpha * tipFade);
        uint32_t col = MakeARGB(vAlpha, 235, 25, 25);

        float sinA = sinf(angle);
        float cosA = cosf(angle);

        // Screen coordinates:
        // relAngle = 0 -> Top (-Y)
        // relAngle = +PI/2 -> Right (+X)
        // relAngle = -PI/2 -> Left (-X)
        // relAngle = +/-PI -> Bottom (+Y)
        Set2DVertex(verts[i * 2 + 0], cx + rIn * sinA, cy - rIn * cosA, col);
        Set2DVertex(verts[i * 2 + 1], cx + rOut * sinA, cy - rOut * cosA, col);
    }

    RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, verts, NUM_VERTS);
}

// Render Smooth Screen-Edge Gradient with Corner Tapering (Clean, non-spilling)
static void DrawEdgeGradient(float screenW, float screenH, float edgeW, float edgeH,
                             float aLeft, float aRight, float aTop, float aBottom) {
    RwIm2DVertex quad[4];

    // Left Edge
    if (aLeft > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)(std::min)(220.0f, aLeft), 220, 20, 20);
        uint32_t colTrans  = MakeARGB(0, 220, 20, 20);

        Set2DVertex(quad[0], 0.0f, edgeH * 0.5f, colOpaque);
        Set2DVertex(quad[1], edgeW, edgeH, colTrans);
        Set2DVertex(quad[2], edgeW, screenH - edgeH, colTrans);
        Set2DVertex(quad[3], 0.0f, screenH - edgeH * 0.5f, colOpaque);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Right Edge
    if (aRight > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)(std::min)(220.0f, aRight), 220, 20, 20);
        uint32_t colTrans  = MakeARGB(0, 220, 20, 20);

        Set2DVertex(quad[0], screenW - edgeW, edgeH, colTrans);
        Set2DVertex(quad[1], screenW, edgeH * 0.5f, colOpaque);
        Set2DVertex(quad[2], screenW, screenH - edgeH * 0.5f, colOpaque);
        Set2DVertex(quad[3], screenW - edgeW, screenH - edgeH, colTrans);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Top Edge
    if (aTop > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)(std::min)(220.0f, aTop), 220, 20, 20);
        uint32_t colTrans  = MakeARGB(0, 220, 20, 20);

        Set2DVertex(quad[0], edgeW * 0.5f, 0.0f, colOpaque);
        Set2DVertex(quad[1], screenW - edgeW * 0.5f, 0.0f, colOpaque);
        Set2DVertex(quad[2], screenW - edgeW, edgeH, colTrans);
        Set2DVertex(quad[3], edgeW, edgeH, colTrans);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Bottom Edge
    if (aBottom > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)(std::min)(220.0f, aBottom), 220, 20, 20);
        uint32_t colTrans  = MakeARGB(0, 220, 20, 20);

        Set2DVertex(quad[0], edgeW, screenH - edgeH, colTrans);
        Set2DVertex(quad[1], screenW - edgeW, screenH - edgeH, colTrans);
        Set2DVertex(quad[2], screenW - edgeW * 0.5f, screenH, colOpaque);
        Set2DVertex(quad[3], edgeW * 0.5f, screenH, colOpaque);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }
}

// Main Render Routine
static void DrawDamageIndicator() {
    __try {
        uintptr_t pPlayer = *(uintptr_t*)ADDR_PLAYERS_ARRAY;
        if (!pPlayer || pPlayer < 0x100000) return;

        int width = *(int*)ADDR_RS_GLOBAL_WIDTH;
        int height = *(int*)ADDR_RS_GLOBAL_HEIGHT;
        if (width <= 0 || height <= 0) return;

        float screenW = (float)width;
        float screenH = (float)height;
        float cx = screenW * 0.5f;
        float cy = screenH * 0.5f;

        float arcRadius = screenH * 0.12f;     // ~80-130px radius around reticle
        float arcThickness = screenH * 0.032f; // ~20-35px arc thickness

        float edgeW = screenW * 0.15f;
        float edgeH = screenH * 0.18f;

        DWORD now = GetTickCount();

        float edgeLeft = 0.0f, edgeRight = 0.0f, edgeTop = 0.0f, edgeBottom = 0.0f;

        // Set RenderWare States for True Smooth Alpha Blending & Gouraud Shading
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, 0);
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, 0);
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, 0);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
        RwRenderStateSet(rwRENDERSTATESHADEMODE, (void*)rwSHADEMODEGOURAUD);

        for (int i = 0; i < MAX_HITS; ++i) {
            if (!g_Hits[i].active) continue;

            DWORD elapsed = now - g_Hits[i].startTime;
            if (elapsed >= g_Hits[i].duration) {
                g_Hits[i].active = false;
                continue;
            }

            float t = 1.0f - ((float)elapsed / (float)g_Hits[i].duration);
            float fade = t * t; // Smooth quadratic decay

            float alpha = fade * 230.0f * g_Hits[i].intensity;

            if (g_Hits[i].hasDirection) {
                // 1. Draw GTA V Center Curved Arc Indicator
                DrawDamageArc(cx, cy, g_Hits[i].relAngle, alpha, arcRadius, arcThickness);

                // 2. Accumulate directional screen-edge glow
                float fwd = cosf(g_Hits[i].relAngle);
                float side = sinf(g_Hits[i].relAngle);

                float wT = (std::max)(0.0f, fwd);
                float wB = (std::max)(0.0f, -fwd);
                float wR = (std::max)(0.0f, side);
                float wL = (std::max)(0.0f, -side);

                float edgeAlpha = alpha * 0.55f;
                edgeTop    += wT * edgeAlpha;
                edgeBottom += wB * edgeAlpha;
                edgeRight  += wR * edgeAlpha;
                edgeLeft   += wL * edgeAlpha;
            } else {
                // Non-directional damage (all edges)
                float nonDirAlpha = alpha * 0.45f;
                edgeTop    += nonDirAlpha;
                edgeBottom += nonDirAlpha;
                edgeRight  += nonDirAlpha;
                edgeLeft   += nonDirAlpha;
            }
        }

        // Low Health Blood Vignette (when health < 25)
        float health = *(float*)(pPlayer + 0x354);
        if (health > 0.0f && health < 25.0f) {
            float pulse = (sinf((float)now * 0.006f) + 1.0f) * 0.5f;
            float vignetteAlpha = (1.0f - (health / 25.0f)) * (35.0f + 30.0f * pulse);
            edgeLeft   += vignetteAlpha;
            edgeRight  += vignetteAlpha;
            edgeTop    += vignetteAlpha;
            edgeBottom += vignetteAlpha;
        }

        // Draw Soft Screen-Edge Glow
        DrawEdgeGradient(screenW, screenH, edgeW, edgeH, edgeLeft, edgeRight, edgeTop, edgeBottom);

        // Restore default render states
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

// Hook CHud::Draw
static uintptr_t g_OrigHudDraw = 0;
static void __cdecl Hooked_HudDraw() {
    ((void(__cdecl*)())g_OrigHudDraw)();
    DrawDamageIndicator();
}

// Installation
static void InstallHooks() {
    // 1. Hook CHud::Draw call at 0x004A64D0
    uintptr_t hudCallAddr = ADDR_HUD_DRAW_CALL;
    DWORD oldProtect;
    VirtualProtect((void*)hudCallAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    int32_t relOffset = *(int32_t*)(hudCallAddr + 1);
    g_OrigHudDraw = hudCallAddr + 5 + relOffset;

    int32_t newRelOffset = (int32_t)((uintptr_t)Hooked_HudDraw - (hudCallAddr + 5));
    *(int32_t*)(hudCallAddr + 1) = newRelOffset;
    VirtualProtect((void*)hudCallAddr, 5, oldProtect, &oldProtect);

    // 2. Hook CPed::InflictDamage at 0x00525B20
    uintptr_t damageAddr = ADDR_INFLICT_DAMAGE;
    VirtualProtect(g_InflictDamageTrampoline, sizeof(g_InflictDamageTrampoline), PAGE_EXECUTE_READWRITE, &oldProtect);

    // Copy original 6 bytes (D9 05 70 41 69 00 -> fld dword ptr [0x694170])
    memcpy(g_InflictDamageTrampoline, (void*)damageAddr, 6);

    // Jump back to 0x00525B26
    g_InflictDamageTrampoline[6] = 0xE9;
    *(int32_t*)&g_InflictDamageTrampoline[7] = (int32_t)((damageAddr + 6) - ((uintptr_t)&g_InflictDamageTrampoline[6] + 5));

    // Place jump to Hooked_InflictDamage at 0x00525B20
    VirtualProtect((void*)damageAddr, 6, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(uint8_t*)damageAddr = 0xE9;
    *(int32_t*)(damageAddr + 1) = (int32_t)((uintptr_t)Hooked_InflictDamage - (damageAddr + 5));
    *(uint8_t*)(damageAddr + 5) = 0x90; // NOP
    VirtualProtect((void*)damageAddr, 6, oldProtect, &oldProtect);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        InstallHooks();
    }
    return TRUE;
}
