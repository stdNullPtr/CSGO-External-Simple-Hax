#include <thread>
#include <iostream>
#include <vector>
#include "CSGOMemory.hpp"
#include "Offsets.hpp"
#include "WeaponType.hpp"
#include "VectorUtils.hpp"

#define M_PI 3.14159265358979323846
#define M_RADPI (180.0 / M_PI)
#define M_DEGRAD (M_PI / 180.0)

using namespace offsets::netvars;
using namespace offsets::signatures;
using namespace csgo_memory;

bool g_whToggle = false;

#pragma region AimBot

const float MINFOV = 2.0f; //3
const int BONEID = 8;	   //8 - head
const float SMOOTH = 20;   //40

#pragma endregion

#pragma region Structs

struct BoneStruct
{
    char _pad1[0x0C];
    float x;
    char _pad2[0x0C];
    float y;
    char _pad3[0x0C];
    float z;
};

struct GlowStruct
{
    float R;
    float G;
    float B;
    float m_flGlowAlpha;
    char m_unk[4];
    float m_flUnk;
    float m_flBloomAmount;
    float m_flUnk1;
    bool m_bRenderWhenOccluded;
    bool m_bRenderWhenUnoccluded;
    bool m_bFullBloomRender;
    char m_unk1;
    int m_nFullBloomStencilTestValue;
    int m_nGlowStyle;
    int m_nSplitScreenSlot;
    int m_nNextFreeSlot;
}g_glowStr;

struct PlayerStruct
{
    char pad_0000[244]; //0x0000
    int32_t m_iTeamNum; //0x00F4
    char pad_00F8[8]; //0x00F8
    int32_t m_iHealth; //0x0100
    char pad_0104[4]; //0x0104
    Vector m_vecViewOffset; //0x0108
    char pad_0114[36]; //0x0114
    Vector m_vecOrigin; //0x0138
};

struct Player
{
    PlayerStruct m_plrData;
    uint32_t m_dwBase = 0;
    uint32_t m_glowIndex = 0;

    bool Update(int p, const CSGOMemory& mem)
    {
        auto tempEnt = mem.Read<uint32_t>(mem.GetClientBase() + dwEntityList + ((p - 1) * 0x10));

        if (tempEnt.has_value() && tempEnt.value() != 0x0)
        {
            m_dwBase = tempEnt.value();

            m_glowIndex = mem.Read<uint32_t>(m_dwBase + m_iGlowIndex).value();
            m_plrData = mem.Read<PlayerStruct>(m_dwBase).value();

            return true;
        }

        return false;
    }

    Vector BonePos(int boneId, const CSGOMemory& mem)
    {
        uint32_t boneBase = *mem.Read<uint32_t>(m_dwBase + m_dwBoneMatrix);
        uint32_t base = boneBase + (0x30 * boneId);
        BoneStruct boneVector = *mem.Read<BoneStruct>(base);

        Vector vBone;
        vBone.x = boneVector.x;
        vBone.y = boneVector.y;
        vBone.z = boneVector.z;

        return vBone;
    }

    int GetWeapon(const CSGOMemory& mem)
    {
        uint32_t weapon = *mem.Read<uint32_t>(m_dwBase + m_hActiveWeapon);
        uint32_t duck = weapon & 0xFFF;
        uint32_t horse = *mem.Read<uint32_t>(mem.GetClientBase() + dwEntityList + (duck - 1) * 0x10);
        return *mem.Read<int>(horse + m_iItemDefinitionIndex);
    }

    void Glow(float R, float G, float B, const CSGOMemory& mem)
    {
        uint32_t glowStructBase = mem.GetGlowBase() + (m_glowIndex * 0x38) + 0x4;

        g_glowStr = *mem.Read<GlowStruct>(glowStructBase);

        g_glowStr.R = R;
        g_glowStr.G = G;
        g_glowStr.B = B;
        g_glowStr.m_flGlowAlpha = 0.7f;
        g_glowStr.m_bRenderWhenOccluded = true;
        g_glowStr.m_bRenderWhenUnoccluded = false;

        mem.Write<GlowStruct>(glowStructBase, g_glowStr);
    }
}g_localPlayer;
std::vector<Player> g_playerList;
#pragma endregion

void UpdateLocalPlayer(const CSGOMemory& mem)
{
    Player localPlayer;
    g_localPlayer = {};

    auto localPLayerPtr = mem.Read<uint32_t>(mem.GetClientBase() + dwLocalPlayer);
    if (localPLayerPtr.has_value() && (localPlayer.m_dwBase = localPLayerPtr.value()) != 0x0)
    {
        localPlayer.m_glowIndex = mem.Read<uint32_t>(localPlayer.m_dwBase + m_iGlowIndex).value();
        localPlayer.m_plrData = mem.Read<PlayerStruct>(localPlayer.m_dwBase).value();

        g_localPlayer = localPlayer;
    }
    else
    {
        std::cout << "WARN: Failed updating localPlayer!" << std::endl;
    }
}

#pragma region AimBot

Vector GetLocalEye()
{
    return g_localPlayer.m_plrData.m_vecOrigin + g_localPlayer.m_plrData.m_vecViewOffset;
}

Vector BonePos(int base, int boneId, const CSGOMemory& mem)
{
    uint32_t boneBase = *mem.Read<uint32_t>(base + m_dwBoneMatrix);

    uint32_t Base = boneBase + (0x30 * boneId);
    BoneStruct bv = *mem.Read<BoneStruct>(Base);

    Vector vBone;
    vBone.x = bv.x;
    vBone.y = bv.y;
    vBone.z = bv.z;

    return vBone;
}

Vector CalculateAngle(Vector delta)
{
    Vector returnVec;

    returnVec.x = (float)(asinf(delta.z / GetMagnitude(delta)) * M_RADPI); //yaw
    returnVec.y = (float)(atanf(delta.y / delta.x) * M_RADPI);	           //pitch
    returnVec.z = 0.0f;

    if (delta.x >= 0.0)
    {
        returnVec.y += 180.0f;
    }

    return returnVec;
}

void NormalizeAngles(Vector* angles)
{
    while (angles->x < -180.0f) angles->x += 360.0f;
    while (angles->x > 180.0f) angles->x -= 360.0f;

    while (angles->y < -180.0f) angles->y += 360.0f;
    while (angles->y > 180.0f) angles->y -= 360.0f;
}

void ClampAngles(Vector* angles)
{
    if (angles->x > 89.0f) angles->x = 89.0f;
    else if (angles->x < -89.0f) angles->x = -89.0f;

    if (angles->y > 180.0f) angles->y = 180.0f;
    else if (angles->y < -180.0f) angles->y = -180.0f;

    angles->z = 0.0f;
}

float GetFOV(const Vector& src, const Vector& dest)
{
    Vector delta = { src.x - dest.x, src.y - dest.y, src.z - dest.z };
    NormalizeAngles(&delta);
    return sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

void GetClosestEnt(const uint32_t& clientState, const Vector& currentAng, int& index, const CSGOMemory& mem)
{
    UpdateLocalPlayer(mem);

    for (size_t i = 0; i < g_playerList.size(); i++)
    {
        g_playerList.at(i).Update(i + 1, mem);

        if (g_playerList.at(i).m_plrData.m_iTeamNum == g_localPlayer.m_plrData.m_iTeamNum)
            continue;
        if (g_playerList.at(i).m_plrData.m_iHealth < 1)
            continue;

        Vector enemyHeadPos = g_playerList.at(i).BonePos(8, mem);

        Vector localEye = GetLocalEye();

        Vector delta = localEye - enemyHeadPos;

        Vector finalAng = CalculateAngle(delta);

        float fov = GetFOV(currentAng, finalAng);

        if (fov < MINFOV)
        {
            index = i;
        }
    }
}

void AimLoop(const CSGOMemory& mem)
{
    if (!GetAsyncKeyState(VK_MBUTTON)) return;

    uint32_t clientState = *mem.Read<uint32_t>(mem.GetEngineBase() + dwClientState);
    Vector currentAng = *mem.Read<Vector>(clientState + dwClientState_ViewAngles);

    int index = -1;
    GetClosestEnt(clientState, currentAng, index, mem);

    if (index == -1)
        return;

    Vector enemyHeadPos = g_playerList.at(index).BonePos(BONEID, mem);
    Vector delta = GetLocalEye() - enemyHeadPos;
    Vector finalAng = CalculateAngle(delta);

    float deltaX = finalAng.x - currentAng.x;
    float deltaY = finalAng.y - currentAng.y;

    if (deltaX < -180.0f) deltaX += 360.0f;
    if (deltaX > 180.0f) deltaX -= 360.0f;

    if (deltaY < -180.0f) deltaY += 360.0f;
    if (deltaY > 180.0f) deltaY -= 360.0f;

    float smoothX = deltaX / SMOOTH;
    float smoothY = deltaY / SMOOTH;

    int loop = 0;
    while (loop < 10)
    {
        NormalizeAngles(&currentAng);
        ClampAngles(&currentAng);
        mem.Write<Vector>(clientState + dwClientState_ViewAngles, currentAng);

        loop++;

        currentAng.x += smoothX;
        currentAng.y += smoothY;
    }
}

#pragma endregion

#pragma region WallHack

void WallLoop(const CSGOMemory& mem)
{
    if (GetAsyncKeyState(VK_INSERT))
    {
        g_whToggle = !g_whToggle;
        if (g_whToggle) std::cout << "Walls On" << std::endl;
        else std::cout << "Walls Off" << std::endl;
        Sleep(500);
    }

    if (!g_whToggle) return;

    for (size_t i = 0; i < g_playerList.size(); i++)
    {
        if (g_playerList.at(i).m_plrData.m_iTeamNum == g_localPlayer.m_plrData.m_iTeamNum)
            continue;
        if (g_playerList.at(i).m_plrData.m_iHealth < 1)
            continue;

        int weaponType = g_playerList.at(i).GetWeapon(mem);

        if (weaponType == WEAPON_AWP)
        {
            g_playerList.at(i).Glow(0, 0, 1, mem); //blue
        }
        else if ((weaponType >= WEAPON_KNIFE && weaponType <= WEAPON_KNIFE_T) || weaponType >= WEAPON_KNIFE_BAYONET) //non-shooting weaps
        {
            g_playerList.at(i).Glow(0, 1, 0, mem); //green
        }
        else
        {
            g_playerList.at(i).Glow(1, 0, 0, mem); //red	
        }
    }
}

#pragma endregion

void UpdatePlayerList(const CSGOMemory& mem)
{
    g_playerList.clear();
    g_playerList.resize(0);

    UpdateLocalPlayer(mem);

    ///TODO: find a way to loop through max players
    // update players start from 1 since world is at 0
    // the loop will stop when we stop reading players
    for (size_t i = 1; i < 64; i++)
    {
        Player player;
        // we reached an entity that is not a player
        if (!player.Update(i, mem))
            continue;

        g_playerList.push_back(player);
    }
}

void MainLoop(const CSGOMemory& mem)
{
    std::cout << "Started main loop, press x to exit" << std::endl;

    UpdatePlayerList(mem);

    int cacheTimer = 1337;
    while (true)
    {
        // exit the cheat when we press 'x'
        if (GetAsyncKeyState(0x58 /*x*/))
            break;

        if (cacheTimer++ >= 200)
        {
            UpdatePlayerList(mem);
            cacheTimer = 0;
        }

        WallLoop(mem);
        AimLoop(mem);

        Sleep(1);
    }
}

int main()
{
    try
    {
        CSGOMemory mem(L"csgo.exe", memory::SafeMemory_AllAccess, CSGOMemory::ConstructProcessName());

        std::cout << "Initialized!" << std::endl;

        MainLoop(mem);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

//void DisableFlash()
//{
//	Mem->Write<float>(LocalPlayer.Local + m_flFlashDuration, 0); //flash
//}