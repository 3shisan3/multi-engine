#ifndef MY_MODULE_IMPL_H
#define MY_MODULE_IMPL_H

#include "MyModule.h"

namespace MyModule
{

/**
 * @brief 模块内部实现（不对外暴露）
 */
class MyServiceImpl : public IMyService
{
public:
    std::string GetName() const override;
    int GetValue() const override;
    void SetValue(int value) override;

private:
    int value_ = 0;
};

} // namespace MyModule

#endif // MY_MODULE_IMPL_H
