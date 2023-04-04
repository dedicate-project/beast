#ifndef BEAST_PROGRAM_FACTORY_PIPE_HPP_
#define BEAST_PROGRAM_FACTORY_PIPE_HPP_

// Standard
#include <deque>
#include <memory>
#include <stdint.h>
#include <vector>

// Internal
#include <beast/pipe.hpp>
#include <beast/program_factory_base.hpp>

namespace beast {

/**
 * @class ProgramFactoryPipe
 * @brief Executes program factories and provides program candidates
 *
 * TODO(fairlight1337): Document this class.
 *
 * @author Jan Winkler
 * @date 2023-04-03
 */
class ProgramFactoryPipe : public Pipe {
 public:
  explicit ProgramFactoryPipe(uint32_t max_candidates, uint32_t max_size, uint32_t memory_size,
                              uint32_t string_table_size, uint32_t string_table_item_length,
                              std::shared_ptr<ProgramFactoryBase> factory);

  void execute() override;

 private:
  const std::shared_ptr<ProgramFactoryBase> factory_;

  uint32_t max_size_;

  uint32_t memory_size_;

  uint32_t string_table_size_;

  uint32_t string_table_item_length_;
};

} // namespace beast

#endif // BEAST_PROGRAM_FACTORY_PIPE_HPP_
