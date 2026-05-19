#ifndef BEAST_PIPE_HPP_
#define BEAST_PIPE_HPP_

// Standard
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
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
  [[nodiscard]] bool inputHasSpace(uint32_t slot_index);

  /**
   * @class Pipe::addInput
   * @brief Add a candidate program code to the input pool
   *
   * This function adds a given program code vector to the input candidate pool. These individuals
   * will be used as initial population for evolution. The candidate is wrapped into an
   * `OutputItem` with score 0.0 internally so the pipe can pass it on with score information
   * later; callers that already have a score should use `addInputWithScore` instead.
   *
   * @param candidate The candidate program code to add to the input pool
   */
  void addInput(uint32_t slot_index, const std::vector<unsigned char>& candidate);

  /**
   * @brief Add a candidate program code with its upstream score to the input pool
   *
   * Used by the `Pipeline` plumbing to preserve a candidate's score across pipe boundaries
   * so downstream observers (notably `ResultsSummaryPipe`) can summarise it without
   * re-evaluating. Existing pipes can still draw the candidate with the score-stripped
   * `drawInput` interface; the score component is silently ignored.
   */
  void addInputWithScore(uint32_t slot_index, const OutputItem& candidate);

  void addInputWithScore(uint32_t slot_index, OutputItem&& candidate);

  /**
   * @class Pipe::drawInput
   * @brief Pull an input candidate from the input buffer
   *
   * Returns the oldest input buffer candidate and removes it from the buffer. The upstream
   * score is discarded; use `drawInputWithScore` instead when the score matters.
   *
   * @return An input candidate program code
   */
  [[nodiscard]] std::vector<unsigned char> drawInput(uint32_t slot_index);

  /**
   * @brief Pull an input candidate from the input buffer, preserving its upstream score
   *
   * Used by passthrough/observer pipes that want to read off the score the previous pipe
   * tagged the candidate with.
   */
  [[nodiscard]] OutputItem drawInputWithScore(uint32_t slot_index);

  /**
   * @brief Return the score of the next candidate at the front of the input buffer without
   *        consuming it
   *
   * Used by pipes that have to decide where to route a candidate based on its score before
   * committing to drawing it -- notably `FilterPipe`, which has to know which output slot
   * the next item should land in before pulling it off the FIFO (so back-pressure on the
   * wrong slot doesn't silently reorder candidates). Throws `std::underflow_error` if the
   * slot is empty; callers should check `getInputSlotAmount` first.
   */
  [[nodiscard]] double peekInputScore(uint32_t slot_index);

  /**
   * @class Pipe::hasOutput
   * @brief Denotes whether output finalists are available
   *
   * @return `true` if at least one output finalist is available, `false` otherwise
   */
  [[nodiscard]] bool hasOutput(uint32_t slot_index);

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
  [[nodiscard]] uint32_t getInputSlotAmount(uint32_t slot_index);

  /**
   * @brief Get the amount of finalists in the specified output slot
   *
   * @param slot_index The index of the output slot
   * @return The number of finalists in the specified output slot
   */
  [[nodiscard]] uint32_t getOutputSlotAmount(uint32_t slot_index);

  /**
   * @brief Get the total number of input slots
   *
   * @return The number of input slots
   */
  [[nodiscard]] uint32_t getInputSlotCount() const;

  /**
   * @brief Get the total number of output slots
   *
   * @return The number of output slots
   */
  [[nodiscard]] uint32_t getOutputSlotCount() const;

  /**
   * @brief Get the maximum number of candidates allowed in the population
   *
   * @return The maximum number of candidates
   */
  [[nodiscard]] uint32_t getMaxCandidates() const;

  /**
   * @brief Checks if the inputs are saturated (i.e., all input slots are full)
   *
   * @return `true` if all input slots are full, `false` otherwise
   */
  [[nodiscard]] virtual bool inputsAreSaturated();

  /**
   * @brief Checks if the outputs are saturated (i.e., all output slots are full)
   *
   * @return `true` if all output slots are full, `false` otherwise
   */
  [[nodiscard]] virtual bool outputsAreSaturated();

  /**
   * @brief Executes the pipe's main functionality
   *
   * This is a pure virtual function that should be implemented by derived classes.
   */
  virtual void execute() = 0;

  /**
   * @brief Attach a process-wide stop token used by long-running operations to short-circuit
   *
   * Pipelines own a single `std::atomic<bool>` instance that's shared across every pipe in the
   * pipeline. `Pipeline::start()` mints a fresh `false` token and passes it to each pipe via
   * this method; `Pipeline::stop()` flips the bit *before* asking workers to exit, giving
   * cooperative long-running paths (currently: the BeastGA evaluator wrapper inside
   * `EvolutionPipe` and the VM step loop in expensive `Evaluator`s) a chance to bail out fast
   * instead of running to natural completion. Without this, stopping a SHA-256 multi-round
   * pipeline would block until the in-flight evolve() cycle finishes -- often minutes.
   *
   * Idempotent and thread-safe. Passing a null token unsets the existing one (handy for
   * tests / standalone pipe usage where there's no Pipeline at all).
   */
  void setStopToken(std::shared_ptr<std::atomic<bool>> token) noexcept;

  /**
   * @brief Cheap, lock-free check for "should I bail out now?"
   *
   * Returns false when no stop token has been attached (the standalone case) or when the
   * token is in the not-requested state. Safe to call from any thread; designed for use in
   * hot inner loops (per VM step, per genome evaluation).
   */
  [[nodiscard]] bool isStopRequested() const noexcept;

  /**
   * @brief Access the underlying stop token so downstream collaborators (notably
   *        `VmSession`) can share the same flag without an extra round-trip through `Pipe`
   *
   * Returns the same null-safe shared_ptr semantics as `setStopToken`. The token outlives
   * the pipeline only if a stray VmSession keeps a copy alive, which is exactly the safety
   * property we want -- the worker thread can finish its in-flight step without dereferencing
   * a freed atomic.
   */
  [[nodiscard]] std::shared_ptr<std::atomic<bool>> getStopToken() const noexcept;

 protected:
  void storeOutput(uint32_t slot_index, const OutputItem& output);

  void storeOutput(uint32_t slot_index, OutputItem&& output);

 private:
  /**
   * @var Pipe::input_
   * @brief Holds the input candidate programs and their upstream-attached scores
   *
   * Storing `OutputItem` (rather than just the byte vector) lets the `Pipeline` carry a
   * candidate's score from one pipe's output to the next pipe's input. Pipes that don't care
   * about the score (factories, sinks, evaluators that re-score) keep using the legacy
   * `addInput` / `drawInput` interface and never see the score field.
   */
  std::vector<std::deque<OutputItem>> inputs_;

  std::mutex inputs_mutex_;

  /**
   * @var Pipe::output_
   * @brief Holds the finalist output buffer
   */
  std::vector<std::deque<OutputItem>> outputs_;

  std::mutex outputs_mutex_;

  /**
   * @var Pipe::max_candidates_
   * @brief Denotes the population size of this pipe
   */
  uint32_t max_candidates_;

  /**
   * @var Pipe::stop_token_
   * @brief Shared `std::atomic<bool>` flipped by `Pipeline::stop()` for cooperative cancellation
   *
   * Pointer (not value) because the same token is shared across every pipe in a pipeline; a
   * single flip then short-circuits every pipe's in-flight long-running operation at once.
   * Default-initialised to nullptr -- `isStopRequested()` treats nullptr as "no stop"
   * so a Pipe built outside a Pipeline (tests, examples) still works without ceremony.
   */
  std::shared_ptr<std::atomic<bool>> stop_token_;
};

} // namespace beast

#endif // BEAST_PIPE_HPP_
