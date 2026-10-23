/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        world_smoke_test.cpp
Description: 世界模型模块冒烟测试

*****************************************************************/

#include "World.h"

#include <cmath>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace WorldModule;

namespace
{

class DemoEntity : public IEntity
{
public:
    DemoEntity(EntityId id, std::string type, Vector3 pos)
        : id_(id), type_(std::move(type)), pos_(pos) {}

    EntityId GetId() const override { return id_; }
    std::string GetType() const override { return type_; }
    Vector3 GetPosition() const override { return pos_; }
    void SetPosition(const Vector3& position) override { pos_ = position; }

private:
    EntityId id_;
    std::string type_;
    Vector3 pos_;
};

bool Near(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) < eps;
}

} // namespace

int main()
{
    auto world = CreateWorldModel();
    if (!world)
    {
        return 1;
    }

    // 1. 添加实体
    auto e1 = std::make_shared<DemoEntity>(1, "ship", Vector3{0, 0, 0});
    auto e2 = std::make_shared<DemoEntity>(2, "ship", Vector3{3, 4, 0});
    auto e3 = std::make_shared<DemoEntity>(3, "plane", Vector3{10, 0, 0});

    if (!world->AddEntity(e1) || !world->AddEntity(e2) || !world->AddEntity(e3))
    {
        std::cerr << "add entity failed" << std::endl;
        return 2;
    }
    if (world->AddEntity(e1)) // 重复添加应失败
    {
        std::cerr << "duplicate add should fail" << std::endl;
        return 3;
    }
    if (world->GetEntityCount() != 3)
    {
        std::cerr << "entity count mismatch" << std::endl;
        return 4;
    }

    // 2. 查询与获取
    if (world->GetEntity(1) != e1)
    {
        std::cerr << "get entity mismatch" << std::endl;
        return 5;
    }
    if (world->GetEntities().size() != 3)
    {
        std::cerr << "get entities mismatch" << std::endl;
        return 6;
    }

    // 3. 类型查询
    if (world->QueryByType("ship").size() != 2)
    {
        std::cerr << "query by type mismatch" << std::endl;
        return 7;
    }

    // 4. 半径查询：e1 在原点，半径 5 应命中 e1、e2（距离 5）
    auto near = world->QueryByRadius(Vector3{0, 0, 0}, 5.0);
    if (near.size() != 2)
    {
        std::cerr << "radius query mismatch: " << near.size() << std::endl;
        return 8;
    }

    // 半径扩大到 10 应包含 e3（距离 10）
    if (world->QueryByRadius(Vector3{0, 0, 0}, 10.0).size() != 3)
    {
        std::cerr << "radius query (10) mismatch" << std::endl;
        return 9;
    }

    // 5. Vector3 距离计算
    if (!Near(Vector3{0, 0, 0}.Distance(Vector3{3, 4, 0}), 5.0))
    {
        std::cerr << "distance mismatch" << std::endl;
        return 10;
    }

    // 6. 实体变更回调
    int addedCount = 0;
    int removedCount = 0;
    world->SetEntityChangeCallback([&](EntityId, const std::string&, bool added) {
        if (added) ++addedCount; else ++removedCount;
    });
    world->RemoveEntity(3);
    if (removedCount != 1 || world->GetEntityCount() != 2)
    {
        std::cerr << "remove entity mismatch" << std::endl;
        return 11;
    }

    // 7. 快照
    WorldSnapshot snapshot = world->CaptureSnapshot();
    if (snapshot.entityTypes.size() != 2 || snapshot.snapshotId != 1)
    {
        std::cerr << "snapshot mismatch" << std::endl;
        return 12;
    }
    if (snapshot.entityTypes[1] != "ship")
    {
        std::cerr << "snapshot type mismatch" << std::endl;
        return 13;
    }

    // 8. 应用快照（清空后由调用方重建）
    if (!world->ApplySnapshot(snapshot))
    {
        std::cerr << "apply snapshot failed" << std::endl;
        return 14;
    }

    // 9. 统计
    WorldStatistics stats = world->GetStatistics();
    if (stats.addCount != 3 || stats.removeCount != 1)
    {
        std::cerr << "statistics mismatch" << std::endl;
        return 15;
    }

    // 10. 清空
    world->Clear();
    if (world->GetEntityCount() != 0)
    {
        std::cerr << "clear mismatch" << std::endl;
        return 16;
    }

    // 11. 多线程添加（唯一 ID）
    auto sharedWorld = CreateWorldModel();
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 25; ++i)
            {
                auto id = static_cast<EntityId>(t * 25 + i + 1);
                sharedWorld->AddEntity(std::make_shared<DemoEntity>(id, "unit", Vector3{0, 0, 0}));
            }
        });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
    if (sharedWorld->GetEntityCount() != 100)
    {
        std::cerr << "concurrent add mismatch: " << sharedWorld->GetEntityCount() << std::endl;
        return 17;
    }

    std::cout << "world smoke test passed" << std::endl;
    return 0;
}
