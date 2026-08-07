#ifndef BEAST_PIPES_VERIFICATION_SINK_PIPE_HPP_
#define BEAST_PIPES_VERIFICATION_SINK_PIPE_HPP_

// Standard
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Internal
#include <beast/evaluators/task_world_evaluator.hpp>
#include <beast/pipe.hpp>

namespace beast {

/**
 * @class VerificationSinkPipe
 * @brief Measures how well trained agents generalise to unseen task worlds.
 *
 * Unlike an EvaluatorPipe, this sink performs NO evolution. Every program that flows in is
 * replayed against a fixed, deterministic set of held-out worlds (drawn from a seed pool
 * that is disjoint from the training pool) and scored for generalisation. For each program
 * we record:
 * - success_rate: fraction of held-out worlds whose task was fully completed,
 * - mean_score:   average milestone score across the held-out worlds,
 * - mean_items:   average fraction of items collected,
 * - train_score:  the incoming upstream fitness (carried on the candidate), so the
 *                 generalisation gap (train_score - mean_score) is directly observable.
 *
 * The top-K programs by (success_rate, then mean_score) are persisted to a JSON report,
 * rewritten atomically on every change -- a durable record of the best generalisers.
 */
class VerificationSinkPipe : public Pipe {
 public:
  /**
   * @struct Entry
   * @brief One verified program plus its held-out generalisation statistics.
   */
  struct Entry {
    double success_rate = 0.0;
    double mean_score = 0.0;
    double mean_items = 0.0;
    double train_score = 0.0;
    std::vector<unsigned char> data;
  };

  /**
   * @param max_candidates  Per-slot buffer capacity.
   * @param path            Destination JSON report; empty disables persistence.
   * @param top_k           Maximum retained entries (0 picks the default of 10).
   * @param verify_worlds   Number of held-out worlds each program is scored on.
   * @param memory_variables VM variable budget used when replaying programs.
   * @param string_table_items       VM string-table slot count.
   * @param string_table_item_length VM string-table slot length.
   * @param config          TaskWorld evaluator config; its pool_residues define the
   *                        held-out (verification) seed pool.
   */
  VerificationSinkPipe(uint32_t max_candidates, std::string path, uint32_t top_k,
                       uint32_t verify_worlds, uint32_t memory_variables,
                       uint32_t string_table_items, uint32_t string_table_item_length,
                       TaskWorldEvaluator::Config config);

  void execute() override;

  [[nodiscard]] bool inputsAreSaturated() override;

  [[nodiscard]] const std::string& getPath() const noexcept { return path_; }
  [[nodiscard]] uint32_t getTopK() const noexcept { return top_k_; }
  [[nodiscard]] uint32_t getVerifyWorlds() const noexcept { return verify_worlds_; }
  [[nodiscard]] uint32_t getMemorySize() const noexcept { return memory_variables_; }
  [[nodiscard]] uint32_t getStringTableSize() const noexcept { return string_table_items_; }
  [[nodiscard]] uint32_t getStringTableItemLength() const noexcept {
    return string_table_item_length_;
  }
  [[nodiscard]] const TaskWorldEvaluator::Config& getConfig() const {
    return evaluator_.getConfig();
  }
  [[nodiscard]] std::vector<Entry> getEntries() const;

 private:
  [[nodiscard]] Entry verifyProgram(const std::vector<unsigned char>& data,
                                    double train_score) const;
  bool foldEntry(const Entry& entry);
  void persistLocked() const;

  std::string path_;
  uint32_t top_k_;
  uint32_t verify_worlds_;
  uint32_t memory_variables_;
  uint32_t string_table_items_;
  uint32_t string_table_item_length_;
  TaskWorldEvaluator evaluator_;

  mutable std::mutex entries_mutex_;
  std::vector<Entry> entries_;  // sorted by (success_rate, mean_score) descending
};

}  // namespace beast

#endif  // BEAST_PIPES_VERIFICATION_SINK_PIPE_HPP_
