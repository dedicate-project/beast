// Standard
#include <chrono>
#include <thread>

// Catch2
#include <catch2/catch.hpp>

// BEAST
#include <beast/beast.hpp>

namespace {
class MockPipe : public beast::EvolutionPipe {
 public:
  MockPipe() : EvolutionPipe(1) {}

  [[nodiscard]] double evaluate(const std::vector<unsigned char>& /*program_data*/) override {
    return 0.0;
  }
};

/// A multi-slot pipe used purely to verify slot-routing inside the pipeline. It exposes
/// `inputs_count` independent input slots and `outputs_count` independent output slots; its
/// `execute()` method copies whatever it received on each input slot to the OUTPUT SLOT WITH THE
/// SAME LOGICAL INDEX. We then drive it through a pipeline that uses non-trivial slot mappings
/// (input slot K wired to a producer's slot K') and assert that data ends up where it should.
class MultiSlotPassthroughPipe : public beast::Pipe {
 public:
  MultiSlotPassthroughPipe(uint32_t max_candidates, uint32_t inputs_count, uint32_t outputs_count)
      : beast::Pipe(max_candidates, inputs_count, outputs_count) {}

  void execute() override {
    for (uint32_t slot = 0; slot < getInputSlotCount(); ++slot) {
      while (getInputSlotAmount(slot) > 0) {
        auto data = drawInput(slot);
        // Forward to the SAME logical slot index on the output side.
        if (slot < getOutputSlotCount()) {
          storeOutput(slot, {std::move(data), 0.0});
        }
      }
    }
  }

  // Sources need their inputs to be saturated before execute() is called, but we want to drain
  // whatever is available regardless - otherwise the pipeline gets stuck if the upstream produces
  // less than `max_candidates` items per cycle.
  [[nodiscard]] bool inputsAreSaturated() override {
    for (uint32_t slot = 0; slot < getInputSlotCount(); ++slot) {
      if (getInputSlotAmount(slot) > 0) {
        return true;
      }
    }
    return getInputSlotCount() == 0;
  }
};

/// Source-only pipe that emits a fixed payload on a specific output slot on every execute().
class TaggedSourcePipe : public beast::Pipe {
 public:
  TaggedSourcePipe(uint32_t max_candidates, uint32_t outputs_count, uint32_t emit_slot,
                   std::vector<unsigned char> payload)
      : beast::Pipe(max_candidates, 0, outputs_count), emit_slot_{emit_slot},
        payload_{std::move(payload)} {}

  void execute() override { storeOutput(emit_slot_, {payload_, 1.0}); }

 private:
  uint32_t emit_slot_;
  std::vector<unsigned char> payload_;
};
} // namespace

TEST_CASE("pipeline") {
  SECTION("adding_pipes_to_pipelines_and_retrieving_them_works_correctly") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe0", pipe0);
    std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe1", pipe1);

    const auto& pipes = pipeline.getPipes();
    auto iter = pipes.begin();
    REQUIRE((*iter)->pipe == pipe0);
    iter++;
    REQUIRE((*iter)->pipe == pipe1);
  }

  SECTION("adding_a_pipe_to_a_pipeline_twice_throws") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe1", pipe);

    bool threw = false;
    try {
      pipeline.addPipe("pipe2", pipe);
    } catch (...) {
      threw = true;
    }

    REQUIRE(threw == true);
  }

  SECTION("adding_a_connection_with_source_pipe_not_in_pipeline_throws") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
    std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe1", pipe1);

    bool threw = false;
    try {
      pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);
    } catch (...) {
      threw = true;
    }

    REQUIRE(threw == true);
  }

  SECTION("adding_a_connection_with_destination_pipe_not_in_pipeline_throws") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe0", pipe0);
    std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();

    bool threw = false;
    try {
      pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);
    } catch (...) {
      threw = true;
    }

    REQUIRE(threw == true);
  }

  SECTION("adding_a_connection_with_either_pipe_not_in_pipeline_throws") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
    std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();

    bool threw = false;
    try {
      pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);
    } catch (...) {
      threw = true;
    }

    REQUIRE(threw == true);
  }

  SECTION("adding_connections_with_the_same_source_throws") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe0", pipe0);
    std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe1", pipe1);
    std::shared_ptr<beast::Pipe> pipe2 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe2", pipe2);

    pipeline.connectPipes(pipe0, 0, pipe1, 1, 1);

    bool threw = false;
    try {
      pipeline.connectPipes(pipe0, 0, pipe2, 2, 1);
    } catch (...) {
      threw = true;
    }

    REQUIRE(threw == true);
  }

  SECTION("adding_connections_with_the_same_destination_throws") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe0", pipe0);
    std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe1", pipe1);
    std::shared_ptr<beast::Pipe> pipe2 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe2", pipe2);

    pipeline.connectPipes(pipe0, 0, pipe2, 2, 1);

    bool threw = false;
    try {
      pipeline.connectPipes(pipe1, 1, pipe2, 2, 1);
    } catch (...) {
      threw = true;
    }

    REQUIRE(threw == true);
  }

  SECTION("adding_the_same_connection_twice_throws") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe0", pipe0);
    std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe1", pipe1);

    pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);

    bool threw = false;
    try {
      pipeline.connectPipes(pipe0, 0, pipe1, 0, 1);
    } catch (...) {
      threw = true;
    }

    REQUIRE(threw == true);
  }

  SECTION("adding_a_connection_and_retrieving_it_works") {
    beast::Pipeline pipeline;
    std::shared_ptr<beast::Pipe> pipe0 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe0", pipe0);
    std::shared_ptr<beast::Pipe> pipe1 = std::make_shared<MockPipe>();
    pipeline.addPipe("pipe1", pipe1);

    pipeline.connectPipes(pipe0, 0, pipe1, 1, 1);

    const auto& connections = pipeline.getConnections();
    REQUIRE(connections.size() == 1);

    const auto& connection = *connections.begin();
    REQUIRE(connection->source_pipe->pipe == pipe0);
    REQUIRE(connection->source_slot_index == 0);
    REQUIRE(connection->destination_pipe->pipe == pipe1);
    REQUIRE(connection->destination_slot_index == 1);
  }

  SECTION("stopping_a_stopped_pipeline_throws") {
    beast::Pipeline pipeline;
    REQUIRE_THROWS_AS(pipeline.stop(), std::invalid_argument);
  }

  SECTION("starting_a_started_pipeline_throws") {
    beast::Pipeline pipeline;
    pipeline.start();
    REQUIRE_THROWS_AS(pipeline.start(), std::invalid_argument);
  }

  SECTION("non_zero_slot_indices_are_routed_correctly") {
    // Regression test for a pair of inverted slot-matching predicates in processInputSlots /
    // processOutputSlots: they used to match `connection->source_slot_index == slot_index` from
    // the destination's perspective (and vice-versa), which silently worked only when every pipe
    // used slot 0 everywhere. With a source that emits on slot 2 wired to a destination on slot
    // 1, the buggy code would have refused to forward any data and this test would time out.
    const std::vector<unsigned char> payload = {0xde, 0xad, 0xbe, 0xef};

    beast::Pipeline pipeline;
    auto source = std::make_shared<TaggedSourcePipe>(/*max_candidates=*/4, /*outputs_count=*/3,
                                                     /*emit_slot=*/2, payload);
    auto sink = std::make_shared<MultiSlotPassthroughPipe>(/*max_candidates=*/4,
                                                           /*inputs_count=*/3,
                                                           /*outputs_count=*/3);
    pipeline.addPipe("source", source);
    pipeline.addPipe("sink", sink);
    pipeline.connectPipes(source, /*source_slot_index=*/2, sink, /*destination_slot_index=*/1,
                          /*buffer_size=*/8);

    pipeline.start();

    // Wait for the sink's output slot 1 to receive the payload (or time out).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (sink->getOutputSlotAmount(1) == 0 && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    pipeline.stop();

    REQUIRE(sink->getOutputSlotAmount(1) > 0);
    REQUIRE(sink->getOutputSlotAmount(0) == 0);
    REQUIRE(sink->getOutputSlotAmount(2) == 0);

    const auto item = sink->drawOutput(1);
    REQUIRE(item.data == payload);
  }

  SECTION("stop_wakes_idle_workers_quickly") {
    // Regression test for the worker-pacing change: previously every worker did a fixed
    // sleep_for(10 ms) at the bottom of its loop, so a pipeline with N idle workers paid up to
    // N * 10 ms before stop() returned. With the condition_variable-based pacing the
    // notify_all() in stop() wakes every idle worker at once, so shutdown should comfortably
    // fit in well under 50 ms even with a handful of completely idle pipes wired up.
    beast::Pipeline pipeline;
    constexpr uint32_t kIdleCount = 8;
    std::vector<std::shared_ptr<MultiSlotPassthroughPipe>> idle_pipes;
    idle_pipes.reserve(kIdleCount);
    for (uint32_t idx = 0; idx < kIdleCount; ++idx) {
      auto pipe = std::make_shared<MultiSlotPassthroughPipe>(/*max_candidates=*/4,
                                                             /*inputs_count=*/1,
                                                             /*outputs_count=*/1);
      pipeline.addPipe("idle_" + std::to_string(idx), pipe);
      idle_pipes.push_back(std::move(pipe));
    }
    pipeline.start();
    // Give workers a couple of ms to definitely reach their first cv wait_for().
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto stop_start = std::chrono::steady_clock::now();
    pipeline.stop();
    const auto stop_duration = std::chrono::steady_clock::now() - stop_start;
    REQUIRE(stop_duration < std::chrono::milliseconds(50));
  }

  SECTION("stop_interrupts_a_long_running_evolution_quickly") {
    // The key user-facing fix: a pipeline with an expensive evolution stage (here
    // simulated with sleeps) used to make stop() block until the in-flight evolve() cycle
    // finished -- minutes for SHA-256 multi-round. With the cooperative stop token wired
    // through Pipeline -> Pipe -> EvolutionPipe -> GA evaluator wrapper, stop() should
    // return promptly even when the worker is deep inside a long cycle.
    class SlowEvolvePipe : public beast::EvolutionPipe {
     public:
      SlowEvolvePipe() : beast::EvolutionPipe(/*max_candidates=*/8) {}
      [[nodiscard]] double
      evaluate(const std::vector<unsigned char>& /*data*/) override {
        ++calls;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return 0.0;
      }
      std::atomic<uint32_t> calls{0};
    };
    auto pipe = std::make_shared<SlowEvolvePipe>();
    beast::EvolutionPipe::EvolutionParameters params;
    // 100 generations * 7 = 700 heavy calls at 1 ms each -> would block ~700ms per cycle.
    // The test fails if stop() doesn't interrupt and waits for the full cycle.
    params.generations = 100;
    params.variable_count = 4;
    params.starting_program_size = 16;
    pipe->setEvolutionParameters(params);
    // The pipeline worker only enters execute() once inputsAreSaturated() is true; for an
    // EvolutionPipe that means max_candidates seed genomes in slot 0. Without this prime
    // step the worker would idle and stop() would be vacuously fast.
    const std::vector<unsigned char> seed(16, 0);
    for (uint32_t i = 0; i < 8; ++i) {
      pipe->addInput(0, seed);
    }

    beast::Pipeline pipeline;
    pipeline.addPipe("slow", pipe);
    pipeline.start();
    // Wait until the slow evaluator has clearly entered a cycle so stop() lands mid-evolve.
    // 30 ms is enough on every machine we've tested (the 1ms sleep per evaluator call gives
    // the worker plenty of time to settle into the GA loop before we ask it to stop).
    for (int i = 0; i < 50 && pipe->calls.load() < 5; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(pipe->calls.load() >= 1); // sanity-check the worker actually ran
    const auto stop_start = std::chrono::steady_clock::now();
    pipeline.stop();
    const auto stop_duration = std::chrono::steady_clock::now() - stop_start;
    // 400 ms is generous. The cycle's nominal duration is ~700 ms; without cancellation
    // we'd land near or above that. With cancellation we typically observe <50 ms but the
    // larger budget keeps the test stable on slow CI / containers.
    REQUIRE(stop_duration < std::chrono::milliseconds(400));
  }

  SECTION("restart_after_stop_resets_the_stop_token") {
    // Cooperative cancellation must NOT bleed across restarts. After stop()+start(), the
    // shared stop flag visible to the pipe must be back at `false` even though the prior
    // run had flipped it to `true`. We assert this directly rather than relying on a
    // second cycle of worker activity -- the latter would have to dance around output
    // saturation / input refill in an isolated single-pipe test, which adds noise
    // unrelated to the token contract under test.
    class ProbePipe : public beast::Pipe {
     public:
      ProbePipe() : beast::Pipe(/*max_candidates=*/1, /*input_slots=*/0, /*output_slots=*/0) {}
      void execute() override {}
      using beast::Pipe::isStopRequested;
    };
    auto pipe = std::make_shared<ProbePipe>();

    beast::Pipeline pipeline;
    pipeline.addPipe("p", pipe);

    pipeline.start();
    REQUIRE_FALSE(pipe->isStopRequested());
    pipeline.stop();
    REQUIRE(pipe->isStopRequested());

    pipeline.start();
    // start() must have minted a fresh false-token; the previously-true one must be gone
    // from the pipe's view.
    REQUIRE_FALSE(pipe->isStopRequested());
    pipeline.stop();
  }

  SECTION("destructor_stops_running_pipeline_without_terminate") {
    // Regression test for ~Pipeline(): a Pipeline that goes out of scope while still running
    // used to invoke ~std::thread on a joinable worker, which calls std::terminate(). The
    // destructor now politely stops everything first.
    auto sink = std::make_shared<MultiSlotPassthroughPipe>(/*max_candidates=*/2,
                                                           /*inputs_count=*/1,
                                                           /*outputs_count=*/1);
    {
      beast::Pipeline pipeline;
      pipeline.addPipe("sink", sink);
      pipeline.start();
      REQUIRE(pipeline.isRunning());
      // Intentionally leave scope without calling stop().
    }
    // If we get here without std::terminate firing, the destructor handled cleanup correctly.
    SUCCEED("Pipeline destructor cleanly tore down its workers.");
  }
}
