/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        MyModule.h
Version:     1.0
Author:      cjx
start date: 2026-10-23
Description: 模块模板对外接口示例
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-10-23      cjx        create

*****************************************************************/

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
