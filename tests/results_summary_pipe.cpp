// Catch2
#include <catch2/catch.hpp>

// Standard
#include <vector>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("ResultsSummaryPipe forwards candidates unchanged") {
  const uint32_t max_candidates = 8;
  beast::ResultsSummaryPipe pipe(max_candidates);

  REQUIRE(pipe.getInputSlotCount() == 1);
  REQUIRE(pipe.getOutputSlotCount() == 1);
  REQUIRE(pipe.getMaxCandidates() == max_candidates);

  // Feed three candidates *with* scores so we exercise the score-preserving path the
  // Pipeline plumbing uses; the legacy `addInput(bytes)` path is exercised implicitly
  // by the other passthrough tests.
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0x1, 0x2, 0x3}, 0.25});
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0x4, 0x5}, 0.75});
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0x6}, 0.5});

  pipe.execute();

  REQUIRE(pipe.getInputSlotAmount(0) == 0);
  REQUIRE(pipe.getOutputSlotAmount(0) == 3);

  // FIFO order is preserved end-to-end so the downstream pipe sees the same sequence the
  // upstream produced.
  const auto first = pipe.drawOutput(0);
  CHECK(first.data == std::vector<unsigned char>{0x1, 0x2, 0x3});
  CHECK(first.score == Approx(0.25));
  const auto second = pipe.drawOutput(0);
  CHECK(second.data == std::vector<unsigned char>{0x4, 0x5});
  CHECK(second.score == Approx(0.75));
  const auto third = pipe.drawOutput(0);
  CHECK(third.data == std::vector<unsigned char>{0x6});
  CHECK(third.score == Approx(0.5));
}

TEST_CASE("ResultsSummaryPipe accumulates rolling statistics") {
  // Tiny window so the rolling eviction is observable in a single test.
  beast::ResultsSummaryPipe pipe(/*max_candidates=*/4, /*window_size=*/3);
  REQUIRE(pipe.getWindowSize() == 3);

  auto feed = [&pipe](double score) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xFF}, score});
    pipe.execute();
    // Drain the output so the buffer doesn't saturate over many iterations -- this isn't
    // what we're testing here.
    while (pipe.getOutputSlotAmount(0) > 0) {
      static_cast<void>(pipe.drawOutput(0));
    }
  };

  feed(0.1);
  feed(0.4);
  feed(0.7);

  auto summary = pipe.getSummary();
  CHECK(summary.count_total == 3);
  CHECK(summary.count_window == 3);
  CHECK(summary.min_score == Approx(0.1));
  CHECK(summary.max_score == Approx(0.7));
  CHECK(summary.mean_score == Approx((0.1 + 0.4 + 0.7) / 3.0));
  CHECK(summary.last_score == Approx(0.7));
  CHECK(summary.best_ever_score == Approx(0.7));

  // Push the window past its capacity. The oldest 0.1 should fall out so the min jumps to
  // the next-lowest in the window (0.4), but the best-ever-seen high water mark should
  // *not* drop just because a lower score arrived.
  feed(0.5);
  summary = pipe.getSummary();
  CHECK(summary.count_total == 4);
  CHECK(summary.count_window == 3);
  CHECK(summary.min_score == Approx(0.4));
  CHECK(summary.max_score == Approx(0.7));
  CHECK(summary.last_score == Approx(0.5));
  CHECK(summary.best_ever_score == Approx(0.7));

  pipe.resetSummary();
  summary = pipe.getSummary();
  CHECK(summary.count_total == 0);
  CHECK(summary.count_window == 0);
  CHECK(summary.best_ever_score == Approx(0.0));
  CHECK(summary.best_ever_data.empty());
}

TEST_CASE("ResultsSummaryPipe applies back-pressure when output saturates") {
  beast::ResultsSummaryPipe pipe(/*max_candidates=*/2);
  // Feed more items than the output slot can hold.
  for (int idx = 0; idx < 5; ++idx) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{static_cast<unsigned char>(idx)},
                                                       0.1 * idx});
  }
  REQUIRE(pipe.getInputSlotAmount(0) == 5);

  pipe.execute();

  // Exactly max_candidates worth made it through before back-pressure kicked in.
  REQUIRE(pipe.getOutputSlotAmount(0) == 2);
  REQUIRE(pipe.getInputSlotAmount(0) == 3);
}
