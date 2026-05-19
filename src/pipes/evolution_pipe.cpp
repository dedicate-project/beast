#include <beast/pipes/evolution_pipe.hpp>

// Standard
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

// Internal
#include <beast/internal/beast_ga.hpp>
#include <beast/program.hpp>
#include <beast/program_parser.hpp>
#include <beast/random_program_factory.hpp>

namespace beast {

namespace {

// =====================================================================================
// Thread pool used by the CPU-side BatchEvaluator implementation.
//
// Process-wide singleton sized to hardware_concurrency() (clamped to [1, 64]). All
// EvolutionPipes in the process share a single pool: a pipeline with 10 parallel
// evolution stages doesn't oversubscribe a 16-core box with 10 * 16 worker threads.
//
// Lifetime: Meyers singleton, destroyed at program exit. The destructor sets the stop
// flag, notifies all workers, and joins them. Pipelines run their destructors before
// program exit (the server owns the pipelines), and every `BatchEvaluator::evaluate()`
// call drains its futures before returning, so no task is ever in flight referencing a
// stale pipe at pool teardown.
// =====================================================================================
class EvaluationThreadPool {
 public:
  static EvaluationThreadPool& instance() {
    static EvaluationThreadPool pool;
    return pool;
  }

  EvaluationThreadPool(const EvaluationThreadPool&) = delete;
  EvaluationThreadPool& operator=(const EvaluationThreadPool&) = delete;
  EvaluationThreadPool(EvaluationThreadPool&&) = delete;
  EvaluationThreadPool& operator=(EvaluationThreadPool&&) = delete;

  /// Submit a void task. The caller is responsible for moving outputs through captures
  /// or shared state; the future is purely a synchronisation point.
  std::future<void> submitVoid(std::function<void()> task) {
    auto packaged = std::make_shared<std::packaged_task<void()>>(std::move(task));
    auto future = packaged->get_future();
    {
      std::scoped_lock lock(mutex_);
      queue_.emplace([packaged] { (*packaged)(); });
    }
    cv_.notify_one();
    return future;
  }

  [[nodiscard]] size_t workerCount() const noexcept { return workers_.size(); }

 private:
  EvaluationThreadPool() {
    const unsigned int hw = std::thread::hardware_concurrency();
    const size_t target = std::clamp<size_t>(hw, 1, 64);
    workers_.reserve(target);
    for (size_t i = 0; i < target; ++i) {
      workers_.emplace_back([this] { run(); });
    }
  }

  ~EvaluationThreadPool() {
    {
      std::scoped_lock lock(mutex_);
      stopping_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  void run() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (stopping_ && queue_.empty()) {
          return;
        }
        task = std::move(queue_.front());
        queue_.pop();
      }
      task();
    }
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> queue_;
  std::vector<std::thread> workers_;
  bool stopping_ = false;
};

// =====================================================================================
// CPU-side `BatchEvaluator` implementation.
//
// Wraps the per-genome `EvolutionPipe::evaluate()` callback and fans it out across the
// process-wide thread pool. Chunked dispatch (`ceil(N / workers)` genomes per task)
// amortises pool overhead -- for cheap evaluators the per-task mutex / cv / future
// allocation is the same order of magnitude as the eval itself, so chunking is what
// keeps the pool's win from collapsing to a wash.
//
// A future GPU implementation lives at the same `BatchEvaluator` boundary: it would
// flatten the input genomes into a single device buffer (`bytes_concat + offsets`),
// launch a kernel, copy scores back, return them. The GA above this layer doesn't
// change.
// =====================================================================================
class ThreadPoolBatchEvaluator : public internal::BatchEvaluator {
 public:
  using PerGenomeEval = std::function<double(const std::vector<uint8_t>&)>;
  using PerGenomeReport = std::function<void()>;

  ThreadPoolBatchEvaluator(PerGenomeEval evaluator, PerGenomeReport per_call_report)
      : evaluator_{std::move(evaluator)}, per_call_report_{std::move(per_call_report)} {}

  std::vector<double> evaluate(const std::vector<std::vector<uint8_t>>& genomes,
                               const std::atomic<bool>* stop_token) override {
    std::vector<double> scores(genomes.size(), 0.0);
    if (genomes.empty()) {
      return scores;
    }

    // Account for the calls *before* fanning out so the UI's progress bar tracks
    // "dispatched" rather than "completed" -- a stuck task still shows up on the bar.
    if (per_call_report_) {
      for (size_t i = 0; i < genomes.size(); ++i) {
        per_call_report_();
      }
    }

    auto& pool = EvaluationThreadPool::instance();
    const size_t worker_count = std::max<size_t>(pool.workerCount(), 1);
    const size_t per_chunk =
        std::max<size_t>(1, (genomes.size() + worker_count - 1) / worker_count);
    const size_t chunk_count = (genomes.size() + per_chunk - 1) / per_chunk;

    // Capture `genomes` by reference into each task: it's a `const&`, outlives the
    // call (we wait on every future below before returning), and copying it would
    // pessimise large-batch / large-genome cases.
    std::vector<std::future<void>> futures;
    futures.reserve(chunk_count);
    for (size_t chunk = 0; chunk < chunk_count; ++chunk) {
      const size_t begin = chunk * per_chunk;
      const size_t end = std::min(begin + per_chunk, genomes.size());
      futures.push_back(pool.submitVoid([this, &genomes, &scores, stop_token, begin, end] {
        for (size_t i = begin; i < end; ++i) {
          // Per-genome stop check: a flip mid-chunk saves the remaining VM runs in
          // this slice. The eval itself (e.g. SHA-256 round) does its own in-VM
          // poll too, so a single VM step is the worst-case latency.
          if (stop_token != nullptr && stop_token->load(std::memory_order_acquire)) {
            scores[i] = 0.0;
            continue;
          }
          try {
            scores[i] = evaluator_(genomes[i]);
          } catch (...) {
            // Sentinel zero on any failure. The legacy GAlib path let exceptions
            // propagate into GAlib's interior, which then half-corrupted the
            // population and crashed a few generations later. Sentinel keeps the
            // GA in a coherent state.
            scores[i] = 0.0;
          }
        }
      }));
    }

    for (auto& future : futures) {
      try {
        future.get();
      } catch (...) {
        // Task bodies already catch everything; this catch is paranoia for the
        // future's own bookkeeping. Scores stay at their default 0.0 on the rare
        // path where a packaged_task signals an exception.
      }
    }
    return scores;
  }

 private:
  PerGenomeEval evaluator_;
  PerGenomeReport per_call_report_;
};

// =====================================================================================
// Operator-aware mutation.
//
// Mutates the genome at instruction boundaries: each parsed operator independently
// rolls against the mutation probability and, when chosen, is either:
//   - byte-flipped at a single random offset (the "gamble" mutation, fraction
//     `byte_mutation_share`), OR
//   - replaced with a freshly minted random operator (~70% of the remainder), OR
//   - deleted (~15%), OR
//   - has a new operator inserted before it (~15%).
//
// Trailing garbage bytes (unparseable tail of the genome) are preserved as-is so the
// byte gamble can still expose them to mutation indirectly via later regenerations.
//
// Returns the number of mutations performed. Informational only; callers ignore it.
// =====================================================================================
int operatorAwareMutate(std::vector<uint8_t>& bytes, double probability,
                        const EvolutionPipe::EvolutionParameters& params,
                        std::mt19937_64& engine) {
  if (bytes.empty()) {
    return 0;
  }

  const auto parse_result = ProgramParser::parse(bytes);
  if (parse_result.spans.empty()) {
    // Nothing recognizable; just flip a single byte at random so we don't go entirely
    // stale.
    std::uniform_int_distribution<size_t> idx_dist(0, bytes.size() - 1);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    const size_t idx = idx_dist(engine);
    bytes[idx] = static_cast<uint8_t>(byte_dist(engine));
    return 1;
  }

  std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
  std::uniform_real_distribution<double> action_dist(0.0, 1.0);
  std::uniform_int_distribution<int> byte_dist(0, 255);

  // Rebuild the byte stream from scratch by appending each (possibly mutated) span to
  // a new vector. This makes deletions / insertions trivial.
  std::vector<uint8_t> new_bytes;
  new_bytes.reserve(bytes.size());
  int mutations = 0;

  for (const auto& span : parse_result.spans) {
    if (prob_dist(engine) >= probability) {
      new_bytes.insert(new_bytes.end(), bytes.begin() + span.offset,
                       bytes.begin() + span.offset + span.length);
      continue;
    }

    ++mutations;
    const double roll = action_dist(engine);

    if (roll < params.byte_mutation_share) {
      // Byte-level "gamble" mutation: copy the span, flip exactly one byte inside it.
      // This can break the operator, but that's the point -- it lets evolution stumble
      // onto neighbors the operator-level mutator would never reach.
      std::vector<uint8_t> span_bytes(bytes.begin() + span.offset,
                                      bytes.begin() + span.offset + span.length);
      std::uniform_int_distribution<size_t> offset_dist(0, span_bytes.size() - 1);
      const size_t offset = offset_dist(engine);
      span_bytes[offset] = static_cast<uint8_t>(byte_dist(engine));
      new_bytes.insert(new_bytes.end(), span_bytes.begin(), span_bytes.end());
      continue;
    }

    // Operator-level mutation: normalise the remaining probability mass and split three
    // ways. The shares (~70% replace / ~15% delete / ~15% insert) are tuned for the
    // tasks BEAST ships -- aggressive replacement keeps the population from clinging to
    // local minima, balanced by lighter delete / insert to vary genome length.
    const double remainder = roll - params.byte_mutation_share;
    const double remaining_share = std::max(1.0 - params.byte_mutation_share, 0.001);
    const double normalized = remainder / remaining_share;

    if (normalized < 0.70) {
      auto replacement = RandomProgramFactory::generateRandomOperator(
          params.variable_count, params.string_table_size, params.string_table_item_length,
          /*max_bytes=*/128, params.opcode_weights, params.subroutine_arities);
      new_bytes.insert(new_bytes.end(), replacement.begin(), replacement.end());
    } else if (normalized < 0.85) {
      // Deletion: contribute nothing for this span.
    } else {
      auto insertion = RandomProgramFactory::generateRandomOperator(
          params.variable_count, params.string_table_size, params.string_table_item_length,
          /*max_bytes=*/128, params.opcode_weights, params.subroutine_arities);
      new_bytes.insert(new_bytes.end(), insertion.begin(), insertion.end());
      new_bytes.insert(new_bytes.end(), bytes.begin() + span.offset,
                       bytes.begin() + span.offset + span.length);
    }
  }

  // Preserve trailing garbage so we don't accidentally shrink the genome on every
  // mutation pass; it's harmless tail data that the parser already ignores at
  // evaluation time.
  if (parse_result.trailing_garbage_bytes > 0) {
    const size_t tail_start = bytes.size() - parse_result.trailing_garbage_bytes;
    new_bytes.insert(new_bytes.end(), bytes.begin() + tail_start, bytes.end());
  }

  // Truncate at the last whole-operator boundary that fits inside max_genome_bytes.
  // This prevents the runaway "bloat" failure mode where insertions and crossover
  // concatenation compound across generations.
  if (params.max_genome_bytes > 0 && new_bytes.size() > params.max_genome_bytes) {
    const auto truncate_parse = ProgramParser::parse(new_bytes);
    size_t kept = 0;
    for (const auto& sp : truncate_parse.spans) {
      if (sp.offset + sp.length > params.max_genome_bytes) {
        break;
      }
      kept = sp.offset + sp.length;
    }
    new_bytes.resize(kept);
  }

  bytes = std::move(new_bytes);
  return mutations;
}

/// Operator-aware single-point crossover. Splits each parent at a randomly-chosen
/// operator boundary and produces two children by swapping the tails. Trailing garbage
/// (unparseable bytes at the end of the parent) is kept attached to its parent's
/// prefix, so garbage gets shuffled around but never lost entirely.
std::pair<std::vector<uint8_t>, std::vector<uint8_t>> operatorAwareCrossover(
    const std::vector<uint8_t>& parent_a, const std::vector<uint8_t>& parent_b,
    const EvolutionPipe::EvolutionParameters& params, std::mt19937_64& engine) {
  auto build_cut_points = [](const std::vector<uint8_t>& bytes) {
    std::vector<size_t> cut_points{0};
    const auto result = ProgramParser::parse(bytes);
    for (const auto& span : result.spans) {
      cut_points.push_back(span.offset + span.length);
    }
    // If parsing produced no spans, add the end-of-stream so the cut-point selection
    // has at least two choices and the crossover doesn't degenerate to "copy the whole
    // parent".
    if (cut_points.size() == 1) {
      cut_points.push_back(bytes.size());
    }
    return cut_points;
  };

  const auto cuts_a = build_cut_points(parent_a);
  const auto cuts_b = build_cut_points(parent_b);

  std::uniform_int_distribution<size_t> cut_a_dist(0, cuts_a.size() - 1);
  std::uniform_int_distribution<size_t> cut_b_dist(0, cuts_b.size() - 1);
  const size_t cut_a = cuts_a[cut_a_dist(engine)];
  const size_t cut_b = cuts_b[cut_b_dist(engine)];

  std::vector<uint8_t> child_a(parent_a.begin(), parent_a.begin() + cut_a);
  child_a.insert(child_a.end(), parent_b.begin() + cut_b, parent_b.end());

  std::vector<uint8_t> child_b(parent_b.begin(), parent_b.begin() + cut_b);
  child_b.insert(child_b.end(), parent_a.begin() + cut_a, parent_a.end());

  // Truncate both to max_genome_bytes at whole-operator boundaries. See the mutate
  // helper for the rationale.
  auto truncate = [&params](std::vector<uint8_t>& bytes) {
    if (params.max_genome_bytes == 0 || bytes.size() <= params.max_genome_bytes) {
      return;
    }
    const auto truncate_parse = ProgramParser::parse(bytes);
    size_t kept = 0;
    for (const auto& sp : truncate_parse.spans) {
      if (sp.offset + sp.length > params.max_genome_bytes) {
        break;
      }
      kept = sp.offset + sp.length;
    }
    bytes.resize(kept);
  };
  truncate(child_a);
  truncate(child_b);

  return {std::move(child_a), std::move(child_b)};
}

/// Produce one initial genome: drain the upstream pipe input pool first, fall back to
/// the random program factory if the pool is empty. The fallback path is essential --
/// the legacy implementation unconditionally called `drawInput()` and let
/// `std::underflow_error` propagate, taking down the worker if the input wasn't
/// pre-seeded.
std::vector<uint8_t> produceInitialGenome(
    EvolutionPipe& pipe, const EvolutionPipe::EvolutionParameters& params) {
  try {
    auto seed = pipe.drawInput(0);
    // `drawInput` returns `std::vector<unsigned char>`; the GA wants
    // `std::vector<uint8_t>`. They're the same type on every platform we target, but
    // the copy keeps us honest about type-safety if that ever changes.
    return std::vector<uint8_t>(seed.begin(), seed.end());
  } catch (const std::underflow_error&) {
    RandomProgramFactory factory;
    const uint32_t size = params.starting_program_size > 0
                              ? params.starting_program_size
                              : std::max<uint32_t>(params.variable_count * 8, 16);
    Program program =
        factory.generate(size, params.variable_count, params.string_table_size,
                         params.string_table_item_length, params.opcode_weights,
                         params.subroutine_arities);
    auto data = program.getData();
    return std::vector<uint8_t>(data.begin(), data.end());
  }
}

} // namespace

EvolutionPipe::EvolutionPipe(uint32_t max_candidates) : Pipe(max_candidates, 1, 1) {}

void EvolutionPipe::execute() {
  // Early-out if the pipeline asked to stop after the worker scheduled us but before we
  // started executing. Cheap; avoids wasting one full cycle's worth of work.
  if (isStopRequested()) {
    return;
  }

  // Stamp cycle bookkeeping *before* we do anything else so the UI sees the cycle as
  // "in flight" the moment execute() is on the stack -- even when the per-call setup
  // takes a noticeable amount of time. Without this, a pipe that's been queued shows
  // zero progress and then a sudden burst, which is exactly the diagnostic confusion
  // the progress bar is meant to dispel.
  const uint32_t population_size = getMaxCandidates();
  const uint32_t generations = evolution_parameters_.generations;
  const uint64_t expected_evals = evolution_parameters_.elitism
                                      ? static_cast<uint64_t>(population_size) +
                                            static_cast<uint64_t>(population_size > 0
                                                                      ? population_size - 1
                                                                      : 0) *
                                                static_cast<uint64_t>(generations)
                                      : static_cast<uint64_t>(population_size) *
                                            (static_cast<uint64_t>(generations) + 1U);
  {
    std::scoped_lock progress_lock(progress_mutex_);
    ++cycle_index_;
    currently_running_ = true;
    expected_evaluations_this_cycle_ = expected_evals;
    cycle_started_at_ = std::chrono::steady_clock::now();
    evaluations_this_cycle_.store(0, std::memory_order_relaxed);
  }
  // RAII guard so an exception in BeastGA cannot leave the pipe stuck in
  // `currently_running_ = true`. The guard captures the pipe by reference; its
  // destructor takes the progress mutex, stamps end-of-cycle, and clears the running
  // flag. If we reach the natural end of execute() with `committed = true` set, the
  // guard skips its own bookkeeping because the regular end-of-cycle stamp already ran.
  struct ProgressGuard {
    EvolutionPipe* self;
    bool committed = false;
    ProgressGuard(EvolutionPipe* s, bool c) noexcept : self(s), committed(c) {}
    ProgressGuard(const ProgressGuard&) = delete;
    ProgressGuard(ProgressGuard&&) = delete;
    ProgressGuard& operator=(const ProgressGuard&) = delete;
    ProgressGuard& operator=(ProgressGuard&&) = delete;
    ~ProgressGuard() {
      if (committed) {
        return;
      }
      std::scoped_lock progress_lock(self->progress_mutex_);
      const auto now = std::chrono::steady_clock::now();
      self->last_cycle_seconds_ =
          std::chrono::duration<double>(now - self->cycle_started_at_).count();
      // Don't overwrite last_cycle_best_score_ on the exception / abort path -- the
      // prior cycle's value is more informative to the UI than zero.
      self->currently_running_ = false;
    }
  };
  ProgressGuard guard{this, false};

  // ---- Wire up BeastGA --------------------------------------------------------------
  //
  // Operators: thin lambdas that own a per-instance RNG (via a shared_ptr because the
  // GA's `Operators` struct stores `std::function` and we want both mutate and
  // crossover to draw from the same engine for deterministic-given-seed behaviour).
  auto mutate_rng = std::make_shared<std::mt19937_64>(std::random_device{}());

  internal::Operators ops;
  ops.produce_initial_genome = [this] {
    return produceInitialGenome(*this, this->evolution_parameters_);
  };
  ops.mutate = [this, mutate_rng](std::vector<uint8_t>& bytes, double probability) {
    return operatorAwareMutate(bytes, probability, this->evolution_parameters_, *mutate_rng);
  };
  ops.crossover = [this, mutate_rng](const std::vector<uint8_t>& a,
                                     const std::vector<uint8_t>& b) {
    return operatorAwareCrossover(a, b, this->evolution_parameters_, *mutate_rng);
  };

  // Evaluator: CPU thread-pool implementation that wraps our virtual `evaluate()`. A
  // future GPU implementation would replace this with a CUDA-backed `BatchEvaluator`
  // -- no other code in this function changes.
  ThreadPoolBatchEvaluator evaluator(
      [this](const std::vector<uint8_t>& bytes) { return this->evaluate(bytes); },
      [this] { this->recordEvaluatorCall(); });

  // Finalist streaming: every cutoff-crossing genome lands in the output buffer the
  // moment its score comes back, instead of waiting for the entire cycle. This is the
  // "no more burst then silence" win on the UI side -- downstream pipes see candidates
  // generation by generation.
  internal::FinalistSink sink = [this](std::vector<uint8_t> bytes, double score) {
    this->storeFinalist(std::move(bytes), static_cast<float>(score));
  };

  internal::GenerationHook hook = [this](uint32_t /*gen*/, double best_score) {
    std::scoped_lock progress_lock(progress_mutex_);
    last_cycle_best_score_ = best_score;
  };

  internal::Config cfg;
  cfg.population_size = population_size;
  cfg.generations = generations;
  cfg.crossover_probability = evolution_parameters_.crossover_probability;
  cfg.mutation_probability = evolution_parameters_.mutation_probability;
  cfg.elitism = evolution_parameters_.elitism;
  cfg.tournament_size = 3;

  internal::BeastGA ga(cfg, std::move(ops), evaluator, std::move(sink), std::move(hook),
                       cut_off_score_);

  // Stop token: we pass the raw atomic pointer through to BeastGA so each generation
  // can poll it. The shared_ptr stays alive in `Pipe::stop_token_` for the duration of
  // this call (we hold the shared_ptr indirectly via `getStopToken()`).
  auto token_handle = getStopToken();
  const std::atomic<bool>* token_ptr = token_handle.get();

  const auto run_result = ga.run(token_ptr);

  // If we got stopped mid-run, the GA's final-population state may be partially
  // contaminated with sentinel zero scores. Match the legacy semantics: the user
  // pressed Stop, not Save -- skip the final hall-of-fame harvest.
  if (run_result.stopped_early || isStopRequested()) {
    return;
  }

  // Hall-of-fame: BeastGA tracked the best-ever genome across all generations
  // (independent of the streaming sink and independent of whether the current
  // population still contains it). Store it once at end of cycle.
  if (!run_result.best_bytes.empty() && run_result.best_score >= cut_off_score_) {
    storeFinalist(run_result.best_bytes, static_cast<float>(run_result.best_score));
  }

  // Stamp cycle end and tell the RAII guard we already committed. The guard's
  // destructor sees `committed = true` and skips its own bookkeeping, so we don't
  // double-write the cycle-end fields. On the early-return paths above the guard
  // handles cleanup itself.
  {
    std::scoped_lock progress_lock(progress_mutex_);
    const auto now = std::chrono::steady_clock::now();
    last_cycle_seconds_ =
        std::chrono::duration<double>(now - cycle_started_at_).count();
    last_cycle_best_score_ = run_result.best_score;
    currently_running_ = false;
  }
  guard.committed = true;
}

void EvolutionPipe::recordEvaluatorCall() noexcept {
  evaluations_this_cycle_.fetch_add(1, std::memory_order_relaxed);
}

EvolutionPipe::Progress EvolutionPipe::getProgress() const noexcept {
  std::scoped_lock progress_lock(progress_mutex_);
  Progress p;
  p.cycle_index = cycle_index_;
  p.currently_running = currently_running_;
  p.evaluations_this_cycle = evaluations_this_cycle_.load(std::memory_order_relaxed);
  p.expected_evaluations_this_cycle = expected_evaluations_this_cycle_;
  if (currently_running_) {
    p.seconds_in_cycle =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - cycle_started_at_)
            .count();
  }
  p.last_cycle_seconds = last_cycle_seconds_;
  p.last_cycle_best_score = last_cycle_best_score_;
  return p;
}

void EvolutionPipe::setCutOffScore(double cut_off_score) { cut_off_score_ = cut_off_score; }

double EvolutionPipe::getCutOffScore() const noexcept { return cut_off_score_; }

void EvolutionPipe::setEvolutionParameters(const EvolutionParameters& parameters) {
  evolution_parameters_ = parameters;
}

const EvolutionPipe::EvolutionParameters& EvolutionPipe::getEvolutionParameters() const noexcept {
  return evolution_parameters_;
}

void EvolutionPipe::setNumGenerations(uint32_t generations) {
  evolution_parameters_.generations = generations;
}

void EvolutionPipe::setMutationProbability(double probability) {
  evolution_parameters_.mutation_probability = probability;
}

void EvolutionPipe::setCrossoverProbability(double probability) {
  evolution_parameters_.crossover_probability = probability;
}

void EvolutionPipe::storeFinalist(const std::vector<unsigned char>& finalist, float score) {
  storeOutput(0, {finalist, static_cast<double>(score)});
}

void EvolutionPipe::storeFinalist(std::vector<unsigned char>&& finalist, float score) {
  storeOutput(0, {std::move(finalist), static_cast<double>(score)});
}

} // namespace beast
