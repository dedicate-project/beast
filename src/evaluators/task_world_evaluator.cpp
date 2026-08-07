#include <beast/evaluators/task_world_evaluator.hpp>

// Standard
#include <algorithm>
#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

// BEAST
#include <beast/cpu_virtual_machine.hpp>

namespace beast {

namespace {
// Encode a signed delta as a compass code: 0 (target has a smaller coordinate), 1 (equal),
// 2 (target has a larger coordinate). Gives the agent a coarse global bearing on the goal.
uint32_t compass(uint32_t self, uint32_t target) {
  if (target < self) {
    return 0U;
  }
  if (target > self) {
    return 2U;
  }
  return 1U;
}
}  // namespace

TaskWorldEvaluator::TaskWorldEvaluator(Config config) : config_(std::move(config)) {}

uint64_t TaskWorldEvaluator::drawPoolSeed() const {
  const uint64_t counter = draw_counter_.fetch_add(1, std::memory_order_relaxed);
  std::mt19937_64 rng(config_.seed_base ^ (counter * 0x9E3779B97F4A7C15ULL + 0x1ULL));
  uint64_t raw = rng();
  const uint32_t modulus = config_.pool_modulus > 0 ? config_.pool_modulus : 1U;
  if (config_.pool_residues.empty() || modulus == 1U) {
    return raw;
  }
  const uint32_t residue = config_.pool_residues[rng() % config_.pool_residues.size()];
  raw = raw - (raw % modulus) + residue;
  return raw;
}

uint64_t TaskWorldEvaluator::enumeratedPoolSeed(uint64_t index) const {
  const uint32_t modulus = config_.pool_modulus > 0 ? config_.pool_modulus : 1U;
  if (config_.pool_residues.empty() || modulus == 1U) {
    return index;
  }
  const uint64_t block = index / config_.pool_residues.size();
  const uint32_t residue = config_.pool_residues[index % config_.pool_residues.size()];
  return block * modulus + residue;
}

double TaskWorldEvaluator::scoreEpisode(const EpisodeResult& result) {
  // Weighted, renormalised milestone reward. Only components that apply to this world's
  // configuration participate, so e.g. a pure navigation world (no items/doors) isn't
  // handed free credit for objectives that don't exist.
  struct Component {
    double weight;
    double value;
  };
  std::vector<Component> components;
  components.push_back({0.45, result.task_complete ? 1.0 : 0.0});
  if (result.items_total > 0) {
    components.push_back(
        {0.20, static_cast<double>(result.items_collected) / result.items_total});
  }
  if (result.doors_total > 0) {
    components.push_back(
        {0.10, static_cast<double>(result.doors_opened) / result.doors_total});
  }
  double closing = 0.0;
  if (result.initial_distance > 0) {
    closing = 1.0 - static_cast<double>(result.final_distance) /
                        static_cast<double>(result.initial_distance);
  } else {
    closing = result.reached_goal ? 1.0 : 0.0;
  }
  components.push_back({0.15, std::clamp(closing, 0.0, 1.0)});
  double efficiency = 0.0;
  if (result.task_complete && result.moves_used > 0) {
    efficiency = std::clamp(static_cast<double>(result.reference_moves) /
                                static_cast<double>(result.moves_used),
                            0.0, 1.0);
  }
  components.push_back({0.10, efficiency});

  double weight_sum = 0.0;
  double accumulated = 0.0;
  for (const auto& component : components) {
    weight_sum += component.weight;
    accumulated += component.weight * component.value;
  }
  double score = weight_sum > 0.0 ? accumulated / weight_sum : 0.0;

  // Bootstrap gradient: a small, capped bonus for genuinely exploring the world (distinct
  // cells entered, not oscillating in place). Random programs almost never emit a move, so
  // without this a program that moves but does not close on the goal scores exactly 0 -- the
  // same as one that never acts -- and the GA has nothing to select on. Counting *distinct*
  // cells (rather than raw moves) denies credit to a two-cell wiggle. Capped well below the
  // milestone weights so real task progress always dominates exploration.
  if (result.cells_visited > 1) {
    const double denom = static_cast<double>(std::max<uint32_t>(result.reference_moves, 4U));
    const double explored = static_cast<double>(result.cells_visited - 1) / denom;
    score += 0.06 * std::clamp(explored, 0.0, 1.0);
  }
  return std::clamp(score, 0.0, 1.0);
}

TaskWorldEvaluator::EpisodeResult TaskWorldEvaluator::runEpisode(const VmSession& session,
                                                                 uint64_t seed) const {
  EpisodeResult result;

  maze::TaskConfig task_config;
  task_config.rows = config_.rows;
  task_config.cols = config_.cols;
  task_config.difficulty = config_.difficulty;
  task_config.num_items = config_.num_items;
  task_config.num_keys = config_.num_keys;
  task_config.num_doors = config_.num_doors;
  task_config.num_food = config_.num_food;
  task_config.starting_food = config_.starting_food;

  try {
    maze::TaskWorld world(task_config, seed);

    result.items_total = world.getItemsTotal();
    result.doors_total = world.getDoorsTotal();
    try {
      result.reference_moves = world.planTask();
    } catch (const std::exception&) {
      result.reference_moves = config_.max_steps;
    }
    result.initial_distance = world.distanceToNextObjective();

    const uint32_t radius = config_.radius;
    const uint32_t span = radius * 2 + 1;
    const uint32_t grid_inputs = span * span;
    const uint32_t idx_food = grid_inputs;
    const uint32_t idx_keys = grid_inputs + 1;
    const uint32_t idx_items = grid_inputs + 2;
    const uint32_t idx_goal_row = grid_inputs + 3;
    const uint32_t idx_goal_col = grid_inputs + 4;
    const uint32_t idx_goal_visible = grid_inputs + 5;
    const uint32_t idx_move_out = grid_inputs + 6;

    VmSession local_session = session;
    for (uint32_t i = 0; i < grid_inputs; ++i) {
      local_session.setVariableBehavior(i, VmSession::VariableIoBehavior::Input);
    }
    for (uint32_t i = idx_food; i <= idx_goal_visible; ++i) {
      local_session.setVariableBehavior(i, VmSession::VariableIoBehavior::Input);
    }
    local_session.setVariableBehavior(idx_move_out, VmSession::VariableIoBehavior::Output);

    auto feed_observation = [&]() {
      uint32_t index = 0;
      const auto perception = world.perceiveTiles(radius);
      bool goal_visible = false;
      for (const auto& row : perception) {
        for (const auto& tile : row) {
          if (tile == maze::TaskWorld::PerceivedTile::END) {
            goal_visible = true;
          }
          local_session.setVariableValue(static_cast<int32_t>(index), true,
                                         static_cast<int32_t>(tile));
          ++index;
        }
      }
      const auto player = world.getPlayerPosition();
      const auto goal = world.getGoalPosition();
      local_session.setVariableValue(static_cast<int32_t>(idx_food), true,
                                     static_cast<int32_t>(world.getPlayerFood()));
      local_session.setVariableValue(static_cast<int32_t>(idx_keys), true,
                                     static_cast<int32_t>(world.getKeysHeld()));
      local_session.setVariableValue(static_cast<int32_t>(idx_items), true,
                                     static_cast<int32_t>(world.getItemsRemaining()));
      // Partial observability: when the global compass is disabled the two bearing sensors are
      // pinned to a constant so they carry no signal, forcing the agent to search and remember
      // rather than home in on a global bearing. Local goal visibility (below) is untouched --
      // seeing the goal inside the line-of-sight window is legitimate perception, not a crutch.
      const int32_t compass_row =
          config_.goal_compass ? static_cast<int32_t>(compass(player.row, goal.row)) : 0;
      const int32_t compass_col =
          config_.goal_compass ? static_cast<int32_t>(compass(player.col, goal.col)) : 0;
      local_session.setVariableValue(static_cast<int32_t>(idx_goal_row), true, compass_row);
      local_session.setVariableValue(static_cast<int32_t>(idx_goal_col), true, compass_col);
      local_session.setVariableValue(static_cast<int32_t>(idx_goal_visible), true,
                                     goal_visible ? 1 : 0);
    };

    feed_observation();

    CpuVirtualMachine virtual_machine;
    virtual_machine.setSilent(true);
    const uint32_t max_steps = config_.max_steps > 0 ? config_.max_steps : 2000U;
    uint32_t vm_steps = 0;
    bool refresh = false;

    std::unordered_set<uint64_t> visited;
    auto mark_visited = [&]() {
      const auto pos = world.getPlayerPosition();
      visited.insert(static_cast<uint64_t>(pos.row) * config_.cols + pos.col);
    };
    mark_visited();

    // The stepping loop is guarded separately from world setup: a random program will
    // frequently execute an illegal instruction and make the VM throw partway through an
    // episode. That must NOT discard the progress made so far -- otherwise the agents that
    // actually act (the ones the GA needs to select for) get their fitness zeroed, and
    // evolution never leaves the flat all-zero plateau. On a VM fault we simply stop stepping
    // and score whatever state the world reached.
    try {
    while (virtual_machine.step(local_session, false)) {
      if ((vm_steps & 0x3FU) == 0 && local_session.isStopRequested()) {
        break;
      }
      if (local_session.hasOutputDataAvailable(static_cast<int32_t>(idx_move_out), true)) {
        const auto raw_move = local_session.getVariableValue(static_cast<int32_t>(idx_move_out),
                                                             true);
        // Fold the raw output into one of the four moves so any integer the program emits is
        // a legal command; this keeps the action space dense and avoids "wasted" outputs.
        const auto move = static_cast<maze::TaskWorld::Move>(
            (static_cast<uint32_t>(raw_move) & 0x3U));
        if (world.movePlayer(move)) {
          ++result.moves_used;
          mark_visited();
          refresh = true;
        } else {
          // A blocked move (wall, edge, or locked door without a key) is a no-op rather than a
          // fatal event. Terminating the episode on the first bump would collapse the fitness
          // landscape to near-zero everywhere and starve the GA of gradient; instead we let the
          // agent keep acting and record the miss. Blocked moves consume no food, so this
          // cannot be exploited to dodge starvation.
          result.invalid_move = true;
          ++result.invalid_moves;
        }
      }
      if (refresh) {
        feed_observation();
        refresh = false;
      }
      if (world.getPlayerFood() == 0) {
        result.starved = true;
        break;
      }
      if (world.isTaskComplete()) {
        break;
      }
      ++vm_steps;
      if (vm_steps > max_steps) {
        break;
      }
    }
    } catch (const std::exception&) {
      // VM fault mid-episode: keep the progress already made and score it below.
    }

    result.items_collected = world.getItemsCollected();
    result.doors_opened = world.getDoorsOpened();
    result.reached_goal = world.isAtGoal();
    result.task_complete = world.isTaskComplete();
    result.final_distance = world.distanceToNextObjective();
    result.cells_visited = static_cast<uint32_t>(visited.size());
  } catch (const std::exception&) {
    // A malformed configuration (e.g. perception window larger than the VM's variable
    // budget) yields a zero-score episode rather than crashing the whole GA.
    result.score = 0.0;
    return result;
  }

  result.score = scoreEpisode(result);
  return result;
}

double TaskWorldEvaluator::evaluate(const VmSession& session) {
  const uint32_t worlds = config_.worlds_per_eval > 0 ? config_.worlds_per_eval : 1U;
  double total = 0.0;
  for (uint32_t i = 0; i < worlds; ++i) {
    const uint64_t seed = drawPoolSeed();
    total += runEpisode(session, seed).score;
  }
  return total / static_cast<double>(worlds);
}

}  // namespace beast
