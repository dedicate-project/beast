#include <beast/cuda/cuda_task_world_evaluator.hpp>

// CUDA
#include <cuda_runtime.h>
#include <curand_kernel.h>

// Standard
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// Internal
#include <beast/cuda/compiler.hpp>
#include <beast/cuda/instruction.hpp>
#include <maze/task_world.hpp>

namespace beast::cuda {

namespace {

// ------------------------------------------------------------------------------------
// Device sizing constants.
//
// The register file is deliberately larger than the SHA-256 kernel's (32) because the
// TaskWorld membrane consumes a whole perception window plus sensors as Input variables:
// radius 2 already needs 25 grid + 6 sensor + 1 move = 32 slots with zero scratch, so the
// GA needs headroom above that to compute anything. 64 admits radius <= 3 with room to work.
// ------------------------------------------------------------------------------------
constexpr int kTwRegisterFileSize = 64;
constexpr int kMaxCells = 256;   ///< 16x16 upper bound on grid size (per-thread mutable copy).
constexpr int kMaxItems = 8;     ///< Upper bound on collectible items (per-thread bitmask).
constexpr int kVisitedWords = (kMaxCells + 63) / 64;

// Device tile codes (mirror maze::TaskWorldLayout::cell_type, extended with an "unlocked
// door" state the layout export never emits but movePlayer produces at runtime):
//   0 empty, 1 wall, 2 food, 3 key, 4 locked door, 5 item, 6 unlocked door.
// PerceivedTile codes (mirror maze::Maze::PerceivedTile):
//   0 UNKNOWN, 1 EMPTY, 2 WALL, 3 FOOD, 4 DOOR, 5 START, 6 END, 7 KEY, 8 ITEM, 9 LOCKED_DOOR.

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): only way to capture the call site.
#define BEAST_CUDA_CHECK(expr)                                                                    \
  do {                                                                                            \
    const cudaError_t _err = (expr);                                                              \
    if (_err != cudaSuccess) {                                                                    \
      throw std::runtime_error(std::string("CUDA error: ") + cudaGetErrorString(_err) +           \
                               " (" #expr ")");                                                   \
    }                                                                                             \
  } while (0)

__device__ inline double devClamp(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

__device__ inline uint32_t devCompass(int self, int target) {
  if (target < self) {
    return 0U;
  }
  if (target > self) {
    return 2U;
  }
  return 1U;
}

// Bresenham line-of-sight over the (mutable) tile grid; only walls block sight -- exactly
// as maze::TaskWorld::lineOfSight.
__device__ bool devLineOfSight(const uint8_t* tiles, int cols, int sr, int sc, int er, int ec) {
  int diff_row = er > sr ? er - sr : sr - er;
  int diff_col = ec > sc ? ec - sc : sc - ec;
  int step_row = (sr < er) ? 1 : -1;
  int step_col = (sc < ec) ? 1 : -1;
  int error = diff_col - diff_row;
  int r = sr;
  int c = sc;
  while (true) {
    if (r == er && c == ec) {
      break;
    }
    if ((r != sr || c != sc) && tiles[r * cols + c] == 1) {
      return false;
    }
    const int error2 = error * 2;
    if (error2 > -diff_row) {
      error -= diff_row;
      c += step_col;
    }
    if (error2 < diff_col) {
      error += diff_col;
      r += step_row;
    }
  }
  return true;
}

// Manhattan distance to the nearest uncollected item, or to the goal when every item is
// already collected. Mirrors maze::TaskWorld::distanceToNextObjective.
__device__ uint32_t devDistanceToObjective(int prow, int pcol, uint32_t collected_mask,
                                            const uint32_t* item_rows, const uint32_t* item_cols,
                                            int item_count, int goal_row, int goal_col) {
  int best = -1;
  for (int k = 0; k < item_count; ++k) {
    if (((collected_mask >> k) & 1U) == 0U) {
      const int ir = static_cast<int>(item_rows[k]);
      const int ic = static_cast<int>(item_cols[k]);
      const int d = (prow > ir ? prow - ir : ir - prow) + (pcol > ic ? pcol - ic : ic - pcol);
      if (best < 0 || d < best) {
        best = d;
      }
    }
  }
  if (best < 0) {
    return static_cast<uint32_t>((prow > goal_row ? prow - goal_row : goal_row - prow) +
                                 (pcol > goal_col ? pcol - goal_col : goal_col - pcol));
  }
  return static_cast<uint32_t>(best);
}

// Writes the full observation (perception window + scalar sensors) into the VM register file,
// bit-for-bit matching TaskWorldEvaluator::feed_observation.
__device__ void devFeedObservation(int32_t* vars, const uint8_t* tiles, int rows, int cols,
                                    int radius, int span, int prow, int pcol, int start_row,
                                    int start_col, int goal_row, int goal_col, int food, int keys,
                                    int items_remaining, int idx_food, int idx_keys, int idx_items,
                                    int idx_gr, int idx_gc, int idx_gv, bool feed_compass) {
  const int sq_radius = radius * radius;
  const int base_row = prow - radius;
  const int base_col = pcol - radius;
  bool goal_visible = false;
  for (int i = 0; i < span; ++i) {
    for (int j = 0; j < span; ++j) {
      int code = 0; // UNKNOWN
      const int row = base_row + i;
      const int col = base_col + j;
      if (row >= 0 && row < rows && col >= 0 && col < cols) {
        const int dr = col - pcol;
        const int dc = row - prow;
        if (dr * dr + dc * dc <= sq_radius &&
            devLineOfSight(tiles, cols, prow, pcol, row, col)) {
          if (row == start_row && col == start_col) {
            code = 5; // START
          } else if (row == goal_row && col == goal_col) {
            code = 6; // END
            goal_visible = true;
          } else {
            switch (tiles[row * cols + col]) {
              case 1: code = 2; break; // WALL
              case 2: code = 3; break; // FOOD
              case 3: code = 7; break; // KEY
              case 4: code = 9; break; // LOCKED_DOOR
              case 5: code = 8; break; // ITEM
              case 6: code = 4; break; // unlocked DOOR
              default: code = 1; break; // EMPTY
            }
          }
        }
      }
      vars[i * span + j] = code;
    }
  }
  vars[idx_food] = food;
  vars[idx_keys] = keys;
  vars[idx_items] = items_remaining;
  // Partial observability: pin the global bearing sensors to a constant when disabled. Matches
  // TaskWorldEvaluator::feed_observation; local goal visibility is always fed.
  vars[idx_gr] = feed_compass ? static_cast<int32_t>(devCompass(prow, goal_row)) : 0;
  vars[idx_gc] = feed_compass ? static_cast<int32_t>(devCompass(pcol, goal_col)) : 0;
  vars[idx_gv] = goal_visible ? 1 : 0;
}

// Milestone-shaped fitness, identical component weights + exploration bonus to
// TaskWorldEvaluator::scoreEpisode.
__device__ double devScoreEpisode(bool task_complete, uint32_t items_collected,
                                  uint32_t items_total, uint32_t doors_opened,
                                  uint32_t doors_total, uint32_t moves_used,
                                  uint32_t cells_visited, uint32_t reference_moves,
                                  bool reached_goal, uint32_t initial_distance,
                                  uint32_t final_distance) {
  double weights[5];
  double values[5];
  int n = 0;
  weights[n] = 0.45;
  values[n] = task_complete ? 1.0 : 0.0;
  ++n;
  if (items_total > 0) {
    weights[n] = 0.20;
    values[n] = static_cast<double>(items_collected) / static_cast<double>(items_total);
    ++n;
  }
  if (doors_total > 0) {
    weights[n] = 0.10;
    values[n] = static_cast<double>(doors_opened) / static_cast<double>(doors_total);
    ++n;
  }
  double closing;
  if (initial_distance > 0) {
    closing = 1.0 - static_cast<double>(final_distance) / static_cast<double>(initial_distance);
  } else {
    closing = reached_goal ? 1.0 : 0.0;
  }
  weights[n] = 0.15;
  values[n] = devClamp(closing, 0.0, 1.0);
  ++n;
  double efficiency = 0.0;
  if (task_complete && moves_used > 0) {
    efficiency = devClamp(static_cast<double>(reference_moves) / static_cast<double>(moves_used),
                          0.0, 1.0);
  }
  weights[n] = 0.10;
  values[n] = efficiency;
  ++n;

  double weight_sum = 0.0;
  double accumulated = 0.0;
  for (int i = 0; i < n; ++i) {
    weight_sum += weights[i];
    accumulated += weights[i] * values[i];
  }
  double score = weight_sum > 0.0 ? accumulated / weight_sum : 0.0;
  if (cells_visited > 1) {
    const uint32_t ref = reference_moves > 4U ? reference_moves : 4U;
    const double explored = static_cast<double>(cells_visited - 1) / static_cast<double>(ref);
    score += 0.06 * devClamp(explored, 0.0, 1.0);
  }
  return devClamp(score, 0.0, 1.0);
}

// Variable-target jump resolution (mirrors the SHA-256 kernel's helper).
__device__ inline uint32_t resolveVarJumpTarget(int32_t addr_value, bool is_relative,
                                                uint32_t current_byte_offset,
                                                const uint32_t* byte_to_insn,
                                                uint32_t byte_to_insn_len) {
  int64_t target_byte =
      is_relative ? static_cast<int64_t>(current_byte_offset) + addr_value : addr_value;
  if (target_byte < 0) {
    return kNoInstructionMapping;
  }
  const auto target = static_cast<uint32_t>(target_byte);
  if (target >= byte_to_insn_len) {
    return kNoInstructionMapping;
  }
  return byte_to_insn[target];
}

// Kernel parameters, grouped so the launch stays legible.
struct TwKernelParams {
  // Compiled programs (SoA over genomes).
  const Instruction* instructions;
  const uint32_t* genome_offsets;
  const uint32_t* genome_lengths;
  const uint32_t* byte_to_insn;
  const uint32_t* genome_byte_offsets;
  const uint32_t* genome_byte_lengths;
  uint32_t num_genomes;

  // Worlds (uniform grid geometry; per-world payload in the arrays below).
  uint32_t num_worlds;
  uint32_t rows;
  uint32_t cols;
  uint32_t radius;
  uint32_t span;
  uint32_t grid_inputs;
  uint32_t variable_count;
  uint32_t max_steps;
  uint32_t feed_compass;           // Non-zero feeds the global goal-bearing sensors.
  const uint8_t* cell_type;        // [num_worlds * rows*cols]
  const uint16_t* food_weight;     // [num_worlds * rows*cols]
  const uint32_t* start_row;       // [num_worlds]
  const uint32_t* start_col;
  const uint32_t* goal_row;
  const uint32_t* goal_col;
  const uint32_t* items_total;
  const uint32_t* doors_total;
  const uint32_t* reference_moves;
  const uint32_t* starting_food;
  const uint32_t* max_food;
  const uint32_t* item_count;      // [num_worlds]
  const uint32_t* item_rows;       // [num_worlds * kMaxItems]
  const uint32_t* item_cols;

  uint64_t random_seed;
  double* scores_out;              // [num_genomes * num_worlds]
};

// One thread per (genome, world). Simulates the entire episode -- device VM interleaved with
// the environment membrane -- and writes a single milestone score.
__global__ void taskWorldKernel(TwKernelParams p) {
  const uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
  const uint32_t total = p.num_genomes * p.num_worlds;
  if (tid >= total) {
    return;
  }
  const uint32_t g = tid / p.num_worlds;
  const uint32_t w = tid % p.num_worlds;

  const int rows = static_cast<int>(p.rows);
  const int cols = static_cast<int>(p.cols);
  const int cells = rows * cols;
  const int radius = static_cast<int>(p.radius);
  const int span = static_cast<int>(p.span);
  const int grid_inputs = static_cast<int>(p.grid_inputs);
  const int idx_food = grid_inputs;
  const int idx_keys = grid_inputs + 1;
  const int idx_items = grid_inputs + 2;
  const int idx_gr = grid_inputs + 3;
  const int idx_gc = grid_inputs + 4;
  const int idx_gv = grid_inputs + 5;
  const int idx_move_out = grid_inputs + 6;

  // ---- Per-thread mutable world copy ----
  uint8_t tiles[kMaxCells];
  const uint8_t* world_cells = p.cell_type + static_cast<size_t>(w) * cells;
  const uint16_t* world_food = p.food_weight + static_cast<size_t>(w) * cells;
  for (int i = 0; i < cells; ++i) {
    tiles[i] = world_cells[i];
  }

  const int start_row = static_cast<int>(p.start_row[w]);
  const int start_col = static_cast<int>(p.start_col[w]);
  const int goal_row = static_cast<int>(p.goal_row[w]);
  const int goal_col = static_cast<int>(p.goal_col[w]);
  const uint32_t items_total = p.items_total[w];
  const uint32_t doors_total = p.doors_total[w];
  const uint32_t max_food = p.max_food[w];
  const int item_count = static_cast<int>(min(p.item_count[w], static_cast<uint32_t>(kMaxItems)));
  const uint32_t* item_rows = p.item_rows + static_cast<size_t>(w) * kMaxItems;
  const uint32_t* item_cols = p.item_cols + static_cast<size_t>(w) * kMaxItems;
  const uint32_t reference_moves =
      p.reference_moves[w] > 0 ? p.reference_moves[w] : p.max_steps;

  int prow = start_row;
  int pcol = start_col;
  int food = static_cast<int>(p.starting_food[w]);
  uint32_t keys = 0;
  uint32_t items_collected = 0;
  uint32_t doors_opened = 0;
  uint32_t collected_mask = 0;
  uint32_t moves_used = 0;

  uint64_t visited[kVisitedWords];
  for (int i = 0; i < kVisitedWords; ++i) {
    visited[i] = 0ULL;
  }
  auto markVisited = [&](int r, int c) {
    const int cell = r * cols + c;
    visited[cell >> 6] |= (1ULL << (cell & 63));
  };
  markVisited(prow, pcol);

  const uint32_t initial_distance = devDistanceToObjective(prow, pcol, collected_mask, item_rows,
                                                           item_cols, item_count, goal_row,
                                                           goal_col);

  // ---- VM state ----
  int32_t vars[kTwRegisterFileSize];
#pragma unroll
  for (int i = 0; i < kTwRegisterFileSize; ++i) {
    vars[i] = 0;
  }
  const Instruction* insns = p.instructions + p.genome_offsets[g];
  const uint32_t insns_len = p.genome_lengths[g];
  const uint32_t* b2i = p.byte_to_insn + p.genome_byte_offsets[g];
  const uint32_t b2i_len = p.genome_byte_lengths[g];

  uint32_t pc = 0;
  bool terminated = false;
  bool move_pending = false;
  bool refresh = false;
  curandState_t rng_state;
  bool rng_initialised = false;

  const bool feed_compass = p.feed_compass != 0U;
  devFeedObservation(vars, tiles, rows, cols, radius, span, prow, pcol, start_row, start_col,
                     goal_row, goal_col, food, static_cast<int>(keys),
                     static_cast<int>(items_total - items_collected), idx_food, idx_keys,
                     idx_items, idx_gr, idx_gc, idx_gv, feed_compass);

  const uint32_t max_steps = p.max_steps > 0 ? p.max_steps : 2000U;
  uint32_t vm_steps = 0;

  while (!terminated && pc < insns_len) {
    const Instruction insn = insns[pc];
    uint32_t next_pc = pc + 1;
    int written = -1;
    int written2 = -1;

    switch (insn.opcode) {
      case Op_NoOp: break;
      case Op_Terminate: terminated = true; break;

      case Op_SetVarConst: vars[insn.op0] = insn.op1; written = insn.op0; break;
      case Op_CopyVarToVar: vars[insn.op1] = vars[insn.op0]; written = insn.op1; break;
      case Op_SwapVars: {
        const int32_t tmp = vars[insn.op0];
        vars[insn.op0] = vars[insn.op1];
        vars[insn.op1] = tmp;
        written = insn.op0;
        written2 = insn.op1;
        break;
      }

      case Op_AddConstToVar: vars[insn.op0] += insn.op1; written = insn.op0; break;
      case Op_AddVarToVar: vars[insn.op1] += vars[insn.op0]; written = insn.op1; break;
      case Op_SubtractConstFromVar: vars[insn.op0] -= insn.op1; written = insn.op0; break;
      case Op_SubtractVarFromVar: vars[insn.op1] -= vars[insn.op0]; written = insn.op1; break;
      case Op_ModuloConst:
        if (insn.op1 != 0) { vars[insn.op0] %= insn.op1; }
        written = insn.op0;
        break;
      case Op_ModuloVar: {
        const int32_t m = vars[insn.op0];
        if (m != 0) { vars[insn.op1] %= m; }
        written = insn.op1;
        break;
      }

      case Op_GetMaxConst:
        if (insn.op1 > vars[insn.op0]) { vars[insn.op0] = insn.op1; }
        written = insn.op0;
        break;
      case Op_GetMinConst:
        if (insn.op1 < vars[insn.op0]) { vars[insn.op0] = insn.op1; }
        written = insn.op0;
        break;
      case Op_GetMaxVar:
        if (vars[insn.op0] > vars[insn.op1]) { vars[insn.op1] = vars[insn.op0]; }
        written = insn.op1;
        break;
      case Op_GetMinVar:
        if (vars[insn.op0] < vars[insn.op1]) { vars[insn.op1] = vars[insn.op0]; }
        written = insn.op1;
        break;

      case Op_CmpGtConst: vars[insn.op0] = (vars[insn.op0] > insn.op1) ? 1 : 0; written = insn.op0; break;
      case Op_CmpLtConst: vars[insn.op0] = (vars[insn.op0] < insn.op1) ? 1 : 0; written = insn.op0; break;
      case Op_CmpEqConst: vars[insn.op0] = (vars[insn.op0] == insn.op1) ? 1 : 0; written = insn.op0; break;
      case Op_CmpGtVar: vars[insn.op1] = (vars[insn.op0] > vars[insn.op1]) ? 1 : 0; written = insn.op1; break;
      case Op_CmpLtVar: vars[insn.op1] = (vars[insn.op0] < vars[insn.op1]) ? 1 : 0; written = insn.op1; break;
      case Op_CmpEqVar: vars[insn.op1] = (vars[insn.op0] == vars[insn.op1]) ? 1 : 0; written = insn.op1; break;

      case Op_BitShiftLeftConst: {
        const uint32_t amt = static_cast<uint32_t>(insn.op1) & 31U;
        vars[insn.op0] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) << amt);
        written = insn.op0;
        break;
      }
      case Op_BitShiftRightConst: {
        const uint32_t amt = static_cast<uint32_t>(insn.op1) & 31U;
        vars[insn.op0] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) >> amt);
        written = insn.op0;
        break;
      }
      case Op_VariableBitShiftLeft: {
        const int32_t places = static_cast<int8_t>(vars[insn.op1] & 0xFF);
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        vars[insn.op0] = places >= 0
            ? static_cast<int32_t>(value << (static_cast<uint32_t>(places) & 31U))
            : static_cast<int32_t>(value >> (static_cast<uint32_t>(-places) & 31U));
        written = insn.op0;
        break;
      }
      case Op_VariableBitShiftRight: {
        const int32_t places = static_cast<int8_t>(vars[insn.op1] & 0xFF);
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        vars[insn.op0] = places >= 0
            ? static_cast<int32_t>(value >> (static_cast<uint32_t>(places) & 31U))
            : static_cast<int32_t>(value << (static_cast<uint32_t>(-places) & 31U));
        written = insn.op0;
        break;
      }
      case Op_RotateLeftConst: {
        const uint32_t amt = static_cast<uint32_t>(insn.op1) & 31U;
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        vars[insn.op0] = static_cast<int32_t>(amt == 0 ? value : ((value << amt) | (value >> (32U - amt))));
        written = insn.op0;
        break;
      }
      case Op_RotateRightConst: {
        const uint32_t amt = static_cast<uint32_t>(insn.op1) & 31U;
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        vars[insn.op0] = static_cast<int32_t>(amt == 0 ? value : ((value >> amt) | (value << (32U - amt))));
        written = insn.op0;
        break;
      }
      case Op_VariableRotateLeft: {
        const int32_t places = static_cast<int8_t>(vars[insn.op1] & 0xFF);
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        const uint32_t amt = static_cast<uint32_t>(places >= 0 ? places : -places) & 31U;
        const uint32_t rotated = amt == 0 ? value
            : (places >= 0 ? ((value << amt) | (value >> (32U - amt)))
                           : ((value >> amt) | (value << (32U - amt))));
        vars[insn.op0] = static_cast<int32_t>(rotated);
        written = insn.op0;
        break;
      }
      case Op_VariableRotateRight: {
        const int32_t places = static_cast<int8_t>(vars[insn.op1] & 0xFF);
        const uint32_t value = static_cast<uint32_t>(vars[insn.op0]);
        const uint32_t amt = static_cast<uint32_t>(places >= 0 ? places : -places) & 31U;
        const uint32_t rotated = amt == 0 ? value
            : (places >= 0 ? ((value >> amt) | (value << (32U - amt)))
                           : ((value << amt) | (value >> (32U - amt))));
        vars[insn.op0] = static_cast<int32_t>(rotated);
        written = insn.op0;
        break;
      }
      case Op_BitwiseInvert:
        vars[insn.op0] = static_cast<int32_t>(~static_cast<uint32_t>(vars[insn.op0]));
        written = insn.op0;
        break;
      case Op_BitwiseAnd:
        vars[insn.op1] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) &
                                              static_cast<uint32_t>(vars[insn.op1]));
        written = insn.op1;
        break;
      case Op_BitwiseOr:
        vars[insn.op1] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) |
                                              static_cast<uint32_t>(vars[insn.op1]));
        written = insn.op1;
        break;
      case Op_BitwiseXor:
        vars[insn.op1] = static_cast<int32_t>(static_cast<uint32_t>(vars[insn.op0]) ^
                                              static_cast<uint32_t>(vars[insn.op1]));
        written = insn.op1;
        break;

      case Op_RelJumpIfVarGt0:
        if (vars[insn.op0] > 0) { next_pc = static_cast<uint32_t>(insn.op1); }
        break;
      case Op_RelJumpIfVarLt0:
        if (vars[insn.op0] < 0) { next_pc = static_cast<uint32_t>(insn.op1); }
        break;
      case Op_RelJumpIfVarEq0:
        if (vars[insn.op0] == 0) { next_pc = static_cast<uint32_t>(insn.op1); }
        break;
      case Op_UnconditionalJump: next_pc = static_cast<uint32_t>(insn.op1); break;

      case Op_LoadMemorySize: vars[insn.op0] = static_cast<int32_t>(p.variable_count); written = insn.op0; break;
      case Op_LoadCurrentAddress: vars[insn.op0] = static_cast<int32_t>(pc); written = insn.op0; break;

      case Op_DeclareVar:
      case Op_UndeclareVar:
        // No declared-mask enforcement in this kernel: the membrane pre-declares every I/O
        // slot and TaskWorld never uses zero-capacity variable ranges, so declare/undeclare
        // are no-ops that keep the PC advancing (matching the CPU's try/catch swallow).
        break;

      case Op_CheckIfVarIsInput: {
        const int q = insn.op1;
        vars[insn.op0] = (q >= 0 && q < idx_move_out) ? 1 : 0;
        written = insn.op0;
        break;
      }
      case Op_CheckIfVarIsOutput:
        vars[insn.op0] = (insn.op1 == idx_move_out) ? 1 : 0;
        written = insn.op0;
        break;
      case Op_LoadInputCount: vars[insn.op0] = idx_move_out; written = insn.op0; break; // inputs are 0..idx_move_out-1
      case Op_LoadOutputCount: vars[insn.op0] = 1; written = insn.op0; break;
      case Op_CheckIfInputWasSet: vars[insn.op0] = 1; written = insn.op0; break;

      case Op_LoadStringTableLimit: vars[insn.op0] = 0; written = insn.op0; break;
      case Op_LoadStringTableItemLen: vars[insn.op0] = 0; written = insn.op0; break;
      case Op_SetStringTableEntry:
      case Op_SetVarStringTableEntry:
      case Op_LoadStringItemLen:
      case Op_LoadVarStringItemLen:
      case Op_LoadStringItemIntoVars:
      case Op_LoadVarStringItemIntoVars:
        break; // zero-capacity string table: no-ops, matching the CPU evaluator

      case Op_LoadRandomValue: {
        if (!rng_initialised) {
          const uint64_t seed = p.random_seed ^
                                (static_cast<uint64_t>(tid) * 0x9E3779B97F4A7C15ULL) ^
                                (static_cast<uint64_t>(vm_steps) * 0x6A88841C9C8FE7E1ULL);
          curand_init(seed, 0, 0, &rng_state);
          rng_initialised = true;
        }
        vars[insn.op0] = static_cast<int32_t>(curand(&rng_state));
        written = insn.op0;
        break;
      }

      case Op_PushVarOnStack: {
        const uint32_t base = static_cast<uint32_t>(insn.op0);
        if (base < kTwRegisterFileSize) {
          const int32_t size = vars[base];
          const uint32_t slot = base + 1U + static_cast<uint32_t>(size);
          if (slot < kTwRegisterFileSize) {
            vars[slot] = vars[insn.op1];
            vars[base] = size + 1;
          }
        }
        written = insn.op0;
        break;
      }
      case Op_PushConstOnStack: {
        const uint32_t base = static_cast<uint32_t>(insn.op0);
        if (base < kTwRegisterFileSize) {
          const int32_t size = vars[base];
          const uint32_t slot = base + 1U + static_cast<uint32_t>(size);
          if (slot < kTwRegisterFileSize) {
            vars[slot] = insn.op1;
            vars[base] = size + 1;
          }
        }
        written = insn.op0;
        break;
      }
      case Op_PopVarFromStack: {
        const uint32_t base = static_cast<uint32_t>(insn.op0);
        const uint32_t dst = static_cast<uint32_t>(insn.op1);
        if (base < kTwRegisterFileSize) {
          const int32_t size = vars[base];
          if (size > 0) {
            const uint32_t slot = base + static_cast<uint32_t>(size);
            if (slot < kTwRegisterFileSize && dst < kTwRegisterFileSize) {
              vars[dst] = vars[slot];
              vars[base] = size - 1;
            }
          }
        }
        written = insn.op1;
        written2 = insn.op0;
        break;
      }
      case Op_PopTopFromStack: {
        const uint32_t base = static_cast<uint32_t>(insn.op0);
        if (base < kTwRegisterFileSize) {
          const int32_t size = vars[base];
          if (size > 0) { vars[base] = size - 1; }
        }
        written = insn.op0;
        break;
      }
      case Op_CheckStackEmpty: {
        const uint32_t base = static_cast<uint32_t>(insn.op1);
        if (base < kTwRegisterFileSize) { vars[insn.op0] = (vars[base] == 0) ? 1 : 0; }
        written = insn.op0;
        break;
      }

      case Op_VarTargetJump: {
        const auto kind = static_cast<uint16_t>(insn.aux);
        bool take = false;
        switch (kind) {
          case 0: case 1: take = true; break;
          case 2: case 5: take = vars[insn.op0] > 0; break;
          case 3: case 6: take = vars[insn.op0] < 0; break;
          case 4: case 7: take = vars[insn.op0] == 0; break;
          default: break;
        }
        if (take) {
          const bool is_relative = (kind == 1) || (kind >= 5);
          const uint32_t target_insn =
              resolveVarJumpTarget(vars[insn.op1], is_relative, pc, b2i, b2i_len);
          if (target_insn != kNoInstructionMapping) {
            next_pc = target_insn;
          }
        }
        break;
      }

      case Op_CallSubroutine:
        // No subroutine library is mounted for the TaskWorld pipeline; the CPU VM panics and
        // ends the program here. We do the same -- keep the progress made and score it.
        terminated = true;
        break;

      default: break;
    }

    pc = next_pc;

    // Interactive membrane: if the program just wrote the move-output variable, consume it
    // and step the world. Mirrors TaskWorldEvaluator::runEpisode's per-step move handling.
    if (written == idx_move_out || written2 == idx_move_out) {
      move_pending = true;
    }
    if (move_pending) {
      const uint32_t raw_move = static_cast<uint32_t>(vars[idx_move_out]) & 0x3U;
      int nr = prow;
      int nc = pcol;
      switch (raw_move) {
        case 0: --nc; break; // LEFT
        case 1: ++nc; break; // RIGHT
        case 2: --nr; break; // UP
        default: ++nr; break; // DOWN
      }
      bool moved = false;
      if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
        const int ncell = nr * cols + nc;
        const uint8_t t = tiles[ncell];
        if (t == 1) {
          moved = false; // wall
        } else if (t == 4) {
          // Locked door: needs a key, which is then spent and the door unlocked.
          if (keys > 0) {
            --keys;
            tiles[ncell] = 6;
            ++doors_opened;
            moved = true;
          } else {
            moved = false;
          }
        } else {
          if (t == 3) {
            ++keys;
            tiles[ncell] = 0;
          } else if (t == 5) {
            for (int k = 0; k < item_count; ++k) {
              if (static_cast<int>(item_rows[k]) == nr && static_cast<int>(item_cols[k]) == nc &&
                  ((collected_mask >> k) & 1U) == 0U) {
                collected_mask |= (1U << k);
                ++items_collected;
                break;
              }
            }
            tiles[ncell] = 0;
          } else if (t == 2) {
            const int gained = static_cast<int>(world_food[ncell]);
            const int headroom = static_cast<int>(max_food) - (food < 0 ? 0 : food);
            food += (gained < headroom ? gained : (headroom > 0 ? headroom : 0));
            tiles[ncell] = 0;
          }
          moved = true;
        }
      }
      if (moved) {
        prow = nr;
        pcol = nc;
        food = food > 0 ? food - 1 : 0; // saturating consumeFood(1)
        markVisited(prow, pcol);
        ++moves_used;
        refresh = true;
      }
      move_pending = false;
    }

    if (refresh) {
      devFeedObservation(vars, tiles, rows, cols, radius, span, prow, pcol, start_row, start_col,
                         goal_row, goal_col, food, static_cast<int>(keys),
                         static_cast<int>(items_total - items_collected), idx_food, idx_keys,
                         idx_items, idx_gr, idx_gc, idx_gv, feed_compass);
      refresh = false;
    }

    if (food == 0) {
      break; // starved
    }
    const bool task_complete =
        (items_collected == items_total) && (prow == goal_row) && (pcol == goal_col);
    if (task_complete) {
      break;
    }
    ++vm_steps;
    if (vm_steps > max_steps) {
      break;
    }
  }

  // ---- Finalize + score ----
  uint32_t cells_visited = 0;
  for (int i = 0; i < kVisitedWords; ++i) {
    cells_visited += static_cast<uint32_t>(__popcll(visited[i]));
  }
  const bool reached_goal = (prow == goal_row) && (pcol == goal_col);
  const bool task_complete = (items_collected == items_total) && reached_goal;
  const uint32_t final_distance = devDistanceToObjective(prow, pcol, collected_mask, item_rows,
                                                        item_cols, item_count, goal_row, goal_col);

  p.scores_out[tid] = devScoreEpisode(task_complete, items_collected, items_total, doors_opened,
                                      doors_total, moves_used, cells_visited, reference_moves,
                                      reached_goal, initial_distance, final_distance);
}

} // anonymous namespace

// ====================================================================================
// Host Impl
// ====================================================================================
struct CudaTaskWorldEvaluator::Impl {
  TaskWorldEvaluator evaluator; ///< Reused for its Config + pool-seed draw policy.
  uint32_t variable_count = 0;

  // Persistent device buffers for compiled programs (grown on demand).
  Instruction* d_instructions = nullptr;
  size_t d_instructions_capacity = 0;
  uint32_t* d_genome_offsets = nullptr;
  uint32_t* d_genome_lengths = nullptr;
  size_t d_offsets_capacity = 0;
  uint32_t* d_byte_to_insn = nullptr;
  size_t d_byte_to_insn_capacity = 0;
  uint32_t* d_genome_byte_offsets = nullptr;
  uint32_t* d_genome_byte_lengths = nullptr;
  size_t d_byte_offsets_capacity = 0;
  double* d_scores = nullptr;
  size_t d_scores_capacity = 0;

  // Persistent device buffers for uploaded worlds (grown on demand).
  uint8_t* d_cell_type = nullptr;
  size_t d_cell_type_capacity = 0;
  uint16_t* d_food_weight = nullptr;
  size_t d_food_weight_capacity = 0;
  uint32_t* d_world_scalars = nullptr; // packed: see layoutWorldScalars
  size_t d_world_scalars_capacity = 0;
  uint32_t* d_item_rc = nullptr;       // [2 * W * kMaxItems]
  size_t d_item_rc_capacity = 0;

  explicit Impl(const TaskWorldEvaluator& source) : evaluator(source.getConfig()) {}

  ~Impl() {
    cudaFree(d_instructions);
    cudaFree(d_genome_offsets);
    cudaFree(d_genome_lengths);
    cudaFree(d_byte_to_insn);
    cudaFree(d_genome_byte_offsets);
    cudaFree(d_genome_byte_lengths);
    cudaFree(d_scores);
    cudaFree(d_cell_type);
    cudaFree(d_food_weight);
    cudaFree(d_world_scalars);
    cudaFree(d_item_rc);
  }
};

bool CudaTaskWorldEvaluator::supportsConfig(const TaskWorldEvaluator::Config& config,
                                            uint32_t variable_count) {
  const uint64_t cells = static_cast<uint64_t>(config.rows) * config.cols;
  if (cells == 0 || cells > static_cast<uint64_t>(kMaxCells)) {
    return false;
  }
  const uint32_t span = config.radius * 2U + 1U;
  const uint32_t needed = span * span + 7U; // grid window + 6 sensors + 1 move output
  if (needed > static_cast<uint32_t>(kTwRegisterFileSize)) {
    return false;
  }
  if (variable_count < needed || variable_count > static_cast<uint32_t>(kTwRegisterFileSize)) {
    return false;
  }
  if (config.num_items > static_cast<uint32_t>(kMaxItems)) {
    return false;
  }
  return true;
}

CudaTaskWorldEvaluator::CudaTaskWorldEvaluator(const TaskWorldEvaluator& config,
                                               uint32_t variable_count)
    : impl_(std::make_unique<Impl>(config)) {
  impl_->variable_count = variable_count;
  if (!supportsConfig(config.getConfig(), variable_count)) {
    throw std::runtime_error(
        "CudaTaskWorldEvaluator: configuration exceeds device limits (grid/window/items)");
  }
}

CudaTaskWorldEvaluator::~CudaTaskWorldEvaluator() = default;

namespace {

// Per-world scalar payload, packed contiguously so one buffer + one memcpy carries them all.
// Order matters -- it must match the unpacking in evaluate().
constexpr int kWorldScalarStride = 10; // start_row,start_col,goal_row,goal_col,items_total,
                                       // doors_total,reference_moves,starting_food,max_food,
                                       // item_count

} // namespace

std::vector<double>
CudaTaskWorldEvaluator::evaluate(const std::vector<std::vector<uint8_t>>& genomes,
                                 const std::atomic<bool>* stop_token) {
  const uint32_t num_genomes = static_cast<uint32_t>(genomes.size());
  std::vector<double> result(num_genomes, 0.0);
  if (num_genomes == 0) {
    return result;
  }
  if (stop_token != nullptr && stop_token->load(std::memory_order_acquire)) {
    return result;
  }

  const auto& cfg = impl_->evaluator.getConfig();
  const uint32_t W = cfg.worlds_per_eval > 0 ? cfg.worlds_per_eval : 1U;
  const uint32_t rows = cfg.rows;
  const uint32_t cols = cfg.cols;
  const uint32_t cells = rows * cols;
  const uint32_t span = cfg.radius * 2U + 1U;

  // ---- Generate + pack the batch's worlds on the host ----
  std::vector<uint8_t> cell_type(static_cast<size_t>(W) * cells, 0U);
  std::vector<uint16_t> food_weight(static_cast<size_t>(W) * cells, 0U);
  std::vector<uint32_t> world_scalars(static_cast<size_t>(W) * kWorldScalarStride, 0U);
  std::vector<uint32_t> item_rc(static_cast<size_t>(2) * W * kMaxItems, 0U);

  for (uint32_t w = 0; w < W; ++w) {
    const uint64_t seed = impl_->evaluator.drawPoolSeed();
    maze::TaskConfig task_config;
    task_config.rows = cfg.rows;
    task_config.cols = cfg.cols;
    task_config.difficulty = cfg.difficulty;
    task_config.num_items = cfg.num_items;
    task_config.num_keys = cfg.num_keys;
    task_config.num_doors = cfg.num_doors;
    task_config.num_food = cfg.num_food;
    task_config.starting_food = cfg.starting_food;

    maze::TaskWorld world(task_config, seed);
    const maze::TaskWorldLayout layout = world.exportLayout();

    std::copy(layout.cell_type.begin(), layout.cell_type.end(),
              cell_type.begin() + static_cast<size_t>(w) * cells);
    std::copy(layout.food_weight.begin(), layout.food_weight.end(),
              food_weight.begin() + static_cast<size_t>(w) * cells);

    uint32_t* sc = world_scalars.data() + static_cast<size_t>(w) * kWorldScalarStride;
    sc[0] = layout.start.row;
    sc[1] = layout.start.col;
    sc[2] = layout.goal.row;
    sc[3] = layout.goal.col;
    sc[4] = layout.items_total;
    sc[5] = layout.doors_total;
    sc[6] = layout.reference_moves;
    sc[7] = layout.starting_food;
    sc[8] = layout.max_food;
    sc[9] = std::min<uint32_t>(static_cast<uint32_t>(layout.item_positions.size()), kMaxItems);

    for (uint32_t k = 0; k < sc[9]; ++k) {
      item_rc[(static_cast<size_t>(w) * kMaxItems + k)] = layout.item_positions[k].row;
      item_rc[(static_cast<size_t>(W) * kMaxItems + static_cast<size_t>(w) * kMaxItems + k)] =
          layout.item_positions[k].col;
    }
  }

  // ---- Compile genomes on the host into the device instruction stream ----
  std::vector<uint32_t> offsets(num_genomes, 0);
  std::vector<uint32_t> lengths(num_genomes, 0);
  std::vector<uint32_t> byte_offsets(num_genomes, 0);
  std::vector<uint32_t> byte_lengths(num_genomes, 0);
  std::vector<Instruction> all_instructions;
  std::vector<uint32_t> all_byte_to_insn;
  all_instructions.reserve(static_cast<size_t>(num_genomes) * 64);
  for (uint32_t g = 0; g < num_genomes; ++g) {
    CompiledProgram cp = compileProgramForGpu(genomes[g], impl_->variable_count);
    offsets[g] = static_cast<uint32_t>(all_instructions.size());
    lengths[g] = static_cast<uint32_t>(cp.instructions.size());
    all_instructions.insert(all_instructions.end(), cp.instructions.begin(),
                            cp.instructions.end());
    byte_offsets[g] = static_cast<uint32_t>(all_byte_to_insn.size());
    byte_lengths[g] = static_cast<uint32_t>(cp.byte_to_insn.size());
    all_byte_to_insn.insert(all_byte_to_insn.end(), cp.byte_to_insn.begin(),
                            cp.byte_to_insn.end());
  }

  // A population of entirely-empty genomes still needs one valid instruction pointer.
  if (all_instructions.empty()) {
    all_instructions.push_back(Instruction{});
  }
  if (all_byte_to_insn.empty()) {
    all_byte_to_insn.push_back(0U);
  }

  auto ensure = [](void** ptr, size_t& cap, size_t bytes) {
    if (bytes > cap) {
      if (*ptr != nullptr) {
        cudaFree(*ptr);
      }
      BEAST_CUDA_CHECK(cudaMalloc(ptr, bytes));
      cap = bytes;
    }
  };

  // Program buffers.
  ensure(reinterpret_cast<void**>(&impl_->d_instructions), impl_->d_instructions_capacity,
         all_instructions.size() * sizeof(Instruction));
  const size_t offsets_bytes = num_genomes * sizeof(uint32_t);
  if (offsets_bytes > impl_->d_offsets_capacity) {
    cudaFree(impl_->d_genome_offsets);
    cudaFree(impl_->d_genome_lengths);
    BEAST_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&impl_->d_genome_offsets), offsets_bytes));
    BEAST_CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&impl_->d_genome_lengths), offsets_bytes));
    impl_->d_offsets_capacity = offsets_bytes;
  }
  ensure(reinterpret_cast<void**>(&impl_->d_byte_to_insn), impl_->d_byte_to_insn_capacity,
         all_byte_to_insn.size() * sizeof(uint32_t));
  if (offsets_bytes > impl_->d_byte_offsets_capacity) {
    cudaFree(impl_->d_genome_byte_offsets);
    cudaFree(impl_->d_genome_byte_lengths);
    BEAST_CUDA_CHECK(
        cudaMalloc(reinterpret_cast<void**>(&impl_->d_genome_byte_offsets), offsets_bytes));
    BEAST_CUDA_CHECK(
        cudaMalloc(reinterpret_cast<void**>(&impl_->d_genome_byte_lengths), offsets_bytes));
    impl_->d_byte_offsets_capacity = offsets_bytes;
  }

  // World buffers.
  ensure(reinterpret_cast<void**>(&impl_->d_cell_type), impl_->d_cell_type_capacity,
         cell_type.size() * sizeof(uint8_t));
  ensure(reinterpret_cast<void**>(&impl_->d_food_weight), impl_->d_food_weight_capacity,
         food_weight.size() * sizeof(uint16_t));
  ensure(reinterpret_cast<void**>(&impl_->d_world_scalars), impl_->d_world_scalars_capacity,
         world_scalars.size() * sizeof(uint32_t));
  ensure(reinterpret_cast<void**>(&impl_->d_item_rc), impl_->d_item_rc_capacity,
         item_rc.size() * sizeof(uint32_t));

  // Scores.
  const size_t scores_bytes = static_cast<size_t>(num_genomes) * W * sizeof(double);
  ensure(reinterpret_cast<void**>(&impl_->d_scores), impl_->d_scores_capacity, scores_bytes);

  // ---- Upload ----
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_instructions, all_instructions.data(),
                              all_instructions.size() * sizeof(Instruction),
                              cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_genome_offsets, offsets.data(), offsets_bytes,
                              cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_genome_lengths, lengths.data(), offsets_bytes,
                              cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_byte_to_insn, all_byte_to_insn.data(),
                              all_byte_to_insn.size() * sizeof(uint32_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_genome_byte_offsets, byte_offsets.data(), offsets_bytes,
                              cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_genome_byte_lengths, byte_lengths.data(), offsets_bytes,
                              cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_cell_type, cell_type.data(),
                              cell_type.size() * sizeof(uint8_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_food_weight, food_weight.data(),
                              food_weight.size() * sizeof(uint16_t), cudaMemcpyHostToDevice));
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_item_rc, item_rc.data(),
                              item_rc.size() * sizeof(uint32_t), cudaMemcpyHostToDevice));

  // The kernel reads the per-world scalars as ten contiguous SoA arrays (start_row[0..W),
  // start_col[0..W), ...). Transpose the AoS staging buffer into that layout and upload once.
  std::vector<uint32_t> soa(static_cast<size_t>(W) * kWorldScalarStride, 0U);
  for (uint32_t w = 0; w < W; ++w) {
    const uint32_t* sc = world_scalars.data() + static_cast<size_t>(w) * kWorldScalarStride;
    for (int f = 0; f < kWorldScalarStride; ++f) {
      soa[static_cast<size_t>(f) * W + w] = sc[f];
    }
  }
  BEAST_CUDA_CHECK(cudaMemcpy(impl_->d_world_scalars, soa.data(),
                              soa.size() * sizeof(uint32_t), cudaMemcpyHostToDevice));

  TwKernelParams params{};
  params.instructions = impl_->d_instructions;
  params.genome_offsets = impl_->d_genome_offsets;
  params.genome_lengths = impl_->d_genome_lengths;
  params.byte_to_insn = impl_->d_byte_to_insn;
  params.genome_byte_offsets = impl_->d_genome_byte_offsets;
  params.genome_byte_lengths = impl_->d_genome_byte_lengths;
  params.num_genomes = num_genomes;
  params.num_worlds = W;
  params.rows = rows;
  params.cols = cols;
  params.radius = cfg.radius;
  params.span = span;
  params.grid_inputs = span * span;
  params.variable_count = impl_->variable_count;
  params.max_steps = cfg.max_steps;
  params.feed_compass = cfg.goal_compass ? 1U : 0U;
  params.cell_type = impl_->d_cell_type;
  params.food_weight = impl_->d_food_weight;
  params.start_row = impl_->d_world_scalars + 0 * W;
  params.start_col = impl_->d_world_scalars + 1 * W;
  params.goal_row = impl_->d_world_scalars + 2 * W;
  params.goal_col = impl_->d_world_scalars + 3 * W;
  params.items_total = impl_->d_world_scalars + 4 * W;
  params.doors_total = impl_->d_world_scalars + 5 * W;
  params.reference_moves = impl_->d_world_scalars + 6 * W;
  params.starting_food = impl_->d_world_scalars + 7 * W;
  params.max_food = impl_->d_world_scalars + 8 * W;
  params.item_count = impl_->d_world_scalars + 9 * W;
  params.item_rows = impl_->d_item_rc;
  params.item_cols = impl_->d_item_rc + static_cast<size_t>(W) * kMaxItems;
  params.random_seed = 0xA5A5C0DEULL ^ cfg.seed_base;
  params.scores_out = impl_->d_scores;

  const uint32_t total_threads = num_genomes * W;
  const uint32_t block_size = 128;
  const uint32_t grid_size = (total_threads + block_size - 1) / block_size;
  taskWorldKernel<<<grid_size, block_size>>>(params);
  BEAST_CUDA_CHECK(cudaGetLastError());

  std::vector<double> host_scores(static_cast<size_t>(num_genomes) * W, 0.0);
  BEAST_CUDA_CHECK(cudaMemcpy(host_scores.data(), impl_->d_scores, scores_bytes,
                              cudaMemcpyDeviceToHost));

  for (uint32_t g = 0; g < num_genomes; ++g) {
    double sum = 0.0;
    for (uint32_t w = 0; w < W; ++w) {
      sum += host_scores[static_cast<size_t>(g) * W + w];
    }
    result[g] = sum / static_cast<double>(W);
  }
  return result;
}

double CudaTaskWorldEvaluator::evaluateOne(const std::vector<uint8_t>& genome,
                                           uint64_t /*world_seed*/) {
  auto scores = evaluate({genome}, nullptr);
  return scores.empty() ? 0.0 : scores.front();
}

} // namespace beast::cuda
