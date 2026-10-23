/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        Simulation.h
Version:     1.0
Author:      cjx
start date: 2026-10-23
Description: 仿真管理模块对外接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-10-23      cjx        create

*****************************************************************/

#ifndef ME_SIMULATION_SIMULATION_H
#define ME_SIMULATION_SIMULATION_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace SimulationModule
{

/**
 * @brief 仿真状态
 */
enum class SimulationState
{
    Uninitialized, // 未初始化
    Ready,         // 就绪
    Running,       // 运行中
    Paused,        // 已暂停
    Stopped,       // 已停止
    Error          // 错误
};

/**
 * @brief 状态名
 */
std::string SimulationStateName(SimulationState state);

/**
 * @brief 仿真配置
 */
struct SimulationConfig
{
    double timeScale = 1.0;             // 时间倍率
    double fixedTimeStep = 1.0 / 60.0;  // 固定时间步长（秒）
    bool useFixedTimeStep = false;      // 是否使用固定步长
    int64_t maxSimulationTimeMs = 0;    // 最大仿真时间（毫秒），0 表示不限制
};

/**
 * @brief 仿真统计
 */
struct SimulationStatistics
{
    uint64_t stepCount = 0;          // 已执行步数
    double totalSimulationTime = 0.0; // 累计仿真时间（秒）
    double totalRealTime = 0.0;      // 累计实时耗时（毫秒）
    double lastStepTimeMs = 0.0;     // 上一步步长（毫秒）
    double averageStepTimeMs = 0.0;  // 平均步长（毫秒）
    double minStepTimeMs = 0.0;      // 最小步长（毫秒）
    double maxStepTimeMs = 0.0;      // 最大步长（毫秒）
};

/**
 * @brief 仿真管理器接口
 *
 * 负责仿真生命周期、状态机、时间推进与统计。
 * 线程安全。
 */
class ISimulation
{
public:
    virtual ~ISimulation() = default;

    virtual bool Initialize(const SimulationConfig& config) = 0;
    virtual bool Start() = 0;
    virtual bool Pause() = 0;
    virtual bool Resume() = 0;
    virtual bool Stop() = 0;
    virtual bool Reset() = 0;

    /**
     * @brief 推进一步
     * @param realDeltaTime 真实时间增量（秒）
     * @return 运行中返回 true，其他状态返回 false
     */
    virtual bool Step(double realDeltaTime) = 0;

    virtual SimulationState GetState() const = 0;
    virtual double GetSimulationTime() const = 0;

    virtual double GetTimeScale() const = 0;
    virtual void SetTimeScale(double scale) = 0;

    virtual SimulationConfig GetConfig() const = 0;
    virtual SimulationStatistics GetStatistics() const = 0;

    /** @brief 注册每步回调 */
    virtual void RegisterStepHandler(const std::string& name,
                                     std::function<void(double simulationDelta)> handler) = 0;

    /** @brief 注销每步回调 */
    virtual void UnregisterStepHandler(const std::string& name) = 0;
};

/**
 * @brief 创建仿真管理器实例
 */
std::shared_ptr<ISimulation> CreateSimulation();

} // namespace SimulationModule

#endif // ME_SIMULATION_SIMULATION_H
