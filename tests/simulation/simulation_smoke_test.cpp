/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        simulation_smoke_test.cpp
Description: 仿真管理模块冒烟测试

*****************************************************************/

#include "Simulation.h"

#include <cmath>
#include <iostream>
#include <string>

using namespace SimulationModule;

namespace
{

bool NearlyEqual(double a, double b, double epsilon = 1e-6)
{
    return std::fabs(a - b) < epsilon;
}

} // namespace

int main()
{
    auto sim = CreateSimulation();
    if (!sim)
    {
        return 1;
    }

    // 1. 初始状态
    if (sim->GetState() != SimulationState::Uninitialized)
    {
        std::cerr << "unexpected initial state" << std::endl;
        return 2;
    }

    // 2. 初始化 -> Ready
    SimulationConfig config;
    config.timeScale = 2.0;
    if (!sim->Initialize(config))
    {
        std::cerr << "initialize failed" << std::endl;
        return 3;
    }
    if (sim->GetState() != SimulationState::Ready)
    {
        std::cerr << "expected Ready" << std::endl;
        return 4;
    }
    if (!NearlyEqual(sim->GetTimeScale(), 2.0))
    {
        std::cerr << "time scale mismatch" << std::endl;
        return 5;
    }

    // 3. 启动 -> Running
    if (!sim->Start())
    {
        std::cerr << "start failed" << std::endl;
        return 6;
    }
    if (sim->GetState() != SimulationState::Running)
    {
        std::cerr << "expected Running" << std::endl;
        return 7;
    }

    // 4. 注册 step 回调
    int callbackCount = 0;
    double callbackSum = 0.0;
    sim->RegisterStepHandler("counter", [&](double dt) {
        ++callbackCount;
        callbackSum += dt;
    });

    // 5. 推进：realDelta=1s, timeScale=2 -> 每步 +2s
    sim->Step(1.0);
    sim->Step(1.0);
    if (!NearlyEqual(sim->GetSimulationTime(), 4.0))
    {
        std::cerr << "simulation time mismatch: " << sim->GetSimulationTime() << std::endl;
        return 8;
    }
    if (callbackCount != 2 || !NearlyEqual(callbackSum, 4.0))
    {
        std::cerr << "callback mismatch" << std::endl;
        return 9;
    }

    // 6. 统计
    SimulationStatistics stats = sim->GetStatistics();
    if (stats.stepCount != 2 || !NearlyEqual(stats.totalSimulationTime, 4.0))
    {
        std::cerr << "statistics mismatch" << std::endl;
        return 10;
    }

    // 7. 暂停：暂停时 Step 应返回 false 且不推进
    if (!sim->Pause())
    {
        std::cerr << "pause failed" << std::endl;
        return 11;
    }
    if (sim->GetState() != SimulationState::Paused)
    {
        std::cerr << "expected Paused" << std::endl;
        return 12;
    }
    if (sim->Step(1.0))
    {
        std::cerr << "step should fail while paused" << std::endl;
        return 13;
    }

    // 8. 恢复
    if (!sim->Resume() || sim->GetState() != SimulationState::Running)
    {
        std::cerr << "resume failed" << std::endl;
        return 14;
    }

    // 9. 修改时间倍率
    sim->SetTimeScale(0.5);
    sim->Step(2.0); // 2 * 0.5 = 1s
    if (!NearlyEqual(sim->GetSimulationTime(), 5.0))
    {
        std::cerr << "time scale change mismatch" << std::endl;
        return 15;
    }

    // 10. 停止
    if (!sim->Stop() || sim->GetState() != SimulationState::Stopped)
    {
        std::cerr << "stop failed" << std::endl;
        return 16;
    }

    // 11. 重置
    if (!sim->Reset())
    {
        std::cerr << "reset failed" << std::endl;
        return 17;
    }
    if (sim->GetState() != SimulationState::Ready || sim->GetSimulationTime() != 0.0)
    {
        std::cerr << "reset mismatch" << std::endl;
        return 18;
    }

    // 12. 固定步长模式
    SimulationConfig fixedConfig;
    fixedConfig.useFixedTimeStep = true;
    fixedConfig.fixedTimeStep = 0.5;
    fixedConfig.timeScale = 1.0;
    if (!sim->Initialize(fixedConfig) || !sim->Start())
    {
        std::cerr << "fixed timestep init failed" << std::endl;
        return 19;
    }
    sim->Step(1000.0); // 忽略真实时间，使用固定步长
    if (!NearlyEqual(sim->GetSimulationTime(), 0.5))
    {
        std::cerr << "fixed timestep mismatch: " << sim->GetSimulationTime() << std::endl;
        return 20;
    }

    // 13. 最大仿真时间触发停止
    sim->Stop(); // 先停止，便于重新初始化
    SimulationConfig limitedConfig;
    limitedConfig.maxSimulationTimeMs = 1000; // 1 秒
    limitedConfig.fixedTimeStep = 0.5;
    limitedConfig.useFixedTimeStep = true;
    sim->Initialize(limitedConfig);
    sim->Start();
    sim->Step(0.0);
    sim->Step(0.0); // 累计 1.0s，达到上限
    if (sim->GetState() != SimulationState::Stopped)
    {
        std::cerr << "expected auto stop at max simulation time" << std::endl;
        return 21;
    }

    // 14. 状态名
    if (SimulationStateName(SimulationState::Running) != "Running")
    {
        std::cerr << "state name mismatch" << std::endl;
        return 22;
    }

    // 15. 注销回调
    sim->Reset();
    sim->Start();
    sim->UnregisterStepHandler("counter");
    if (sim->GetState() != SimulationState::Running)
    {
        std::cerr << "state mismatch after unregister" << std::endl;
        return 23;
    }

    std::cout << "simulation smoke test passed" << std::endl;
    return 0;
}
