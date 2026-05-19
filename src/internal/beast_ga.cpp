#include <beast/internal/beast_ga.hpp>

// Standard
#include <algorithm>
#include <atomic>
#include <limits>
#include <random>
#include <utility>
#include <vector>

namespace beast::internal {

namespace {

/// Per-genome record carried inside the GA. The `evaluated` flag exists so we can skip
/// re-scoring the elitism carrier each generation (which would change its score under
/// non-deterministic evaluators and undo the convergence guarantee elitism is supposed
/// to provide).
struct GenomeRecord {
  std::vector<uint8_t> bytes;
  double score = 0.0;
  bool evaluated = false;
};

/// Resolve `seed == 0` into a real entropy-sourced seed at GA construction time.
uint64_t resolveSeed(uint64_t requested) {
  if (requested != 0) {
    return requested;
  }
  std::random_device rd;
  // 64 bits of entropy. std::random_device produces 32 bits; concatenate two draws so we
  // populate the full mt19937_64 state space and don't get two pipes accidentally sharing
  // a 32-bit seed.
  const uint64_t high = static_cast<uint64_t>(rd()) << 32U;
  const uint64_t low = static_cast<uint64_t>(rd());
  // Guarantee a non-zero seed even in the (vanishingly unlikely) case where both draws
  // returned 0; downstream code uses 0 as the "unseeded" sentinel.
  const uint64_t resolved = high | low;
  return resolved != 0 ? resolved : 1U;
}

} // namespace

BeastGA::BeastGA(Config config, Operators operators, BatchEvaluator& evaluator,
                 FinalistSink finalist_sink, GenerationHook generation_hook,
                 double cutoff_score)
    : config_{config},
      operators_{std::move(operators)},
      evaluator_{evaluator},
      finalist_sink_{std::move(finalist_sink)},
      generation_hook_{std::move(generation_hook)},
      cutoff_score_{cutoff_score},
      seed_{resolveSeed(config.seed)} {}

// Tournament selection: pick `tournament_size` random indices, return the index of the
// fittest among them. Tournament is the right default for noisy fitness because (unlike
// roulette wheel) it doesn't recompute fitness scaling each generation and it's robust
// against score-distribution skew.
//
// We pull this method's local state from a passed-in RNG reference because the caller
// holds the per-instance engine. Free function would also work; method form lets us
// access `population_` without an extra parameter.
namespace {
template <typename Rng>
uint32_t tournamentSelectImpl(const std::vector<GenomeRecord>& population,
                              uint32_t tournament_size, Rng& rng) {
  std::uniform_int_distribution<uint32_t> dist(
      0, static_cast<uint32_t>(population.size() - 1));
  uint32_t best_idx = dist(rng);
  double best_score = population[best_idx].score;
  for (uint32_t i = 1; i < tournament_size; ++i) {
    const uint32_t candidate = dist(rng);
    if (population[candidate].score > best_score) {
      best_idx = candidate;
      best_score = population[candidate].score;
    }
  }
  return best_idx;
}
} // namespace

BeastGA::Result BeastGA::run(const std::atomic<bool>* stop_token) {
  Result result;
  if (config_.population_size == 0 || config_.generations == 0) {
    return result;
  }

  std::mt19937_64 rng{seed_};

  // ---- Build the initial population ---------------------------------------------------
  //
  // We construct every slot up front and then hand the byte vectors to the evaluator as
  // a single batch. The evaluator is responsible for its own parallelism; the GA just
  // hands it work.
  std::vector<GenomeRecord> population;
  population.reserve(config_.population_size);
  for (uint32_t i = 0; i < config_.population_size; ++i) {
    GenomeRecord rec;
    rec.bytes = operators_.produce_initial_genome();
    population.push_back(std::move(rec));
  }

  // Cooperative cancellation: if the user pressed stop while we were initialising, skip
  // straight to the unwind. The initial-population work was cheap; we don't bill it.
  if (stop_token != nullptr && stop_token->load(std::memory_order_acquire)) {
    result.stopped_early = true;
    return result;
  }

  // ---- Initial evaluation -------------------------------------------------------------
  {
    std::vector<std::vector<uint8_t>> batch;
    batch.reserve(population.size());
    for (auto& rec : population) {
      batch.push_back(rec.bytes); // copy; the evaluator may need it past this scope
    }
    const auto scores = evaluator_.evaluate(batch, stop_token);
    for (size_t i = 0; i < population.size(); ++i) {
      population[i].score = (i < scores.size()) ? scores[i] : 0.0;
      population[i].evaluated = true;
    }
    result.evaluations_performed += population.size();

    // Stream initial-population finalists. After the first eval we already know which
    // random programs landed above the cutoff -- no reason to make downstream wait until
    // generation N to see them.
    if (finalist_sink_) {
      for (const auto& rec : population) {
        if (rec.score >= cutoff_score_ && !rec.bytes.empty()) {
          finalist_sink_(rec.bytes, rec.score); // copy; sink takes ownership of its copy
        }
      }
    }

    // Update hall-of-fame.
    for (const auto& rec : population) {
      if (rec.score > result.best_score || result.best_bytes.empty()) {
        result.best_score = rec.score;
        result.best_bytes = rec.bytes;
      }
    }
    if (generation_hook_) {
      generation_hook_(0, result.best_score);
    }
  }

  // Re-check the stop token after the (potentially expensive) initial eval.
  if (stop_token != nullptr && stop_token->load(std::memory_order_acquire)) {
    result.stopped_early = true;
    return result;
  }

  // ---- Generation loop ----------------------------------------------------------------
  //
  // Per generation:
  //   1. Build offspring via tournament + crossover + mutation.
  //   2. (Optional) reserve one slot for the elitism carrier -- the best of the previous
  //      generation copied unchanged into the new generation. Elitism guarantees
  //      monotonic best-score across generations and is the standard GAlib default.
  //   3. Batch-eval the offspring (NOT the elitism carrier; its score is known).
  //   4. Stream any finalists, update hall-of-fame, hand off to the next generation.
  //
  // We do NOT pipeline gen N+1's offspring against gen N's eval: offspring selection
  // depends on gen N's scores, so the work is strictly sequential. The eval itself is
  // where the parallelism happens, and the BatchEvaluator already owns that.
  for (uint32_t gen = 1; gen <= config_.generations; ++gen) {
    // Stop polling at generation boundaries gives "snap to stopped within one
    // generation" latency. The BatchEvaluator can shorten this further by polling the
    // same token internally and short-circuiting the in-flight batch.
    if (stop_token != nullptr && stop_token->load(std::memory_order_acquire)) {
      result.stopped_early = true;
      return result;
    }

    // Identify the elitism carrier *before* we mutate anything. It's the genome with
    // the highest current score; ties are broken by lowest index (deterministic).
    uint32_t elite_idx = 0;
    for (uint32_t i = 1; i < population.size(); ++i) {
      if (population[i].score > population[elite_idx].score) {
        elite_idx = i;
      }
    }
    GenomeRecord elite_carrier = population[elite_idx]; // copy

    // Build offspring. If elitism is on we produce population_size - 1 offspring and
    // splice the carrier into slot 0 at the end. Otherwise we produce population_size.
    const uint32_t offspring_count =
        config_.elitism ? (config_.population_size > 0 ? config_.population_size - 1 : 0)
                        : config_.population_size;

    std::vector<GenomeRecord> next_population;
    next_population.reserve(config_.population_size);
    std::uniform_real_distribution<double> roll(0.0, 1.0);

    for (uint32_t i = 0; i < offspring_count;) {
      const uint32_t parent_a_idx =
          tournamentSelectImpl(population, config_.tournament_size, rng);
      const uint32_t parent_b_idx =
          tournamentSelectImpl(population, config_.tournament_size, rng);
      const auto& parent_a = population[parent_a_idx].bytes;
      const auto& parent_b = population[parent_b_idx].bytes;

      std::vector<uint8_t> child_a;
      std::vector<uint8_t> child_b;
      if (roll(rng) < config_.crossover_probability) {
        auto pair = operators_.crossover(parent_a, parent_b);
        child_a = std::move(pair.first);
        child_b = std::move(pair.second);
      } else {
        // No crossover: clone parents into the offspring slots.
        child_a = parent_a;
        child_b = parent_b;
      }

      operators_.mutate(child_a, config_.mutation_probability);
      operators_.mutate(child_b, config_.mutation_probability);

      next_population.push_back({std::move(child_a), 0.0, false});
      ++i;
      if (i < offspring_count) {
        next_population.push_back({std::move(child_b), 0.0, false});
        ++i;
      }
    }

    // ---- Eval the offspring (NOT the elitism carrier) -------------------------------
    std::vector<std::vector<uint8_t>> batch;
    batch.reserve(next_population.size());
    for (auto& rec : next_population) {
      batch.push_back(rec.bytes); // copy for the evaluator
    }
    const auto scores = evaluator_.evaluate(batch, stop_token);
    for (size_t i = 0; i < next_population.size(); ++i) {
      next_population[i].score = (i < scores.size()) ? scores[i] : 0.0;
      next_population[i].evaluated = true;
    }
    result.evaluations_performed += next_population.size();

    // Stream finalists immediately. This is the cadence change -- downstream pipes see
    // candidates as they're produced, not in one end-of-cycle burst.
    if (finalist_sink_) {
      for (const auto& rec : next_population) {
        if (rec.score >= cutoff_score_ && !rec.bytes.empty()) {
          finalist_sink_(rec.bytes, rec.score);
        }
      }
    }

    // ---- Splice the elitism carrier into the new population --------------------------
    if (config_.elitism) {
      next_population.push_back(std::move(elite_carrier));
    }

    // ---- Update hall-of-fame ---------------------------------------------------------
    for (const auto& rec : next_population) {
      if (rec.score > result.best_score) {
        result.best_score = rec.score;
        result.best_bytes = rec.bytes;
      }
    }

    population = std::move(next_population);

    if (generation_hook_) {
      generation_hook_(gen, result.best_score);
    }
  }

  return result;
}

} // namespace beast::internal
