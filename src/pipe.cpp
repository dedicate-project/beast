#include <beast/pipe.hpp>

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
float staticEvaluatorWrapper(GAGenome& /*genome*/) {
  //Pipe* pipe = static_cast<Pipe*>(genome.userData());
  return static_cast<float>(beast::Pipe::evaluate(/*genome*/));
}
}  // namespace

Pipe::Pipe(uint32_t max_size, uint32_t item_size)
  : max_size_{max_size}, item_size_{item_size} {
}

void Pipe::addInput(VmSession& session) {
  input_.push_back(session);
}

void Pipe::evolve() {
  GAListGenome<int32_t> genome(staticEvaluatorWrapper);
  genome.userData(this);

  GASimpleGA algorithm(genome);
  // TODO(fairlight1337): Fill and parameterize the GA here.
  algorithm.evolve();
}

double Pipe::evaluate() {
  return 0.0;
}

bool Pipe::hasSpace() {
  return input_.size() < max_size_;
}

}  // namespace beast
