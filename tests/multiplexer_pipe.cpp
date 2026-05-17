// Catch2
#include <catch2/catch.hpp>

// Standard
#include <vector>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("MultiplexerPipe drains every input round-robin") {
  beast::MultiplexerPipe pipe(/*max_candidates=*/10, /*input_slots=*/3);
  REQUIRE(pipe.getInputSlotCount() == 3);
  REQUIRE(pipe.getOutputSlotCount() == 1);

  // Slot 0: A1, A2. Slot 1: B1. Slot 2: C1, C2, C3.
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xA1}, 0.1});
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xA2}, 0.2});
  pipe.addInputWithScore(1, beast::Pipe::OutputItem{{0xB1}, 0.3});
  pipe.addInputWithScore(2, beast::Pipe::OutputItem{{0xC1}, 0.4});
  pipe.addInputWithScore(2, beast::Pipe::OutputItem{{0xC2}, 0.5});
  pipe.addInputWithScore(2, beast::Pipe::OutputItem{{0xC3}, 0.6});

  pipe.execute();

  // First pass (cursor 0): A1, B1, C1. Cursor advances to 1.
  // Second pass: slot 1 is empty so we skip to 2 -> C2, then 0 -> A2. Cursor advances to 2.
  // Third pass: 2 -> C3, 0 empty, 1 empty. Done.
  // The "skip empty slot in the same pass before advancing the cursor" semantics give us
  // every available item per pass without burning idle ticks on empty slots, which is the
  // saner behaviour for an uneven upstream.
  REQUIRE(pipe.getOutputSlotAmount(0) == 6);
  std::vector<unsigned char> expected{0xA1, 0xB1, 0xC1, 0xC2, 0xA2, 0xC3};
  for (auto& byte : expected) {
    const auto item = pipe.drawOutput(0);
    INFO("expected byte 0x" << std::hex << static_cast<int>(byte));
    CHECK(item.data == std::vector<unsigned char>{byte});
  }
}

TEST_CASE("MultiplexerPipe preserves scores end-to-end") {
  beast::MultiplexerPipe pipe(/*max_candidates=*/5, /*input_slots=*/2);
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0x01}, 0.75});
  pipe.addInputWithScore(1, beast::Pipe::OutputItem{{0x02}, 0.25});

  pipe.execute();
  const auto first = pipe.drawOutput(0);
  const auto second = pipe.drawOutput(0);
  CHECK(first.score == Approx(0.75));
  CHECK(second.score == Approx(0.25));
}

TEST_CASE("MultiplexerPipe applies back-pressure when the single output saturates") {
  beast::MultiplexerPipe pipe(/*max_candidates=*/2, /*input_slots=*/2);
  for (int i = 0; i < 4; ++i) {
    pipe.addInputWithScore(i % 2, beast::Pipe::OutputItem{{static_cast<unsigned char>(i)}, 0});
  }
  pipe.execute();
  REQUIRE(pipe.getOutputSlotAmount(0) == 2);
  // The two undelivered items should still be sitting in the input slots.
  REQUIRE(pipe.getInputSlotAmount(0) + pipe.getInputSlotAmount(1) == 2);
}

TEST_CASE("MultiplexerPipe is ready when any single input has a candidate") {
  // The mux ought to fire as long as *some* upstream has produced something; waiting
  // until *every* slot is full would deadlock common loop-back wirings where one of
  // the inputs only produces sporadically (e.g. the survivor feedback loop).
  beast::MultiplexerPipe pipe(/*max_candidates=*/50, /*input_slots=*/3);
  CHECK_FALSE(pipe.inputsAreSaturated());

  pipe.addInputWithScore(1, beast::Pipe::OutputItem{{0xAB}, 0.0});
  CHECK(pipe.inputsAreSaturated());
}

TEST_CASE("MultiplexerPipe clamps weird slot counts to a sane range") {
  beast::MultiplexerPipe zero(/*max_candidates=*/2, /*input_slots=*/0);
  CHECK(zero.getInputSlotCount() == 1);

  beast::MultiplexerPipe absurd(/*max_candidates=*/2, /*input_slots=*/9999);
  CHECK(absurd.getInputSlotCount() == 16);
}
