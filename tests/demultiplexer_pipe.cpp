// Catch2
#include <catch2/catch.hpp>

// Standard
#include <vector>

// BEAST
#include <beast/beast.hpp>

namespace {
/// Thin subclass that exposes `storeOutput` so tests can set up asymmetric output-slot
/// states (e.g. "slot 0 is full because downstream hasn't drained yet") without having
/// to wire up a full pipeline graph. Demux routing decisions only depend on the slot
/// loads, so this is sufficient for unit-level coverage.
class TestableDemux : public beast::DemultiplexerPipe {
 public:
  using beast::DemultiplexerPipe::DemultiplexerPipe;
  using beast::DemultiplexerPipe::storeOutput;
};
} // namespace

TEST_CASE("DemultiplexerPipe round-robin spreads input across output slots") {
  beast::DemultiplexerPipe pipe(/*max_candidates=*/10, /*output_slots=*/3,
                                beast::DemultiplexerPipe::Strategy::RoundRobin);
  REQUIRE(pipe.getInputSlotCount() == 1);
  REQUIRE(pipe.getOutputSlotCount() == 3);

  for (unsigned char b : {0x10, 0x20, 0x30, 0x40, 0x50}) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{b}, 0.01 * b});
  }
  pipe.execute();
  REQUIRE(pipe.getOutputSlotAmount(0) == 2);
  REQUIRE(pipe.getOutputSlotAmount(1) == 2);
  REQUIRE(pipe.getOutputSlotAmount(2) == 1);
  CHECK(pipe.drawOutput(0).data == std::vector<unsigned char>{0x10});
  CHECK(pipe.drawOutput(0).data == std::vector<unsigned char>{0x40});
  CHECK(pipe.drawOutput(1).data == std::vector<unsigned char>{0x20});
  CHECK(pipe.drawOutput(1).data == std::vector<unsigned char>{0x50});
  CHECK(pipe.drawOutput(2).data == std::vector<unsigned char>{0x30});
}

TEST_CASE("DemultiplexerPipe broadcast copies each item to every output slot") {
  beast::DemultiplexerPipe pipe(/*max_candidates=*/10, /*output_slots=*/3,
                                beast::DemultiplexerPipe::Strategy::Broadcast);

  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xAA}, 0.5});
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xBB}, 0.6});
  pipe.execute();

  for (uint32_t slot = 0; slot < 3; ++slot) {
    REQUIRE(pipe.getOutputSlotAmount(slot) == 2);
    auto first = pipe.drawOutput(slot);
    auto second = pipe.drawOutput(slot);
    CHECK(first.data == std::vector<unsigned char>{0xAA});
    CHECK(first.score == Approx(0.5));
    CHECK(second.data == std::vector<unsigned char>{0xBB});
    CHECK(second.score == Approx(0.6));
  }
}

TEST_CASE("DemultiplexerPipe applies back-pressure rather than dropping candidates") {
  beast::DemultiplexerPipe pipe(/*max_candidates=*/2, /*output_slots=*/2);
  for (int i = 0; i < 5; ++i) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{static_cast<unsigned char>(i)}, 0});
  }
  pipe.execute();
  // Two went to slot 0, two to slot 1; the 5th was left in the input queue (and the
  // 6th, 7th, ... would also queue up there until downstream drains).
  REQUIRE(pipe.getOutputSlotAmount(0) == 2);
  REQUIRE(pipe.getOutputSlotAmount(1) == 2);
  REQUIRE(pipe.getInputSlotAmount(0) == 1);
}

TEST_CASE("DemultiplexerPipe is ready as soon as a single candidate arrives") {
  // Same regression as the other passthroughs: the worker loop calls
  // inputsAreSaturated() to decide whether to execute the pipe, and the base-class
  // "all slots full" gate would silently stall a demux fed by a slow upstream.
  beast::DemultiplexerPipe pipe(/*max_candidates=*/50, /*output_slots=*/2);
  CHECK_FALSE(pipe.inputsAreSaturated());
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xAB}, 0.0});
  CHECK(pipe.inputsAreSaturated());
}

TEST_CASE("DemultiplexerPipe broadcast blocks when *any* slot is full") {
  beast::DemultiplexerPipe pipe(/*max_candidates=*/3, /*output_slots=*/3,
                                beast::DemultiplexerPipe::Strategy::Broadcast);
  for (int i = 0; i < 3; ++i) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{static_cast<unsigned char>(i)}, 0});
  }
  pipe.execute();
  REQUIRE(pipe.getInputSlotAmount(0) == 0);
  // Pre-fill slot 1 so the next broadcast can't proceed even though slot 0 and 2 have space.
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xFF}, 0.99});
  pipe.execute();
  REQUIRE(pipe.getInputSlotAmount(0) == 1); // the new item stayed put
}

TEST_CASE("DemultiplexerPipe least-loaded skips a full slot instead of stalling") {
  // The regression this guards: the original round-robin demux stuck its cursor on the
  // slow/full slot and spun the worker without making progress, starving the fast slot
  // (the SHA-256 pipeline stall the user reported). least-loaded must route around it.
  TestableDemux pipe(/*max_candidates=*/4, /*output_slots=*/2,
                     beast::DemultiplexerPipe::Strategy::LeastLoaded);
  // Stuff slot 0 to capacity directly (simulating a slow downstream that hasn't drained
  // its input yet) and queue more candidates on the input.
  for (int i = 0; i < 4; ++i) {
    pipe.storeOutput(0, beast::Pipe::OutputItem{{static_cast<unsigned char>(i)}, 0});
  }
  for (int i = 0; i < 3; ++i) {
    pipe.addInputWithScore(0,
                           beast::Pipe::OutputItem{{static_cast<unsigned char>(0xB0 | i)}, 0});
  }
  pipe.execute();
  // All three new items must land on the empty slot 1 (slot 0 is full and skipped).
  REQUIRE(pipe.getInputSlotAmount(0) == 0);
  REQUIRE(pipe.getOutputSlotAmount(0) == 4);
  REQUIRE(pipe.getOutputSlotAmount(1) == 3);
}

TEST_CASE("DemultiplexerPipe least-loaded fans out evenly when slots are symmetric") {
  // When the demux degenerates to "all slots equally empty" it should still spread
  // candidates round-robin so a symmetric workload isn't biased toward slot 0.
  beast::DemultiplexerPipe pipe(/*max_candidates=*/10, /*output_slots=*/3,
                                beast::DemultiplexerPipe::Strategy::LeastLoaded);
  for (int i = 0; i < 6; ++i) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{static_cast<unsigned char>(i)}, 0});
  }
  pipe.execute();
  CHECK(pipe.getOutputSlotAmount(0) == 2);
  CHECK(pipe.getOutputSlotAmount(1) == 2);
  CHECK(pipe.getOutputSlotAmount(2) == 2);
}

TEST_CASE("DemultiplexerPipe least-loaded back-pressures when every slot is full") {
  // If we genuinely can't route anywhere, the input must stay queued (no silent drops).
  TestableDemux pipe(/*max_candidates=*/2, /*output_slots=*/2,
                     beast::DemultiplexerPipe::Strategy::LeastLoaded);
  pipe.storeOutput(0, beast::Pipe::OutputItem{{0x01}, 0});
  pipe.storeOutput(0, beast::Pipe::OutputItem{{0x02}, 0});
  pipe.storeOutput(1, beast::Pipe::OutputItem{{0x03}, 0});
  pipe.storeOutput(1, beast::Pipe::OutputItem{{0x04}, 0});
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xFF}, 0.0});
  pipe.execute();
  CHECK(pipe.getInputSlotAmount(0) == 1);
  CHECK(pipe.getOutputSlotAmount(0) == 2);
  CHECK(pipe.getOutputSlotAmount(1) == 2);
}
