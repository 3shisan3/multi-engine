/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        ServiceRegistry.h
Version:     1.0
Author:      cjx
start date: 2026-06-10
Description: 服务注册表接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-06-10      cjx        create

*****************************************************************/

#ifndef ME_RUNTIME_SERVICE_REGISTRY_H
#define ME_RUNTIME_SERVICE_REGISTRY_H

#include <memory>
#include <string>

namespace RuntimeModule
{

class IServiceRegistry
{
public:
    virtual ~IServiceRegistry() = default;

    virtual bool RegisterService(const std::string& name, std::shared_ptr<void> service) = 0;
    virtual std::shared_ptr<void> GetService(const std::string& name) const = 0;
    virtual bool HasService(const std::string& name) const = 0;
    virtual bool RemoveService(const std::string& name) = 0;
};

std::shared_ptr<IServiceRegistry> CreateServiceRegistry();

template <typename T>
bool RegisterTypedService(IServiceRegistry& registry, const std::string& name, std::shared_ptr<T> service)
{
    return registry.RegisterService(name, std::static_pointer_cast<void>(service));
}

template <typename T>
std::shared_ptr<T> GetTypedService(const IServiceRegistry& registry, const std::string& name)
{
    return std::static_pointer_cast<T>(registry.GetService(name));
}

} // namespace RuntimeModule

#endif // ME_RUNTIME_SERVICE_REGISTRY_H