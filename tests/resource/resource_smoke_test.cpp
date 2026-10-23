/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        resource_smoke_test.cpp
Description: 资源管理模块冒烟测试

*****************************************************************/

#include "Resource.h"

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace ResourceModule;

namespace
{

class DemoResource : public IResource
{
public:
    DemoResource(const std::string& id, std::string data)
        : id_(id), data_(std::move(data)) {}

    ResourceId GetId() const override { return id_; }
    ResourceType GetType() const override { return "demo"; }

    const std::string& GetData() const { return data_; }

private:
    std::string id_;
    std::string data_;
};

class DemoProvider : public IResourceProvider
{
public:
    std::shared_ptr<IResource> Load(
        const ResourceId& id,
        const std::map<std::string, std::any>& params) override
    {
        (void)params;

        if (id == "missing")
        {
            return nullptr;
        }
        return std::make_shared<DemoResource>(id, "data-of-" + id);
    }

    ResourceType GetType() const override { return "demo"; }
    std::string GetName() const override { return "DemoProvider"; }
};

} // namespace

int main()
{
    auto manager = CreateResourceManager();
    if (!manager)
    {
        return 1;
    }

    // 1. 注册 provider
    auto provider = std::make_shared<DemoProvider>();
    if (!manager->RegisterProvider(provider))
    {
        std::cerr << "register provider failed" << std::endl;
        return 2;
    }
    if (manager->RegisterProvider(provider)) // 重复注册应失败
    {
        std::cerr << "duplicate provider should fail" << std::endl;
        return 3;
    }

    // 2. Acquire 首次加载
    auto res = manager->Acquire("a");
    if (!res)
    {
        std::cerr << "acquire failed" << std::endl;
        return 4;
    }
    if (res->GetId() != "a" || res->GetType() != "demo")
    {
        std::cerr << "unexpected resource identity" << std::endl;
        return 5;
    }
    if (std::static_pointer_cast<DemoResource>(res)->GetData() != "data-of-a")
    {
        std::cerr << "unexpected resource data" << std::endl;
        return 6;
    }

    // 3. 缓存命中应返回同一实例
    {
        auto again = manager->Acquire("a");
        if (again.get() != res.get())
        {
            std::cerr << "cache should return same instance" << std::endl;
            return 7;
        }
    }

    // 4. 加载失败统计
    if (manager->Acquire("missing") != nullptr)
    {
        std::cerr << "missing resource should fail" << std::endl;
        return 8;
    }

    ResourceStats stats = manager->GetStatistics();
    if (stats.providerCount != 1)
    {
        std::cerr << "provider count mismatch" << std::endl;
        return 9;
    }
    if (stats.loadFailures != 1)
    {
        std::cerr << "load failure count mismatch" << std::endl;
        return 10;
    }

    // 5. ReleaseUnused：外部仍持有时不释放
    if (manager->ReleaseUnused() != 0)
    {
        std::cerr << "should not release in-use resource" << std::endl;
        return 11;
    }
    if (!manager->IsCached("a"))
    {
        std::cerr << "in-use resource should stay cached" << std::endl;
        return 12;
    }

    // 6. 外部引用释放后，ReleaseUnused 回收
    res.reset();
    size_t released = manager->ReleaseUnused();
    if (released != 1 || manager->IsCached("a"))
    {
        std::cerr << "unused resource should be released" << std::endl;
        return 13;
    }

    // 7. Release 与 GetCachedIds
    manager->Acquire("x");
    manager->Acquire("y");
    std::vector<ResourceId> ids = manager->GetCachedIds();
    if (ids.size() != 2)
    {
        std::cerr << "cached id count mismatch" << std::endl;
        return 14;
    }
    if (!manager->Release("x") || manager->IsCached("x"))
    {
        std::cerr << "release failed" << std::endl;
        return 15;
    }

    // 8. GetCacheUseCount
    {
        auto held = manager->Acquire("z");
        if (manager->GetCacheUseCount("z") < 2)
        {
            std::cerr << "cache use count mismatch" << std::endl;
            return 16;
        }
    }

    // 9. 注销 provider
    if (!manager->UnregisterProvider("demo") || manager->UnregisterProvider("demo"))
    {
        std::cerr << "unregister provider failed" << std::endl;
        return 17;
    }

    // 10. Clear
    manager->Clear();
    if (!manager->GetCachedIds().empty())
    {
        std::cerr << "clear failed" << std::endl;
        return 18;
    }

    // 11. 多线程并发获取同一资源：最终缓存一致且不崩溃
    manager->RegisterProvider(provider);
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<IResource>> results(4);
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&, i]() {
            results[i] = manager->Acquire("shared");
        });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
    for (const auto& result : results)
    {
        if (!result || result->GetId() != "shared")
        {
            std::cerr << "concurrent acquire mismatch" << std::endl;
            return 19;
        }
    }
    if (!manager->IsCached("shared"))
    {
        std::cerr << "concurrent resource should be cached" << std::endl;
        return 20;
    }

    std::cout << "resource smoke test passed" << std::endl;
    return 0;
}
