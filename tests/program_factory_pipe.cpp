#include <catch2/catch.hpp>

#include <beast/beast.hpp>

class MockFactory : public beast::ProgramFactoryBase {
 public:
  [[nodiscard]] beast::Program generate(uint32_t size, uint32_t /*memory_size*/, uint32_t /*string_table_size*/,
                                        uint32_t /*string_table_item_length*/) override {
    return beast::Program(size);
  }
};

TEST_CASE("when_calling_execute_fills_output_with_candidates", "program_factory_pipe") {
  const uint32_t candidate_count = 10;
  const uint32_t candidate_size = 1;
  const uint32_t candidate_mem = 1;
  const uint32_t candidate_sts = 1;
  const uint32_t candidate_stil = 1;

  std::shared_ptr<beast::ProgramFactoryBase> factory = std::make_shared<MockFactory>();
  beast::ProgramFactoryPipe pipe(
      candidate_count, candidate_size, candidate_mem, candidate_sts, candidate_stil, factory);

  pipe.execute();

  REQUIRE(pipe.getOutputSlotAmount(0) == candidate_count);
}
