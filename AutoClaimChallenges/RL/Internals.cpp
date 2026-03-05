// Internals.cpp - Rocket League Internals (GObjects/GNames Access)
 
#include "pch.h"
#include "Internals.h"
#include "../Utils/RLUtils.h"
#include "../logging.h"
#include <cstring>
#include <unordered_map>
#include <Windows.h>
#include <Psapi.h>
#include <vector>
#include <algorithm> 
#include <functional> 
#include <execution>  
#include <span>       

// ==================== Globals ====================

static TArray<UObject*>* GObjects = nullptr;
static TArray<FNameEntry*>* GNames = nullptr;
static bool globalsInitialized = false;
static std::unordered_map<std::string, FName> fnameCache;

// ==================== Pattern Scanning ====================

static uintptr_t FindPattern(HMODULE module, const std::vector<uint8_t>& pattern, std::string_view mask)
{
    MODULEINFO info = {};
    if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(MODULEINFO)))
    {
        return 0;
    }
    
    uintptr_t start = reinterpret_cast<uintptr_t>(module);
    size_t size = info.SizeOfImage;
    std::span<const uint8_t> memory(reinterpret_cast<const uint8_t*>(start), size);
    
    if (mask.find('?') == std::string_view::npos)
    {
        auto it = std::search(
            memory.begin(), memory.end(),
            std::boyer_moore_horspool_searcher(pattern.begin(), pattern.end())
        );
        if (it != memory.end())
        {
            return start + std::distance(memory.begin(), it);
        }
    }
    else
    {
        const uint8_t* pStart = memory.data();
        const uint8_t* pEnd = pStart + size - mask.length();
        for (const uint8_t* p = pStart; p < pEnd; ++p)
        {
            bool found = true;
            for (size_t i = 0; i < mask.length(); i++)
            {
                if (mask[i] != '?' && pattern[i] != p[i])
                {
                    found = false;
                    break;
                }
            }
            if (found)
            {
                return reinterpret_cast<uintptr_t>(p);
            }
        }
    }
    return 0;
}

// ==================== Initialization ====================

void Internals::Initialize()
{
    if (globalsInitialized)
    {
        return;
    }
    
    HMODULE hModule = GetModuleHandleW(L"RocketLeague.exe");
    if (!hModule)
    {
        hModule = GetModuleHandleW(NULL);
        LOG("[SDK] GetModuleHandle(RocketLeague.exe) failed, using fallback.");
    }

    MODULEINFO modInfo = {};
    GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO));
    LOG("[SDK] Module base: " + std::to_string(reinterpret_cast<uintptr_t>(hModule)) + 
            " size: " + std::to_string(modInfo.SizeOfImage));

    std::vector<uint8_t> pattern = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x01, 0x00, 0x35, 0x25, 0x02, 0x00
    };
    std::string_view mask = "??????xx??xxxxxx";
    
    uintptr_t foundAddr = FindPattern(hModule, pattern, mask);
    if (foundAddr == 0)
    {
        LOG("[SDK] ERROR: Failed to find GObjects pattern.");
        return;
    }
    
    GNames = reinterpret_cast<TArray<FNameEntry*>*>(foundAddr);
    GObjects = reinterpret_cast<TArray<UObject*>*>(foundAddr + 0x48);
    
    if (IsBadReadPtr(GNames, sizeof(void*)) || IsBadReadPtr(GObjects, sizeof(void*)))
    {
        LOG("[SDK] ERROR: GNames/GObjects pointer validation failed.");
        globalsInitialized = false;
        return;
    }

    globalsInitialized = true;
    LOG("[SDK] GObjects & GNames initialized. GNames count=" + 
            std::to_string(GNames->Count) + " GObjects count=" + std::to_string(GObjects->Count));
}

bool Internals::IsInitialized()
{
    return globalsInitialized;
}

// ==================== Object Lookup ====================

bool Internals::GetFNameByString(const std::string& name, FName& outFName)
{
    if (!GNames)
    {
        return false;
    }
    
    auto it = fnameCache.find(name);
    if (it != fnameCache.end())
    {
        outFName = it->second;
        return true;
    }
    
    std::wstring targetName(name.begin(), name.end());
    std::span<FNameEntry*> namesSpan(GNames->Data, GNames->Count);
    
    auto itScan = std::find_if(std::execution::par, namesSpan.begin(), namesSpan.end(), 
        [&targetName](FNameEntry* entry) {
            if (!entry) return false;
            return wcscmp(entry->Name, targetName.c_str()) == 0;
        });

    if (itScan != namesSpan.end())
    {
        int32_t index = static_cast<int32_t>(std::distance(namesSpan.begin(), itScan));
        FName fname;
        fname.FNameEntryId = index;
        fname.InstanceNumber = 0;
        fnameCache[name] = fname;
        outFName = fname;
        return true;
    }
    return false;
}

static std::string_view GetLeafName(std::string_view fullName)
{
    size_t lastDot = fullName.rfind('.');
    if (lastDot == std::string_view::npos)
    {
        return fullName;
    }
    return fullName.substr(lastDot + 1);
}

UObject* Internals::FindObject(const std::string& fullName)
{
    if (!GObjects || fullName.empty())
    {
        return nullptr;
    }
    
    std::string_view targetFull(fullName);
    std::string_view targetLeaf = GetLeafName(targetFull);
    
    for (int32_t index = 0; index < GObjects->Count; index++)
    {
        UObject* obj = (*GObjects)[index];
        if (!obj)
        {
            continue;
        }
        
        if (obj->HasName(targetLeaf))
        {
            if (obj->GetFullName() == fullName)
            {
                return obj;
            }
        }
    }
    return nullptr;
}

UObject* Internals::FindObjectBySuffix(const std::string& suffix)
{
    if (!GObjects) return nullptr;

    for (int32_t i = 0; i < GObjects->Count; i++)
    {
        UObject* obj = (*GObjects)[i];
        if (!obj) continue;
        
        std::string name = obj->GetFullName();
        if (name.length() > suffix.length() &&
            name.compare(name.length() - suffix.length(), suffix.length(), suffix) == 0)
        {
            return obj;
        }
    }
    return nullptr;
}

// ==================== FName / Object Methods ====================

TArray<FNameEntry*>* FName::Names()
{
    return GNames;
}

std::string FNameEntry::ToString() const
{
    return RLUtils::WideToUtf8(Name);
}

std::string FName::ToString() const
{
    if (FNameEntryId < 0 || !GNames || FNameEntryId >= GNames->Count)
    {
        return "UnknownName";
    }
    FNameEntry* entry = (*GNames)[FNameEntryId];
    return entry ? entry->ToString() : "NullName";
}

std::string UObject::GetName()
{
    return this->Name.ToString();
}

template<typename T> 
static T GetVirtualFunction(const void* instance, size_t index)
{
    auto vtable = *static_cast<const void***>(const_cast<void*>(instance));
    return reinterpret_cast<T>(vtable[index]);
}

void UObject::ProcessEvent(UFunction* uFunction, void* uParams, void* /*uResult*/)
{
    if (!this || !uFunction)
    {
        return;
    }
    auto func = GetVirtualFunction<void(*)(UObject*, UFunction*, void*)>(this, 67);
    func(this, uFunction, uParams);
}

bool UObject::HasName(std::string_view targetName)
{
    if (Name.FNameEntryId < 0 || !GNames || Name.FNameEntryId >= GNames->Count)
    {
        return false;
    }
    FNameEntry* entry = (*GNames)[Name.FNameEntryId];
    if (!entry)
    {
        return false;
    }
    
    size_t i = 0;
    for (; i < targetName.size(); ++i)
    {
        if (entry->Name[i] != static_cast<wchar_t>(targetName[i]))
        {
            return false;
        }
    }
    return entry->Name[i] == L'\0';
}

std::string UObject::GetFullName()
{
    if (!this || !Class)
    {
        return "None";
    }
    std::string name = GetName();
    std::string className = Class->GetName();
    std::string outerName;
    for (UObject* uOuter = this->Outer; uOuter; uOuter = uOuter->Outer)
    {
        outerName = uOuter->GetName() + "." + outerName;
    }
    return className + " " + outerName + name;
}

// ==================== FString & UFunction ====================

std::string FString::ToUTF8() const
{
    if (!Data || Count <= 0)
    {
        return "";
    }
    return RLUtils::WideToUtf8(std::wstring(Data, Count > 0 ? Count - 1 : 0));
}

TArray<UObject*>* UObject::GObjObjects()
{
    return GObjects;
}

UFunction* UFunction::FindFunction(const char* functionName)
{
    static std::unordered_map<std::string, UFunction*> cache;
    auto it = cache.find(functionName);
    if (it != cache.end())
    {
        return it->second;
    }
    
    UObject* obj = Internals::FindObject(functionName);
    if (obj)
    {
        UFunction* func = static_cast<UFunction*>(obj);
        cache[functionName] = func;
        return func;
    }
    return nullptr;
}
