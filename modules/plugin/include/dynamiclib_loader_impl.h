/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        DynamicLibraryLoaderImpl.h
Version:     1.0
Author:      cjx
start date: 2026-1-11
Description: 动态库加载器实现类（内部）
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-10      cjx            create
*****************************************************************/

#ifndef PLUGIN_SYSTEM_DYNAMIC_LIBRARY_LOADER_IMPL_H
#define PLUGIN_SYSTEM_DYNAMIC_LIBRARY_LOADER_IMPL_H

#include "DynamicLibraryLoader.h"

namespace PluginSystem
{

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
    std::string lastError_;             ///< 最后错误信息
};

} // namespace PluginSystem

#endif // PLUGIN_SYSTEM_DYNAMIC_LIBRARY_LOADER_IMPL_H