#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <string_view>

// ==================== UE3 Base Classes ====================

template<typename T>
struct TArray
{
    T* Data;
    int32_t Count;
    int32_t Max;
    
    int32_t size() const { return Count; }
    T& operator[](int32_t i) { return Data[i]; }
    const T& operator[](int32_t i) const { return Data[i]; }
    T* begin() { return Data; }
    T* end() { return Data + Count; }
};

class FNameEntry
{
public:
    uint64_t Flags;
    int32_t Index;
    uint8_t UnknownData00[0xC];
    wchar_t Name[0x400];
    
    std::string ToString() const;
};

class FName
{
public:
    int32_t FNameEntryId;
    int32_t InstanceNumber;
    
    static TArray<FNameEntry*>* Names();
    std::string ToString() const;
    bool operator==(const FName& other) const { return FNameEntryId == other.FNameEntryId; }
};

class FString
{
public:
    wchar_t* Data;
    int32_t Count;
    int32_t Max;
    
    std::string ToUTF8() const;
};

class UClass;
class UFunction;

class UObject
{
public:
    void* VfTableObject;
    void* HashNext;
    uint64_t ObjectFlags;
    void* HashOuterNext;
    void* StateFrame;
    UObject* Linker;
    void* LinkerIndex;
    int32_t ObjectInternalInteger;
    int32_t NetIndex;
    UObject* Outer;
    FName Name;
    UClass* Class;
    UObject* ObjectArchetype;
    
    void ProcessEvent(UFunction* uFunction, void* uParams, void* uResult = nullptr);
    
    std::string GetName();
    std::string GetFullName();
    bool HasName(std::string_view name);
    
    static TArray<UObject*>* GObjObjects();
};

class UClass : public UObject {};

class UFunction : public UObject
{
public:
    static UFunction* FindFunction(const char* functionName);
};

// ==================== Public API ====================

namespace Internals
{   
    void Initialize();  
    bool IsInitialized();
    
    bool GetFNameByString(const std::string& name, FName& outFName);
    UObject* FindObject(const std::string& fullName);
    UObject* FindObjectBySuffix(const std::string& suffix);
}
