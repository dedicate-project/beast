#ifndef BEAST_PIPE_HPP_
#define BEAST_PIPE_HPP_

// Standard
#include <cstdint>
#include <deque>
#include <vector>

namespace beast {

class Pipe {
 public:
  Pipe(uint32_t max_candidates);

  virtual ~Pipe() = default;

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
   * @class Pipe::hasSpace
   * @brief Denote whether space is left in the input pool
   *
   * @return `true` if the number of candidates in the input pool is less than the population size,
   *         `false` otherwise.
   */
  bool hasSpace() const;

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

  [[nodiscard]] uint32_t getInputSlotAmount(uint32_t /*slot_index*/) const { return input_.size(); }

  [[nodiscard]] uint32_t getOutputSlotAmount(uint32_t /*slot_index*/) const {
    return output_.size();
  }

  virtual uint32_t getInputSlotCount() const { return 1; }

  virtual uint32_t getOutputSlotCount() const { return 1; }

  uint32_t getMaxCandidates() const { return max_candidates_; }

  virtual void execute() = 0;

  virtual bool inputsAreSaturated() const {
    bool saturated = true;
    for (uint32_t idx = 0; idx < getInputSlotCount() && saturated; ++idx) {
      saturated &= getInputSlotAmount(idx) >= max_candidates_;
    }
    return saturated;
  }

  virtual bool outputsAreSaturated() const {
    bool saturated = true;
    for (uint32_t idx = 0; idx < getOutputSlotCount() && saturated; ++idx) {
      saturated &= getOutputSlotAmount(idx) >= max_candidates_;
    }
    return saturated;
  }

 protected:
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
   * @var Pipe::max_candidates_
   * @brief Denotes the population size of this pipe
   */
  uint32_t max_candidates_;
};

} // namespace beast

#endif // BEAST_PIPE_HPP_
