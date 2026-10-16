#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cmath>
#include <algorithm>

// GTA Vice City v1.0 Addresses
#define ADDR_PLAYERS_ARRAY       0x0094AD28  // CWorld::Players[0].m_pPed
#define ADDR_THE_CAMERA          0x007E4688  // CCamera TheCamera
#define ADDR_RS_GLOBAL_WIDTH     0x009B48DC  // RsGlobal.maximumWidth
#define ADDR_RS_GLOBAL_HEIGHT    0x009B48E0  // RsGlobal.maximumHeight
#define ADDR_NEAR_SCREEN_Z       0x0097F05C  // CSprite2d::NearScreenZ
#define ADDR_RECIP_NEAR_CLIP     0x0094DB54  // CSprite2d::RecipNearClip
#define ADDR_HUD_DRAW_CALL       0x004A64D0  // call CHud::Draw inside Render2DStuff
#define ADDR_INFLICT_DAMAGE      0x00525B20  // CPed::InflictDamage()
#define ADDR_INFLICT_JUMP_BACK   0x00525B26  // CPed::InflictDamage() + 6 bytes

// RenderWare Addresses & Constants
#define ADDR_RW_RENDER_STATE_SET 0x00649BA0
#define ADDR_RW_IM2D_RENDER_PRIM 0x00649C10

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

typedef int(__cdecl* tRwRenderStateSet)(int state, void* value);
typedef int(__cdecl* tRwIm2DRenderPrimitive)(int primType, RwIm2DVertex* vertices, int numVertices);

static tRwRenderStateSet RwRenderStateSet = (tRwRenderStateSet)ADDR_RW_RENDER_STATE_SET;
static tRwIm2DRenderPrimitive RwIm2DRenderPrimitive = (tRwIm2DRenderPrimitive)ADDR_RW_IM2D_RENDER_PRIM;

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

// Damage Event
struct DamageHit {
    DWORD startTime;
    DWORD duration;
    float sourceX;
    float sourceY;
    bool  hasWorldPos;
    float intensity;
    bool  active;
};

static const int MAX_HITS = 16;
static DamageHit g_Hits[MAX_HITS];
static int g_NextHitIdx = 0;

// Correct CEntity world position:
// CEntity has virtual functions, so vtable is at 0x00.
// CPlaceable (CMatrix) starts at 0x04.
// Matrix pos is at +0x30 in CMatrix, so in CEntity it is at 0x04 + 0x30 = 0x34!
static inline void GetEntityPos(uintptr_t pEntity, float& x, float& y) {
    if (!pEntity || pEntity < 0x100000) {
        x = y = 0.0f;
        return;
    }
    __try {
        uintptr_t pVeh = *(uintptr_t*)(pEntity + 0x3A8);
        bool inVeh = *(bool*)(pEntity + 0x3AC);
        if (inVeh && pVeh >= 0x100000) {
            x = *(float*)(pVeh + 0x34);
            y = *(float*)(pVeh + 0x38);
            return;
        }
        x = *(float*)(pEntity + 0x34);
        y = *(float*)(pEntity + 0x38);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        x = y = 0.0f;
    }
}

// Called from naked hook when CPed::InflictDamage is invoked on player
static void __stdcall OnInflictDamage(void* pVictim, void* pInflictor, float damage) {
    __try {
        if (!pVictim || damage <= 0.05f) return;

        DWORD now = GetTickCount();

        float px = 0.0f, py = 0.0f;
        GetEntityPos((uintptr_t)pVictim, px, py);

        bool hasPos = false;
        float ix = 0.0f, iy = 0.0f;

        if (pInflictor && (uintptr_t)pInflictor >= 0x100000 && pInflictor != pVictim) {
            GetEntityPos((uintptr_t)pInflictor, ix, iy);
            float dx = ix - px;
            float dy = iy - py;
            if (dx * dx + dy * dy > 0.04f) {
                hasPos = true;
            }
        }

        float intensity = (std::min)(1.0f, (std::max)(0.35f, damage / 20.0f));

        DamageHit& hit = g_Hits[g_NextHitIdx];
        hit.startTime = now;
        hit.duration = 750;
        hit.sourceX = ix;
        hit.sourceY = iy;
        hit.hasWorldPos = hasPos;
        hit.intensity = intensity;
        hit.active = true;

        g_NextHitIdx = (g_NextHitIdx + 1) % MAX_HITS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static const uintptr_t JUMP_BACK_ADDR = ADDR_INFLICT_JUMP_BACK;

__declspec(naked) void Hook_InflictDamage() {
    __asm {
        pushad
        pushfd

        // Check if victim (ECX) is Tommy
        mov eax, ds:[ADDR_PLAYERS_ARRAY]
        cmp ecx, eax
        jne skip_hook

        // Stack layout after pushad (32 bytes) + pushfd (4 bytes):
        // [esp + 36] = return address to caller
        // [esp + 40] = pInflictor
        // [esp + 44] = weaponType
        // [esp + 48] = damage
        push dword ptr [esp + 48] // damage
        push dword ptr [esp + 44] // pInflictor (40 + 4 pushed)
        push ecx                  // pVictim
        call OnInflictDamage

    skip_hook:
        popfd
        popad

        // Execute original 6 bytes: fld dword ptr ds:[0x00694170]
        fld dword ptr ds:[0x00694170]

        // Jump back to 0x00525B26
        jmp dword ptr ds:[JUMP_BACK_ADDR]
    }
}

// Render full rectangular screen-edge gradients (NO diagonal trapezoid corner cuts)
static void DrawEdgeGlow(float screenW, float screenH, float edgeW, float edgeH,
                         float aLeft, float aRight, float aTop, float aBottom,
                         float nearZ, float recipNearClip) {
    RwIm2DVertex quad[4];
    const uint8_t R = 215, G = 18, B = 18;

    // Top Edge: spans full screen width, fades downwards
    if (aTop > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)aTop, R, G, B);
        uint32_t colTrans  = MakeARGB(0, R, G, B);
        SetVertex(quad[0], 0.0f, 0.0f, colOpaque, nearZ, recipNearClip);
        SetVertex(quad[1], screenW, 0.0f, colOpaque, nearZ, recipNearClip);
        SetVertex(quad[2], screenW, edgeH, colTrans, nearZ, recipNearClip);
        SetVertex(quad[3], 0.0f, edgeH, colTrans, nearZ, recipNearClip);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Bottom Edge: spans full screen width, fades upwards
    if (aBottom > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)aBottom, R, G, B);
        uint32_t colTrans  = MakeARGB(0, R, G, B);
        SetVertex(quad[0], 0.0f, screenH - edgeH, colTrans, nearZ, recipNearClip);
        SetVertex(quad[1], screenW, screenH - edgeH, colTrans, nearZ, recipNearClip);
        SetVertex(quad[2], screenW, screenH, colOpaque, nearZ, recipNearClip);
        SetVertex(quad[3], 0.0f, screenH, colOpaque, nearZ, recipNearClip);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Left Edge: spans full screen height, fades rightwards
    if (aLeft > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)aLeft, R, G, B);
        uint32_t colTrans  = MakeARGB(0, R, G, B);
        SetVertex(quad[0], 0.0f, 0.0f, colOpaque, nearZ, recipNearClip);
        SetVertex(quad[1], edgeW, 0.0f, colTrans, nearZ, recipNearClip);
        SetVertex(quad[2], edgeW, screenH, colTrans, nearZ, recipNearClip);
        SetVertex(quad[3], 0.0f, screenH, colOpaque, nearZ, recipNearClip);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }

    // Right Edge: spans full screen height, fades leftwards
    if (aRight > 1.0f) {
        uint32_t colOpaque = MakeARGB((uint8_t)aRight, R, G, B);
        uint32_t colTrans  = MakeARGB(0, R, G, B);
        SetVertex(quad[0], screenW - edgeW, 0.0f, colTrans, nearZ, recipNearClip);
        SetVertex(quad[1], screenW, 0.0f, colOpaque, nearZ, recipNearClip);
        SetVertex(quad[2], screenW, screenH, colOpaque, nearZ, recipNearClip);
        SetVertex(quad[3], screenW - edgeW, screenH, colTrans, nearZ, recipNearClip);
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
    }
}

static void DrawDamageIndicator() {
    __try {
        uintptr_t pPlayer = *(uintptr_t*)ADDR_PLAYERS_ARRAY;
        if (!pPlayer || pPlayer < 0x100000) return;

        int width  = *(int*)ADDR_RS_GLOBAL_WIDTH;
        int height = *(int*)ADDR_RS_GLOBAL_HEIGHT;
        if (width <= 0 || height <= 0) return;

        DWORD now = GetTickCount();

        // Check if there are active hits
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

        float screenW = (float)width;
        float screenH = (float)height;
        float edgeW   = screenW * 0.16f;
        float edgeH   = screenH * 0.18f;

        float nearZ = *(float*)ADDR_NEAR_SCREEN_Z;
        float recipNearClip = *(float*)ADDR_RECIP_NEAR_CLIP;
        if (recipNearClip <= 0.00001f) recipNearClip = 1.0f;

        // Player position
        float px = 0.0f, py = 0.0f;
        GetEntityPos(pPlayer, px, py);

        // Active Camera 2D Forward Vector (Classic Axis & Vanilla compatible)
        uint8_t activeCam = *(uint8_t*)(ADDR_THE_CAMERA + 0x76);
        if (activeCam > 2) activeCam = 0;
        uintptr_t pCam = (ADDR_THE_CAMERA + 0x188) + (activeCam * 0x1CC);

        float camLookX = *(float*)(pCam + 0x168);
        float camLookY = *(float*)(pCam + 0x168 + 4);

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
            float fade = t * t; // Smooth quadratic decay
            float alpha = fade * 195.0f * g_Hits[i].intensity;

            if (g_Hits[i].hasWorldPos) {
                float dx = g_Hits[i].sourceX - px;
                float dy = g_Hits[i].sourceY - py;

                // Project attacker onto Camera Forward and Right
                float fwd   = dx * camLookX  + dy * camLookY;
                float right = dx * camRightX + dy * camRightY;

                float len = sqrtf(fwd * fwd + right * right);
                if (len > 0.0001f) {
                    fwd /= len;
                    right /= len;
                }

                edgeTop    += (std::max)(0.0f, fwd)    * alpha;
                edgeBottom += (std::max)(0.0f, -fwd)   * alpha;
                edgeRight  += (std::max)(0.0f, right)  * alpha;
                edgeLeft   += (std::max)(0.0f, -right) * alpha;
            } else {
                // Non-directional damage (explosions, fall, fire)
                float nonDirAlpha = alpha * 0.7f;
                edgeTop    += nonDirAlpha;
                edgeBottom += nonDirAlpha;
                edgeRight  += nonDirAlpha;
                edgeLeft   += nonDirAlpha;
            }
        }

        edgeLeft   = (std::min)(210.0f, edgeLeft);
        edgeRight  = (std::min)(210.0f, edgeRight);
        edgeTop    = (std::min)(210.0f, edgeTop);
        edgeBottom = (std::min)(210.0f, edgeBottom);

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

static uintptr_t g_OrigHudDraw = 0;

static void Hooked_HudDraw() {
    if (g_OrigHudDraw) {
        ((void(__cdecl*)())g_OrigHudDraw)(); // Call original hook target (Classic Axis crosshair / CHud::Draw)
    }
    DrawDamageIndicator();
}

static void InstallHooks() {
    DWORD oldProtect;

    // 1. Hook CHud::Draw call at 0x004A64D0 dynamically preserving whatever was hooked before us
    uintptr_t hudCallAddr = ADDR_HUD_DRAW_CALL;
    VirtualProtect((void*)hudCallAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    int32_t relOffset = *(int32_t*)(hudCallAddr + 1);
    g_OrigHudDraw = hudCallAddr + 5 + relOffset;

    int32_t newRelOffset = (int32_t)((uintptr_t)Hooked_HudDraw - (hudCallAddr + 5));
    *(int32_t*)(hudCallAddr + 1) = newRelOffset;
    VirtualProtect((void*)hudCallAddr, 5, oldProtect, &oldProtect);

    // 2. Hook CPed::InflictDamage at 0x00525B20 (overwrite 6 bytes with JMP + NOP)
    uintptr_t damageAddr = ADDR_INFLICT_DAMAGE;
    VirtualProtect((void*)damageAddr, 6, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(uint8_t*)damageAddr = 0xE9; // JMP rel32
    *(int32_t*)(damageAddr + 1) = (int32_t)((uintptr_t)Hook_InflictDamage - (damageAddr + 5));
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
