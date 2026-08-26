/**
 * Lightweight proportional feedback used by the FirstOrderDubinsBicycle Tube-MPPI rollouts.
 *
 * The filename is retained because it is the package-local override included by the existing
 * controller wiring. The implementation is intentionally no longer a zero controller.
 */
#pragma once

#include <Eigen/Core>
#include <mppi/feedback_controllers/feedback.cuh>

#include <cmath>

struct FirstOrderDubinsFeedbackGains
{
  float velocity{0.5F};
  float acceleration{0.2F};
  float lateral_position{0.8F};
  float yaw{1.0F};
  float steering{0.5F};
};

/**
 * Compute a stabilizing correction around a nominal FirstOrderDubinsBicycle state.
 * State indices intentionally mirror FirstOrderDubinsBicycleParams::StateIndex without including
 * the dynamics header (which also uses this helper).
 */
__host__ __device__ inline void computeFirstOrderDubinsFeedback(
  const FirstOrderDubinsFeedbackGains & gains, const float * x_act, const float * x_goal,
  float * control_output)
{
  constexpr int kVelocityIndex = 0;
  constexpr int kYawIndex = 1;
  constexpr int kPositionXIndex = 2;
  constexpr int kPositionYIndex = 3;
  constexpr int kSteeringIndex = 4;
  constexpr int kAccelerationIndex = 5;
  constexpr int kAccelerationControlIndex = 0;
  constexpr int kSteeringControlIndex = 1;

  const float goal_yaw = x_goal[kYawIndex];
  const float dx = x_act[kPositionXIndex] - x_goal[kPositionXIndex];
  const float dy = x_act[kPositionYIndex] - x_goal[kPositionYIndex];
  const float lateral_error = -sinf(goal_yaw) * dx + cosf(goal_yaw) * dy;
  const float yaw_error =
    atan2f(sinf(x_act[kYawIndex] - goal_yaw), cosf(x_act[kYawIndex] - goal_yaw));

  control_output[kAccelerationControlIndex] =
    gains.velocity * (x_goal[kVelocityIndex] - x_act[kVelocityIndex]) +
    gains.acceleration * (x_goal[kAccelerationIndex] - x_act[kAccelerationIndex]);
  control_output[kSteeringControlIndex] =
    -gains.lateral_position * lateral_error - gains.yaw * yaw_error +
    gains.steering * (x_goal[kSteeringIndex] - x_act[kSteeringIndex]);
}

struct FirstOrderDubinsFeedbackState
{
  FirstOrderDubinsFeedbackGains gains{};
};

template <class DYN_T, int NUM_TIMESTEPS>
class FirstOrderDubinsFeedbackImpl
: public GPUFeedbackController<
    FirstOrderDubinsFeedbackImpl<DYN_T, NUM_TIMESTEPS>, DYN_T, FirstOrderDubinsFeedbackState>
{
public:
  using Parent = GPUFeedbackController<
    FirstOrderDubinsFeedbackImpl<DYN_T, NUM_TIMESTEPS>, DYN_T, FirstOrderDubinsFeedbackState>;

  explicit FirstOrderDubinsFeedbackImpl(cudaStream_t stream = 0) : Parent(stream) {}

  __device__ void k(
    const float * __restrict__ x_act, const float * __restrict__ x_goal, const int,
    float * __restrict__, float * __restrict__ control_output)
  {
    computeFirstOrderDubinsFeedback(this->state_.gains, x_act, x_goal, control_output);
  }
};

template <class DYN_T, int NUM_TIMESTEPS>
class FirstOrderDubinsFeedback : public FeedbackController<
                                   FirstOrderDubinsFeedbackImpl<DYN_T, NUM_TIMESTEPS>,
                                   FirstOrderDubinsFeedbackGains, NUM_TIMESTEPS>
{
public:
  using Parent = FeedbackController<
    FirstOrderDubinsFeedbackImpl<DYN_T, NUM_TIMESTEPS>, FirstOrderDubinsFeedbackGains,
    NUM_TIMESTEPS>;
  using control_array = typename Parent::control_array;
  using state_array = typename Parent::state_array;
  using FeedbackState = typename Parent::TEMPLATED_FEEDBACK_STATE;

  explicit FirstOrderDubinsFeedback(DYN_T * = nullptr, const float dt = 0.01F)
  : Parent(dt, NUM_TIMESTEPS)
  {
  }

  void initTrackingController() override { syncState(); }

  void setParams(const FirstOrderDubinsFeedbackGains & params) override
  {
    Parent::setParams(params);
    syncState();
  }

  control_array k_(
    const Eigen::Ref<const state_array> & x_act, const Eigen::Ref<const state_array> & x_goal, int,
    FeedbackState & state) override
  {
    control_array correction = control_array::Zero();
    computeFirstOrderDubinsFeedback(state.gains, x_act.data(), x_goal.data(), correction.data());
    return correction;
  }

  void computeFeedback(
    const Eigen::Ref<const state_array> & init_state,
    const Eigen::Ref<const typename Parent::state_trajectory> & goal_traj,
    const Eigen::Ref<const typename Parent::control_trajectory> & control_traj) override
  {
    (void)init_state;
    (void)goal_traj;
    (void)control_traj;
    syncState();
  }

private:
  void syncState()
  {
    FirstOrderDubinsFeedbackState state;
    state.gains = this->params_;
    this->setFeedbackState(state);
  }
};

// Compatibility alias for downstream code that still uses the original API name.
template <class DYN_T, int NUM_TIMESTEPS>
using ZeroFeedbackImpl = FirstOrderDubinsFeedbackImpl<DYN_T, NUM_TIMESTEPS>;

template <class DYN_T, int NUM_TIMESTEPS>
using ZeroFeedback = FirstOrderDubinsFeedback<DYN_T, NUM_TIMESTEPS>;
