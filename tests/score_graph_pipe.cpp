// Catch2
#include <catch2/catch.hpp>

// Standard
#include <chrono>
#include <thread>
#include <vector>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("ScoreGraphPipe forwards candidates unchanged") {
  // Smallest interesting setup: 1 input, 1 output, scores preserved end-to-end. Same
  // contract ResultsSummaryPipe has -- both are passthroughs that fan a side-channel
  // out to the metrics endpoint without changing what downstream consumers see.
  beast::ScoreGraphPipe pipe(/*max_candidates=*/8);
  REQUIRE(pipe.getInputSlotCount() == 1);
  REQUIRE(pipe.getOutputSlotCount() == 1);

  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0x1, 0x2}, 0.3});
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0x9}, 0.8});
  pipe.execute();

  REQUIRE(pipe.getInputSlotAmount(0) == 0);
  REQUIRE(pipe.getOutputSlotAmount(0) == 2);
  const auto first = pipe.drawOutput(0);
  CHECK(first.data == std::vector<unsigned char>{0x1, 0x2});
  CHECK(first.score == Approx(0.3));
  const auto second = pipe.drawOutput(0);
  CHECK(second.data == std::vector<unsigned char>{0x9});
  CHECK(second.score == Approx(0.8));
}

TEST_CASE("ScoreGraphPipe records every score and exposes summary stats") {
  beast::ScoreGraphPipe pipe(/*max_candidates=*/8);
  auto feed = [&pipe](double score) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xFF}, score});
    pipe.execute();
    while (pipe.getOutputSlotAmount(0) > 0) {
      static_cast<void>(pipe.drawOutput(0));
    }
  };
  feed(0.2);
  feed(0.5);
  feed(0.9);
  feed(0.4);

  const auto snap = pipe.getSnapshot();
  REQUIRE(snap.samples.size() == 4);
  REQUIRE(snap.total_seen == 4);
  CHECK(snap.last_score == Approx(0.4));
  CHECK(snap.min_score == Approx(0.2));
  CHECK(snap.max_score == Approx(0.9));
  CHECK(snap.mean_score == Approx((0.2 + 0.5 + 0.9 + 0.4) / 4.0));
  // The samples come out oldest-first so the UI sparkline can stream them straight into
  // an SVG polyline without re-sorting.
  CHECK(snap.samples.front().score == Approx(0.2));
  CHECK(snap.samples.back().score == Approx(0.4));
  // Timestamps must be non-decreasing -- otherwise the polyline would back-track and
  // the user would see a meaningless squiggle.
  for (size_t i = 1; i < snap.samples.size(); ++i) {
    CHECK(snap.samples[i].t_seconds >= snap.samples[i - 1].t_seconds);
  }
}

TEST_CASE("ScoreGraphPipe prunes samples older than window_seconds") {
  // 80 ms window: long enough that the test isn't fragile on a busy CI box, short
  // enough that a 200 ms sleep deterministically pushes the first sample out.
  beast::ScoreGraphPipe pipe(/*max_candidates=*/8, /*window_seconds=*/0.080);
  REQUIRE(pipe.getWindowSeconds() == Approx(0.080));

  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xAB}, 0.1});
  pipe.execute();
  static_cast<void>(pipe.drawOutput(0));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xCD}, 0.9});
  pipe.execute();
  static_cast<void>(pipe.drawOutput(0));

  const auto snap = pipe.getSnapshot();
  // The first sample is well outside the 80 ms window now, so the snapshot should only
  // contain the freshly recorded one. total_seen continues to count both -- it's the
  // lifetime sample count, not the in-window count.
  REQUIRE(snap.samples.size() == 1);
  CHECK(snap.samples.front().score == Approx(0.9));
  CHECK(snap.total_seen == 2);
}

TEST_CASE("ScoreGraphPipe caps the in-memory sample count at max_samples") {
  // Big window so time-pruning doesn't fire; tiny max_samples so size-pruning does.
  beast::ScoreGraphPipe pipe(/*max_candidates=*/16, /*window_seconds=*/3600.0,
                             /*max_samples=*/3);
  REQUIRE(pipe.getMaxSamples() == 3);
  for (int i = 0; i < 10; ++i) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{static_cast<unsigned char>(i)},
                                                       static_cast<double>(i) / 10.0});
    pipe.execute();
    while (pipe.getOutputSlotAmount(0) > 0) {
      static_cast<void>(pipe.drawOutput(0));
    }
  }
  const auto snap = pipe.getSnapshot();
  // Only the last 3 samples survive; total_seen still tracks all 10.
  REQUIRE(snap.samples.size() == 3);
  CHECK(snap.total_seen == 10);
  CHECK(snap.samples.front().score == Approx(0.7));
  CHECK(snap.samples.back().score == Approx(0.9));
}

TEST_CASE("ScoreGraphPipe inputsAreSaturated fires on any input (observer semantics)") {
  // Critical for the worker loop: passthrough observer pipes must run as soon as
  // anything arrives, not wait for the buffer to fill. The base-class default is the
  // opposite (wait for saturation), which would silently never run for low-traffic
  // pipes.
  beast::ScoreGraphPipe pipe(/*max_candidates=*/8);
  CHECK_FALSE(pipe.inputsAreSaturated());
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0x1}, 0.5});
  CHECK(pipe.inputsAreSaturated());
}
