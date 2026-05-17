#include <beast/pipeline_server.hpp>

// Standard
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

// Internal
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

} // namespace beast
