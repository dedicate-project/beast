#include <beast/pipe.hpp>

// GALGO-2.0
#include <Galgo.hpp>

namespace beast {

Pipe::Pipe(uint32_t population_size, uint32_t individual_size)
  : population_size_{population_size}, individual_size_{individual_size} {
}

void Pipe::addIndividualToInitialPopulation(VmSession& session) {
  initial_population_.push_back(session);
}

void Pipe::evolve() {
}

bool Pipe::hasSpace() {
  return initial_population_.size() < population_size_;
}

}  // namespace beast
