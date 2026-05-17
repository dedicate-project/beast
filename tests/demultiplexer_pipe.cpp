// Catch2
#include <catch2/catch.hpp>

// Standard
#include <vector>

// BEAST
#include <beast/beast.hpp>

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
