// Catch2
#include <catch2/catch.hpp>

// Standard
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>

// BEAST
#include <beast/beast.hpp>

namespace {

// Unique temp file per test invocation so parallel ctest runs don't tread on each other.
std::string makeTempLedgerPath(const std::string& tag) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist;
  auto base = std::filesystem::temp_directory_path();
  std::string name = "beast-storage-test-" + tag + "-" + std::to_string(dist(gen)) + ".json";
  return (base / name).string();
}

} // namespace

TEST_CASE("ProgramStorageSinkPipe persists the top-K by score") {
  const std::string path = makeTempLedgerPath("topk");
  std::filesystem::remove(path);
  {
    beast::ProgramStorageSinkPipe sink(/*max_candidates=*/16, path, /*top_k=*/3);
    REQUIRE(sink.getInputSlotCount() == 1);
    REQUIRE(sink.getOutputSlotCount() == 0);
    REQUIRE(sink.getTopK() == 3);
    REQUIRE(sink.getEntries().empty());

    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xA}, 0.10});
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xB}, 0.50});
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xC}, 0.30});
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xD}, 0.90});
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xE}, 0.05});
    sink.execute();
    const auto kept = sink.getEntries();
    REQUIRE(kept.size() == 3);
    CHECK(kept[0].score == Approx(0.90));
    CHECK(kept[1].score == Approx(0.50));
    CHECK(kept[2].score == Approx(0.30));
    CHECK(kept[0].data == std::vector<unsigned char>{0xD});
  }

  // Re-instantiating against the same path reloads the persisted top-K so a restart
  // doesn't lose progress.
  beast::ProgramStorageSinkPipe reloaded(/*max_candidates=*/16, path, /*top_k=*/3);
  const auto rehydrated = reloaded.getEntries();
  REQUIRE(rehydrated.size() == 3);
  CHECK(rehydrated[0].score == Approx(0.90));
  CHECK(rehydrated[0].data == std::vector<unsigned char>{0xD});
  std::filesystem::remove(path);
}

TEST_CASE("ProgramStorageSinkPipe skips exact duplicates and ignores worse candidates") {
  const std::string path = makeTempLedgerPath("dupes");
  std::filesystem::remove(path);
  beast::ProgramStorageSinkPipe sink(/*max_candidates=*/8, path, /*top_k=*/2);
  sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xA}, 0.5});
  sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xA}, 0.5}); // exact duplicate
  sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xB}, 0.4});
  sink.execute();
  REQUIRE(sink.getEntries().size() == 2);

  // A worse candidate that the ledger is already full shouldn't even cause a re-sort.
  sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xC}, 0.1});
  sink.execute();
  const auto entries = sink.getEntries();
  REQUIRE(entries.size() == 2);
  CHECK(entries[0].data == std::vector<unsigned char>{0xA});
  CHECK(entries[1].data == std::vector<unsigned char>{0xB});
  std::filesystem::remove(path);
}

TEST_CASE("ProgramStorageSinkPipe is ready as soon as a single candidate arrives") {
  // A sink should record each candidate as soon as it shows up; the base-class "all
  // slots full" readiness gate would otherwise hold the sink off until 50 candidates
  // had accumulated, which for a slow upstream means new high-scoring programs never
  // make it to disk in any reasonable wall-clock time.
  beast::ProgramStorageSinkPipe sink(/*max_candidates=*/50, /*path=*/"", /*top_k=*/3);
  CHECK_FALSE(sink.inputsAreSaturated());
  sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xAB}, 0.5});
  CHECK(sink.inputsAreSaturated());
}

TEST_CASE("ProgramStorageSinkPipe degrades gracefully with no path configured") {
  beast::ProgramStorageSinkPipe sink(/*max_candidates=*/4, /*path=*/"", /*top_k=*/0);
  sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xA}, 0.5});
  sink.execute();
  // top_k=0 should fall back to the implementation default (10) -- not the literal 0,
  // otherwise the pipe would discard everything.
  REQUIRE(sink.getTopK() == 10);
  REQUIRE(sink.getEntries().size() == 1);
}

TEST_CASE("ProgramStorageSourcePipe emits cached entries until exhausted") {
  const std::string path = makeTempLedgerPath("emit");
  std::filesystem::remove(path);
  // Pre-seed the ledger via the sink so we exercise the on-disk format symmetrically.
  {
    beast::ProgramStorageSinkPipe sink(/*max_candidates=*/8, path, /*top_k=*/3);
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xA, 0xB}, 0.10});
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xC}, 0.50});
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xD, 0xE, 0xF}, 0.90});
    sink.execute();
  }

  beast::ProgramStorageSourcePipe source(/*max_candidates=*/2, path, /*loop=*/false);
  REQUIRE(source.getInputSlotCount() == 0);
  REQUIRE(source.getOutputSlotCount() == 1);
  REQUIRE(source.getEntryCount() == 3);

  source.execute();
  REQUIRE(source.getOutputSlotAmount(0) == 2);
  auto first = source.drawOutput(0);
  auto second = source.drawOutput(0);
  CHECK(first.score == Approx(0.90));
  CHECK(first.data == std::vector<unsigned char>{0xD, 0xE, 0xF});
  CHECK(second.score == Approx(0.50));

  source.execute();
  REQUIRE(source.getOutputSlotAmount(0) == 1);
  CHECK(source.drawOutput(0).score == Approx(0.10));

  // No loop: a further execute() leaves the output empty even with capacity.
  source.execute();
  REQUIRE(source.getOutputSlotAmount(0) == 0);
  std::filesystem::remove(path);
}

TEST_CASE("ProgramStorageSourcePipe loops when configured to") {
  const std::string path = makeTempLedgerPath("loop");
  std::filesystem::remove(path);
  {
    beast::ProgramStorageSinkPipe sink(/*max_candidates=*/4, path, /*top_k=*/2);
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xA}, 0.6});
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xB}, 0.4});
    sink.execute();
  }
  beast::ProgramStorageSourcePipe source(/*max_candidates=*/4, path, /*loop=*/true);
  source.execute();
  REQUIRE(source.getOutputSlotAmount(0) == 4);
  // First two are the top-2 entries; the next two are the same in the same order
  // because the source loops back from the start.
  std::vector<double> got;
  got.reserve(4);
  for (int i = 0; i < 4; ++i) {
    got.push_back(source.drawOutput(0).score);
  }
  CHECK(got == std::vector<double>{0.6, 0.4, 0.6, 0.4});
  std::filesystem::remove(path);
}

TEST_CASE("ProgramStorageSourcePipe is empty when path missing or unreadable") {
  beast::ProgramStorageSourcePipe missing(/*max_candidates=*/4, "/nonexistent/path.json",
                                          /*loop=*/false);
  REQUIRE(missing.getEntryCount() == 0);
  missing.execute();
  REQUIRE(missing.getOutputSlotAmount(0) == 0);
}

TEST_CASE("ProgramStorageSourcePipe picks up a ledger that appears after construction") {
  // Regression: the staged-pipeline pattern (Stage A's sink writes a ledger that Stage
  // B's source reads) used to deadlock because the source loaded its `entries_` vector
  // exactly once in the constructor. If Stage B's source was instantiated before Stage
  // A had a chance to write anything, the source spun forever with an empty cache even
  // after the file appeared on disk. The fix re-reads the file when its mtime advances
  // (or, in the absence-then-presence case here, when stat() starts succeeding).
  const std::string path = makeTempLedgerPath("appears");
  std::filesystem::remove(path);

  beast::ProgramStorageSourcePipe source(/*max_candidates=*/4, path, /*loop=*/true);
  REQUIRE(source.getEntryCount() == 0);
  source.execute();
  REQUIRE(source.getOutputSlotAmount(0) == 0); // file isn't there yet

  // Sink the ledger *after* the source already exists. This is exactly the production
  // race: a downstream stage writes to the path the upstream source is watching.
  {
    beast::ProgramStorageSinkPipe sink(/*max_candidates=*/4, path, /*top_k=*/2);
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xA1}, 0.6});
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xA2}, 0.4});
    sink.execute();
  }

  // Wait a hair longer than the source's refresh interval (500 ms) so the next
  // execute() will actually stat() the file again rather than reuse its cached "no
  // entries" result. The interval is intentionally throttled in production to keep
  // syscall overhead low when the source is in a hot empty spin; the test pays the
  // throttle once.
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  source.execute();
  REQUIRE(source.getEntryCount() == 2);
  REQUIRE(source.getOutputSlotAmount(0) == 4); // 2 entries × 2 loop passes fits the buffer
  std::vector<double> got;
  got.reserve(4);
  for (int i = 0; i < 4; ++i) {
    got.push_back(source.drawOutput(0).score);
  }
  CHECK(got == std::vector<double>{0.6, 0.4, 0.6, 0.4});
  std::filesystem::remove(path);
}

TEST_CASE("ProgramStorageSourcePipe re-reads when the ledger is overwritten") {
  // Same machinery as the "appears after construction" case, exercised the other way:
  // the source has a snapshot already, the sink overwrites the file with a different
  // top-K, the source should switch to emitting the new contents from the top on its
  // next refresh cycle.
  const std::string path = makeTempLedgerPath("refresh");
  std::filesystem::remove(path);
  {
    beast::ProgramStorageSinkPipe sink(/*max_candidates=*/4, path, /*top_k=*/1);
    sink.addInputWithScore(0, beast::Pipe::OutputItem{{0xB0}, 0.5});
    sink.execute();
  }

  beast::ProgramStorageSourcePipe source(/*max_candidates=*/2, path, /*loop=*/true);
  REQUIRE(source.getEntryCount() == 1);
  source.execute();
  CHECK(source.drawOutput(0).data == std::vector<unsigned char>{0xB0});
  CHECK(source.drawOutput(0).data == std::vector<unsigned char>{0xB0});

  // Overwrite the ledger with completely different content. The mtime advances, so the
  // source's next execute() should pick it up and emit the new top genome instead of
  // the stale one.
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  {
    beast::ProgramStorageSinkPipe sink2(/*max_candidates=*/4, path, /*top_k=*/1);
    // The sink's foldCandidate deduplicates against existing entries, so the new top
    // has to actually score higher to displace the old one.
    sink2.addInputWithScore(0, beast::Pipe::OutputItem{{0xC0}, 0.9});
    sink2.execute();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(600));

  source.execute();
  REQUIRE(source.getEntryCount() == 1);
  CHECK(source.drawOutput(0).data == std::vector<unsigned char>{0xC0});
  std::filesystem::remove(path);
}
