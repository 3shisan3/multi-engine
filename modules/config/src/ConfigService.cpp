#include "config_service_impl.h"

#include <fstream>
#include <sstream>

namespace ConfigModule
{

bool ConfigServiceImpl::LoadFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        lastError_ = "failed to open config file: " + path;
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return LoadString(buffer.str());
}

bool ConfigServiceImpl::LoadString(const std::string& content)
{
    try
    {
        root_ = nlohmann::json::parse(content);
        lastError_.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        lastError_ = e.what();
        return false;
    }
}

bool ConfigServiceImpl::Has(const std::string& key) const
{
    return FindNode(key) != nullptr;
}

std::string ConfigServiceImpl::GetString(const std::string& key, const std::string& defaultValue) const
{
    auto node = FindNode(key);
    if (!node || !node->is_string())
    {
        return defaultValue;
    }
    return node->get<std::string>();
}

int ConfigServiceImpl::GetInt(const std::string& key, int defaultValue) const
{
    auto node = FindNode(key);
    if (!node || !node->is_number_integer())
    {
        return defaultValue;
    }
    return node->get<int>();
}

double ConfigServiceImpl::GetDouble(const std::string& key, double defaultValue) const
{
    auto node = FindNode(key);
    if (!node || !node->is_number())
    {
        return defaultValue;
    }
    return node->get<double>();
}

bool ConfigServiceImpl::GetBool(const std::string& key, bool defaultValue) const
{
    auto node = FindNode(key);
    if (!node || !node->is_boolean())
    {
        return defaultValue;
    }
    return node->get<bool>();
}

std::vector<std::string> ConfigServiceImpl::GetStringArray(const std::string& key) const
{
    std::vector<std::string> values;
    auto node = FindNode(key);
    if (!node || !node->is_array())
    {
        return values;
    }

    for (const auto& item : *node)
    {
        if (item.is_string())
        {
            values.push_back(item.get<std::string>());
        }
    }
    return values;
}

std::string ConfigServiceImpl::GetSectionJson(const std::string& key) const
{
    auto node = FindNode(key);
    if (!node)
    {
        return "{}";
    }
    return node->dump();
}

std::string ConfigServiceImpl::GetLastError() const
{
    return lastError_;
}

const nlohmann::json* ConfigServiceImpl::FindNode(const std::string& key) const
{
    if (key.empty())
    {
        return &root_;
    }

    const nlohmann::json* current = &root_;
    size_t start = 0;
    while (start < key.size())
    {
        auto end = key.find('.', start);
        auto part = key.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!current->is_object() || !current->contains(part))
        {
            return nullptr;
        }
        current = &current->at(part);
        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return current;
}

std::shared_ptr<IConfigService> CreateConfigService()
{
    return std::make_shared<ConfigServiceImpl>();
}

} // namespace ConfigModule