#include <beast/pipes/evolution_pipe.hpp>

// Standard
#include <algorithm>
#include <mutex>
#include <random>
#include <stdexcept>
#include <vector>

// Internal
#include <beast/program.hpp>
#include <beast/program_parser.hpp>
#include <beast/random_program_factory.hpp>

// GAlib
// NOTE: For these includes, the `register` error needs to be ignored as this 3rdparty library
// uses outdated C++03-style code. The library itself is fine; we just suppress the noise.
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#endif
#include <ga/GAListGenome.h>
#include <ga/GASimpleGA.h>
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

namespace beast {

namespace {

/// Bundle of data passed to GAlib via `GAGenome::userData`.
///
/// GAlib threads our `EvolutionPipe*` into static callbacks via the `userData` pointer. We need
/// both the pipe (to invoke evaluation and draw seed inputs) and the evolution parameters (to
/// mint random operators with the right environment).
struct GenomeUserData {
  EvolutionPipe* pipe;
  const EvolutionPipe::EvolutionParameters* parameters;
};

/// Thread-local RNG used by the GAlib callbacks. GAlib is single-threaded by default so a
/// thread-local engine has the right ownership semantics (one engine per worker thread).
std::mt19937& threadEngine() {
  thread_local std::mt19937 engine{std::random_device{}()};
  return engine;
}

/// Process-wide mutex serialising every entry into GAlib.
///
/// GAlib (3rdparty/galib/ga/garandom.C) keeps its RNG state in *file-static* variables --
/// `idum`, `iy`, `iv[NTAB]`, etc. Every selector/mutator/crossover dispatched by
/// `GASimpleGA::evolve()` calls back into `GARandomFloat()` / `GARandomInt()`, which means
/// running two `EvolutionPipe::execute()`s concurrently across worker threads is an
/// unprotected race on those statics. In practice (e.g. a pipeline with four parallel
/// evolution stages running at the same time) this manifests as `iv[]` getting indexed
/// with a corrupted `iy`, and the process dies with a SIGSEGV inside `garandom.C` while
/// the rest of the pipeline looks perfectly healthy from the outside.
///
/// Until the day we fork GAlib to make its RNG thread-local, the cheap-and-correct fix is
/// to serialise *all* GAlib entries behind one mutex. This is a global mutex (not per
/// pipe) because the contention is on GAlib's globals, not anything we own. Throughput
/// loss is acceptable: in a real pipeline most time is spent inside evaluators (which
/// themselves don't call GAlib RNG directly -- they go through VM execution), and there
/// is usually one bottleneck evaluator anyway.
std::mutex& galibSerialisationMutex() {
  static std::mutex mutex;
  return mutex;
}

/// Read a `GAListGenome<unsigned char>` into a contiguous byte vector. Uses `operator[]`
/// (technically O(N) per index, so O(N^2) overall), but with `max_genome_bytes` capping the
/// genome at a few KB this is fine in practice and avoids messing with GAlib's internal
/// iterator state.
std::vector<unsigned char> genomeToBytes(GAListGenome<unsigned char>& genome) {
  std::vector<unsigned char> data;
  data.reserve(genome.size());
  for (int idx = 0; idx < genome.size(); ++idx) {
    data.push_back(*genome[idx]);
  }
  return data;
}

/// Replace the contents of a `GAListGenome<unsigned char>` with the given bytes.
///
/// IMPORTANT: GAlib's `GAList::destroy()` removes *only the current node*, not the entire list
/// (the name is misleading). We have to walk and destroy every node manually, otherwise the
/// genome grows unbounded across generations and the GA spends progressively longer evaluating
/// ever-larger bytecode. This was a long-standing source of "the pipeline mysteriously slows to
/// a crawl after ~20 generations" reports.
void bytesToGenome(GAListGenome<unsigned char>& genome, const std::vector<unsigned char>& bytes) {
  while (genome.size() > 0) {
    genome.GAList<unsigned char>::destroy();
  }
  for (unsigned char value : bytes) {
    genome.insert(value);
  }
}

/// Clamp a byte stream to `max_bytes` by parsing it back into operator spans and chopping off
/// any tail that doesn't fit. We always preserve full operators (never half-instructions) so
/// the genome stays parseable on the next iteration.
std::vector<unsigned char> truncateToMaxBytes(const std::vector<unsigned char>& bytes,
                                              uint32_t max_bytes) {
  if (max_bytes == 0 || bytes.size() <= max_bytes) {
    return bytes;
  }
  const auto parse_result = ProgramParser::parse(bytes);
  size_t kept = 0;
  for (const auto& span : parse_result.spans) {
    if (span.offset + span.length > max_bytes) {
      break;
    }
    kept = span.offset + span.length;
  }
  return std::vector<unsigned char>(bytes.begin(), bytes.begin() + kept);
}

// NOLINTNEXTLINE: GAlib's evaluator signature mandates a non-const reference parameter.
float staticEvaluatorWrapper(GAGenome& genome) {
  auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(genome);
  auto* user_data = static_cast<GenomeUserData*>(genome.userData());
  const std::vector<unsigned char> data = genomeToBytes(list_genome);
  return static_cast<float>(user_data->pipe->evaluate(data));
}

// NOLINTNEXTLINE: GAlib's initializer signature mandates a non-const reference parameter.
void staticInitializerWrapper(GAGenome& genome) {
  auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(genome);
  auto* user_data = static_cast<GenomeUserData*>(genome.userData());
  const auto& params = *user_data->parameters;

  // Always drain the user-supplied seed pool first; this lets callers prime the GA with
  // hand-crafted starting programs or upstream pipe output. If we run out of seeds, fall
  // back to fresh random programs so GAlib's population fills up without crashing - the
  // legacy implementation unconditionally called drawInput() and propagated the underflow
  // exception straight into GAlib's initialization loop, taking down the worker.
  std::vector<unsigned char> seed;
  try {
    seed = user_data->pipe->drawInput(0);
  } catch (const std::underflow_error&) {
    RandomProgramFactory factory;
    const uint32_t size = params.starting_program_size > 0
                              ? params.starting_program_size
                              : std::max<uint32_t>(params.variable_count * 8, 16);
    Program program = factory.generate(size, params.variable_count, params.string_table_size,
                                       params.string_table_item_length, params.opcode_weights);
    seed = program.getData();
  }
  bytesToGenome(list_genome, seed);
}

/// Operator-aware mutation.
///
/// Mutates the genome at instruction boundaries: each parsed operator independently rolls
/// against the mutation probability and, when chosen, is either:
///   - byte-flipped at a single random offset (the "gamble" mutation, fraction
///     `byte_mutation_share`), OR
///   - replaced with a freshly minted random operator (~70% of the remainder), OR
///   - deleted (~15%), OR
///   - has a new operator inserted before it (~15%).
///
/// Trailing garbage bytes (unparseable tail of the genome) are preserved as-is so the byte
/// gamble can still expose them to mutation indirectly via later regenerations.
///
/// Returns the number of mutations performed, as GAlib expects.
// NOLINTNEXTLINE: GAlib mutator signature.
int operatorAwareMutator(GAGenome& genome, float probability) {
  auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(genome);
  auto* user_data = static_cast<GenomeUserData*>(genome.userData());
  const auto& params = *user_data->parameters;
  auto& engine = threadEngine();

  std::vector<unsigned char> bytes = genomeToBytes(list_genome);
  if (bytes.empty()) {
    return 0;
  }

  const auto parse_result = ProgramParser::parse(bytes);
  if (parse_result.spans.empty()) {
    // Nothing recognizable; just flip a single byte at random so we don't go entirely stale.
    std::uniform_int_distribution<size_t> idx_dist(0, bytes.size() - 1);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    const size_t idx = idx_dist(engine);
    bytes[idx] = static_cast<unsigned char>(byte_dist(engine));
    bytesToGenome(list_genome, bytes);
    return 1;
  }

  std::uniform_real_distribution<float> prob_dist(0.0F, 1.0F);
  std::uniform_real_distribution<float> action_dist(0.0F, 1.0F);
  std::uniform_int_distribution<int> byte_dist(0, 255);

  // Rebuild the byte stream from scratch by appending each (possibly mutated) span to a new
  // vector. This makes deletions/insertions trivial.
  std::vector<unsigned char> new_bytes;
  new_bytes.reserve(bytes.size());
  int mutations = 0;

  for (const auto& span : parse_result.spans) {
    if (prob_dist(engine) >= probability) {
      // No mutation: copy the span verbatim.
      new_bytes.insert(new_bytes.end(), bytes.begin() + span.offset,
                       bytes.begin() + span.offset + span.length);
      continue;
    }

    ++mutations;
    const float roll = action_dist(engine);

    if (roll < static_cast<float>(params.byte_mutation_share)) {
      // Byte-level "gamble" mutation: copy the span, flip exactly one byte inside it. This
      // can break the operator, but that's the point - it lets evolution stumble onto
      // neighbors the operator-level mutator would never reach.
      std::vector<unsigned char> span_bytes(bytes.begin() + span.offset,
                                            bytes.begin() + span.offset + span.length);
      std::uniform_int_distribution<size_t> offset_dist(0, span_bytes.size() - 1);
      const size_t offset = offset_dist(engine);
      span_bytes[offset] = static_cast<unsigned char>(byte_dist(engine));
      new_bytes.insert(new_bytes.end(), span_bytes.begin(), span_bytes.end());
      continue;
    }

    // Operator-level mutation. Normalize the remaining probability mass and split three ways.
    const float remainder = roll - static_cast<float>(params.byte_mutation_share);
    const float remaining_share =
        std::max(1.0F - static_cast<float>(params.byte_mutation_share), 0.001F);
    const float normalized = remainder / remaining_share;

    if (normalized < 0.70F) {
      auto replacement = RandomProgramFactory::generateRandomOperator(
          params.variable_count, params.string_table_size, params.string_table_item_length,
          /*max_bytes=*/64, params.opcode_weights);
      new_bytes.insert(new_bytes.end(), replacement.begin(), replacement.end());
    } else if (normalized < 0.85F) {
      // Deletion: contribute nothing for this span.
    } else {
      auto insertion = RandomProgramFactory::generateRandomOperator(
          params.variable_count, params.string_table_size, params.string_table_item_length,
          /*max_bytes=*/64, params.opcode_weights);
      new_bytes.insert(new_bytes.end(), insertion.begin(), insertion.end());
      new_bytes.insert(new_bytes.end(), bytes.begin() + span.offset,
                       bytes.begin() + span.offset + span.length);
    }
  }

  // Preserve trailing garbage so we don't accidentally shrink the genome on every mutation
  // pass; it's harmless tail data that the parser already ignores at evaluation time.
  if (parse_result.trailing_garbage_bytes > 0) {
    const size_t tail_start = bytes.size() - parse_result.trailing_garbage_bytes;
    new_bytes.insert(new_bytes.end(), bytes.begin() + tail_start, bytes.end());
  }

  new_bytes = truncateToMaxBytes(new_bytes, params.max_genome_bytes);
  bytesToGenome(list_genome, new_bytes);
  return mutations;
}

/// Operator-aware single-point crossover.
///
/// Splits each parent at a randomly-chosen operator boundary and produces two children by
/// swapping the tails. Trailing garbage (unparseable bytes at the end of the parent) is kept
/// attached to its parent's prefix, so garbage gets shuffled around but never lost entirely.
///
/// Returns the number of children produced (GAlib convention).
// NOLINTNEXTLINE: GAlib crossover signature.
int operatorAwareCrossover(const GAGenome& parent1, const GAGenome& parent2, GAGenome* child1,
                           GAGenome* child2) {
  const auto* user_data = static_cast<GenomeUserData*>(parent1.userData());
  const auto& params = *user_data->parameters;
  const auto& p1 = dynamic_cast<const GAListGenome<unsigned char>&>(parent1);
  const auto& p2 = dynamic_cast<const GAListGenome<unsigned char>&>(parent2);
  // GAlib hands us non-const pointers to children of the same type as the parents; the const
  // cast is the standard idiom to read parents through the non-const accessor for the
  // conversion.
  std::vector<unsigned char> p1_bytes =
      genomeToBytes(const_cast<GAListGenome<unsigned char>&>(p1));
  std::vector<unsigned char> p2_bytes =
      genomeToBytes(const_cast<GAListGenome<unsigned char>&>(p2));

  auto buildCutPoints = [](const std::vector<unsigned char>& bytes) {
    std::vector<size_t> cut_points{0};
    const auto result = ProgramParser::parse(bytes);
    for (const auto& span : result.spans) {
      cut_points.push_back(span.offset + span.length);
    }
    // If parsing produced no spans, add the end-of-stream so the cut-point selection has
    // at least two choices and the crossover doesn't degenerate to "copy the whole parent".
    if (cut_points.size() == 1) {
      cut_points.push_back(bytes.size());
    }
    return cut_points;
  };

  const auto cuts1 = buildCutPoints(p1_bytes);
  const auto cuts2 = buildCutPoints(p2_bytes);

  auto& engine = threadEngine();
  std::uniform_int_distribution<size_t> cut1_dist(0, cuts1.size() - 1);
  std::uniform_int_distribution<size_t> cut2_dist(0, cuts2.size() - 1);
  const size_t cut1 = cuts1[cut1_dist(engine)];
  const size_t cut2 = cuts2[cut2_dist(engine)];

  std::vector<unsigned char> child1_bytes(p1_bytes.begin(), p1_bytes.begin() + cut1);
  child1_bytes.insert(child1_bytes.end(), p2_bytes.begin() + cut2, p2_bytes.end());

  std::vector<unsigned char> child2_bytes(p2_bytes.begin(), p2_bytes.begin() + cut2);
  child2_bytes.insert(child2_bytes.end(), p1_bytes.begin() + cut1, p1_bytes.end());

  child1_bytes = truncateToMaxBytes(child1_bytes, params.max_genome_bytes);
  child2_bytes = truncateToMaxBytes(child2_bytes, params.max_genome_bytes);

  int produced = 0;
  if (child1 != nullptr) {
    auto& c1 = dynamic_cast<GAListGenome<unsigned char>&>(*child1);
    bytesToGenome(c1, child1_bytes);
    ++produced;
  }
  if (child2 != nullptr) {
    auto& c2 = dynamic_cast<GAListGenome<unsigned char>&>(*child2);
    bytesToGenome(c2, child2_bytes);
    ++produced;
  }
  return produced;
}

} // namespace

EvolutionPipe::EvolutionPipe(uint32_t max_candidates) : Pipe(max_candidates, 1, 1) {}

void EvolutionPipe::execute() {
  // Hold the GAlib serialisation mutex for the entirety of execute(). Both the
  // `algorithm.evolve()` call AND the post-run harvest (`algorithm.statistics()`,
  // `algorithm.population()`, genome iteration) reach into GAlib internals that touch
  // the shared RNG, so the lock has to cover the full lifetime of the local algorithm
  // instance. See `galibSerialisationMutex` for the why.
  std::scoped_lock galib_lock(galibSerialisationMutex());

  GenomeUserData user_data{this, &evolution_parameters_};

  GAListGenome<unsigned char> genome(staticEvaluatorWrapper);
  genome.initializer(staticInitializerWrapper);
  genome.mutator(operatorAwareMutator);
  genome.crossover(operatorAwareCrossover);
  genome.userData(&user_data);

  GASimpleGA algorithm(genome);
  algorithm.populationSize(getMaxCandidates());
  algorithm.nGenerations(evolution_parameters_.generations);
  algorithm.pCrossover(static_cast<float>(evolution_parameters_.crossover_probability));
  algorithm.pMutation(static_cast<float>(evolution_parameters_.mutation_probability));
  algorithm.elitist(evolution_parameters_.elitism ? gaTrue : gaFalse);

  algorithm.evolve();

  // Save finalists from the final population (filtered by cut-off score). With elitism enabled
  // the best-ever individual is *usually* in there, but GAlib's elitism only protects across
  // consecutive generations - a high scorer that survived for a while can still be lost if
  // mutation pressure is high. The hall-of-fame block below catches that case.
  const GAPopulation& population = algorithm.population();
  for (int32_t pop_idx = 0; pop_idx < population.size(); ++pop_idx) {
    GAGenome& individual = population.individual(pop_idx);
    auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(individual);
    if (list_genome.size() > 0 && individual.score() >= cut_off_score_) {
      storeFinalist(genomeToBytes(list_genome), static_cast<float>(individual.score()));
    }
  }

  // Also harvest the best-ever individual GAlib has been tracking via its own statistics.
  // GAlib stores a *copy* of the best individual seen so far (`maxever`), so this is the
  // canonical "best program this run produced", independent of whether it currently exists
  // in the final population.
  const GAGenome& best_ever = algorithm.statistics().bestIndividual();
  if (best_ever.score() >= cut_off_score_) {
    auto& best_list =
        dynamic_cast<GAListGenome<unsigned char>&>(const_cast<GAGenome&>(best_ever));
    if (best_list.size() > 0) {
      storeFinalist(genomeToBytes(best_list), static_cast<float>(best_ever.score()));
    }
  }
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
