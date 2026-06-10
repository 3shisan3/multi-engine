#ifndef ME_CONFIG_SERVICE_IMPL_H
#define ME_CONFIG_SERVICE_IMPL_H

#include "ConfigService.h"

#include <nlohmann/json.hpp>

#include <string>

namespace ConfigModule
{

class ConfigServiceImpl : public IConfigService
{
public:
    bool LoadFile(const std::string& path) override;
    bool LoadString(const std::string& content) override;
    bool Has(const std::string& key) const override;

    std::string GetString(const std::string& key, const std::string& defaultValue = "") const override;
    int GetInt(const std::string& key, int defaultValue = 0) const override;
    double GetDouble(const std::string& key, double defaultValue = 0.0) const override;
    bool GetBool(const std::string& key, bool defaultValue = false) const override;
    std::vector<std::string> GetStringArray(const std::string& key) const override;
    std::string GetSectionJson(const std::string& key) const override;
    std::string GetLastError() const override;

private:
    const nlohmann::json* FindNode(const std::string& key) const;

    nlohmann::json root_;
    std::string lastError_;
};

} // namespace ConfigModule

#endif // ME_CONFIG_SERVICE_IMPL_H