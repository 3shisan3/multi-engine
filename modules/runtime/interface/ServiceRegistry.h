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