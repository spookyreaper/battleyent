#include <Windows.h>
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
 
typedef void (*Il2CppMethodPointer)();
 
struct Il2CppDomain;
struct Il2CppAssembly;
struct Il2CppThread;
struct Il2CppClass;
struct Il2CppMethod;
struct Il2CppImage;
 
 
 
using il2cpp_get_root_domain_prot = Il2CppDomain * (*)();
static il2cpp_get_root_domain_prot il2cpp_get_root_domain = nullptr;
 
 
using il2cpp_thread_attach_prot = Il2CppThread * (*)(Il2CppDomain*);
static il2cpp_thread_attach_prot il2cpp_thread_attach = nullptr;
 
using il2cpp_domain_get_assemblies_prot = const Il2CppAssembly** (*)(Il2CppDomain* domain, size_t* size);
static il2cpp_domain_get_assemblies_prot il2cpp_domain_get_assemblies = nullptr;
 
 
using il2cpp_class_from_name_prot = Il2CppClass * (*)(const Il2CppImage* image, const char* name_space, const char* name);
static il2cpp_class_from_name_prot il2cpp_class_from_name = nullptr;
 
 
using il2cpp_class_get_methods_prot = const Il2CppMethod* (*)(Il2CppClass* klass, void** iter);
static il2cpp_class_get_methods_prot il2cpp_class_get_methods = nullptr;
 
 
using il2cpp_method_get_name_prot = const char* (*)(const Il2CppMethod* method);
static il2cpp_method_get_name_prot il2cpp_method_get_name = nullptr;
 
using il2cpp_assembly_get_image_prot =
const Il2CppImage* (*)(const Il2CppAssembly*);
 
using il2cpp_image_get_name_prot = const char* (*)(const Il2CppImage*);
static il2cpp_assembly_get_image_prot il2cpp_assembly_get_image = nullptr;
static il2cpp_image_get_name_prot    il2cpp_image_get_name = nullptr;
 
//ValidateAnticheat
const Il2CppMethod* find_battleye_init(Il2CppClass* main_application)
{
    void* iter = nullptr;
    const Il2CppMethod* method;
 
    while (method = il2cpp_class_get_methods(main_application, &iter))
    {
        auto name = il2cpp_method_get_name(method);
        if ((unsigned(name[0]) & 0xFF) == 0xEE && (unsigned(name[1]) & 0xFF) == 0x80 && (unsigned(name[2]) & 0xFF) == 0x81) // UTF-8 for 
            return method;
 
        if (strcmp(name, "ValidateAnticheat") == 0)
            return method;
    }
 
    return nullptr;
}
 
void debug_method_slots(const Il2CppMethod* method)
{
    if (!method) return;
 
    auto slots = reinterpret_cast<void* const*>(method);
 
    printf("Method @ %p\n", method);
 
    for (int i = 0; i < 6; ++i)
    {
        void* candidate = slots[i];
 
        MEMORY_BASIC_INFORMATION mbi{};
        VirtualQuery(candidate, &mbi, sizeof(mbi));
 
        printf("slot[%d] = %p, Protect = 0x%lX, State = 0x%lX, Type = 0x%lX\n",
            i,
            candidate,
            mbi.Protect,
            mbi.State,
            mbi.Type);
    }
}
 
std::vector<const Il2CppMethod*> error_screen_methods{};
 
const Il2CppMethod* find_show_error_screen(Il2CppClass* preloader_ui)
{
    void* iter = nullptr;
    const Il2CppMethod* method;
 
    while (method = il2cpp_class_get_methods(preloader_ui, &iter))
    {
        auto name = il2cpp_method_get_name(method);
 
        if (strcmp(name, "ShowErrorScreen") == 0)
        {
            printf("error screen method found with address %p\n", method);
            error_screen_methods.push_back(method);
            continue;
        }
 
    }
 
    return nullptr;
}
 
 
void patch_method(const Il2CppMethod* method)
{
    *(unsigned char*)(method) = 0xC3; // ret
    printf("- method patched\n");
}
 
void patch_method_ret(const Il2CppMethod* method)
{
    if (!method)
        return;
 
    // пе вое поле MethodInfo — pointer на нативную функцию
    auto fn_ptr_ptr = reinterpret_cast<void* const*>(method);
    void* fn = *fn_ptr_ptr;
    if (!fn)
        return;
 
    DWORD oldProtect;
    if (!VirtualProtect(fn, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        printf("VirtualProtect failed\n");
        return;
    }
 
    auto code = reinterpret_cast<std::uint8_t*>(fn);
    *code = 0xC3; // ret
 
    VirtualProtect(fn, 1, oldProtect, &oldProtect);
 
    printf("Patched method at %p -> RET\n", fn);
}
 
Il2CppImage* il2cpp_image_loaded(const char* image_name)
{
    Il2CppDomain* domain = il2cpp_get_root_domain();
    if (!domain)
    {
        printf("failed to get domain!\n");
        return nullptr;
    }
 
 
    size_t assembly_count = 0;
    const Il2CppAssembly* const* assemblies = il2cpp_domain_get_assemblies(domain, &assembly_count);
    if (!assemblies || !assembly_count)
    {
        printf("failed to assemblies or assembly_count!\n");
        return nullptr;
    }
 
 
    printf("assemblies = %p, count = %zu\n", assemblies, assembly_count);
 
    for (size_t i = 0; i < assembly_count; ++i)
    {
        const Il2CppAssembly* assembly = assemblies[i];
        if (!assembly)
            continue;
 
        const Il2CppImage* image = il2cpp_assembly_get_image(assembly);
        if (!image)
            continue;
 
        const char* name = il2cpp_image_get_name(image);
        if (!name)
            continue;
 
        printf("image[%zu] = %s\n", i, name);
 
        if (_stricmp(name, image_name) == 0)
            return const_cast<Il2CppImage*>(image);
    }
 
    return nullptr;
}
 
// im too lazy to even open github so heres what copilot gave me
static std::optional<std::pair<std::vector<int>, std::vector<bool>>>
parse_pattern(const std::string& pattern)
{
    std::istringstream iss(pattern);
    std::string tok;
    std::vector<int> bytes;
    std::vector<bool> mask;
 
    while (iss >> tok) {
        if (tok == "?" || tok == "??") {
            bytes.push_back(0);
            mask.push_back(false);
            continue;
        }
        if (tok.size() > 2)
            return std::nullopt;
 
        for (char c : tok)
            if (!isxdigit((unsigned char)c))
                return std::nullopt;
 
        int b = std::stoi(tok, nullptr, 16);
        bytes.push_back(b & 0xFF);
        mask.push_back(true);
    }
 
    if (bytes.empty())
        return std::nullopt;
 
    return std::make_pair(bytes, mask);
}
 
static std::vector<size_t>
scan_region(uint8_t* data, size_t dataSize, const std::string& pattern)
{
    std::vector<size_t> results;
 
    auto parsed = parse_pattern(pattern);
    if (!parsed)
        return results;
 
    auto& pat_bytes = parsed->first;
    auto& pat_mask = parsed->second;
    size_t plen = pat_bytes.size();
 
    if (plen > dataSize)
        return results;
 
    for (size_t i = 0; i + plen <= dataSize; i++) {
        bool ok = true;
 
        for (size_t j = 0; j < plen; j++) {
            if (pat_mask[j] && data[i + j] != (uint8_t)pat_bytes[j]) {
                ok = false;
                break;
            }
        }
 
        if (ok)
            results.push_back(i);
    }
 
    return results;
}
 
std::vector<uintptr_t> find_pattern_all_process(const std::string& pattern)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
 
    uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
    uintptr_t end = (uintptr_t)si.lpMaximumApplicationAddress;
 
    MEMORY_BASIC_INFORMATION mbi{};
    std::vector<uintptr_t> results;
 
    while (addr < end) {
        if (VirtualQuery((void*)addr, &mbi, sizeof(mbi)) != sizeof(mbi)) {
            addr += 0x1000;
            continue;
        }
 
        bool readable =
            (mbi.State == MEM_COMMIT) &&
            !(mbi.Protect & PAGE_NOACCESS) &&
            !(mbi.Protect & PAGE_GUARD);
 
        if (readable) {
            uint8_t* region = (uint8_t*)mbi.BaseAddress;
            size_t size = mbi.RegionSize;
 
            auto hits = scan_region(region, size, pattern);
            for (size_t offset : hits)
                results.push_back((uintptr_t)region + offset);
        }
 
        addr += mbi.RegionSize;
    }
 
    return results;
}
 
 
bool patch(void* addr, const unsigned char* data, std::size_t size) {
    if (IsBadReadPtr(addr, size)) return false;
    DWORD old{};
    VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &old);
 
    memcpy(addr, data, size);
 
    VirtualProtect(addr, size, old, &old);
    return true;
}
 
void start()
{
    HMODULE hAdvapi = GetModuleHandleA("Advapi32.dll");
    if (!hAdvapi)
    {
        std::cerr << "Failed to get Advapi32.dll handle\n";
        return;
    }
 
    FARPROC pOpenServiceA = GetProcAddress(hAdvapi, "OpenServiceA");
    FARPROC pQueryServiceStatusEx = GetProcAddress(hAdvapi, "QueryServiceStatusEx");
 
    if (!pOpenServiceA || !pQueryServiceStatusEx)
    {
        std::cerr << "Failed to get function addresses\n";
        return;
    }
 
    const unsigned char patchOpenServiceA[] = { 0xB0, 0x01, 0xC3 };
    const unsigned char patchQueryServiceStatusEx[] = {
        0x41, 0xC7, 0x40, 0x04, 0x04, 0x00, 0x00, 0x00, 0xB0, 0x01, 0xC3
    };
 
    if (!patch(reinterpret_cast<void*>(pOpenServiceA),
        patchOpenServiceA,
        sizeof(patchOpenServiceA)))
    {
        std::cerr << "Failed to patch OpenServiceA\n";
        return;
    }
 
    if (!patch(reinterpret_cast<void*>(pQueryServiceStatusEx),
        patchQueryServiceStatusEx,
        sizeof(patchQueryServiceStatusEx)))
    {
        std::cerr << "Failed to patch QueryServiceStatusEx\n";
        return;
    }
 
    std::cout << "Patches applied.\n";
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
 
    HMODULE il2cpp = GetModuleHandleA("GameAssembly.dll");
 
    while (!il2cpp)
    {
        il2cpp = GetModuleHandleA("GameAssembly.dll");
    }
 
    printf("il2cpp is %llx\n", il2cpp);
 
    il2cpp_get_root_domain = reinterpret_cast<il2cpp_get_root_domain_prot>(GetProcAddress(il2cpp, "il2cpp_domain_get"));
 
    il2cpp_thread_attach = reinterpret_cast<il2cpp_thread_attach_prot>(GetProcAddress(il2cpp, "il2cpp_thread_attach"));
 
    il2cpp_domain_get_assemblies = reinterpret_cast<il2cpp_domain_get_assemblies_prot>(GetProcAddress(il2cpp, "il2cpp_domain_get_assemblies"));
 
    il2cpp_class_from_name = reinterpret_cast<il2cpp_class_from_name_prot>(GetProcAddress(il2cpp, "il2cpp_class_from_name"));
 
    il2cpp_class_get_methods = reinterpret_cast<il2cpp_class_get_methods_prot>(GetProcAddress(il2cpp, "il2cpp_class_get_methods"));
 
    il2cpp_method_get_name = reinterpret_cast<il2cpp_method_get_name_prot>(GetProcAddress(il2cpp, "il2cpp_method_get_name"));
 
    il2cpp_assembly_get_image = reinterpret_cast<il2cpp_assembly_get_image_prot>(GetProcAddress(il2cpp, "il2cpp_assembly_get_image"));
 
    il2cpp_image_get_name = reinterpret_cast<il2cpp_image_get_name_prot>(GetProcAddress(il2cpp, "il2cpp_image_get_name"));
 
    printf("il2cpp_get_root_domain = %p\n", il2cpp_get_root_domain);
    printf("il2cpp_domain_get_assemblies = %p\n", il2cpp_domain_get_assemblies);
    printf("il2cpp_class_from_name = %p\n", il2cpp_class_from_name);
    printf("il2cpp_class_get_methods = %p\n", il2cpp_class_get_methods);
    printf("il2cpp_method_get_name = %p\n", il2cpp_method_get_name);
    printf("il2cpp_assembly_get_image = %p\n", il2cpp_assembly_get_image);
    printf("il2cpp_image_get_name = %p\n", il2cpp_image_get_name);
 
    Sleep(1000);
    auto domain = il2cpp_get_root_domain();
    printf("- gotten root domain, il2cpp domain: %p\n", domain);
    auto thread = il2cpp_thread_attach(domain);
    printf("- attached to thread, il2cpp thread: %p\n", thread);
 
    Sleep(1000);
    // find image
    Il2CppImage* image = nullptr;
    while (image == nullptr)
    {
        image = il2cpp_image_loaded("Assembly-CSharp.dll");
        Sleep(500);
    }
    printf("- Assembly-CSharp found, il2cpp image: 0x%p\n", image);
 
    printf("\n- patching BattlEye init method\n");
 
    auto main_application = il2cpp_class_from_name(image, "EFT", "TarkovApplication");
    if (main_application == nullptr)
    {
        printf("- can't find EFT.MainApplcation class!\n");
        return;
    }
    printf("- EFT.MainApplication found, MonoClass: 0x%p\n", main_application);
 
    auto bub = find_battleye_init(main_application);
    if (bub == nullptr)
    {
        printf("- can't find BattlEye initialization method!\n");
    }
    printf("- bub initialization method found, MonoMethod: 0x%p\n", bub);
 
    patch_method_ret(bub);
 
    printf("\n- patching error screen method\n");
 
    auto preloader_ui = il2cpp_class_from_name(image, "EFT.UI", "PreloaderUI");
    if (preloader_ui == nullptr)
    {
        printf("- can't find EFT.UI.PreloaderUI class!\n");
        return;
    }
    printf("- EFT.UI.PreloaderUI found, il2cppClass: 0x%p\n", preloader_ui);
 
    auto show_error_screen = find_show_error_screen(preloader_ui);
 
    for (auto i : error_screen_methods)
    {
        patch_method_ret(i);
    }
 
    HMODULE be = GetModuleHandleA("BEClient_x64.dll");
 
    if (be)
    {
        printf("BEClient_x64 found, attempting to unload\n");
 
        if (!FreeLibrary(be))
            return;
        printf("Unloaded!\n");
 
    }
 
    printf("\n- all done!\n");
}
 
BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(nullptr, 0, LPTHREAD_START_ROUTINE(start), nullptr, 0, nullptr);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
