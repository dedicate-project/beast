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
 * @brief NEEDS DOCUMENTATION
 *
 * TODO(fairlight1337): Document this class.
 */
class Pipe {
 public:
  /**
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this struct.
   */
  struct OutputItem {
    std::vector<unsigned char> data;  ///< tbd
    double score;                     ///< tbd
  };

  /**
   * @class Pipe::Pipe
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @param max_candidates tbd
   */
  Pipe(uint32_t max_candidates);

  /**
   * @class Pipe::~Pipe
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  virtual ~Pipe() = default;

  /**
   * @class Pipe::addIndividualToInitialPopulation
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @param candidate tbd
   */
  void addInput(const std::vector<unsigned char>& candidate);

  /**
   * @class Pipe::evolve
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   */
  void evolve();

  /**
   * @class Pipe::hasSpace
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @return tbd
   */
  bool hasSpace() const;

  /**
   * @class Pipe::evaluate
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @param program_data tbd
   * @return tbd
   */
  [[nodiscard]] virtual double evaluate(const std::vector<unsigned char>& program_data) const = 0;

  /**
   * @class Pipe::drawInput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @return tbd
   */
  [[nodiscard]] std::vector<unsigned char> drawInput();

  /**
   * @class Pipe::hasOutput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @return tbd
   */
  [[nodiscard]] bool hasOutput() const;

  /**
   * @class Pipe::drawOutput
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @return tbd
   */
  [[nodiscard]] OutputItem drawOutput();

  /**
   * @class Pipe::setCutOffScore
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @param cut_off_score tbd
   */
  void setCutOffScore(double cut_off_score);

 protected:
  /**
   * @class Pipe::storeFinalist
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this function.
   *
   * @param finalist tbd
   * @param score tbd
   */
  void storeFinalist(const std::vector<unsigned char>& finalist, float score);

 private:
  /**
   * @var Pipe::max_candidates_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
   */
  uint32_t max_candidates_;

  /**
   * @var Pipe::input_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
   */
  std::deque<std::vector<unsigned char>> input_;

  /**
   * @var Pipe::output_
   * @brief NEEDS DOCUMENTATION
   *
   * TODO(fairlight1337): Document this var.
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
