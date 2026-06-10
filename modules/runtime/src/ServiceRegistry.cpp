#include "service_registry_impl.h"

namespace RuntimeModule
{

bool ServiceRegistryImpl::RegisterService(const std::string& name, std::shared_ptr<void> service)
{
    if (name.empty() || !service)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (services_.find(name) != services_.end())
    {
        return false;
    }
    services_[name] = std::move(service);
    return true;
}

std::shared_ptr<void> ServiceRegistryImpl::GetService(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = services_.find(name);
    return it == services_.end() ? nullptr : it->second;
}

bool ServiceRegistryImpl::HasService(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return services_.find(name) != services_.end();
}

bool ServiceRegistryImpl::RemoveService(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return services_.erase(name) > 0;
}

std::shared_ptr<IServiceRegistry> CreateServiceRegistry()
{
    return std::make_shared<ServiceRegistryImpl>();
}

} // namespace RuntimeModule