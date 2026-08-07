#ifndef BEAST_EVALUATORS_TASK_WORLD_EVALUATOR_HPP_
#define BEAST_EVALUATORS_TASK_WORLD_EVALUATOR_HPP_

// Standard
#include <atomic>
#include <cstdint>
#include <vector>

// Maze
#include <maze/task_world.hpp>

// BEAST
#include <beast/evaluator.hpp>

namespace beast {

/**
 * @class TaskWorldEvaluator
 * @brief Scores a program's ability to autonomously navigate a task-bearing grid world.
 *
 * Each evaluation steps the program through one or more freshly generated maze::TaskWorld
 * instances. The program perceives a line-of-sight window of tile codes plus a handful of
 * scalar sensors (food, keys held, items remaining, a coarse goal compass) as VM Input
 * variables, and issues moves through a single Output variable -- exactly the host<->program
 * membrane pattern used by MazeEvaluator.
 *
 * Worlds are addressed by a reproducible seed policy so that training and verification can
 * draw from disjoint seed pools (same size/difficulty, worlds the agent has never seen).
 * Fitness is milestone-shaped: partial credit for closing distance, collecting items, and
 * opening doors, with the bulk of the reward reserved for completing the task and a bonus
 * for doing so efficiently relative to the optimal plan.
 */
class TaskWorldEvaluator : public Evaluator {
 public:
  /**
   * @struct Config
   * @brief All parameters governing world generation, the agent interface, and the seed pool.
   */
  struct Config {
    uint32_t rows = 9;             ///< World rows.
    uint32_t cols = 9;             ///< World columns.
    double difficulty = 0.2;       ///< Extra wall density in [0, 1].
    uint32_t num_items = 2;        ///< Collectible objectives.
    uint32_t num_keys = 0;         ///< Keys placed in the world.
    uint32_t num_doors = 0;        ///< Locked doors gating the goal.
    uint32_t num_food = 2;         ///< Food tiles.
    uint32_t radius = 2;           ///< Perception radius (window is 2*radius+1 per side).
    uint32_t max_steps = 2000;     ///< VM step budget per world.
    uint32_t worlds_per_eval = 2;  ///< Worlds averaged per evaluate() call (variance control).
    uint32_t starting_food = 0;    ///< Starting food; 0 selects the world default.
    bool goal_compass = true;      ///< Feed the global goal-bearing sensors; off = partial obs.
    uint64_t seed_base = 1;        ///< Base for the seed pool draws.
    uint32_t pool_modulus = 5;     ///< Seeds are partitioned by (seed % pool_modulus).
    std::vector<uint32_t> pool_residues{1, 2, 3, 4};  ///< Residues this evaluator may draw.
  };

  /**
   * @struct EpisodeResult
   * @brief The structured outcome of running the program through a single world.
   */
  struct EpisodeResult {
    bool task_complete = false;   ///< All items collected AND agent on the goal.
    bool reached_goal = false;    ///< Agent stood on the goal at least once at episode end.
    uint32_t items_collected = 0;
    uint32_t items_total = 0;
    uint32_t doors_opened = 0;
    uint32_t doors_total = 0;
    uint32_t moves_used = 0;      ///< Successful moves the agent made.
    uint32_t invalid_moves = 0;   ///< Blocked-move attempts (no-ops).
    uint32_t cells_visited = 0;   ///< Distinct cells entered; drives the exploration bonus.
    uint32_t reference_moves = 0; ///< Optimal moves to complete the task (planTask()).
    bool starved = false;         ///< Episode ended because food hit zero.
    bool invalid_move = false;    ///< At least one blocked move was attempted.
    uint32_t initial_distance = 0;///< Distance to the first objective at spawn.
    uint32_t final_distance = 0;  ///< Distance to the next objective at episode end.
    double score = 0.0;           ///< Milestone-shaped fitness in [0, 1].
  };

  /**
   * @brief Constructs the evaluator with the given configuration.
   */
  explicit TaskWorldEvaluator(Config config);

  /**
   * @brief Averages the milestone score over `worlds_per_eval` worlds drawn from the pool.
   */
  [[nodiscard]] double evaluate(const VmSession& session) override;

  /**
   * @brief Runs the program through exactly one world identified by `seed`.
   *
   * Deterministic given the seed and program; used by both evaluate() (random pool draws)
   * and the verification harness (enumerated held-out seeds).
   */
  [[nodiscard]] EpisodeResult runEpisode(const VmSession& session, uint64_t seed) const;

  /**
   * @brief Pure scoring function mapping an EpisodeResult to a fitness in [0, 1].
   */
  [[nodiscard]] static double scoreEpisode(const EpisodeResult& result);

  /**
   * @brief Draws a pseudo-random seed from this evaluator's pool (thread-safe).
   */
  [[nodiscard]] uint64_t drawPoolSeed() const;

  /**
   * @brief Returns the deterministic k-th seed of this evaluator's pool.
   */
  [[nodiscard]] uint64_t enumeratedPoolSeed(uint64_t index) const;

  [[nodiscard]] const Config& getConfig() const { return config_; }

 private:
  Config config_;
  mutable std::atomic<uint64_t> draw_counter_{0};
};

}  // namespace beast

#endif  // BEAST_EVALUATORS_TASK_WORLD_EVALUATOR_HPP_
