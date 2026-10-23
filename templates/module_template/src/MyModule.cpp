#include "my_module_impl.h"

namespace MyModule
{

std::string MyServiceImpl::GetName() const
{
    return "MyService";
}

int MyServiceImpl::GetValue() const
{
    return value_;
}

void MyServiceImpl::SetValue(int value)
{
    value_ = value;
}

std::shared_ptr<IMyService> CreateMyService()
{
    return std::make_shared<MyServiceImpl>();
}

} // namespace MyModule
