#pragma once

#include "Memory.hpp"
#include "Offsets.hpp"

namespace csgo_memory
{
    class CSGOMemory : public memory::SafeMemory
    {
    public:
        CSGOMemory(const std::wstring_view process_name, const memory::SafeMemory_Access processFlags, const ConstructProcessName tag) noexcept(false)
            : memory::SafeMemory(process_name, processFlags, tag)
        {
            const std::optional<uint32_t> temp_engineBase = GetModuleBaseAddress(L"engine.dll");
            if (!temp_engineBase.has_value())
                throw std::system_error(GetLastError(), std::system_category(), "Engine.dll not found!");

            const std::optional<uint32_t> temp_clientBase = GetModuleBaseAddress(L"client_panorama.dll");
            if (!temp_clientBase.has_value())
                throw std::system_error(GetLastError(), std::system_category(), "Client.dll not found!");

            this->m_engineBase = temp_engineBase.value();
            this->m_clientBase = temp_clientBase.value();

            const std::optional<uint32_t> glowBase = Read<uint32_t>(this->m_clientBase + offsets::signatures::dwGlowObjectManager);
            if (!glowBase.has_value())
                throw std::system_error(GetLastError(), std::system_category(), "'Glow base' not found!");

            this->m_glowBase = glowBase.value();
        }

    public:
        uint32_t GetEngineBase() const
        {
            return m_engineBase;
        }

        uint32_t GetClientBase() const
        {
            return m_clientBase;
        }

        uint32_t GetGlowBase() const
        {
            return m_glowBase;
        }

    private:
        uint32_t m_engineBase = 0;
        uint32_t m_clientBase = 0;
        uint32_t m_glowBase = 0;
    };
}