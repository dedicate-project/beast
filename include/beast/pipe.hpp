#ifndef BEAST_PIPE_HPP_
#define BEAST_PIPE_HPP_

// Standard
#include <deque>
#include <stdint.h>
#include <vector>

// Internal
#include <beast/vm_session.hpp>

namespace beast {

/**
 * @class Pipe
 * @brief Base class for implementing evolutionary pipes
 *
 * Evolutionary pipes are the main mechanism for fitting a set of preliminary program candidates to
 * a given task. A pipe implements the entire logic to judge the fitness of a program for the task,
 * by implementing an `evaluate` function. This class is virtual and must be subclassed.
 *
 * @author Jan Winkler
 * @date 2023-02-04
 */
class Pipe {
 public:
  /**
   * @brief Holds information about finalist programs
   *
   * The result of a successful evolution consists of finalist programs that passed the minimum
   * score in the task given. This struct holds the respective program code, as well as its score.
   */
  struct OutputItem {
    std::vector<unsigned char> data;  ///< The program code
    double score;                     ///< The evaluation score this code achieved
  };

  /**
   * @class Pipe::Pipe
   * @brief Constructs the pipe for a maximum candidate buffer size
   *
   * Pipes have a specific initial population size that needs to be met with candidates before
   * evolution can begin. This size is specified in this constructor.
   *
   * @param max_candidates The candidate population size for this pipe
   */
  explicit Pipe(uint32_t max_candidates);

  /**
   * @class Pipe::~Pipe
   * @brief Deconstructs this instance
   *
   * Required for vtable consistency.
   */
  virtual ~Pipe() = default;

  /**
   * @class Pipe::addInput
   * @brief Add a candidate program code to the input pool
   *
   * This function adds a given program code vector to the input candidate pool. These individuals
   * will be used as initial population for evolution.
   *
   * @param candidate The candidate program code to add to the input pool
   */
  void addInput(const std::vector<unsigned char>& candidate);

  /**
   * @class Pipe::evolve
   * @brief Performs an evolution of the input candidate programs according to the Pipe's task
   *
   * Based on the `evaluate` function implemented in the concrete Pipe implementation, candidate
   * programs are scored according to their performance in these tasks. This function performs the
   * evolutionary steps required for recombination and formulation of new programs, and stores
   * programs that pass the cut-off score into the output finalist buffer.
   */
  void evolve();

  /**
   * @class Pipe::hasSpace
   * @brief Denote whether space is left in the input pool
   *
   * @return `true` if the number of candidates in the input pool is less than the population size,
   *         `false` otherwise.
   */
  bool hasSpace() const;

  /**
   * @class Pipe::evaluate
   * @brief Scores a candidate program according to a concrete task
   *
   * The passed in program code shall be tested for suitability for a concrete task. The better the
   * performance, the higher the score (0.0 - 1.0). Subclasses of the Pipe class need to implement
   * this function, and it is the main driver for the evolutionary step.
   *
   * @param program_data The program candidate to score
   * @return The evaluation score the program candidate achieved (0.0 - 1.0)
   */
  [[nodiscard]] virtual double evaluate(const std::vector<unsigned char>& program_data) const = 0;

  /**
   * @class Pipe::drawInput
   * @brief Pull an input candidate from the input buffer
   *
   * Returns the oldest input buffer candidate and removes it from the buffer.
   *
   * @return An input candidate program code
   */
  [[nodiscard]] std::vector<unsigned char> drawInput();

  /**
   * @class Pipe::hasOutput
   * @brief Denotes whether output finalists are available
   *
   * @return `true` if at least one output finalist is available, `false` otherwise
   */
  [[nodiscard]] bool hasOutput() const;

  /**
   * @class Pipe::drawOutput
   * @brief Pull an output finalist from the output buffer
   *
   * Returns the oldest output buffer candidate and removes it from the buffer.
   *
   * @return An output finalist item consisting of program code and score
   */
  [[nodiscard]] OutputItem drawOutput();

  /**
   * @class Pipe::setCutOffScore
   * @brief Sets the cut-off score
   *
   * Finalists that score below this score are discarded after evolution.
   *
   * @param cut_off_score The cut-off score under which to discard finalists
   */
  void setCutOffScore(double cut_off_score);

 protected:
  /**
   * @class Pipe::storeFinalist
   * @brief Stores a finalist in the output buffer
   *
   * @param finalist The finalist program code to add to the buffer
   * @param score The score the finalist program code has achieved in the evaluation
   * @sa hasOutput(), drawOutput()
   */
  void storeFinalist(const std::vector<unsigned char>& finalist, float score);

 private:
  /**
   * @var Pipe::max_candidates_
   * @brief Denotes the population size of this pipe
   */
  uint32_t max_candidates_;

  /**
   * @var Pipe::input_
   * @brief Holds the input candidate programs
   */
  std::deque<std::vector<unsigned char>> input_;

  /**
   * @var Pipe::output_
   * @brief Holds the finalist output buffer
   */
  std::deque<OutputItem> output_;

  /**
   * @var Pipe::cut_off_score_
   * @brief Determines the minimum score a finalist needs to have
   *
   * Finalists below this score are not added to the output buffer.
   */
  double cut_off_score_ = 0.0;
};

}  // namespace beast

#endif  // BEAST_PIPE_HPP_
