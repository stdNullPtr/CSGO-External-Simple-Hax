#include <thread>
#include <iostream>
#include "Memory.hpp"
#include "Offsets.hpp"
#include "WeaponType.hpp"
#include "Util_Vector.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#define M_RADPI (180.0 / M_PI)
#define M_DEGRAD (M_PI / 180.0)

bool WhToggled = false;

DWORD ClientBase = 0;
DWORD EngineBase = 0;

#pragma region Misc

const DWORD SLEEP = 2;
const int PLAYERCNT = 32;

#pragma endregion


#pragma region AimBot

const float MINFOV = 2.0f; //3
const int BONEID = 8;	   //8 - head
const float SMOOTH = 20;   //40

#pragma endregion

#pragma region Structs

struct LocalPlayer
{
    DWORD Local;
    int Team;
    int Health;
    int InCross;
    void Read(win_raii::SafeMemory* mem)
    {
        Local = *mem->SafeReadMemory<DWORD>(ClientBase + dwLocalPlayer);

        if (Local != NULL)
        {
            Team = *mem->SafeReadMemory<int>(Local + m_iTeamNum);
            Health = *mem->SafeReadMemory<int>(Local + m_iHealth);
            InCross = *mem->SafeReadMemory<int>(Local + m_iCrosshairId);
        }
    }
    Vector GetLocalEye(win_raii::SafeMemory* mem)
    {
        Vector origin = *mem->SafeReadMemory<Vector>(Local + m_vecOrigin);
        Vector viewOffset = *mem->SafeReadMemory<Vector>(Local + m_vecViewOffset);
        return origin + viewOffset;
    }
}LocalPlayer;

struct Player
{
    DWORD Entity;
    int Team;
    int Health;
    DWORD GlowIndex;
    void Read(int p, win_raii::SafeMemory* mem)
    {
        Entity = *mem->SafeReadMemory<DWORD>(ClientBase + dwEntityList + ((p - 1) * 0x10));
        //Checking if it is a valid entity
        if (Entity != NULL)
        {
            GlowIndex = *mem->SafeReadMemory<DWORD>(Entity + m_iGlowIndex);
            Team = *mem->SafeReadMemory<int>(Entity + m_iTeamNum);
            Health = *mem->SafeReadMemory<int>(Entity + m_iHealth);
        }
    }
}PlayerList[PLAYERCNT];

#pragma endregion

#pragma region AimBot

Vector BonePos(int base, int boneId, win_raii::SafeMemory* mem)
{
    DWORD boneBase;
    boneBase = *mem->SafeReadMemory<DWORD>(base + m_dwBoneMatrix);

    Vector vBone;
    DWORD Base = boneBase + (0x30 * boneId);

    /// TODO: Read all at once
    vBone.x = *mem->SafeReadMemory<float>(Base + 0x0C);
    vBone.y = *mem->SafeReadMemory<float>(Base + 0x1C);
    vBone.z = *mem->SafeReadMemory<float>(Base + 0x2C);

    return vBone;
}

Vector CalculateAngle(Vector delta)
{
    Vector returnVec;

    returnVec.x = (float)(asinf(delta.z / GetMagnitude(delta)) * M_RADPI); //yaw
    returnVec.y = (float)(atanf(delta.y / delta.x) * M_RADPI);	           //pitch
    returnVec.z = 0.0f;

    if (delta.x >= 0.0) { returnVec.y += 180.0f; }

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

float GetFOV(const Vector& Src, const Vector& Dst)
{
    Vector Delta = { Src.x - Dst.x, Src.y - Dst.y, Src.z - Dst.z };
    NormalizeAngles(&Delta);
    return sqrt(Delta.x * Delta.x + Delta.y * Delta.y + Delta.z * Delta.z);
}

int GetClosestEnt(DWORD clientState, win_raii::SafeMemory* mem)
{
    int index = -1;

    for (int i = 1; i <= PLAYERCNT; i++)
    {
        PlayerList[i].Read(i, mem);

        if (PlayerList[i].Entity == NULL)
            continue;
        if (PlayerList[i].Team == LocalPlayer.Team)
            continue;
        if (PlayerList[i].Health < 1)
            continue;

        Vector currentAng = *mem->SafeReadMemory<Vector>(clientState + dwClientState_ViewAngles);

        Vector enemyHeadPos = BonePos(PlayerList[i].Entity, 8, mem);

        Vector localEye = LocalPlayer.GetLocalEye(mem);

        Vector delta = localEye - enemyHeadPos;

        Vector finalAng = CalculateAngle(delta);

        float fov = GetFOV(currentAng, finalAng);

        if (fov < MINFOV)
        {
            index = i;
        }
    }
    return index;
}

void AimLoop(win_raii::SafeMemory* mem)
{
    if (!GetAsyncKeyState(VK_MBUTTON)) return;

    int index = -1;

    DWORD clientState = *mem->SafeReadMemory<DWORD>(EngineBase + dwClientState);
    
    Vector currentAng = *mem->SafeReadMemory<Vector>(clientState + dwClientState_ViewAngles);

    index = GetClosestEnt(clientState, mem);

    if (index == -1)
        return;

    Vector enemyHeadPos = BonePos(PlayerList[index].Entity, BONEID, mem);

    Vector delta = LocalPlayer.GetLocalEye(mem) - enemyHeadPos;

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
        mem->SafeWriteMemory<Vector>(clientState + dwClientState_ViewAngles, currentAng);

        //Sleep(1);
        loop++;

        currentAng.x += smoothX;
        currentAng.y += smoothY;
    }
}

#pragma endregion

#pragma region WallHack

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
}glowStr;

void Glow(DWORD GlowIndex, float R, float G, float B, win_raii::SafeMemory* mem)
{
    DWORD GlowBase = *mem->SafeReadMemory<DWORD>(ClientBase + dwGlowObjectManager);
    DWORD glowstrBase = GlowBase + (GlowIndex * 0x38) + 0x4;

    glowStr = *mem->SafeReadMemory<GlowStruct>(glowstrBase);

    glowStr.R = R;
    glowStr.G = G;
    glowStr.B = B;
    glowStr.m_flGlowAlpha = 0.7f;
    glowStr.m_bRenderWhenOccluded = true;
    glowStr.m_bRenderWhenUnoccluded = false;

    mem->SafeWriteMemory<GlowStruct>(glowstrBase, glowStr);
}

int GetWeapon(DWORD player, win_raii::SafeMemory* mem)
{
    DWORD Weapon = *mem->SafeReadMemory<DWORD>(player + m_hActiveWeapon);
    DWORD duck = Weapon & 0xFFF;
    DWORD horse = *mem->SafeReadMemory<DWORD>(ClientBase + dwEntityList + (duck - 1) * 0x10);
    return *mem->SafeReadMemory<int>(horse + m_iItemDefinitionIndex);
}

void WallLoop(struct Player player, win_raii::SafeMemory* mem)
{
    if (GetAsyncKeyState(VK_INSERT))
    {
        WhToggled = !WhToggled;
        if (WhToggled) std::cout << "Walls On" << std::endl;
        else std::cout << "Walls Off" << std::endl;
        Sleep(500);
    }

    if (!WhToggled) return;

    int cock = GetWeapon(player.Entity, mem);

    if (cock == WEAPON_AWP)
    {
        Glow(player.GlowIndex, 0, 0, 1, mem); //blue
    }
    else if ((cock >= WEAPON_KNIFE && cock <= WEAPON_KNIFE_T) || cock >= WEAPON_KNIFE_BAYONET) //non-shooting weaps
    {
        Glow(player.GlowIndex, 0, 1, 0, mem); //green
    }
    else
    {
        Glow(player.GlowIndex, 1, 0, 0, mem); //red	
    }
}

#pragma endregion

void MainLoop(win_raii::SafeMemory* mem)
{
    while (true)
    {
        LocalPlayer.Read(mem);

        for (int i = 1; i <= PLAYERCNT; i++)
        {
            PlayerList[i].Read(i, mem);

            if (PlayerList[i].Entity == NULL)
                continue;
            if (PlayerList[i].Team == LocalPlayer.Team)
                continue;
            if (PlayerList[i].Health < 1)
                continue;

            WallLoop(PlayerList[i], mem);
        }
        AimLoop(mem);

        Sleep(SLEEP);
    }
}

int main()
{
    // TODO: try/catch mem and pass mem by ref dont accept pointers

    win_raii::SafeMemory mem(L"csgo.exe", win_raii::SafeMemory_AllAccess, win_raii::SafeMemory::ConstructProcessName());
    EngineBase = *mem.GetModuleBaseAddress(L"engine.dll");
    ClientBase = *mem.GetModuleBaseAddress(L"client_panorama.dll");

    std::cout << "Lets go!" << std::endl;

    MainLoop(&mem);

    return 0;
}

//void DisableFlash()
//{
//	Mem->Write<float>(LocalPlayer.Local + m_flFlashDuration, 0); //flash
//}