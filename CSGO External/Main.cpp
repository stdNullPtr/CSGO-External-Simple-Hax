#include <thread>
#include <iostream>
#include "Memory.hpp"
#include "Offsets.hpp"
#include "WeaponType.hpp"
#include "Util_Vector.hpp"
#include "Consts.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#define M_RADPI (180.0 / M_PI)
#define M_DEGRAD (M_PI / 180.0)

MemoryManager* Mem;

bool WhToggled = false;

#pragma region Structs

struct LocalPlayer
{
    DWORD Local;
    int Team;
    int Health;
    int InCross;
    void Read()
    {
        Local = Mem->Read<DWORD>(Mem->ClientDLL_Base + dwLocalPlayer);

        if (Local != NULL)
        {
            Team = Mem->Read<int>(Local + m_iTeamNum);
            Health = Mem->Read<int>(Local + m_iHealth);
            InCross = Mem->Read<int>(Local + m_iCrosshairId);
        }
    }
    Vector GetLocalEye()
    {
        Vector origin = Mem->Read<Vector>(Local + m_vecOrigin);
        Vector viewOffset = Mem->Read<Vector>(Local + m_vecViewOffset);
        return origin + viewOffset;
    }
}LocalPlayer;

struct Player
{
    DWORD Entity;
    int Team;
    int Health;
    DWORD GlowIndex;
    void Read(int p)
    {
        Entity = Mem->Read<DWORD>(Mem->ClientDLL_Base + dwEntityList + ((p - 1) * 0x10));
        //Checking if it is a valid entity
        if (Entity != NULL)
        {
            GlowIndex = Mem->Read<DWORD>(Entity + m_iGlowIndex);
            Team = Mem->Read<int>(Entity + m_iTeamNum);
            Health = Mem->Read<int>(Entity + m_iHealth);
        }
    }
}PlayerList[PLAYERCNT];

#pragma endregion

#pragma region AimBot

Vector BonePos(int base, int boneId)
{
    DWORD boneBase;
    boneBase = Mem->Read<DWORD>(base + m_dwBoneMatrix);

    Vector vBone;
    DWORD Base = boneBase + (0x30 * boneId);

    vBone.x = Mem->Read<float>(Base + 0x0C);
    vBone.y = Mem->Read<float>(Base + 0x1C);
    vBone.z = Mem->Read<float>(Base + 0x2C);

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

int GetClosestEnt(DWORD clientState)
{
    int index = -1;

    for (int i = 1; i <= PLAYERCNT; i++)
    {
        PlayerList[i].Read(i);

        if (PlayerList[i].Entity == NULL)
            continue;
        if (PlayerList[i].Team == LocalPlayer.Team)
            continue;
        if (PlayerList[i].Health < 1)
            continue;

        Vector currentAng = Mem->Read<Vector>(clientState + dwClientState_ViewAngles);

        Vector enemyHeadPos = BonePos(PlayerList[i].Entity, 8);

        Vector localEye = LocalPlayer.GetLocalEye();

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

void AimLoop()
{
    if (!GetAsyncKeyState(VK_MBUTTON)) return;

    int index = -1;

    DWORD clientState = Mem->Read<DWORD>(Mem->EngineDLL_Base + dwClientState);

    Vector currentAng = Mem->Read<Vector>(clientState + dwClientState_ViewAngles);

    index = GetClosestEnt(clientState);

    if (index == -1)
        return;

    Vector enemyHeadPos = BonePos(PlayerList[index].Entity, BONEID);

    Vector delta = LocalPlayer.GetLocalEye() - enemyHeadPos;

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
        Mem->Write<Vector>(clientState + dwClientState_ViewAngles, currentAng);

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

void Glow(DWORD GlowIndex, float R, float G, float B)
{
    DWORD GlowBase = Mem->Read<DWORD>(Mem->ClientDLL_Base + dwGlowObjectManager);
    DWORD glowstrBase = GlowBase + (GlowIndex * 0x38) + 0x4;

    glowStr = Mem->Read<GlowStruct>(glowstrBase);

    glowStr.R = R;
    glowStr.G = G;
    glowStr.B = B;
    glowStr.m_flGlowAlpha = 0.7f;
    glowStr.m_bRenderWhenOccluded = true;
    glowStr.m_bRenderWhenUnoccluded = false;

    Mem->Write<GlowStruct>(glowstrBase, glowStr);
}

int GetWeapon(DWORD player)
{
    DWORD Weapon = Mem->Read<DWORD>(player + m_hActiveWeapon);
    DWORD duck = Weapon & 0xFFF;
    DWORD horse = Mem->Read<DWORD>(Mem->ClientDLL_Base + dwEntityList + (duck - 1) * 0x10);
    return Mem->Read<int>(horse + m_iItemDefinitionIndex);
}

void WallLoop(struct Player player)
{
    if (GetAsyncKeyState(VK_INSERT))
    {
        WhToggled = !WhToggled;
        if (WhToggled) std::cout << "On" << std::endl;
        else std::cout << "Off" << std::endl;
        Sleep(500);
    }

    if (!WhToggled) return;

    int cock = GetWeapon(player.Entity);

    if (cock == WEAPON_AWP)
    {
        Glow(player.GlowIndex, 0, 0, 1); //blue
    }
    else if ((cock >= WEAPON_KNIFE && cock <= WEAPON_KNIFE_T) || cock >= WEAPON_KNIFE_BAYONET) //non-shooting weaps
    {
        Glow(player.GlowIndex, 0, 1, 0); //green
    }
    else
    {
        Glow(player.GlowIndex, 1, 0, 0); //red	
    }
}

#pragma endregion

void MainLoop()
{
    while (true)
    {
        LocalPlayer.Read();

        for (int i = 1; i <= PLAYERCNT; i++)
        {
            PlayerList[i].Read(i);

            if (PlayerList[i].Entity == NULL)
                continue;
            if (PlayerList[i].Team == LocalPlayer.Team)
                continue;
            if (PlayerList[i].Health < 1)
                continue;

            WallLoop(PlayerList[i]);
        }
        AimLoop();

        Sleep(SLEEP);
    }
}

char* EncryptDecrypt(char* str, int size)
{
    char key[3] = { 'F', 'E', 'L' };
    char* output = str;

    for (int i = 0; i < size; i++)
        output[i] = str[i] ^ key[i % (sizeof(key) / sizeof(char))];

    return output;
}

int main()
{
    Mem = new MemoryManager();

    std::cout << "Lets go!" << std::endl;

    MainLoop();

    delete Mem;

    return 0;
}

//void DisableFlash()
//{
//	Mem->Write<float>(LocalPlayer.Local + m_flFlashDuration, 0); //flash
//}