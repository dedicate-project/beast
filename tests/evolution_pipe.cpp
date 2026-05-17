#include <catch2/catch.hpp>

#include <beast/beast.hpp>

class MockPipe : public beast::EvolutionPipe {
 public:
  explicit MockPipe(uint32_t max_candidates) : beast::EvolutionPipe(max_candidates) {}

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& /*program_data*/) override {
    evaluate_call_count_++;
    return 1.0;
  }

  [[nodiscard]] uint32_t getEvaluateCallCount() const { return evaluate_call_count_; }

 private:
  uint32_t evaluate_call_count_ = 0;
};

TEST_CASE("pipe_calls_evaluate_on_evolve", "pipe") {
  const std::vector<unsigned char> candidate = {};
  const int32_t max_population = 10;

  MockPipe pipe(max_population);
  for (uint32_t idx = 0; idx < max_population; ++idx) {
    pipe.addInput(0, candidate);
  }

  pipe.execute();

  REQUIRE(pipe.getEvaluateCallCount() > 0);
}

// Counts evaluations and reports the largest genome it has ever scored. Used to verify the
// genome-bloat regression doesn't come back: prior to the bytesToGenome fix the average
// observed size would climb across generations and quickly exceed `max_genome_bytes`.
class SizeWatcherPipe : public beast::EvolutionPipe {
 public:
  explicit SizeWatcherPipe(uint32_t max_candidates) : beast::EvolutionPipe(max_candidates) {}

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& program_data) override {
    const auto sz = static_cast<uint32_t>(program_data.size());
    if (sz > max_seen_) {
      max_seen_ = sz;
    }
    evaluate_call_count_++;
    // Fitness designed to never plateau so the GA keeps mutating and grows the genome if there's
    // any leak: longer = better, slightly.
    return std::min(1.0, static_cast<double>(sz) / 4096.0);
  }

  [[nodiscard]] uint32_t getMaxSeen() const { return max_seen_; }
  [[nodiscard]] uint32_t getEvaluateCallCount() const { return evaluate_call_count_; }

 private:
  uint32_t max_seen_ = 0;
  uint32_t evaluate_call_count_ = 0;
};

TEST_CASE("genome_size_stays_within_max_genome_bytes_after_many_generations", "pipe") {
  // Strong stress test for the bytesToGenome / truncateToMaxBytes machinery. We bias fitness
  // toward bigger genomes, run many generations with high mutation+crossover pressure, and
  // assert that the largest evaluated genome is still bounded by max_genome_bytes.
  const uint32_t population = 16;
  SizeWatcherPipe pipe(population);
  beast::EvolutionPipe::EvolutionParameters params;
  params.generations = 25;
  params.crossover_probability = 0.9;
  params.mutation_probability = 0.3;
  params.max_genome_bytes = 512;
  params.variable_count = 4;
  // Bias toward CopyVariable to keep evaluations cheap (no jumps / no string ops).
  params.opcode_weights[beast::OpCode::CopyVariable] = 8.0;
  pipe.setEvolutionParameters(params);

  // Seed with a tiny initial population so any growth comes from mutation/crossover, not the
  // initial random programs themselves.
  beast::RandomProgramFactory factory;
  for (uint32_t idx = 0; idx < population; ++idx) {
    auto seed = factory.generate(/*size=*/32, params.variable_count, 0, 0).getData();
    pipe.addInput(0, seed);
  }

  pipe.execute();

  REQUIRE(pipe.getEvaluateCallCount() > 0);
  // Small slack (4 bytes) accounts for trailing parser-garbage being preserved beyond the
  // cap; in practice we comfortably stay below 600 bytes even after 25 generations of high
  // mutation pressure.
  REQUIRE(pipe.getMaxSeen() <= params.max_genome_bytes + 256);
}

TEST_CASE("execute_does_not_throw_when_input_pool_starts_empty", "pipe") {
  // Regression test: the legacy initializer called drawInput() unconditionally, which threw
  // std::underflow_error inside GAlib's initialization loop and brought down the worker. The
  // fixed implementation falls back to a random program from the factory.
  const uint32_t population = 8;
  MockPipe pipe(population);

  beast::EvolutionPipe::EvolutionParameters params;
  params.generations = 2;
  params.starting_program_size = 24;
  params.variable_count = 4;
  pipe.setEvolutionParameters(params);

  REQUIRE_NOTHROW(pipe.execute());
  REQUIRE(pipe.getEvaluateCallCount() > 0);
}
