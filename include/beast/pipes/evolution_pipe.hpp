#ifndef BEAST_EVOLUTION_PIPE_HPP_
#define BEAST_EVOLUTION_PIPE_HPP_

// Standard
#include <deque>
#include <stdint.h>
#include <vector>

// Internal
#include <beast/pipe.hpp>
#include <beast/vm_session.hpp>

namespace beast {

/**
 * @class EvolutionPipe
 * @brief Base class for implementing evolutionary pipes
 *
 * Evolutionary pipes are the main mechanism for fitting a set of preliminary program candidates to
 * a given task. A pipe implements the entire logic to judge the fitness of a program for the task,
 * by implementing an `evaluate` function. This class is virtual and must be subclassed.
 *
 * @author Jan Winkler
 * @date 2023-02-04
 */
class EvolutionPipe : public Pipe {
 public:
  /**
   * @class EvolutionPipe::EvolutionPipe
   * @brief Constructs the pipe for a maximum candidate buffer size
   *
   * Pipes have a specific initial population size that needs to be met with candidates before
   * evolution can begin. This size is specified in this constructor.
   *
   * @param max_candidates The candidate population size for this pipe
   */
  explicit EvolutionPipe(uint32_t max_candidates);

  /**
   * @class EvolutionPipe::execute
   * @brief Performs an evolution of the input candidate programs according to the Pipe's task
   *
   * Based on the `evaluate` function implemented in the concrete Pipe implementation, candidate
   * programs are scored according to their performance in these tasks. This function performs the
   * evolutionary steps required for recombination and formulation of new programs, and stores
   * programs that pass the cut-off score into the output finalist buffer.
   */
  void execute() override;

  /**
   * @class EvolutionPipe::evaluate
   * @brief Scores a candidate program according to a concrete task
   *
   * The passed in program code shall be tested for suitability for a concrete task. The better the
   * performance, the higher the score (0.0 - 1.0). Subclasses of the Pipe class need to implement
   * this function, and it is the main driver for the evolutionary step.
   *
   * @param program_data The program candidate to score
   * @return The evaluation score the program candidate achieved (0.0 - 1.0)
   */
  [[nodiscard]] virtual double evaluate(const std::vector<unsigned char>& program_data) = 0;

  /**
   * @class EvolutionPipe::setCutOffScore
   * @brief Sets the cut-off score
   *
   * Finalists that score below this score are discarded after evolution.
   *
   * @param cut_off_score The cut-off score under which to discard finalists
   */
  void setCutOffScore(double cut_off_score);

 protected:
  /**
   * @class EvolutionPipe::storeFinalist
   * @brief Stores a finalist in the output buffer
   *
   * @param finalist The finalist program code to add to the buffer
   * @param score The score the finalist program code has achieved in the evaluation
   * @sa hasOutput(), drawOutput()
   */
  void storeFinalist(const std::vector<unsigned char>& finalist, float score);

  void storeFinalist(std::vector<unsigned char>&& finalist, float score);

 private:
  /**
   * @var EvolutionPipe::cut_off_score_
   * @brief Determines the minimum score a finalist needs to have
   *
   * Finalists below this score are not added to the output buffer.
   */
  double cut_off_score_ = 0.0;

  /**
   * @var EvolutionPipe::num_generations_
   * @brief The number of generations to run through during evolution
   */
  uint32_t num_generations_ = 10;

  /**
   * @var EvolutionPipe::mutation_probability_
   * @brief The probability for gene mutation during evolution
   */
  float mutation_probability_ = 0.001f;

  /**
   * @var EvolutionPipe::crossover_probability_
   * @brief The probability for gene crossover during evolution
   */
  float crossover_probability_ = 0.5f;
};

} // namespace beast

#endif // BEAST_EVOLUTION_PIPE_HPP_
