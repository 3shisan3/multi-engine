#ifndef ME_RESOURCE_RESOURCE_H
#define ME_RESOURCE_RESOURCE_H

#include <any>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ResourceModule
{

using ResourceId = std::string;
using ResourceType = std::string;

/**
 * @brief 资源统计信息
 */
struct ResourceStats
{
    size_t cachedCount = 0;     // 当前缓存中的资源数
    size_t activeCount = 0;     // 正被外部引用的资源数
    size_t loadAttempts = 0;    // 累计加载尝试次数
    size_t loadFailures = 0;    // 累计加载失败次数
    size_t providerCount = 0;   // 已注册的资源提供者数
};

/**
 * @brief 资源接口
 *
 * 具体资源（场景、模型、纹理、配置等）继承此接口。
 */
class IResource
{
public:
    virtual ~IResource() = default;

    virtual ResourceId GetId() const = 0;
    virtual ResourceType GetType() const = 0;
};

/**
 * @brief 资源提供者接口
 *
 * 负责加载某一类型的资源，由管理器按类型分派。
 */
class IResourceProvider
{
public:
    virtual ~IResourceProvider() = default;

    /**
     * @brief 加载资源
     * @param id 资源标识
     * @param params 加载参数
     * @return 成功返回资源实例，失败返回 nullptr
     */
    virtual std::shared_ptr<IResource> Load(
        const ResourceId& id,
        const std::map<std::string, std::any>& params) = 0;

    /**
     * @brief 提供者所能处理的资源类型
     */
    virtual ResourceType GetType() const = 0;

    /**
     * @brief 提供者名称
     */
    virtual std::string GetName() const = 0;
};

/**
 * @brief 资源管理器接口
 *
 * 统一注册资源提供者、按 ID 缓存资源、按引用计数回收。
 * 线程安全，可在多线程环境下使用。
 */
class IResourceManager
{
public:
    virtual ~IResourceManager() = default;

    /** @brief 注册资源提供者，同一类型重复注册返回 false */
    virtual bool RegisterProvider(std::shared_ptr<IResourceProvider> provider) = 0;

    /** @brief 注销资源提供者 */
    virtual bool UnregisterProvider(const ResourceType& type) = 0;

    /**
     * @brief 获取资源：命中缓存则直接返回，否则分派 provider 加载并入库
     * @return 成功返回资源实例，失败返回 nullptr
     */
    virtual std::shared_ptr<IResource> Acquire(
        const ResourceId& id,
        const std::map<std::string, std::any>& params = {}) = 0;

    /**
     * @brief 仅从缓存获取，不触发加载
     */
    virtual std::shared_ptr<IResource> Get(const ResourceId& id) const = 0;

    /**
     * @brief 从缓存移除指定资源
     */
    virtual bool Release(const ResourceId& id) = 0;

    /**
     * @brief 释放所有仅被缓存持有（外部无引用）的资源
     * @return 释放的数量
     */
    virtual size_t ReleaseUnused() = 0;

    /** @brief 清空缓存与统计 */
    virtual void Clear() = 0;

    /** @brief 缓存的资源 ID 列表 */
    virtual std::vector<ResourceId> GetCachedIds() const = 0;

    /** @brief 是否已缓存某资源 */
    virtual bool IsCached(const ResourceId& id) const = 0;

    /** @brief 获取统计信息 */
    virtual ResourceStats GetStatistics() const = 0;

    /** @brief 获取指定 ID 的缓存引用计数（不存在返回 0） */
    virtual long GetCacheUseCount(const ResourceId& id) const = 0;
};

/**
 * @brief 创建资源管理器实例
 */
std::shared_ptr<IResourceManager> CreateResourceManager();

} // namespace ResourceModule

#endif // ME_RESOURCE_RESOURCE_H
