// Catch2
#include <catch2/catch.hpp>

// Standard
#include <chrono>
#include <thread>
#include <vector>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("FanPipe forwards every candidate unchanged with scores preserved") {
  beast::FanPipe pipe(/*max_candidates=*/8, /*window_seconds=*/1.0);
  REQUIRE(pipe.getInputSlotCount() == 1);
  REQUIRE(pipe.getOutputSlotCount() == 1);

  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xAA, 0xBB}, 0.42});
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xCC}, 0.99});
  pipe.execute();

  REQUIRE(pipe.getOutputSlotAmount(0) == 2);
  auto first = pipe.drawOutput(0);
  auto second = pipe.drawOutput(0);
  CHECK(first.data == std::vector<unsigned char>{0xAA, 0xBB});
  CHECK(first.score == Approx(0.42));
  CHECK(second.data == std::vector<unsigned char>{0xCC});
  CHECK(second.score == Approx(0.99));

  const auto throughput = pipe.getThroughput();
  CHECK(throughput.total_seen == 2);
  CHECK(throughput.window_seen == 2);
  CHECK(throughput.candidates_per_second == Approx(2.0));
}

TEST_CASE("FanPipe respects back-pressure when output saturates") {
  beast::FanPipe pipe(/*max_candidates=*/2, /*window_seconds=*/1.0);
  for (int i = 0; i < 5; ++i) {
    pipe.addInputWithScore(0, beast::Pipe::OutputItem{{static_cast<unsigned char>(i)}, 0});
  }
  pipe.execute();
  REQUIRE(pipe.getOutputSlotAmount(0) == 2);
  REQUIRE(pipe.getInputSlotAmount(0) == 3);

  const auto throughput = pipe.getThroughput();
  CHECK(throughput.total_seen == 2);
  CHECK(throughput.window_seen == 2);
}

TEST_CASE("FanPipe sliding window forgets older samples") {
  // Use a deliberately small window so the test runs quickly. The exact rate isn't
  // important; what we care about is "samples older than the window are excluded".
  beast::FanPipe pipe(/*max_candidates=*/8, /*window_seconds=*/0.05);
  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xAA}, 0});
  pipe.execute();
  REQUIRE(pipe.getThroughput().window_seen == 1);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  const auto after_wait = pipe.getThroughput();
  CHECK(after_wait.total_seen == 1); // monotonic lifetime counter is untouched
  CHECK(after_wait.window_seen == 0); // the only sample aged out of the window
  CHECK(after_wait.candidates_per_second == Approx(0.0));
}

TEST_CASE("FanPipe falls back to the default window when given a non-positive value") {
  beast::FanPipe zero(/*max_candidates=*/2, /*window_seconds=*/0.0);
  CHECK(zero.getWindowSeconds() == Approx(2.0));
  beast::FanPipe negative(/*max_candidates=*/2, /*window_seconds=*/-3.5);
  CHECK(negative.getWindowSeconds() == Approx(2.0));
}

TEST_CASE("FanPipe is ready as soon as a single candidate arrives") {
  // Regression: the base-class `inputsAreSaturated()` returns true only when *every*
  // input slot is full, which would cause the worker loop to never call execute() on
  // a passthrough fed by a slow upstream (the practical symptom: the pipe shows
  // non-zero input rate but zero output rate, candidates effectively black-holed
  // until the input buffer fills). FanPipe must say "ready" with even one item.
  beast::FanPipe pipe(/*max_candidates=*/50, /*window_seconds=*/1.0);
  CHECK_FALSE(pipe.inputsAreSaturated());

  pipe.addInputWithScore(0, beast::Pipe::OutputItem{{0xAA}, 0.0});
  CHECK(pipe.inputsAreSaturated());
}
