#ifndef BEAST_PIPELINE_SERVER_HPP_
#define BEAST_PIPELINE_SERVER_HPP_

// Crow
#define CROW_DISABLE_STATIC_DIR
#include <crow.h>

// BEAST
#include <beast/pipeline_manager.hpp>

namespace beast {

/**
 * @class PipelineServer
 * @brief Serves stored pipelines and allows maintaining them
 */
class PipelineServer {
 public:
  explicit PipelineServer(const std::string& storage_folder);

  /**
   * Serve a JSON response containing the version of the application.
   *
   * @return JSON response containing version information.
   */
  [[nodiscard]] static crow::json::wvalue serveStatus();

  /**
   * Serve a JSON response for creating a new pipeline.
   *
   * @param req Request object.
   * @param pipeline_manager Pointer to the PipelineManager instance.
   * @return JSON response containing pipeline creation status.
   */
  [[nodiscard]] crow::json::wvalue serveNewPipeline(const crow::request& req);

  /**
   * Serve a JSON response for getting pipeline status by ID.
   *
   * @param pipeline_manager Pointer to the PipelineManager instance.
   * @param pipeline_id ID of the pipeline.
   * @return JSON response containing pipeline status.
   */
  [[nodiscard]] crow::json::wvalue servePipelineById(uint32_t pipeline_id);

  /**
   * Serve a JSON response for handling pipeline actions.
   *
   * Supported JSON request actions:
   *  - "start": start the pipeline. Returns "already_running" error if pipeline is already running.
   *  - "stop": stop the pipeline. Returns "not_running" error if pipeline is not running.
   *  - "update": update the pipeline. Expects a JSON payload with "action" and "name" fields.
   *    Supported update actions:
   *     - "change_name": changes the name of the pipeline.
   *  - "delete": delete the pipeline.
   *
   * @param req Request object.
   * @param pipeline_manager Pointer to the PipelineManager instance.
   * @param pipeline_id ID of the pipeline.
   * @param path Path of the action.
   * @return JSON response containing pipeline action status.
   */
  [[nodiscard]] crow::json::wvalue
  servePipelineAction(const crow::request& req, uint32_t pipeline_id, const std::string_view path);

  /**
   * Serve a JSON response containing all pipelines and their status.
   *
   * @param pipeline_manager Pointer to the PipelineManager instance.
   * @return JSON response containing all pipeline status.
   */
  [[nodiscard]] crow::json::wvalue serveAllPipelines() const;

 private:
  beast::PipelineManager pipeline_manager_;
};

} // namespace beast

#endif // BEAST_PIPELINE_SERVER_HPP_
