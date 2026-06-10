#ifndef ME_RUNTIME_SERVICE_REGISTRY_IMPL_H
#define ME_RUNTIME_SERVICE_REGISTRY_IMPL_H

#include "ServiceRegistry.h"

#include <mutex>
#include <unordered_map>

namespace RuntimeModule
{

class ServiceRegistryImpl : public IServiceRegistry
{
public:
    bool RegisterService(const std::string& name, std::shared_ptr<void> service) override;
    std::shared_ptr<void> GetService(const std::string& name) const override;
    bool HasService(const std::string& name) const override;
    bool RemoveService(const std::string& name) override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<void>> services_;
};

} // namespace RuntimeModule

#endif // ME_RUNTIME_SERVICE_REGISTRY_IMPL_H