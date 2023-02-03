#include <beast/pipe.hpp>

// Standard
#include <stdexcept>

// GAlib
// NOTE: For these includes, the `register` error needs to be ignored as this 3rdparty library uses
// outdated code. This is not an issue for the library using it though.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <ga/GAListGenome.h>
#include <ga/GASimpleGA.h>
#pragma GCC diagnostic pop

namespace beast {

namespace {
/**
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Document this function.
 */
// NOLINTNEXTLINE
float staticEvaluatorWrapper(GAGenome& genome) {
  auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(genome);
  std::vector<unsigned char> data;
  for (uint32_t idx = 0; idx < list_genome.size(); ++idx) {
    data.push_back(*list_genome[idx]);
  }

  const Pipe* pipe = static_cast<Pipe*>(genome.userData());
  return static_cast<float>(pipe->evaluate(data));
}

/**
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Document this function.
 */
// NOLINTNEXTLINE
void staticInitializerWrapper(GAGenome& genome) {
  auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(genome);
  auto* pipe = static_cast<Pipe*>(genome.userData());
  std::vector<unsigned char> item = pipe->drawInput();
  for (unsigned char value : item) {
    list_genome.insert(value);
  }
}
}  // namespace

Pipe::Pipe(uint32_t max_size, uint32_t item_size)
  : max_size_{max_size}, item_size_{item_size} {
}

void Pipe::addInput(const std::vector<unsigned char>& candidate) {
  input_.push_back(candidate);
}

void Pipe::evolve() {
  GAListGenome<unsigned char> genome(staticEvaluatorWrapper);
  genome.initializer(staticInitializerWrapper);
  genome.userData(this);

  GASimpleGA algorithm(genome);
  algorithm.populationSize(max_size_);
  // TODO(fairlight1337): Fill and parameterize the GA here.
  algorithm.evolve();
}

bool Pipe::hasSpace() const {
  return input_.size() < max_size_;
}

std::vector<unsigned char> Pipe::drawInput() {
  if (input_.empty()) {
    throw std::underflow_error("No input candidates available to draw.");
  }

  std::vector<unsigned char> item = input_.front();
  input_.pop_front();

  return item;
}

}  // namespace beast
