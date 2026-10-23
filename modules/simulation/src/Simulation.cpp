/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        Simulation.cpp
Description: 仿真管理模块实现

*****************************************************************/

#include "simulation_impl.h"

#include <chrono>

using namespace std::chrono;

namespace SimulationModule
{

namespace
{

int64_t NowMs()
{
    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch())
        .count();
}

} // namespace

std::string SimulationStateName(SimulationState state)
{
    switch (state)
    {
        case SimulationState::Uninitialized: return "Uninitialized";
        case SimulationState::Ready:         return "Ready";
        case SimulationState::Running:       return "Running";
        case SimulationState::Paused:        return "Paused";
        case SimulationState::Stopped:       return "Stopped";
        case SimulationState::Error:         return "Error";
    }
    return "Unknown";
}

bool SimulationImpl::Initialize(const SimulationConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 运行中或暂停时不允许重新初始化，其余状态均可（含 Ready/Stopped）
    if (state_ == SimulationState::Running || state_ == SimulationState::Paused)
    {
        return false;
    }

    config_ = config;
    if (config_.timeScale <= 0.0)
    {
        config_.timeScale = 1.0;
    }
    if (config_.fixedTimeStep <= 0.0)
    {
        config_.fixedTimeStep = 1.0 / 60.0;
    }

    simulationTime_ = 0.0;
    statistics_ = SimulationStatistics{};
    hasLastStep_ = false;
    state_ = SimulationState::Ready;
    return true;
}

bool SimulationImpl::Start()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != SimulationState::Ready && state_ != SimulationState::Stopped)
    {
        return false;
    }
    state_ = SimulationState::Running;
    return true;
}

bool SimulationImpl::Pause()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != SimulationState::Running)
    {
        return false;
    }
    state_ = SimulationState::Paused;
    return true;
}

bool SimulationImpl::Resume()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != SimulationState::Paused)
    {
        return false;
    }
    state_ = SimulationState::Running;
    return true;
}

bool SimulationImpl::Stop()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == SimulationState::Uninitialized)
    {
        return false;
    }
    state_ = SimulationState::Stopped;
    return true;
}

bool SimulationImpl::Reset()
{
    std::lock_guard<std::mutex> lock(mutex_);

    simulationTime_ = 0.0;
    statistics_ = SimulationStatistics{};
    hasLastStep_ = false;
    state_ = SimulationState::Ready;
    return true;
}

bool SimulationImpl::Step(double realDeltaTime)
{
    if (realDeltaTime < 0.0)
    {
        realDeltaTime = 0.0;
    }

    double simulationDelta = 0.0;
    std::vector<StepHandler> handlers;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != SimulationState::Running)
        {
            return false;
        }

        simulationDelta = config_.useFixedTimeStep ? config_.fixedTimeStep : realDeltaTime;
        simulationDelta *= config_.timeScale;

        simulationTime_ += simulationDelta;

        // 统计
        statistics_.stepCount++;
        statistics_.totalSimulationTime += simulationDelta;

        const int64_t now = NowMs();
        if (hasLastStep_)
        {
            const double stepMs = static_cast<double>(now - lastStepTimestamp_);
            statistics_.lastStepTimeMs = stepMs;
            statistics_.totalRealTime += stepMs;

            if (statistics_.stepCount == 1)
            {
                statistics_.minStepTimeMs = stepMs;
                statistics_.maxStepTimeMs = stepMs;
            }
            else
            {
                if (stepMs < statistics_.minStepTimeMs)
                {
                    statistics_.minStepTimeMs = stepMs;
                }
                if (stepMs > statistics_.maxStepTimeMs)
                {
                    statistics_.maxStepTimeMs = stepMs;
                }
            }
            statistics_.averageStepTimeMs =
                statistics_.totalRealTime / static_cast<double>(statistics_.stepCount);
        }
        lastStepTimestamp_ = now;
        hasLastStep_ = true;

        // 拷贝回调，避免在锁内执行业务逻辑
        handlers = stepHandlers_;

        if (config_.maxSimulationTimeMs > 0 &&
            simulationTime_ * 1000.0 >= static_cast<double>(config_.maxSimulationTimeMs))
        {
            state_ = SimulationState::Stopped;
        }
    }

    for (const auto& handler : handlers)
    {
        if (handler.callback)
        {
            handler.callback(simulationDelta);
        }
    }

    return true;
}

SimulationState SimulationImpl::GetState() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

double SimulationImpl::GetSimulationTime() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return simulationTime_;
}

double SimulationImpl::GetTimeScale() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.timeScale;
}

void SimulationImpl::SetTimeScale(double scale)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (scale > 0.0)
    {
        config_.timeScale = scale;
    }
}

SimulationConfig SimulationImpl::GetConfig() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

SimulationStatistics SimulationImpl::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return statistics_;
}

void SimulationImpl::RegisterStepHandler(
    const std::string& name,
    std::function<void(double simulationDelta)> handler)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& entry : stepHandlers_)
    {
        if (entry.name == name)
        {
            entry.callback = std::move(handler);
            return;
        }
    }

    stepHandlers_.push_back({name, std::move(handler)});
}

void SimulationImpl::UnregisterStepHandler(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = stepHandlers_.begin(); it != stepHandlers_.end(); ++it)
    {
        if (it->name == name)
        {
            stepHandlers_.erase(it);
            return;
        }
    }
}

std::shared_ptr<ISimulation> CreateSimulation()
{
    return std::make_shared<SimulationImpl>();
}

} // namespace SimulationModule
