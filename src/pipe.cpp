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
 * @brief Intermediary function to trigger evaluation of Genomes
 *
 * The pipe object is dereferences from the genome's user data to trigger the instance's evaluation
 * function. The resulting score value is then returned to the GAlib mechanism.
 *
 * The function needs to be excluded from the clang-tidy linting process because the parameter would
 * need to be made const, which does not match GAlib's evaluator signature. Ignoring it does no harm
 * here as the function is not used anywhere else.
 *
 * @param genome The GAlib genome to evaluate
 * @return The score value resulting from the evaluation
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
 * @brief Intermediary function to initialize Genomes
 *
 * The pipe object is dereferences from the genome's user data to draw from the instance's initial
 * population candidates. The GAlib genomes are then initialized with that data.
 *
 * The function needs to be excluded from the clang-tidy linting process because the parameter would
 * need to be made const, which does not match GAlib's evaluator signature. Ignoring it does no harm
 * here as the function is not used anywhere else.
 *
 * @param genome The GAlib genome to initialize
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
  algorithm.populationSize(max_candidates_);
  
  /* TODO(fairlight1337): Fill and parameterize the GA here.
   *
   * GAParameterList params{};
   * GASimpleGA::registerDefaultParameters(params);
   * algorithm.parameters(params);
   */
  
  algorithm.evolve();

  // Save the finalists if they pass the cut-off score.
  const GAPopulation& population = algorithm.population();
  for (uint32_t idx = 0; idx < population.size(); ++idx) {
    GAGenome& individual = population.individual(idx);
    auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(individual);
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
