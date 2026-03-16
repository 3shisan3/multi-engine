#include "DynamicLibraryLoader.h"
#include "PluginInterface.h" // 包含IPlugin定义

#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace PluginSystem
{

// =============== DynamicLibraryLoaderImpl 声明 ===============

class DynamicLibraryLoaderImpl : public IDynamicLibraryLoader
{
public:
    DynamicLibraryLoaderImpl();
    ~DynamicLibraryLoaderImpl() override;

    // 禁止拷贝
    DynamicLibraryLoaderImpl(const DynamicLibraryLoaderImpl &) = delete;
    DynamicLibraryLoaderImpl &operator=(const DynamicLibraryLoaderImpl &) = delete;

    // 允许移动
    DynamicLibraryLoaderImpl(DynamicLibraryLoaderImpl &&other) noexcept;
    DynamicLibraryLoaderImpl &operator=(DynamicLibraryLoaderImpl &&other) noexcept;

    // IDynamicLibraryLoader 接口实现
    bool Load(const std::string &libraryPath) override;
    void Unload() override;
    void *GetRawSymbol(const std::string &symbolName) override;
    bool IsLoaded() const override;
    std::string GetLibraryPath() const override;
    std::string GetLastError() const override;

private:
#ifdef _WIN32
    static std::string GetLastErrorString();
#endif

    void *handle_ = nullptr;           ///< 库句柄
    std::string libraryPath_;          ///< 库路径
    std::string lastError_;            ///< 最后错误信息
};

// =============== DynamicLibraryLoaderImpl 实现 ===============

DynamicLibraryLoaderImpl::DynamicLibraryLoaderImpl() = default;

DynamicLibraryLoaderImpl::~DynamicLibraryLoaderImpl()
{
    Unload();
}

DynamicLibraryLoaderImpl::DynamicLibraryLoaderImpl(DynamicLibraryLoaderImpl &&other) noexcept
    : handle_(other.handle_)
    , libraryPath_(std::move(other.libraryPath_))
    , lastError_(std::move(other.lastError_))
{
    other.handle_ = nullptr;
}

DynamicLibraryLoaderImpl &DynamicLibraryLoaderImpl::operator=(DynamicLibraryLoaderImpl &&other) noexcept
{
    if (this != &other)
    {
        Unload();
        handle_ = other.handle_;
        libraryPath_ = std::move(other.libraryPath_);
        lastError_ = std::move(other.lastError_);
        other.handle_ = nullptr;
    }
    return *this;
}

bool DynamicLibraryLoaderImpl::Load(const std::string &libraryPath)
{
    if (handle_)
    {
        lastError_ = "Library already loaded";
        return false;
    }

#ifdef _WIN32
    // Windows平台使用LoadLibrary
    handle_ = LoadLibraryA(libraryPath.c_str());
    if (!handle_)
    {
        lastError_ = GetLastErrorString();
        return false;
    }
#else
    // Linux/macOS平台使用dlopen
    // RTLD_LAZY: 延迟解析符号，RTLD_LOCAL: 符号不对外可见
    handle_ = dlopen(libraryPath.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle_)
    {
        lastError_ = dlerror();
        return false;
    }
#endif

    libraryPath_ = libraryPath;
    lastError_.clear();
    return true;
}

void DynamicLibraryLoaderImpl::Unload()
{
    if (handle_)
    {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
        libraryPath_.clear();
    }
}

void *DynamicLibraryLoaderImpl::GetRawSymbol(const std::string &symbolName)
{
    if (!handle_)
    {
        lastError_ = "Library not loaded";
        return nullptr;
    }

#ifdef _WIN32
    void *symbol = GetProcAddress(static_cast<HMODULE>(handle_), symbolName.c_str());
    if (!symbol)
    {
        lastError_ = GetLastErrorString();
    }
    return symbol;
#else
    // dlerror会清除之前的错误，所以先获取错误状态
    dlerror();
    void *symbol = dlsym(handle_, symbolName.c_str());
    const char *error = dlerror();
    if (error)
    {
        lastError_ = error;
        return nullptr;
    }
    return symbol;
#endif
}

bool DynamicLibraryLoaderImpl::IsLoaded() const
{
    return handle_ != nullptr;
}

std::string DynamicLibraryLoaderImpl::GetLibraryPath() const
{
    return libraryPath_;
}

std::string DynamicLibraryLoaderImpl::GetLastError() const
{
    return lastError_;
}

#ifdef _WIN32
std::string DynamicLibraryLoaderImpl::GetLastErrorString()
{
    DWORD error = GetLastError();
    if (error == 0)
    {
        return "Unknown error";
    }

    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buffer,
        0,
        nullptr);

    std::string message(buffer, size);
    LocalFree(buffer);
    
    // 移除末尾的换行符
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
    {
        message.pop_back();
    }
    
    return message;
}
#endif

// =============== 静态方法实现 ===============

std::string IDynamicLibraryLoader::GetPlatformLibraryName(const std::string &baseName)
{
#ifdef _WIN32
    return baseName + ".dll";
#elif __APPLE__
    return "lib" + baseName + ".dylib";
#else
    return "lib" + baseName + ".so";
#endif
}

// =============== 工厂函数 ===============

std::shared_ptr<IDynamicLibraryLoader> CreateDynamicLibraryLoader()
{
    return std::make_shared<DynamicLibraryLoaderImpl>();
}

} // namespace PluginSystem