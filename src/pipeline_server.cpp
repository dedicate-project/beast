#include <beast/pipeline_server.hpp>

// Standard
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

// Standard
#include <filesystem>
#include <fstream>
#include <set>

// Third-party
#include <nlohmann/json.hpp>

// Internal
#include <beast/pipes/evolution_pipe.hpp>
#include <beast/pipes/fan_pipe.hpp>
#include <beast/pipes/program_storage_sink_pipe.hpp>
#include <beast/pipes/program_storage_source_pipe.hpp>
#include <beast/pipes/results_summary_pipe.hpp>
#include <beast/pipes/score_graph_pipe.hpp>
#include <beast/program_c_codegen.hpp>
#include <beast/program_disassembler.hpp>
#include <beast/time_functions.hpp>
#include <beast/version.hpp>

namespace beast {

namespace {
std::string
timePointToIso8601(const std::chrono::time_point<std::chrono::system_clock>& timepoint) {
  // Convert to time_t.
  auto time_t_value = std::chrono::system_clock::to_time_t(timepoint);

  // Convert to tm structure.
  std::tm tm_value{};
  gmtime_r(&time_t_value, &tm_value); // Use gmtime_r for thread-safety.

  // Extract milliseconds.
  auto time_since_epoch = timepoint.time_since_epoch();
  auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(time_since_epoch) % 1000;

  // Format the time as an ISO 8601 string.
  std::ostringstream oss;
  oss << std::put_time(&tm_value, "%Y-%m-%dT%H:%M:%S");
  oss << '.' << std::setfill('0') << std::setw(3) << milliseconds.count() << "Z";
  return oss.str();
}
} // namespace

PipelineServer::PipelineServer(const std::string& storage_folder)
    : pipeline_manager_{storage_folder, 10, 250} {}

crow::json::wvalue PipelineServer::serveStatus() {
  crow::json::wvalue value;
  value["version"] = getVersionString();
  return value;
}

crow::json::wvalue PipelineServer::serveNewPipeline(const crow::request& req) {
  crow::json::wvalue value;
  if (const auto req_body = crow::json::load(req.body); req_body && req_body.has("name")) {
    const auto name = static_cast<std::string>(req_body["name"]);
    try {
      const auto pipeline_id = pipeline_manager_.createPipeline(name);
      value["status"] = "success";
      value["id"] = pipeline_id;
    } catch (const std::invalid_argument& exception) {
      value["status"] = "failed";
      value["error"] = exception.what();
    }
  } else {
    value["status"] = "failed";
    value["error"] = "Missing 'name' in request body";
  }
  return value;
}

crow::json::wvalue PipelineServer::servePipelineById(uint32_t pipeline_id) {
  crow::json::wvalue value;
  value["id"] = pipeline_id;
  try {
    const auto& descriptor = pipeline_manager_.getPipelineById(pipeline_id);
    value["state"] =
        descriptor.pipeline->isRunning() ? std::string("running") : std::string("stopped");
    value["name"] = descriptor.name;
    value["metadata"] = crow::json::load(descriptor.metadata.dump());
    value["model"] = crow::json::load(pipeline_manager_.getJsonForPipeline(pipeline_id).dump());

    value["status"] = "success";
  } catch (const std::invalid_argument& exception) {
    value["status"] = "failed";
    value["error"] = exception.what();
  }
  return value;
}

crow::json::wvalue PipelineServer::servePipelineAction(const crow::request& req,
                                                       uint32_t pipeline_id,
                                                       const std::string_view path) {
  crow::json::wvalue value;
  value["id"] = pipeline_id;
  try {
    auto& pipeline = pipeline_manager_.getPipelineById(pipeline_id);
    const bool running = pipeline.pipeline->isRunning();
    if (path == "start") {
      if (running) {
        value["status"] = "failed";
        value["error"] = "already_running";
      } else {
        try {
          pipeline.pipeline->start();
          value["status"] = "success";
        } catch (const std::invalid_argument& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        }
      }
    } else if (path == "stop") {
      if (running) {
        try {
          pipeline.pipeline->stop();
          value["status"] = "success";
        } catch (const std::invalid_argument& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        }
      } else {
        value["status"] = "failed";
        value["error"] = "not_running";
      }
    } else if (path == "update") {
      if (req.get_header_value("Content-Type") == "application/json") {
        try {
          const auto req_body = crow::json::load(req.body);
          const auto action = static_cast<std::string>(req_body["action"]);

          if (action == "change_name") {
            const auto new_name = static_cast<std::string>(req_body["name"]);
            pipeline_manager_.updatePipelineName(pipeline.id, new_name);
            value["status"] = "success";
          } else if (action == "move_pipe") {
            auto& descriptor = pipeline_manager_.getPipelineById(pipeline.id);
            const auto pipe_name = static_cast<std::string>(req_body["name"]);
            descriptor.metadata["pipes"][pipe_name]["position"]["x"] =
                static_cast<int32_t>(req_body["x"]);
            descriptor.metadata["pipes"][pipe_name]["position"]["y"] =
                static_cast<int32_t>(req_body["y"]);
            pipeline_manager_.savePipeline(pipeline.id);
            value["status"] = "success";
          } else if (action == "reset_summary") {
            // Targeted at ResultsSummaryPipe instances; lets the user clear the rolling
            // statistics window (e.g. after bumping the maze difficulty) so the displayed
            // numbers reflect the new regime without having to delete and re-create the
            // pipe. Works on a running pipeline -- the summary internals are safely
            // mutex-guarded and `resetSummary()` is cheap.
            try {
              if (!req_body.has("name")) {
                value["status"] = "failed";
                value["error"] = "name is required";
              } else {
                const auto pipe_name = static_cast<std::string>(req_body["name"]);
                auto& descriptor = pipeline_manager_.getPipelineById(pipeline.id);
                std::shared_ptr<ResultsSummaryPipe> target;
                for (const auto& managed : descriptor.pipeline->getPipes()) {
                  if (managed->name == pipe_name) {
                    target = std::dynamic_pointer_cast<ResultsSummaryPipe>(managed->pipe);
                    break;
                  }
                }
                if (!target) {
                  value["status"] = "failed";
                  value["error"] = "no such ResultsSummaryPipe";
                } else {
                  target->resetSummary();
                  value["status"] = "success";
                }
              }
            } catch (const std::invalid_argument& exception) {
              value["status"] = "failed";
              value["error"] = exception.what();
            }
          } else if (action == "add_pipe" || action == "delete_pipe" ||
                     action == "add_connection" || action == "delete_connection" ||
                     action == "update_pipe_parameters") {
            // Re-parse the raw body as nlohmann::json so the structural handlers can splice
            // nested objects (e.g. EvaluatorPipe's `parameters.evaluators` list) into the
            // pipeline-manager JSON without going through Crow's value plumbing.
            nlohmann::json nlohmann_body;
            try {
              nlohmann_body = nlohmann::json::parse(req.body);
            } catch (const nlohmann::detail::parse_error& exception) {
              value["status"] = "failed";
              value["error"] = exception.what();
              return value;
            }
            if (action == "add_pipe") {
              handleAddPipe(value, pipeline.id, nlohmann_body);
            } else if (action == "delete_pipe") {
              handleDeletePipe(value, pipeline.id, nlohmann_body);
            } else if (action == "add_connection") {
              handleAddConnection(value, pipeline.id, nlohmann_body);
            } else if (action == "delete_connection") {
              handleDeleteConnection(value, pipeline.id, nlohmann_body);
            } else {
              handleUpdatePipeParameters(value, pipeline.id, nlohmann_body);
            }
          } else {
            value["status"] = "failed";
            value["action"] = action;
            value["error"] = "invalid_action";
          }
        } catch (const nlohmann::detail::parse_error& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        } catch (const std::invalid_argument& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        } catch (const std::runtime_error& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        }
      } else {
        value["status"] = "failed";
        value["error"] = "invalid_request";
      }
    } else if (path == "metrics") {
      const auto& metrics = pipeline_manager_.getPipelineMetrics(pipeline.id);
      value["time"] = timePointToIso8601(metrics.measure_time_start);
      value["state"] =
          pipeline.pipeline->isRunning() ? std::string("running") : std::string("stopped");
      value["pipes"] = crow::json::wvalue::list();
      uint32_t idx = 0;
      // Index the live pipes so we can pull per-pipe extras (currently:
      // ResultsSummaryPipe statistics) without doing a linear scan per metrics entry.
      std::unordered_map<std::string, std::shared_ptr<Pipe>> pipes_by_name;
      for (const auto& managed : pipeline.pipeline->getPipes()) {
        pipes_by_name.emplace(managed->name, managed->pipe);
      }
      for (const auto& pipe_pair : metrics.pipes) {
        crow::json::wvalue pipe_item;
        pipe_item["name"] = pipe_pair.first;
        pipe_item["execution_count"] = pipe_pair.second.execution_count;
        pipe_item["inputs"] = crow::json::wvalue::list();
        for (const auto& input_pair : pipe_pair.second.inputs_received) {
          pipe_item["inputs"][input_pair.first] = input_pair.second;
        }
        pipe_item["outputs"] = crow::json::wvalue::list();
        for (const auto& output_pair : pipe_pair.second.outputs_sent) {
          pipe_item["outputs"][output_pair.first] = output_pair.second;
        }
        // ResultsSummaryPipe surfaces score statistics here so the UI can render them
        // alongside the throughput numbers. Other pipe types don't populate the field.
        const auto pipe_iter = pipes_by_name.find(pipe_pair.first);
        if (pipe_iter != pipes_by_name.end()) {
          // Per-slot fill state: the UI renders this as a coloured indicator next to
          // each port so the user can see at a glance which ports are saturated /
          // back-pressured. Reported as both the absolute count and the per-slot
          // capacity so the client can render however it likes (percentage, raw, ...).
          // The capacity is the pipe's max_candidates -- the same threshold every
          // pipe's input/output slot uses for saturation decisions.
          const auto& pipe_ptr = pipe_iter->second;
          const uint32_t capacity = pipe_ptr->getMaxCandidates();
          pipe_item["max_candidates"] = capacity;
          {
            crow::json::wvalue input_fills = crow::json::wvalue::list();
            const uint32_t input_slot_count = pipe_ptr->getInputSlotCount();
            for (uint32_t slot = 0; slot < input_slot_count; ++slot) {
              input_fills[slot] = pipe_ptr->getInputSlotAmount(slot);
            }
            pipe_item["input_fills"] = std::move(input_fills);
          }
          {
            crow::json::wvalue output_fills = crow::json::wvalue::list();
            const uint32_t output_slot_count = pipe_ptr->getOutputSlotCount();
            for (uint32_t slot = 0; slot < output_slot_count; ++slot) {
              output_fills[slot] = pipe_ptr->getOutputSlotAmount(slot);
            }
            pipe_item["output_fills"] = std::move(output_fills);
          }
          // EvolutionPipe (and its subclass EvaluatorPipe) surfaces cycle progress so
          // the UI can render a progress bar under each evolution-stage block. The
          // bursts-then-silence pattern of long evolve() cycles makes the rest of the
          // UI look idle; the progress bar gives the user a "this pipe IS working,
          // here's how far along it is" signal that would otherwise have to be
          // inferred from rate counters.
          if (const auto evolution_pipe =
                  std::dynamic_pointer_cast<EvolutionPipe>(pipe_ptr)) {
            const auto prog = evolution_pipe->getProgress();
            crow::json::wvalue progress_json;
            progress_json["cycle_index"] = prog.cycle_index;
            progress_json["currently_running"] = prog.currently_running;
            progress_json["evaluations_this_cycle"] = prog.evaluations_this_cycle;
            progress_json["expected_evaluations_this_cycle"] =
                prog.expected_evaluations_this_cycle;
            progress_json["seconds_in_cycle"] = prog.seconds_in_cycle;
            progress_json["last_cycle_seconds"] = prog.last_cycle_seconds;
            progress_json["last_cycle_best_score"] = prog.last_cycle_best_score;
            pipe_item["evolution_progress"] = std::move(progress_json);
          }
          if (const auto summary_pipe =
                  std::dynamic_pointer_cast<ResultsSummaryPipe>(pipe_ptr)) {
            const auto summary = summary_pipe->getSummary();
            crow::json::wvalue summary_json;
            summary_json["count_total"] = summary.count_total;
            summary_json["count_window"] = summary.count_window;
            summary_json["min_score"] = summary.min_score;
            summary_json["max_score"] = summary.max_score;
            summary_json["mean_score"] = summary.mean_score;
            summary_json["last_score"] = summary.last_score;
            summary_json["best_ever_score"] = summary.best_ever_score;
            summary_json["best_ever_size"] =
                static_cast<uint64_t>(summary.best_ever_data.size());
            summary_json["window_size"] = summary_pipe->getWindowSize();
            pipe_item["summary"] = std::move(summary_json);
          }
          if (const auto fan_pipe = std::dynamic_pointer_cast<FanPipe>(pipe_ptr)) {
            const auto throughput = fan_pipe->getThroughput();
            crow::json::wvalue throughput_json;
            throughput_json["total_seen"] = throughput.total_seen;
            throughput_json["window_seen"] = throughput.window_seen;
            throughput_json["window_seconds"] = throughput.window_seconds;
            throughput_json["candidates_per_second"] = throughput.candidates_per_second;
            pipe_item["throughput"] = std::move(throughput_json);
          }
          // ScoreGraphPipe surfaces the rolling time-series here. We materialise the
          // samples list inline so the UI can render the sparkline without a follow-up
          // request; the per-poll JSON payload grows by ~24 bytes per sample (timestamp +
          // score + JSON brackets), which at the default 1024-sample cap is ~24 KB per
          // ScoreGraph pipe per poll. That's small enough to live in the same payload as
          // the other metrics; if it ever becomes a hotspot the natural fix is to gate
          // samples behind a `?include_score_graph_samples=1` query parameter.
          if (const auto graph_pipe =
                  std::dynamic_pointer_cast<ScoreGraphPipe>(pipe_ptr)) {
            const auto snap = graph_pipe->getSnapshot();
            crow::json::wvalue graph_json;
            graph_json["window_seconds"] = snap.window_seconds;
            graph_json["total_seen"] = snap.total_seen;
            graph_json["min_score"] = snap.min_score;
            graph_json["max_score"] = snap.max_score;
            graph_json["mean_score"] = snap.mean_score;
            graph_json["last_score"] = snap.last_score;
            for (size_t si = 0; si < snap.samples.size(); ++si) {
              crow::json::wvalue sample_json;
              sample_json["t"] = snap.samples[si].t_seconds;
              sample_json["score"] = snap.samples[si].score;
              graph_json["samples"][si] = std::move(sample_json);
            }
            pipe_item["score_graph"] = std::move(graph_json);
          }
        }
        value["pipes"][idx] = std::move(pipe_item);
        idx++;
      }
      value["status"] = "success";
    } else if (path == "delete") {
      if (pipeline.pipeline->isRunning()) {
        pipeline.pipeline->stop();
      }
      pipeline_manager_.deletePipeline(pipeline.id);
      value["status"] = "success";
    } else {
      value["status"] = "failed";
      value["error"] = "invalid_command";
      value["command"] = std::string(path);
    }
  } catch (const std::invalid_argument& exception) {
    value["status"] = "failed";
    value["error"] = exception.what();
  }
  return value;
}

namespace {
// Pull a nested `parameters` object out of a request body. Returns an empty object when
// missing so callers can treat "no parameters" identically to "empty parameters".
nlohmann::json extractParameters(const nlohmann::json& req_body) {
  if (!req_body.contains("parameters")) {
    return nlohmann::json::object();
  }
  return req_body["parameters"];
}

// Populate value["status"]/value["error"] from a thrown exception, mapping the most common
// invalid_argument failure modes (e.g. "Cannot mutate a running pipeline") to stable error
// codes the frontend can pattern-match against without parsing free-form text.
void reportFailure(crow::json::wvalue& value, const std::string& message) {
  value["status"] = "failed";
  if (message.find("running pipeline") != std::string::npos) {
    value["error"] = std::string("pipeline_running");
  } else {
    value["error"] = message;
  }
}
} // namespace

void PipelineServer::handleAddPipe(crow::json::wvalue& value, uint32_t pipeline_id,
                                   const nlohmann::json& req_body) {
  try {
    if (!req_body.contains("name") || !req_body.contains("type")) {
      reportFailure(value, "name and type are required");
      return;
    }
    const auto pipe_name = req_body["name"].get<std::string>();
    const auto pipe_type = req_body["type"].get<std::string>();
    nlohmann::json parameters = extractParameters(req_body);

    // Optional position; default to (0, 0) so new pipes always show up somewhere visible
    // even if the client didn't bother to suggest a spot.
    int32_t pos_x = 0;
    int32_t pos_y = 0;
    if (req_body.contains("position")) {
      const auto& pos = req_body["position"];
      if (pos.contains("x")) {
        pos_x = pos["x"].get<int32_t>();
      }
      if (pos.contains("y")) {
        pos_y = pos["y"].get<int32_t>();
      }
    }

    pipeline_manager_.mutatePipeline(
        pipeline_id,
        [&pipe_name, &pipe_type, &parameters, pos_x, pos_y](nlohmann::json& model,
                                                            nlohmann::json& metadata) {
          if (!model.contains("pipes") || !model["pipes"].is_object()) {
            model["pipes"] = nlohmann::json::object();
          }
          if (model["pipes"].contains(pipe_name)) {
            throw std::invalid_argument("Pipe '" + pipe_name + "' already exists");
          }
          nlohmann::json pipe_json;
          pipe_json["type"] = pipe_type;
          pipe_json["parameters"] = parameters;
          model["pipes"][pipe_name] = std::move(pipe_json);

          if (!metadata.contains("pipes") || !metadata["pipes"].is_object()) {
            metadata["pipes"] = nlohmann::json::object();
          }
          metadata["pipes"][pipe_name]["position"]["x"] = pos_x;
          metadata["pipes"][pipe_name]["position"]["y"] = pos_y;
        });
    value["status"] = "success";
  } catch (const std::exception& ex) {
    reportFailure(value, ex.what());
  }
}

void PipelineServer::handleDeletePipe(crow::json::wvalue& value, uint32_t pipeline_id,
                                      const nlohmann::json& req_body) {
  try {
    if (!req_body.contains("name")) {
      reportFailure(value, "name is required");
      return;
    }
    const auto pipe_name = req_body["name"].get<std::string>();

    pipeline_manager_.mutatePipeline(
        pipeline_id, [&pipe_name](nlohmann::json& model, nlohmann::json& metadata) {
          if (!model.contains("pipes") || !model["pipes"].contains(pipe_name)) {
            throw std::invalid_argument("Pipe '" + pipe_name + "' does not exist");
          }
          model["pipes"].erase(pipe_name);

          // Also rip out any connections that reference the deleted pipe -- otherwise the
          // rebuild would throw on the dangling connection.
          if (model.contains("connections") && model["connections"].is_array()) {
            auto& connections = model["connections"];
            connections.erase(std::remove_if(connections.begin(), connections.end(),
                                             [&pipe_name](const nlohmann::json& conn) {
                                               return conn.value("source_pipe", "") == pipe_name ||
                                                      conn.value("destination_pipe", "") ==
                                                          pipe_name;
                                             }),
                              connections.end());
          }

          // Finally drop the cosmetic metadata so deleted pipes don't accumulate ghost
          // position entries on disk.
          if (metadata.contains("pipes") && metadata["pipes"].is_object() &&
              metadata["pipes"].contains(pipe_name)) {
            metadata["pipes"].erase(pipe_name);
          }
        });
    value["status"] = "success";
  } catch (const std::exception& ex) {
    reportFailure(value, ex.what());
  }
}

void PipelineServer::handleAddConnection(crow::json::wvalue& value, uint32_t pipeline_id,
                                         const nlohmann::json& req_body) {
  try {
    for (const char* field :
         {"source_pipe", "source_slot", "destination_pipe", "destination_slot"}) {
      if (!req_body.contains(field)) {
        reportFailure(value, std::string(field) + " is required");
        return;
      }
    }
    nlohmann::json connection;
    connection["source_pipe"] = req_body["source_pipe"].get<std::string>();
    connection["source_slot"] = req_body["source_slot"].get<uint32_t>();
    connection["destination_pipe"] = req_body["destination_pipe"].get<std::string>();
    connection["destination_slot"] = req_body["destination_slot"].get<uint32_t>();
    // buffer_size is optional; the Pipeline core will refuse a 0-sized buffer, so default
    // to a reasonable in-between size that's also what the example pipelines use.
    connection["buffer_size"] =
        req_body.contains("buffer_size") ? req_body["buffer_size"].get<uint32_t>() : 100U;

    pipeline_manager_.mutatePipeline(
        pipeline_id, [&connection](nlohmann::json& model, nlohmann::json& /*metadata*/) {
          if (!model.contains("connections") || !model["connections"].is_array()) {
            model["connections"] = nlohmann::json::array();
          }
          model["connections"].push_back(connection);
        });
    value["status"] = "success";
  } catch (const std::exception& ex) {
    reportFailure(value, ex.what());
  }
}

void PipelineServer::handleDeleteConnection(crow::json::wvalue& value, uint32_t pipeline_id,
                                            const nlohmann::json& req_body) {
  try {
    for (const char* field :
         {"source_pipe", "source_slot", "destination_pipe", "destination_slot"}) {
      if (!req_body.contains(field)) {
        reportFailure(value, std::string(field) + " is required");
        return;
      }
    }
    const auto source_pipe = req_body["source_pipe"].get<std::string>();
    const auto source_slot = req_body["source_slot"].get<uint32_t>();
    const auto destination_pipe = req_body["destination_pipe"].get<std::string>();
    const auto destination_slot = req_body["destination_slot"].get<uint32_t>();

    pipeline_manager_.mutatePipeline(
        pipeline_id,
        [&](nlohmann::json& model, nlohmann::json& /*metadata*/) {
          if (!model.contains("connections") || !model["connections"].is_array()) {
            throw std::invalid_argument("Connection not found");
          }
          auto& connections = model["connections"];
          const auto before = connections.size();
          connections.erase(std::remove_if(connections.begin(), connections.end(),
                                           [&](const nlohmann::json& conn) {
                                             return conn.value("source_pipe", "") == source_pipe &&
                                                    conn.value("source_slot", 0U) == source_slot &&
                                                    conn.value("destination_pipe", "") ==
                                                        destination_pipe &&
                                                    conn.value("destination_slot", 0U) ==
                                                        destination_slot;
                                           }),
                            connections.end());
          if (connections.size() == before) {
            throw std::invalid_argument("Connection not found");
          }
        });
    value["status"] = "success";
  } catch (const std::exception& ex) {
    reportFailure(value, ex.what());
  }
}

void PipelineServer::handleUpdatePipeParameters(crow::json::wvalue& value, uint32_t pipeline_id,
                                                const nlohmann::json& req_body) {
  try {
    if (!req_body.contains("name")) {
      reportFailure(value, "name is required");
      return;
    }
    const auto pipe_name = req_body["name"].get<std::string>();
    nlohmann::json parameters = extractParameters(req_body);

    pipeline_manager_.mutatePipeline(
        pipeline_id, [&pipe_name, &parameters](nlohmann::json& model,
                                                nlohmann::json& /*metadata*/) {
          if (!model.contains("pipes") || !model["pipes"].contains(pipe_name)) {
            throw std::invalid_argument("Pipe '" + pipe_name + "' does not exist");
          }
          model["pipes"][pipe_name]["parameters"] = parameters;
        });
    value["status"] = "success";
  } catch (const std::exception& ex) {
    reportFailure(value, ex.what());
  }
}

crow::json::wvalue PipelineServer::serveAllPipelines() const {
  crow::json::wvalue value = crow::json::wvalue::list();
  uint32_t idx = 0;
  for (const auto& pipeline : pipeline_manager_.getPipelines()) {
    crow::json::wvalue pipeline_item;
    pipeline_item["id"] = pipeline.id;
    pipeline_item["name"] = pipeline.name;
    pipeline_item["state"] =
        pipeline.pipeline->isRunning() ? std::string("running") : std::string("stopped");
    value[idx] = std::move(pipeline_item);
    idx++;
  }
  return value;
}

namespace {

/// One reference to a ledger from a single pipe in a single pipeline. The UI uses this
/// to show "where is this file being read/written" without the user having to crawl
/// through every pipeline.
struct LedgerReference {
  uint32_t pipeline_id;
  std::string pipeline_name;
  std::string pipe_name;
  std::string role; // "sink" or "source"
};

/// Aggregate of every reference to a particular ledger path. Built once per
/// `/api/v1/program-collection` request by walking every pipeline.
struct LedgerInfo {
  std::vector<LedgerReference> references;
};

/// Load a ledger JSON file from disk and return the parsed entries, sorted by
/// descending score. Tolerant: missing/unreadable/malformed files return empty.
std::vector<Pipe::OutputItem> loadLedgerFromDisk(const std::string& path) {
  std::vector<Pipe::OutputItem> entries;
  if (path.empty()) {
    return entries;
  }
  std::ifstream in(path);
  if (!in) {
    return entries;
  }
  try {
    nlohmann::json doc;
    in >> doc;
    if (!doc.is_array()) {
      return entries;
    }
    for (const auto& entry : doc) {
      if (!entry.contains("data") || !entry.contains("score")) {
        continue;
      }
      Pipe::OutputItem item;
      item.score = entry["score"].get<double>();
      item.data = entry["data"].get<std::vector<unsigned char>>();
      entries.push_back(std::move(item));
    }
  } catch (...) {
    // Same best-effort behaviour as ProgramStorageSinkPipe: a malformed ledger isn't
    // worth surfacing as an error -- the UI just shows an empty entry list and the
    // user can investigate manually if they care.
    return {};
  }
  std::sort(entries.begin(), entries.end(),
            [](const Pipe::OutputItem& a, const Pipe::OutputItem& b) { return a.score > b.score; });
  return entries;
}

} // namespace

crow::json::wvalue PipelineServer::serveProgramCollection() const {
  // Build an ordered map of ledger path -> references so the JSON output is stable
  // across requests (helps the UI render without rearranging rows).
  std::map<std::string, LedgerInfo> ledgers;
  for (const auto& managed : pipeline_manager_.getPipelines()) {
    for (const auto& pipe : managed.pipeline->getPipes()) {
      if (!pipe || !pipe->pipe) {
        continue;
      }
      std::string path;
      std::string role;
      if (const auto sink = std::dynamic_pointer_cast<ProgramStorageSinkPipe>(pipe->pipe)) {
        path = sink->getPath();
        role = "sink";
      } else if (const auto source =
                     std::dynamic_pointer_cast<ProgramStorageSourcePipe>(pipe->pipe)) {
        path = source->getPath();
        role = "source";
      }
      if (path.empty()) {
        continue;
      }
      ledgers[path].references.push_back(
          LedgerReference{managed.id, managed.name, pipe->name, role});
    }
  }

  crow::json::wvalue value;
  value["status"] = "success";
  value["ledgers"] = crow::json::wvalue::list();
  size_t li = 0;
  for (const auto& [path, info] : ledgers) {
    crow::json::wvalue entry;
    entry["path"] = path;
    // File-system metadata is read fresh each time so the UI shows up-to-date
    // existence / size information; cheap because there's usually only a handful of
    // ledgers per session.
    std::error_code ec;
    const auto fs_path = std::filesystem::path(path);
    const bool exists = std::filesystem::exists(fs_path, ec);
    entry["exists"] = exists;
    if (exists) {
      const auto size = std::filesystem::file_size(fs_path, ec);
      entry["size_bytes"] = static_cast<uint64_t>(ec ? 0 : size);
    } else {
      entry["size_bytes"] = static_cast<uint64_t>(0);
    }
    // Quick program count via loading + counting. Cheap for typical top-K = 10
    // ledgers; if a future workflow ships ledgers with thousands of entries we'd want
    // to swap this for a streaming line-counter, but not yet.
    entry["program_count"] = static_cast<uint64_t>(loadLedgerFromDisk(path).size());

    entry["references"] = crow::json::wvalue::list();
    for (size_t ri = 0; ri < info.references.size(); ++ri) {
      crow::json::wvalue ref;
      ref["pipeline_id"] = info.references[ri].pipeline_id;
      ref["pipeline_name"] = info.references[ri].pipeline_name;
      ref["pipe_name"] = info.references[ri].pipe_name;
      ref["role"] = info.references[ri].role;
      entry["references"][ri] = std::move(ref);
    }

    value["ledgers"][li] = std::move(entry);
    ++li;
  }
  return value;
}

crow::json::wvalue PipelineServer::serveLedger(const crow::request& req) const {
  crow::json::wvalue value;
  auto* const path_param = req.url_params.get("path");
  if (path_param == nullptr) {
    value["status"] = "failed";
    value["error"] = "missing 'path' query parameter";
    return value;
  }
  const std::string path(path_param);

  // Validate the requested path against the known set: anything else is rejected so
  // this endpoint can't be turned into a generic file-read primitive. We rebuild the
  // set on each call (cheap, small) instead of caching it, which would need
  // invalidation any time a pipeline changes its storage path.
  std::set<std::string> known_paths;
  for (const auto& managed : pipeline_manager_.getPipelines()) {
    for (const auto& pipe : managed.pipeline->getPipes()) {
      if (!pipe || !pipe->pipe) {
        continue;
      }
      if (const auto sink = std::dynamic_pointer_cast<ProgramStorageSinkPipe>(pipe->pipe)) {
        if (!sink->getPath().empty()) {
          known_paths.insert(sink->getPath());
        }
      } else if (const auto src =
                     std::dynamic_pointer_cast<ProgramStorageSourcePipe>(pipe->pipe)) {
        if (!src->getPath().empty()) {
          known_paths.insert(src->getPath());
        }
      }
    }
  }
  if (known_paths.find(path) == known_paths.end()) {
    value["status"] = "failed";
    value["error"] = "path is not referenced by any current pipeline";
    return value;
  }

  const auto entries = loadLedgerFromDisk(path);
  value["status"] = "success";
  value["path"] = path;
  value["programs"] = crow::json::wvalue::list();
  for (size_t i = 0; i < entries.size(); ++i) {
    crow::json::wvalue program_json;
    program_json["score"] = entries[i].score;
    program_json["size"] = static_cast<uint64_t>(entries[i].data.size());
    // Embed the raw bytes as an array of unsigned 8-bit integers. The UI uses this to
    // POST back when requesting the C-code conversion -- saves round-tripping through
    // a file path or hex-encoded string.
    program_json["data"] = crow::json::wvalue::list();
    for (size_t b = 0; b < entries[i].data.size(); ++b) {
      program_json["data"][b] = static_cast<uint64_t>(entries[i].data[b]);
    }

    // Inline disassembly so the UI can show the bytecode view immediately on expand,
    // no extra round-trip needed. Small relative to the bytes themselves.
    const auto disasm = ProgramDisassembler::disassemble(entries[i].data);
    program_json["disassembly_clean"] = disasm.clean;
    program_json["disassembly_trailing_garbage_bytes"] = disasm.trailing_garbage_bytes;
    program_json["disassembly"] = crow::json::wvalue::list();
    for (size_t di = 0; di < disasm.instructions.size(); ++di) {
      const auto& inst = disasm.instructions[di];
      crow::json::wvalue inst_json;
      inst_json["offset"] = inst.offset;
      inst_json["length"] = inst.length;
      inst_json["mnemonic"] = inst.mnemonic;
      inst_json["text"] = inst.text;
      inst_json["bytes_hex"] = inst.bytes_hex;
      program_json["disassembly"][di] = std::move(inst_json);
    }
    value["programs"][i] = std::move(program_json);
  }
  return value;
}

crow::json::wvalue PipelineServer::serveCCodeForProgram(const crow::request& req) {
  crow::json::wvalue value;
  std::vector<unsigned char> bytes;
  std::string source;
  try {
    const auto body = nlohmann::json::parse(req.body);
    if (!body.contains("data") || !body["data"].is_array()) {
      value["status"] = "failed";
      value["error"] = "missing or non-array 'data' field";
      return value;
    }
    for (const auto& byte_json : body["data"]) {
      // Accept both unsigned (0..255) and signed (-128..127) byte representations
      // since the ledger JSON serialises as int. We normalise into uint8 here.
      const auto raw = byte_json.get<int64_t>();
      if (raw < -128 || raw > 255) {
        value["status"] = "failed";
        value["error"] = "byte value out of range";
        return value;
      }
      bytes.push_back(static_cast<unsigned char>(raw & 0xFF));
    }
    if (body.contains("source") && body["source"].is_string()) {
      source = body["source"].get<std::string>();
    }
  } catch (const std::exception& ex) {
    value["status"] = "failed";
    value["error"] = std::string("invalid request body: ") + ex.what();
    return value;
  }
  ProgramCCodeGenerator::Options opts;
  opts.source_description = source;
  value["status"] = "success";
  value["c_code"] = ProgramCCodeGenerator::generate(bytes, opts);
  return value;
}

} // namespace beast
