#include <thread>
#include <iostream>
#include <vector>
#include <algorithm>
#include "CSGOMemory.hpp"
#include "Offsets.hpp"
#include "WeaponType.hpp"
#include "VectorUtils.hpp"

using namespace offsets::netvars;
using namespace offsets::signatures;
using namespace csgo_memory;

const double g_pi = 3.14159265358979323846;
const double g_radPi = 180.0 / g_pi;

bool g_whToggle = false;

#pragma region AimBot

const float g_minFov = 2.0f; //3
const int g_boneId = 8;	     //8 - head
const float g_smooth = 20;   //40

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
    BYTE _pad_0x0[8];
    float red;
    float green;
    float blue;
    float alpha;
    BYTE buffer[16];
    bool renderWhenOccluded;
    bool renderWhenUnOccluded;
    bool fullBloom;
    BYTE buffer1[5];
    int glowStyle;
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
    PlayerStruct m_plrData{};
    uint32_t m_dwBase = 0;
    uint32_t m_glowIndex = 0;

    bool Update(const uint32_t index, const CSGOMemory& mem)
    {
        auto player = mem.Read<uint32_t>(mem.GetClientBase() + dwEntityList + ((index - 1) * 0x10));

        if (player != std::nullopt && player != 0x0)
        {
            m_dwBase = player.value();

            m_glowIndex = mem.Read<uint32_t>(m_dwBase + m_iGlowIndex).value();
            m_plrData = mem.Read<PlayerStruct>(m_dwBase).value();

            return true;
        }

        return false;
    }

    Vector BonePos(const int boneId, const CSGOMemory& mem) const
    {
        const auto boneBase = mem.Read<uint32_t>(m_dwBase + m_dwBoneMatrix);
        if (boneBase == std::nullopt || boneBase == 0x0)
            throw std::exception("'boneBase' not found!");

        const uint32_t base = boneBase.value() + (0x30 * boneId);
        const BoneStruct boneVector = *mem.Read<BoneStruct>(base);

        Vector vBone;
        vBone.x = boneVector.x;
        vBone.y = boneVector.y;
        vBone.z = boneVector.z;

        return vBone;
    }

    int GetWeapon(const CSGOMemory& mem) const
    {
        const uint32_t weapon = *mem.Read<uint32_t>(m_dwBase + m_hActiveWeapon);
        const uint32_t duck = weapon & 0xFFF;
        const uint32_t horse = *mem.Read<uint32_t>(mem.GetClientBase() + dwEntityList + (duck - 1) * 0x10);

        return mem.Read<int>(horse + m_iItemDefinitionIndex).value_or(0);
    }

    void Glow(const float r, const float g, const float b, const CSGOMemory& mem) const
    {
        const uint32_t glowStructBase = mem.GetGlowBase() + (m_glowIndex * 0x38);

        g_glowStr = *mem.Read<GlowStruct>(glowStructBase);

        g_glowStr.red = r;
        g_glowStr.green = g;
        g_glowStr.blue = b;
        g_glowStr.alpha = 0.7f;
        g_glowStr.renderWhenOccluded = true;
        g_glowStr.renderWhenUnOccluded = false;

        (void)mem.Write<GlowStruct>(glowStructBase, g_glowStr);
    }
}g_localPlayer; // TODO: tidy up global variables across the project
std::vector<Player> g_playerList;
#pragma endregion

void UpdateLocalPlayer(const CSGOMemory& mem)
{
    g_localPlayer = {};
    Player newLocalPlayer;

    auto localPlayer = mem.Read<uint32_t>(mem.GetClientBase() + dwLocalPlayer);
    if (localPlayer != std::nullopt && localPlayer != 0x0)
    {
        newLocalPlayer.m_dwBase = localPlayer.value();
        newLocalPlayer.m_glowIndex = mem.Read<uint32_t>(newLocalPlayer.m_dwBase + m_iGlowIndex).value();
        newLocalPlayer.m_plrData = mem.Read<PlayerStruct>(newLocalPlayer.m_dwBase).value();

        g_localPlayer = newLocalPlayer;
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

Vector CalculateAngle(const Vector& delta)
{
    Vector returnVec;

    returnVec.x = static_cast<float>(asinf(delta.z / GetMagnitude(delta)) * g_radPi); //yaw
    returnVec.y = static_cast<float>(atanf(delta.y / delta.x) * g_radPi);	             //pitch
    returnVec.z = 0.0f;

    if (delta.x >= 0.0)
    {
        returnVec.y += 180.0f;
    }

    return returnVec;
}

void NormalizeAngles(Vector& angles)
{
    while (angles.x < -180.0f) angles.x += 360.0f;
    while (angles.x > 180.0f) angles.x -= 360.0f;

    while (angles.y < -180.0f) angles.y += 360.0f;
    while (angles.y > 180.0f) angles.y -= 360.0f;
}

void ClampAngles(Vector& angles)
{
    angles.x = std::clamp(angles.x, -89.0f, 89.0f);
    angles.y = std::clamp(angles.y, -180.0f, 180.0f);
    angles.z = 0.0f;
}

float GetFOV(const Vector& src, const Vector& dest)
{
    Vector delta = { src.x - dest.x, src.y - dest.y, src.z - dest.z };
    NormalizeAngles(delta);
    return sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

void GetClosestEnt(const Vector& currentAng, int& index, const CSGOMemory& mem)
{
    UpdateLocalPlayer(mem);

    for (size_t i = 0; i < g_playerList.size(); i++)
    {
        if (!g_playerList.at(i).Update(i + 1, mem))
            continue;

        if (g_playerList.at(i).m_plrData.m_iTeamNum == g_localPlayer.m_plrData.m_iTeamNum)
            continue;
        if (g_playerList.at(i).m_plrData.m_iHealth < 1)
            continue;

        Vector enemyHeadPos = g_playerList.at(i).BonePos(8, mem);

        Vector localEye = GetLocalEye();

        const Vector delta = localEye - enemyHeadPos;

        Vector finalAng = CalculateAngle(delta);

        const float fov = GetFOV(currentAng, finalAng);

        if (fov < g_minFov)
        {
            index = i;
        }
    }
}

void AimLoop(const CSGOMemory& mem)
{
    if (!GetAsyncKeyState(VK_MBUTTON /*middle mouse*/)) return;

    const uint32_t clientState = *mem.Read<uint32_t>(mem.GetEngineBase() + dwClientState);
    Vector currentAng = *mem.Read<Vector>(clientState + dwClientState_ViewAngles);

    int index = -1;
    GetClosestEnt(currentAng, index, mem);

    if (index == -1)
        return;

    const Vector enemyHeadPos = g_playerList.at(index).BonePos(g_boneId, mem);
    const Vector delta = GetLocalEye() - enemyHeadPos;
    const Vector finalAng = CalculateAngle(delta);

    float deltaX = finalAng.x - currentAng.x;
    float deltaY = finalAng.y - currentAng.y;

    if (deltaX < -180.0f) deltaX += 360.0f;
    if (deltaX > 180.0f) deltaX -= 360.0f;

    if (deltaY < -180.0f) deltaY += 360.0f;
    if (deltaY > 180.0f) deltaY -= 360.0f;

    const float smoothX = deltaX / g_smooth;
    const float smoothY = deltaY / g_smooth;

    int loop = 0;
    while (loop < 10)
    {
        NormalizeAngles(currentAng);
        ClampAngles(currentAng);
        (void)mem.Write<Vector>(clientState + dwClientState_ViewAngles, currentAng);

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

    for (auto& player : g_playerList)
    {
        if (player.m_plrData.m_iTeamNum == g_localPlayer.m_plrData.m_iTeamNum)
            continue;
        if (player.m_plrData.m_iHealth < 1)
            continue;

        const int weaponType = player.GetWeapon(mem);

        if (weaponType == WEAPON_AWP)
        {
            player.Glow(0, 0, 1, mem); //blue
        }
        else if ((weaponType >= WEAPON_KNIFE && weaponType <= WEAPON_KNIFE_T) || weaponType >= WEAPON_KNIFE_BAYONET) //non-shooting weapons
        {
            player.Glow(0, 1, 0, mem); //green
        }
        else
        {
            player.Glow(1, 0, 0, mem); //red	
        }
    }
}

#pragma endregion

void UpdatePlayerList(const CSGOMemory& mem)
{
    g_playerList.clear();
    g_playerList.resize(0);

    // localplayer is always first in the list vs bots, but in real games we have to separately update
    UpdateLocalPlayer(mem);

    // TODO: find a way to loop through max players
    // TODO: iterate through the list correctly -> it is a linked list -> https://github.com/ValveSoftware/source-sdk-2013/blob/master/mp/src/game/shared/entitylist_base.h#L20
    // update players start from 1 since world is at 0
    // the loop will stop when we stop reading players
    for (size_t i = 1; i < 32; i++)
    {
        Player player;
        // TODO: sometimes we fail reading an entity even though it should be a player???
        // we reached an entity that is not a player
        if (!player.Update(i, mem))
            continue;

        g_playerList.push_back(player);
    }
}

void MainLoop(const CSGOMemory& mem)
{
    std::cout << "Started main loop!\nF3 to exit\nINSERT to enable walls\nHold MIDDLE MOUSE to enter aimLoop" << std::endl;

    UpdatePlayerList(mem);

    int cacheTimer = 1337;
    while (true)
    {
        // exit the cheat when we press 'F3'
        if (GetAsyncKeyState(VK_F3))
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
        const CSGOMemory mem(L"csgo.exe", memory::SafeMemory_AllAccess, CSGOMemory::ConstructProcessName());

        std::cout << "Initialized!" << std::endl;

        MainLoop(mem);
    }
    catch (const std::system_error & e)
    {
        std::cout << "Caught system_error with code " << e.code()
            << " meaning " << e.what() << '\n';
    }
    catch (const std::exception & e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

//void DisableFlash()
//{
//	Mem->Write<float>(LocalPlayer.Local + m_flFlashDuration, 0); //flash
//}