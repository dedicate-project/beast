#include <beast/pipeline_server.hpp>

// Internal
#include <beast/version.hpp>

namespace beast {

PipelineServer::PipelineServer(const std::string& storage_folder)
    : pipeline_manager_{storage_folder} {}

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
    } catch (const std::runtime_error& exception) {
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
        descriptor.pipeline.isRunning() ? std::string("running") : std::string("stopped");
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
    const bool running = pipeline.pipeline.isRunning();
    if (path == "start") {
      if (running) {
        value["status"] = "failed";
        value["error"] = "already_running";
      } else {
        try {
          pipeline.pipeline.start();
          value["status"] = "success";
        } catch (const std::runtime_error& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        }
      }
    } else if (path == "stop") {
      if (running) {
        try {
          pipeline.pipeline.stop();
          value["status"] = "success";
        } catch (const std::runtime_error& exception) {
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
          } else {
            value["status"] = "failed";
            value["action"] = action;
            value["error"] = "invalid_action";
          }
        } catch (const std::runtime_error& exception) {
          value["status"] = "failed";
          value["error"] = exception.what();
        }
      } else {
        value["status"] = "failed";
        value["error"] = "invalid_request";
      }
    } else if (path == "delete") {
      if (pipeline.pipeline.isRunning()) {
        pipeline.pipeline.stop();
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

crow::json::wvalue PipelineServer::serveAllPipelines() const {
  crow::json::wvalue value = crow::json::wvalue::list();
  uint32_t idx = 0;
  for (const auto& pipeline : pipeline_manager_.getPipelines()) {
    crow::json::wvalue pipeline_item;
    pipeline_item["id"] = pipeline.id;
    pipeline_item["name"] = pipeline.name;
    pipeline_item["state"] =
        pipeline.pipeline.isRunning() ? std::string("running") : std::string("stopped");
    value[idx] = std::move(pipeline_item);
    idx++;
  }
  return value;
}

} // namespace beast
