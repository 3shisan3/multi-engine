#ifndef ME_CONFIG_SERVICE_H
#define ME_CONFIG_SERVICE_H

#include <memory>
#include <string>
#include <vector>

namespace ConfigModule
{

class IConfigService
{
public:
    virtual ~IConfigService() = default;

    virtual bool LoadFile(const std::string& path) = 0;
    virtual bool LoadString(const std::string& content) = 0;
    virtual bool Has(const std::string& key) const = 0;

    virtual std::string GetString(const std::string& key, const std::string& defaultValue = "") const = 0;
    virtual int GetInt(const std::string& key, int defaultValue = 0) const = 0;
    virtual double GetDouble(const std::string& key, double defaultValue = 0.0) const = 0;
    virtual bool GetBool(const std::string& key, bool defaultValue = false) const = 0;
    virtual std::vector<std::string> GetStringArray(const std::string& key) const = 0;
    virtual std::string GetSectionJson(const std::string& key) const = 0;
    virtual std::string GetLastError() const = 0;
};

std::shared_ptr<IConfigService> CreateConfigService();

} // namespace ConfigModule

#endif // ME_CONFIG_SERVICE_H