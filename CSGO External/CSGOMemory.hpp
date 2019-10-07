#pragma once

#include "Memory.hpp"

namespace csgo_memory
{
    class CSGOMemory : public memory::SafeMemory
    {
    public:
        CSGOMemory(std::wstring_view process_name, const memory::SafeMemory_Access processFlags, ConstructProcessName tag) noexcept(false)
            : memory::SafeMemory(process_name, processFlags, tag)
        {
            const std::optional<uint32_t> temp_engineBase = GetModuleBaseAddress(L"engine.dll");
            if (!temp_engineBase.has_value())
                throw std::exception("Engine.dll not found!");

            const std::optional<uint32_t> temp_clientBase = GetModuleBaseAddress(L"client_panorama.dll");
            if (!temp_clientBase.has_value())
                throw std::exception("Client.dll not found!");

            this->m_engineBase = temp_engineBase.value();
            this->m_clientBase = temp_clientBase.value();
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

    private:
        uint32_t m_engineBase = 0;
        uint32_t m_clientBase = 0;
    };
}