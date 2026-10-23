#ifndef ME_WORLD_IMPL_H
#define ME_WORLD_IMPL_H

#include "World.h"

#include <functional>
#include <mutex>
#include <unordered_map>

namespace WorldModule
{

class WorldModelImpl : public IWorldModel
{
public:
    bool AddEntity(std::shared_ptr<IEntity> entity) override;
    bool RemoveEntity(EntityId id) override;
    std::shared_ptr<IEntity> GetEntity(EntityId id) const override;
    std::vector<std::shared_ptr<IEntity>> GetEntities() const override;

    std::vector<std::shared_ptr<IEntity>> QueryByRadius(
        const Vector3& center, double radius) const override;

    std::vector<std::shared_ptr<IEntity>> QueryByType(const std::string& type) const override;

    size_t GetEntityCount() const override;
    void Clear() override;

    WorldSnapshot CaptureSnapshot() override;
    bool ApplySnapshot(const WorldSnapshot& snapshot) override;

    WorldStatistics GetStatistics() const override;

    void SetEntityChangeCallback(
        std::function<void(EntityId id, const std::string& type, bool added)> callback) override;

private:
    void NotifyChange(EntityId id, const std::string& type, bool added);

    mutable std::mutex mutex_;
    std::unordered_map<EntityId, std::shared_ptr<IEntity>> entities_;
    WorldStatistics statistics_;
    uint64_t nextSnapshotId_ = 1;
    std::function<void(EntityId, const std::string&, bool)> changeCallback_;
};

} // namespace WorldModule

#endif // ME_WORLD_IMPL_H
