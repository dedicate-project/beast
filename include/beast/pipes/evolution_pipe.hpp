#ifndef BEAST_EVOLUTION_PIPE_HPP_
#define BEAST_EVOLUTION_PIPE_HPP_

// Standard
#include <cstdint>
#include <deque>
#include <vector>

// Internal
#include <beast/pipe.hpp>
#include <beast/random_program_factory.hpp>
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
 * Genetic algorithm details
 * -------------------------
 *  - The underlying genome is `GAListGenome<unsigned char>` (a raw byte list).
 *  - Mutation and crossover are operator-aware: the genome is decoded via `ProgramParser` into
 *    instruction spans, and edits happen at instruction boundaries. A configurable fraction of
 *    mutations (`EvolutionParameters::byte_mutation_share`) is still byte-level so the search
 *    can stumble onto neighbors the operator-aware mutator would never reach.
 *  - Genomes are hard-capped at `EvolutionParameters::max_genome_bytes`. This prevents the
 *    runaway "bloat" failure mode where insertions and crossover concatenation compound across
 *    generations.
 *  - The hall-of-fame entry tracked by GAlib's statistics is harvested in addition to the final
 *    population, so transient peak scorers don't get lost when mutation pressure is high.
 *
 * @author Jan Winkler
 * @date 2023-02-04
 */
class EvolutionPipe : public Pipe {
 public:
  /**
   * @brief Tunable parameters of the GAlib evolution run
   *
   * `EvolutionPipe::execute` configures GAlib explicitly with these values. Each parameter has
   * a documented meaning and a sensible default; callers can either rely on the defaults or
   * tune per task.
   */
  struct EvolutionParameters {
    uint32_t generations = 50;          ///< Number of generations to evolve
    double crossover_probability = 0.7; ///< Probability that two parents recombine
    double mutation_probability = 0.05; ///< Per-operator mutation probability
    bool elitism = true;                ///< Carry the best individual across each generation

    /// Fraction of mutations that are byte-level (the "gamble"); the rest are operator-level.
    double byte_mutation_share = 0.2;

    /// VM environment used when minting random operators inside the mutator and as the
    /// fallback initializer.
    uint32_t variable_count = 16;
    uint32_t string_table_size = 4;
    uint32_t string_table_item_length = 16;

    /// Hard cap on genome size in bytes after mutation/crossover. We truncate at the last
    /// whole-operator boundary that fits, so the genome stays parseable.
    uint32_t max_genome_bytes = 2048;

    /// Byte budget for the random program emitted by the initializer when the input pool is
    /// empty. 0 means "use variable_count * 8" (the legacy default). Small values (8-64)
    /// shrink the search space dramatically for trivial tasks.
    uint32_t starting_program_size = 0;

    /// Per-opcode sampling weights for both initial program generation and the mutator's
    /// replace/insert operators. Empty map = uniform across all opcodes. Set entries to 0.0
    /// to forbid an opcode entirely for this stage.
    OpcodeWeights opcode_weights{};
  };

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
   * The passed in program code shall be tested for suitability for a concrete task. The better
   * the performance, the higher the score (0.0 - 1.0). Subclasses of the Pipe class need to
   * implement this function, and it is the main driver for the evolutionary step.
   */
  [[nodiscard]] virtual double evaluate(const std::vector<unsigned char>& program_data) = 0;

  /**
   * @class EvolutionPipe::setCutOffScore
   * @brief Sets the cut-off score
   *
   * Finalists that score below this score are discarded after evolution.
   */
  void setCutOffScore(double cut_off_score);

  /**
   * @brief Replace the evolution parameter set used by the next `execute()` call
   */
  void setEvolutionParameters(const EvolutionParameters& parameters);

  /**
   * @brief Currently configured evolution parameters
   */
  [[nodiscard]] const EvolutionParameters& getEvolutionParameters() const noexcept;

  /**
   * @brief Compatibility shim for callers that want to tweak just the generation count
   *
   * Equivalent to `params.generations = generations`; preserved because the legacy
   * implementation exposed `num_generations_` as a private member with no setter and several
   * downstream demos read/wrote it through this helper in their fork.
   */
  void setNumGenerations(uint32_t generations);

  /**
   * @brief Compatibility shim for the legacy `mutation_probability_` access pattern
   */
  void setMutationProbability(double probability);

  /**
   * @brief Compatibility shim for the legacy `crossover_probability_` access pattern
   */
  void setCrossoverProbability(double probability);

 protected:
  /**
   * @class EvolutionPipe::storeFinalist
   * @brief Stores a finalist in the output buffer
   */
  void storeFinalist(const std::vector<unsigned char>& finalist, float score);

  void storeFinalist(std::vector<unsigned char>&& finalist, float score);

 private:
  /// Cut-off score below which finalists are discarded.
  double cut_off_score_ = 0.0;

  /// Currently configured GA evolution parameters.
  EvolutionParameters evolution_parameters_{};
};

} // namespace beast

#endif // BEAST_EVOLUTION_PIPE_HPP_
