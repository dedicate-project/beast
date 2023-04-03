#include <beast/pipes/evolution_pipe.hpp>

// Standard
#include <stdexcept>

// GAlib
// NOTE: For these includes, the `register` error needs to be ignored as this 3rdparty library uses
// outdated code. This is not an issue for the library using it though.
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#endif
#include <ga/GAListGenome.h>
#include <ga/GASimpleGA.h>
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

namespace beast {

namespace {
/**
 * @brief Intermediary function to trigger evaluation of Genomes
 *
 * The evolution pipe object is dereferences from the genome's user data to trigger the instance's
 * evaluation function. The resulting score value is then returned to the GAlib mechanism.
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
  data.resize(list_genome.size());
  for (int32_t idx = 0; idx < list_genome.size(); ++idx) {
    data[idx] = *list_genome[idx];
  }

  auto* pipe = static_cast<EvolutionPipe*>(genome.userData());
  return static_cast<float>(pipe->evaluate(data));
}

/**
 * @brief Intermediary function to initialize Genomes
 *
 * The evolution pipe object is dereferences from the genome's user data to draw from the instance's
 * initial population candidates. The GAlib genomes are then initialized with that data.
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
  auto* pipe = static_cast<EvolutionPipe*>(genome.userData());
  std::vector<unsigned char> item = pipe->drawInput(0);
  for (unsigned char value : item) {
    list_genome.insert(value);
  }
}
}  // namespace

EvolutionPipe::EvolutionPipe(uint32_t max_candidates)
  : Pipe(max_candidates) {
}

void EvolutionPipe::execute() {
  GAListGenome<unsigned char> genome(staticEvaluatorWrapper);
  genome.initializer(staticInitializerWrapper);
  genome.userData(this);

  GASimpleGA algorithm(genome);
  algorithm.populationSize(getMaxCandidates());
  algorithm.nGenerations(num_generations_);
  algorithm.pMutation(mutation_probability_);
  algorithm.pCrossover(crossover_probability_);

  algorithm.evolve();

  // Save the finalists if they pass the cut-off score.
  const GAPopulation& population = algorithm.population();
  for (int32_t pop_idx = 0; pop_idx < population.size(); ++pop_idx) {
    GAGenome& individual = population.individual(pop_idx);
    auto& list_genome = dynamic_cast<GAListGenome<unsigned char>&>(individual);
    if (list_genome.size() > 0 && individual.score() >= cut_off_score_) {
      std::vector<unsigned char> data;
      data.resize(list_genome.size());
      for (int32_t idx = 0; idx < list_genome.size(); ++idx) {
        data[idx] = *list_genome[idx];
      }

      storeFinalist(data, individual.score());
    }
  }
}

void EvolutionPipe::setCutOffScore(double cut_off_score) {
  cut_off_score_ = cut_off_score;
}

void EvolutionPipe::storeFinalist(const std::vector<unsigned char>& finalist, float score) {
  output_.push_back({finalist, static_cast<double>(score)});
}

}  // namespace beast
