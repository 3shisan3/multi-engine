#ifndef MY_MODULE_INTERFACE_H
#define MY_MODULE_INTERFACE_H

#include <memory>
#include <string>

namespace MyModule
{

/**
 * @brief 模块对外服务接口示例
 */
class IMyService
{
public:
    virtual ~IMyService() = default;

    virtual std::string GetName() const = 0;
    virtual int GetValue() const = 0;
    virtual void SetValue(int value) = 0;
};

/**
 * @brief 创建服务实例
 */
std::shared_ptr<IMyService> CreateMyService();

} // namespace MyModule

#endif // MY_MODULE_INTERFACE_H
