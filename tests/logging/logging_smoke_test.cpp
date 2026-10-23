/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        logging_smoke_test.cpp
Description: 日志与指标统计冒烟测试

*****************************************************************/

#include "Logger.h"
#include "Metrics.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace LogModule;
using namespace std::chrono;

int main()
{
    // 1. 日志初始化
    if (!Logger::InitializeDefault("logs"))
    {
        return 1;
    }
    ME_LOG_INFO(LogCategory::SYSTEM, "logging_smoke_test", "logger ready");

    // 2. 计数器
    auto metrics = CreateMetrics();
    if (!metrics)
    {
        return 2;
    }

    metrics->Increment("requests", 1);
    metrics->Increment("requests", 2);
    metrics->Decrement("requests", 1);
    if (metrics->Get("requests").counter != 2)
    {
        std::cerr << "counter mismatch" << std::endl;
        return 3;
    }

    // 不会低于 0
    metrics->Decrement("requests", 100);
    if (metrics->Get("requests").counter != 0)
    {
        std::cerr << "counter underflow" << std::endl;
        return 4;
    }

    // 3. 仪表盘
    metrics->Gauge("cpu.usage", 45.5);
    if (metrics->Get("cpu.usage").gauge != 45.5)
    {
        std::cerr << "gauge mismatch" << std::endl;
        return 5;
    }

    // 4. 观测值
    metrics->Observe("latency.ms", 10.0);
    metrics->Observe("latency.ms", 20.0);
    metrics->Observe("latency.ms", 30.0);

    MetricValue latency = metrics->Get("latency.ms");
    if (latency.count != 3 || latency.sum != 60.0 ||
        latency.min != 10.0 || latency.max != 30.0)
    {
        std::cerr << "observe mismatch" << std::endl;
        return 6;
    }

    // 5. ScopedTimer
    {
        ScopedTimer timer(*metrics, "step.cost");
        std::this_thread::sleep_for(milliseconds(5));
    }
    if (metrics->Get("step.cost").count != 1)
    {
        std::cerr << "scoped timer not recorded" << std::endl;
        return 7;
    }

    // 6. 序列化 / 反序列化
    std::string snapshot = metrics->Serialize();
    if (snapshot.empty())
    {
        std::cerr << "serialize empty" << std::endl;
        return 8;
    }

    auto restored = CreateMetrics();
    if (!restored->Deserialize(snapshot))
    {
        std::cerr << "deserialize failed" << std::endl;
        return 9;
    }
    if (restored->Get("latency.ms").max != 30.0)
    {
        std::cerr << "restored value mismatch" << std::endl;
        return 10;
    }

    if (restored->Deserialize("not a json"))
    {
        std::cerr << "invalid deserialize should fail" << std::endl;
        return 11;
    }

    // 7. 重置
    metrics->Reset("cpu.usage");
    if (metrics->Get("cpu.usage").count != 0)
    {
        std::cerr << "reset(name) failed" << std::endl;
        return 12;
    }

    // 8. 多线程安全
    auto shared = CreateMetrics();
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&shared]() {
            for (int j = 0; j < 1000; ++j)
            {
                shared->Increment("hits");
            }
        });
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
    if (shared->Get("hits").counter != 4000)
    {
        std::cerr << "thread safety mismatch" << std::endl;
        return 13;
    }

    ME_LOG_INFO(LogCategory::SYSTEM, "logging_smoke_test",
                "metrics ok, hits = {}", shared->Get("hits").counter);

    Logger::GetInstance().Shutdown();
    std::cout << "logging smoke test passed" << std::endl;
    return 0;
}
