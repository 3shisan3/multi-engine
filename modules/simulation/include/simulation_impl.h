#ifndef ME_SIMULATION_IMPL_H
#define ME_SIMULATION_IMPL_H

#include "Simulation.h"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace SimulationModule
{

class SimulationImpl : public ISimulation
{
public:
    bool Initialize(const SimulationConfig& config) override;
    bool Start() override;
    bool Pause() override;
    bool Resume() override;
    bool Stop() override;
    bool Reset() override;

    bool Step(double realDeltaTime) override;

    SimulationState GetState() const override;
    double GetSimulationTime() const override;

    double GetTimeScale() const override;
    void SetTimeScale(double scale) override;

    SimulationConfig GetConfig() const override;
    SimulationStatistics GetStatistics() const override;

    void RegisterStepHandler(const std::string& name,
                             std::function<void(double simulationDelta)> handler) override;
    void UnregisterStepHandler(const std::string& name) override;

private:
    struct StepHandler
    {
        std::string name;
        std::function<void(double)> callback;
    };

    mutable std::mutex mutex_;
    SimulationState state_ = SimulationState::Uninitialized;
    SimulationConfig config_;
    double simulationTime_ = 0.0;

    std::vector<StepHandler> stepHandlers_;
    SimulationStatistics statistics_;
    int64_t lastStepTimestamp_ = 0;
    bool hasLastStep_ = false;
};

} // namespace SimulationModule

#endif // ME_SIMULATION_IMPL_H
