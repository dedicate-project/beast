#include <beast/pipes/program_factory_pipe.hpp>

namespace beast {

ProgramFactoryPipe::ProgramFactoryPipe(
    uint32_t max_candidates, uint32_t max_size, uint32_t memory_size, uint32_t string_table_size,
    uint32_t string_table_item_length, std::shared_ptr<ProgramFactoryBase> factory)
  : Pipe(max_candidates), max_size_{max_size}, memory_size_{memory_size}
  , string_table_size_{string_table_size}, string_table_item_length_{string_table_item_length}
  , factory_{std::move(factory)} {
}

void ProgramFactoryPipe::execute() {
  while (output_.size() < max_candidates_) {
    Pipe::OutputItem item;
    Program program =
        factory_->generate(max_size_, memory_size_, string_table_size_, string_table_item_length_);
    item.data = program.getData();
    item.score = 0.0;
    output_.push_back(std::move(item));
  }
}

} // namespace beast
