/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        DynamicLibraryLoader.h
Version:     1.0
Author:      cjx
start date: 2026-2-06
Description: 跨平台动态库加载器接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-2-06      cjx            create
*****************************************************************/

#ifndef PLUGIN_SYSTEM_DYNAMIC_LIBRARY_LOADER_H
#define PLUGIN_SYSTEM_DYNAMIC_LIBRARY_LOADER_H

#include "PluginInterface.h"

#include <functional>
#include <memory>
#include <string>

#ifdef _WIN32
    #ifdef PLUGIN_SYSTEM_EXPORTS
        #define PLUGIN_EXPORT __declspec(dllexport)
    #else
        #define PLUGIN_EXPORT __declspec(dllimport)
    #endif
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace PluginSystem
{

/**
 * @brief 动态库加载器
 * 
 * 提供跨平台的动态库加载、卸载和符号解析功能。
 * 支持Windows (.dll)、Linux (.so) 和 macOS (.dylib) 平台。
 */
class IDynamicLibraryLoader
{
public:
    virtual ~IDynamicLibraryLoader() = default;

    /**
     * @brief 加载动态库
     * @param libraryPath 库文件路径
     * @return 成功返回true
     */
    virtual bool Load(const std::string& libraryPath) = 0;

    /**
     * @brief 卸载动态库
     */
    virtual void Unload() = 0;

    /**
     * @brief 获取符号地址
     * @tparam T 符号类型（函数指针类型）
     * @param symbolName 符号名称
     * @return 符号地址的包装，失败返回空std::function
     * 
     * 使用示例：
     * auto createFunc = loader->GetSymbol<CreatePluginFunc>("CreatePlugin");
     * if (createFunc) {
     *     IPlugin* plugin = createFunc();
     * }
     */
    template <typename T>
    std::function<typename std::remove_pointer<T>::type> GetSymbol(const std::string &symbolName)
    {
        void *symbol = GetRawSymbol(symbolName);
        if (!symbol)
        {
            return nullptr;
        }
        
        // 将void*转换为函数指针，然后包装为std::function
        T funcPtr = reinterpret_cast<T>(symbol);
        return std::function<typename std::remove_pointer<T>::type>(funcPtr);
    }

    /**
     * @brief 获取原始符号地址
     * @param symbolName 符号名称
     * @return 符号地址，失败返回nullptr
     */
    virtual void* GetRawSymbol(const std::string& symbolName) = 0;

    /**
     * @brief 检查库是否已加载
     */
    virtual bool IsLoaded() const = 0;

    /**
     * @brief 获取库路径
     */
    virtual std::string GetLibraryPath() const = 0;

    /**
     * @brief 获取最后错误信息
     */
    virtual std::string GetLastError() const = 0;

    /**
     * @brief 获取平台特定的库文件名
     * @param baseName 基础名称
     * @return 平台特定的文件名
     */
    static std::string GetPlatformLibraryName(const std::string& baseName);
};

/**
 * @brief 创建动态库加载器
 */
PLUGIN_EXPORT std::shared_ptr<IDynamicLibraryLoader> CreateDynamicLibraryLoader();

} // namespace PluginSystem

#endif // PLUGIN_SYSTEM_DYNAMIC_LIBRARY_LOADER_H