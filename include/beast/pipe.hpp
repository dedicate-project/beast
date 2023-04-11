#ifndef BEAST_PIPE_HPP_
#define BEAST_PIPE_HPP_

// Standard
#include <cstdint>
#include <deque>
#include <vector>

namespace beast {

/**
 * @class Pipe
 * @brief Provides an abstract interface for managing input and output buffers of candidate programs
 * and finalist programs
 *
 * The Pipe class serves as a base class for different types of evolutionary algorithms. It manages
 * the input and output buffers of candidate programs and finalist programs while providing methods
 * to add, draw, and check the availability of programs in the buffers. Derived classes should
 * implement the execute() method to define their specific behavior.
 */
class Pipe {
 public:
  /**
   * @brief Constructs a new Pipe with the specified number of candidates,
   *        input slots, and output slots
   *
   * @param max_candidates The maximum number of candidates in the population
   * @param input_slots The number of input slots for candidate programs
   * @param output_slots The number of output slots for finalist programs
   */
  Pipe(uint32_t max_candidates, uint32_t input_slots, uint32_t output_slots);

  /**
   * @brief Destructor
   *
   * Added for vtable consistency.
   */
  virtual ~Pipe() = default;

  /**
   * @brief Holds information about finalist programs
   *
   * The result of a successful evolution consists of finalist programs that passed the minimum
   * score in the task given. This struct holds the respective program code, as well as its score.
   */
  struct OutputItem {
    std::vector<unsigned char> data; ///< The program code
    double score;                    ///< The evaluation score this code achieved
  };

  /**
   * @class Pipe::inputHasSpace
   * @brief Denote whether space is left in the input pool
   *
   * @return `true` if the number of candidates in the input pool is less than the population size,
   *         `false` otherwise.
   */
  bool inputHasSpace(uint32_t slot_index) const;

  /**
   * @class Pipe::addInput
   * @brief Add a candidate program code to the input pool
   *
   * This function adds a given program code vector to the input candidate pool. These individuals
   * will be used as initial population for evolution.
   *
   * @param candidate The candidate program code to add to the input pool
   */
  void addInput(uint32_t slot_index, const std::vector<unsigned char>& candidate);

  /**
   * @class Pipe::drawInput
   * @brief Pull an input candidate from the input buffer
   *
   * Returns the oldest input buffer candidate and removes it from the buffer.
   *
   * @return An input candidate program code
   */
  [[nodiscard]] std::vector<unsigned char> drawInput(uint32_t slot_index);

  /**
   * @class Pipe::hasOutput
   * @brief Denotes whether output finalists are available
   *
   * @return `true` if at least one output finalist is available, `false` otherwise
   */
  [[nodiscard]] bool hasOutput(uint32_t slot_index) const;

  /**
   * @class Pipe::drawOutput
   * @brief Pull an output finalist from the output buffer
   *
   * Returns the oldest output buffer candidate and removes it from the buffer.
   *
   * @return An output finalist item consisting of program code and score
   */
  [[nodiscard]] OutputItem drawOutput(uint32_t slot_index);

  /**
   * @brief Get the amount of candidates in the specified input slot
   *
   * @param slot_index The index of the input slot
   * @return The number of candidates in the specified input slot
   */
  [[nodiscard]] uint32_t getInputSlotAmount(uint32_t slot_index) const;

  /**
   * @brief Get the amount of finalists in the specified output slot
   *
   * @param slot_index The index of the output slot
   * @return The number of finalists in the specified output slot
   */
  [[nodiscard]] uint32_t getOutputSlotAmount(uint32_t slot_index) const;

  /**
   * @brief Get the total number of input slots
   *
   * @return The number of input slots
   */
  [[nodiscard]] uint32_t getInputSlotCount() const { return inputs_.size(); }

  /**
   * @brief Get the total number of output slots
   *
   * @return The number of output slots
   */
  [[nodiscard]] uint32_t getOutputSlotCount() const { return outputs_.size(); }

  /**
   * @brief Get the maximum number of candidates allowed in the population
   *
   * @return The maximum number of candidates
   */
  [[nodiscard]] uint32_t getMaxCandidates() const { return max_candidates_; }

  /**
   * @brief Checks if the inputs are saturated (i.e., all input slots are full)
   *
   * @return `true` if all input slots are full, `false` otherwise
   */
  [[nodiscard]] virtual bool inputsAreSaturated() const;

  /**
   * @brief Checks if the outputs are saturated (i.e., all output slots are full)
   *
   * @return `true` if all output slots are full, `false` otherwise
   */
  [[nodiscard]] virtual bool outputsAreSaturated() const;

  /**
   * @brief Executes the pipe's main functionality
   *
   * This is a pure virtual function that should be implemented by derived classes.
   */
  virtual void execute() = 0;

 protected:
  /**
   * @var Pipe::input_
   * @brief Holds the input candidate programs
   */
  std::vector<std::deque<std::vector<unsigned char>>> inputs_;

  /**
   * @var Pipe::output_
   * @brief Holds the finalist output buffer
   */
  std::vector<std::deque<OutputItem>> outputs_;

  /**
   * @var Pipe::max_candidates_
   * @brief Denotes the population size of this pipe
   */
  uint32_t max_candidates_;
};

} // namespace beast

#endif // BEAST_PIPE_HPP_
