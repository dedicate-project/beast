#include <beast/pipes/program_factory_pipe.hpp>

#include <iostream>

namespace beast {

ProgramFactoryPipe::ProgramFactoryPipe(uint32_t max_candidates, uint32_t max_size,
                                       uint32_t memory_size, uint32_t string_table_size,
                                       uint32_t string_table_item_length,
                                       std::shared_ptr<ProgramFactoryBase> factory)
    : Pipe(max_candidates, 0, 1), max_size_{max_size}, memory_size_{memory_size},
      string_table_size_{string_table_size},
      string_table_item_length_{string_table_item_length}, factory_{std::move(factory)} {}

void ProgramFactoryPipe::execute() {
  while (outputs_[0].size() < max_candidates_) {
    Pipe::OutputItem item;
    Program program =
        factory_->generate(max_size_, memory_size_, string_table_size_, string_table_item_length_);
    item.data = program.getData();
    item.score = 0.0;
    outputs_[0].push_back(std::move(item));
  }
}

uint32_t ProgramFactoryPipe::getMaxSize() const { return max_size_; }

uint32_t ProgramFactoryPipe::getMemorySize() const { return memory_size_; }

uint32_t ProgramFactoryPipe::getStringTableSize() const { return string_table_size_; }

uint32_t ProgramFactoryPipe::getStringTableItemLength() const { return string_table_item_length_; }

std::shared_ptr<ProgramFactoryBase> ProgramFactoryPipe::getFactory() const { return factory_; }

} // namespace beast
