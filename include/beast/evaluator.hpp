#ifndef BEAST_EVALUATOR_HPP_
#define BEAST_EVALUATOR_HPP_

#include <beast/vm_session.hpp>

namespace beast {

/**
 * @class Evaluator
 * @brief Base class for Program and Session evaluation
 *
 * Evaluators are used to score the static structure and dynamic runtime statistics of programs and
 * execution sessions.
 *
 * @author Jan Winkler
 * @date 2023-01-25
 */
class Evaluator {
 public:
  /**
   * @fn Evaluator::~Evaluator
   * @brief Virtual destructor performing no operation to ensure vtable consistency
   */
  virtual ~Evaluator() = default;

  /**
   * @fn Evaluator::evaluate
   * @brief Determines the fitness score of a session object
   *
   * The fitness score is determined based on the static program contained therein, and the dynamic
   * runtime statistics collected while the program was executed. Concrete implementations of this
   * base class may put emphasize in different aspects of this, and evaluate programs and sessions
   * in different ways.
   *
   * The score value returned is a value between 0.0 and 1.0, where 0.0 means "no fit at all" and
   * 1.0 means "perfect fit". All implementations of this interface must adhere to this rule.
   *
   * @param session The session object to base the score determination on
   * @return A score value from 0.0 (no fit) to 1.0 (perfect fit)
   */
  virtual double evaluate(const VmSession& session) const = 0;
};

}  // namespace beast

#endif  // BEAST_EVALUATOR_HPP_
