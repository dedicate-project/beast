#include <beast/pipes/evaluator_pipe.hpp>

// Standard
#include <algorithm>
#include <fstream>

// Third-party
#include <nlohmann/json.hpp>

namespace beast {

EvaluatorPipe::EvaluatorPipe(uint32_t max_candidates, size_t variable_count,
                             size_t string_table_count, size_t max_string_size)
    : EvolutionPipe(max_candidates), variable_count_{variable_count},
      string_table_count_{string_table_count}, max_string_size_{max_string_size} {}

void EvaluatorPipe::addEvaluator(const std::shared_ptr<Evaluator>& evaluator, double weight,
                                 bool invert_logic) {
  evaluator_.addEvaluator(evaluator, weight, invert_logic);
}

void EvaluatorPipe::execute() {
  // Reload the library at the start of every cycle so we pick up any new survivors
  // an upstream `ProgramStorageSinkPipe` may have flushed since the previous cycle.
  // The reload also re-syncs `EvolutionParameters::subroutine_arities` with the
  // current library shape -- without that, the GA-side toolchain could still emit
  // `CallSubroutine` instructions referencing a stale id.
  if (!subroutine_sources_.empty()) {
    rebuildSubroutineLibrary();
  }
  EvolutionPipe::execute();
}

double EvaluatorPipe::evaluate(const std::vector<unsigned char>& program_data) {
  VmSession session(Program(program_data), variable_count_, string_table_count_, max_string_size_);
  if (subroutine_library_) {
    session.setSubroutineLibrary(subroutine_library_);
  }
  // Forward the pipeline's cooperative-cancellation token so per-trial / per-round step
  // loops inside expensive evaluators (notably Sha256RoundEvaluator with large
  // `max_steps_per_trial` / `rounds_per_trial`) can short-circuit instead of running to
  // their natural completion when the user hits Stop. Most evaluators perform a tight VM
  // step loop; checking `session.isStopRequested()` per step is essentially free
  // (one relaxed atomic load) but takes the stop latency from "minutes" to "milliseconds"
  // for the worst-offender evaluators.
  session.setStopToken(getStopToken());
  return evaluator_.evaluate(session);
}

uint32_t EvaluatorPipe::getMemorySize() const { return static_cast<uint32_t>(variable_count_); }

uint32_t EvaluatorPipe::getStringTableSize() const {
  return static_cast<uint32_t>(string_table_count_);
}

uint32_t EvaluatorPipe::getStringTableItemLength() const {
  return static_cast<uint32_t>(max_string_size_);
}

const std::vector<AggregationEvaluator::EvaluatorDescription>&
EvaluatorPipe::getEvaluators() const {
  return evaluator_.getEvaluators();
}

void EvaluatorPipe::addSubroutineSource(const SubroutineSource& source) {
  subroutine_sources_.push_back(source);
}

const std::vector<EvaluatorPipe::SubroutineSource>&
EvaluatorPipe::getSubroutineSources() const noexcept {
  return subroutine_sources_;
}

std::shared_ptr<const SubroutineLibrary>
EvaluatorPipe::getSubroutineLibrary() const noexcept {
  return subroutine_library_;
}

namespace {

/// Load the top-K genomes from a `ProgramStorageSinkPipe`-format ledger file.
///
/// The ledger is a JSON array of `{score, data}` objects already sorted by descending
/// score (the sink rewrites the file in sorted order on every flush). We trust that
/// ordering but defensively re-sort here so a hand-edited ledger still does the right
/// thing.
///
/// Tolerates missing / malformed files: returns an empty vector and the caller treats
/// that source as "skip, but keep going". Failing loudly here would break the
/// "subroutines are best-effort" contract.
std::vector<std::vector<unsigned char>>
loadTopKGenomesFromLedger(const std::string& path, uint32_t top_k) {
  std::vector<std::vector<unsigned char>> result;
  if (path.empty() || top_k == 0) {
    return result;
  }
  std::ifstream in(path);
  if (!in) {
    return result;
  }
  try {
    nlohmann::json doc;
    in >> doc;
    if (!doc.is_array()) {
      return result;
    }
    struct Entry {
      double score;
      std::vector<unsigned char> data;
    };
    std::vector<Entry> entries;
    entries.reserve(doc.size());
    for (const auto& item : doc) {
      if (!item.contains("data") || !item.contains("score")) {
        continue;
      }
      entries.push_back({item["score"].get<double>(),
                         item["data"].get<std::vector<unsigned char>>()});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.score > b.score; });
    const size_t take = std::min<size_t>(top_k, entries.size());
    result.reserve(take);
    for (size_t i = 0; i < take; ++i) {
      result.push_back(std::move(entries[i].data));
    }
  } catch (const std::exception&) {
    // Best-effort: a malformed file is "no usable survivors".
    return {};
  }
  return result;
}

} // namespace

void EvaluatorPipe::rebuildSubroutineLibrary() {
  auto library = std::make_shared<SubroutineLibrary>();
  SubroutineArityTable arities;

  for (const auto& source : subroutine_sources_) {
    auto bodies = loadTopKGenomesFromLedger(source.ledger_path, source.top_k);
    for (auto& body : bodies) {
      // Defense-in-depth: drop any genome that already contains a CallSubroutine
      // opcode. v1 explicitly disallows recursion; a ledger that picks up a buggy
      // genome (e.g. from a future schema version) must not be mounted.
      if (!subroutineBodyIsCallFree(body)) {
        continue;
      }
      // Bounded by the library size cap to keep `subroutine_id` (a uint8) in range.
      if (library->size() >= kMaxSubroutineLibrarySize) {
        break;
      }
      SubroutineEntry entry;
      entry.bytecode = std::move(body);
      entry.input_arity = source.input_arity;
      entry.output_arity = source.output_arity;
      entry.max_steps_per_call = source.max_steps_per_call;
      library->push_back(std::move(entry));
      arities.emplace_back(source.input_arity, source.output_arity);
    }
  }

  // Keep the GA-side arity table in sync with the library we just built so the
  // factory and mutator see the same shape. This is the *only* spot where the two
  // sides are reconciled.
  auto current_params = getEvolutionParameters();
  current_params.subroutine_arities = std::move(arities);
  setEvolutionParameters(current_params);

  subroutine_library_ = std::move(library);
}

} // namespace beast
