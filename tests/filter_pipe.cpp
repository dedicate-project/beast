// Catch2
#include <catch2/catch.hpp>

// Standard
#include <vector>

// BEAST
#include <beast/beast.hpp>

namespace {

beast::Pipe::OutputItem makeItem(unsigned char byte, double score) {
  return beast::Pipe::OutputItem{{byte}, score};
}

} // namespace

TEST_CASE("FilterPipe routes by score with the threshold being inclusive on the pass side") {
  beast::FilterPipe pipe(/*max_candidates=*/10, /*threshold=*/0.5);
  REQUIRE(pipe.getInputSlotCount() == 1);
  REQUIRE(pipe.getOutputSlotCount() == 2);
  REQUIRE(pipe.getThreshold() == Approx(0.5));

  pipe.addInputWithScore(0, makeItem(0x01, 0.10));  // below
  pipe.addInputWithScore(0, makeItem(0x02, 0.50));  // boundary -> pass
  pipe.addInputWithScore(0, makeItem(0x03, 0.49));  // below
  pipe.addInputWithScore(0, makeItem(0x04, 0.90));  // pass

  pipe.execute();

  REQUIRE(pipe.getOutputSlotAmount(0) == 2);
  REQUIRE(pipe.getOutputSlotAmount(1) == 2);
  CHECK(pipe.drawOutput(0).data == std::vector<unsigned char>{0x01});
  CHECK(pipe.drawOutput(0).data == std::vector<unsigned char>{0x03});
  CHECK(pipe.drawOutput(1).data == std::vector<unsigned char>{0x02});
  CHECK(pipe.drawOutput(1).data == std::vector<unsigned char>{0x04});
}

TEST_CASE("FilterPipe preserves the upstream score on both branches") {
  beast::FilterPipe pipe(/*max_candidates=*/4, /*threshold=*/0.5);
  pipe.addInputWithScore(0, makeItem(0xAA, 0.20));
  pipe.addInputWithScore(0, makeItem(0xBB, 0.80));

  pipe.execute();

  REQUIRE(pipe.getOutputSlotAmount(0) == 1);
  REQUIRE(pipe.getOutputSlotAmount(1) == 1);
  CHECK(pipe.drawOutput(0).score == Approx(0.20));
  CHECK(pipe.drawOutput(1).score == Approx(0.80));
}

TEST_CASE("FilterPipe is ready as soon as a single candidate arrives") {
  // Same regression guard as the other passthroughs: the worker loop calls
  // inputsAreSaturated() to decide whether to execute, and the base "all input slots
  // saturated" default would stall a filter fed by a slow upstream.
  beast::FilterPipe pipe(/*max_candidates=*/50, /*threshold=*/0.5);
  CHECK_FALSE(pipe.inputsAreSaturated());
  pipe.addInputWithScore(0, makeItem(0x01, 0.0));
  CHECK(pipe.inputsAreSaturated());
}

TEST_CASE("FilterPipe back-pressures when the target slot is full and does NOT reroute") {
  beast::FilterPipe pipe(/*max_candidates=*/2, /*threshold=*/0.5);

  // Pre-fill the pass slot. The next pass-side candidate should not be reclassified into
  // the reject slot just because the pass slot is full -- silent reclassification is the
  // exact bug a filter exists to prevent.
  pipe.addInputWithScore(0, makeItem(0xA1, 0.9));
  pipe.addInputWithScore(0, makeItem(0xA2, 0.9));
  pipe.addInputWithScore(0, makeItem(0xA3, 0.9));  // can't fit; pass slot is full
  pipe.addInputWithScore(0, makeItem(0xB1, 0.1));  // reject side has space

  pipe.execute();

  REQUIRE(pipe.getOutputSlotAmount(1) == 2);
  // Pipe stopped at the first item that couldn't be routed (0xA3). The reject-side item
  // 0xB1 is *not* eligible to be dequeued ahead of it -- FIFO would be violated.
  REQUIRE(pipe.getOutputSlotAmount(0) == 0);
  // Two items remain in the input queue, waiting for the pass slot to drain.
  REQUIRE(pipe.getInputSlotAmount(0) == 2);

  // Drain one pass slot and execute again: the pipe should now route 0xA3 to pass and
  // 0xB1 to reject.
  (void)pipe.drawOutput(1);
  pipe.execute();
  REQUIRE(pipe.getOutputSlotAmount(1) == 2);
  REQUIRE(pipe.getOutputSlotAmount(0) == 1);
  REQUIRE(pipe.getInputSlotAmount(0) == 0);
  CHECK(pipe.drawOutput(0).data == std::vector<unsigned char>{0xB1});
}

TEST_CASE("FilterPipe with all candidates above threshold leaves the reject slot empty") {
  beast::FilterPipe pipe(/*max_candidates=*/10, /*threshold=*/0.0);
  // threshold = 0 with non-negative scores means everything passes (score >= 0).
  for (int i = 0; i < 5; ++i) {
    pipe.addInputWithScore(0, makeItem(static_cast<unsigned char>(i), 0.0));
  }
  pipe.execute();
  CHECK(pipe.getOutputSlotAmount(0) == 0);
  CHECK(pipe.getOutputSlotAmount(1) == 5);
}

TEST_CASE("FilterPipe with a high threshold pushes everything to the reject slot") {
  beast::FilterPipe pipe(/*max_candidates=*/10, /*threshold=*/2.0);
  for (int i = 0; i < 3; ++i) {
    pipe.addInputWithScore(0, makeItem(static_cast<unsigned char>(i), 1.0));
  }
  pipe.execute();
  CHECK(pipe.getOutputSlotAmount(0) == 3);
  CHECK(pipe.getOutputSlotAmount(1) == 0);
}
