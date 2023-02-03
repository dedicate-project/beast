#include <beast/pipe.hpp>

// Standard
#include <iostream>
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
  std::cout << "Initialized: " << list_genome.size() << std::endl;
}
}  // namespace

Pipe::Pipe(uint32_t max_candidates)
  : max_candidates_{max_candidates} {
}

void Pipe::addInput(const std::vector<unsigned char>& candidate) {
  input_.push_back(candidate);
}

void Pipe::evolve() {
  GAListGenome<unsigned char> genome(staticEvaluatorWrapper);
  genome.initializer(staticInitializerWrapper);
  genome.userData(this);

  GASimpleGA algorithm(genome);
  algorithm.maximize();
  algorithm.populationSize(max_candidates_);
  // TODO(fairlight1337): Fill and parameterize the GA here.
  algorithm.evolve();

  // Save the finalists if they pass the cut-off score.
  const GAPopulation& population = algorithm.population();
  for (uint32_t idx = 0; idx < population.size(); ++idx) {
    GAGenome& individual = population.individual(idx);
    auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(genome);
    std::cout << list_genome.size() << ", " << individual.score() << std::endl;
    if (list_genome.size() > 0 && individual.score() >= cut_off_score_) {
      std::vector<unsigned char> data;
      for (uint32_t idx = 0; idx < list_genome.size(); ++idx) {
        data.push_back(*list_genome[idx]);
      }

      storeFinalist(data, individual.score());
    }
  }
}

bool Pipe::hasSpace() const {
  return input_.size() < max_candidates_;
}

std::vector<unsigned char> Pipe::drawInput() {
  if (input_.empty()) {
    throw std::underflow_error("No input candidates available to draw.");
  }

  std::vector<unsigned char> item = input_.front();
  input_.pop_front();

  return item;
}

bool Pipe::hasOutput() const {
  return !output_.empty();
}

Pipe::OutputItem Pipe::drawOutput() {
  if (output_.empty()) {
    throw std::underflow_error("No output candidates available to draw.");
  }

  OutputItem item = output_.front();
  output_.pop_front();

  return item;
}

void Pipe::setCutOffScore(double cut_off_score) {
  cut_off_score_ = cut_off_score;
}

void Pipe::storeFinalist(const std::vector<unsigned char>& finalist, float score) {
  output_.push_back({finalist, static_cast<double>(score)});
}

}  // namespace beast
