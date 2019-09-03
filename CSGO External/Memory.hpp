#pragma once 

#include <Windows.h>
#include <TlHelp32.h>
#include "Consts.hpp"

class MemoryManager
{
private:
    DWORD dwPID;
    HANDLE hProcess;

public:
    MODULEENTRY32 ClientDLL;
    MODULEENTRY32 EngineDLL;
    DWORD ClientDLL_Base, ClientDLL_Size;
    DWORD EngineDLL_Base, EngineDLL_Size;

    bool AttachProcess(const char* ProcessName)
    {
        HANDLE hPID = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(procEntry);

        const WCHAR* procNameChar;
        int nChars = MultiByteToWideChar(CP_ACP, 0, ProcessName, -1, NULL, 0);
        procNameChar = new WCHAR[nChars];
        MultiByteToWideChar(CP_ACP, 0, ProcessName, -1, (LPWSTR)procNameChar, nChars);

        do
            if (!wcscmp(procEntry.szExeFile, procNameChar))
            {
                this->dwPID = procEntry.th32ProcessID;
                CloseHandle(hPID);
                this->hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->dwPID);
                return true;
            }
        while (Process32Next(hPID, &procEntry));

        CloseHandle(hPID);
        return false;
    }
    MODULEENTRY32 GetModule(const char* ModuleName)
    {
        HANDLE hModule = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPID);
        MODULEENTRY32 mEntry;
        mEntry.dwSize = sizeof(mEntry);

        const WCHAR* modNameChar;
        int nChars = MultiByteToWideChar(CP_ACP, 0, ModuleName, -1, NULL, 0);
        modNameChar = new WCHAR[nChars];
        MultiByteToWideChar(CP_ACP, 0, ModuleName, -1, (LPWSTR)modNameChar, nChars);

        do
            if (!wcscmp(mEntry.szModule, modNameChar))
            {
                CloseHandle(hModule);
                return mEntry;
            }
        while (Module32Next(hModule, &mEntry));

        CloseHandle(hModule);
        mEntry.modBaseAddr = 0x0;
        return mEntry;
    }

    template<class c>
    c Read(DWORD dwAddress)
    {
        c val;
        ReadProcessMemory(hProcess, (LPVOID)dwAddress, &val, sizeof(c), NULL);
        return val;
    }

    template<class c>
    BOOL Write(DWORD dwAddress, c ValueToWrite)
    {
        return WriteProcessMemory(hProcess, (LPVOID)dwAddress, &ValueToWrite, sizeof(c), NULL);
    }

    DWORD GetProcID() { return this->dwPID; }
    HANDLE GetProcHandle() { return this->hProcess; }

    MemoryManager()
    {
        this->hProcess = NULL;
        this->dwPID = NULL;
        try {
            if (!AttachProcess(csgoStr)) throw 1;
            this->ClientDLL = GetModule(clientStr);
            this->EngineDLL = GetModule(engineStr);
            this->ClientDLL_Base = (DWORD)this->ClientDLL.modBaseAddr;
            this->EngineDLL_Base = (DWORD)this->EngineDLL.modBaseAddr;
            if (this->ClientDLL_Base == 0x0) throw 2;
            if (this->EngineDLL_Base == 0x0) throw 3;
            this->ClientDLL_Size = this->ClientDLL.dwSize;
            this->EngineDLL_Size = this->EngineDLL.dwSize;
        }
        catch (int iEx) {
            switch (iEx)
            {
            case 1: MessageBoxA(NULL, gameMustBeRunningStr, errorWindowCaption, MB_ICONSTOP | MB_OK); exit(0); break;
            case 2: MessageBoxA(NULL, clientStr, errorWindowCaption, MB_ICONSTOP | MB_OK); exit(0); break;
            case 3: MessageBoxA(NULL, engineStr, errorWindowCaption, MB_ICONSTOP | MB_OK); exit(0); break;
            default: throw;
            }
        }
        catch (...) {
            MessageBoxA(NULL, unhandledExceptionStr, errorWindowCaption, MB_ICONSTOP | MB_OK);
            exit(0);
        }
    }

    ~MemoryManager()
    {
        CloseHandle(this->hProcess);
    }
};