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
   * Supported top-level paths:
   *  - "start": start the pipeline. Returns "already_running" if it is.
   *  - "stop": stop the pipeline. Returns "not_running" if it isn't.
   *  - "delete": delete the pipeline.
   *  - "metrics": return the most recent metrics window.
   *  - "update": expects a JSON payload with an "action" field. Supported update actions:
   *     - "change_name":              `{ "name": ... }`
   *     - "move_pipe":                `{ "name": ..., "x": ..., "y": ... }`
   *     - "add_pipe":                 `{ "name": ..., "type": ..., "parameters": {...},
   *                                     "position": { "x": ..., "y": ... } }`
   *     - "delete_pipe":              `{ "name": ... }`
   *     - "add_connection":           `{ "source_pipe": ..., "source_slot": ...,
   *                                     "destination_pipe": ..., "destination_slot": ...,
   *                                     "buffer_size": ... }`
   *     - "delete_connection":        `{ "source_pipe": ..., "source_slot": ...,
   *                                     "destination_pipe": ..., "destination_slot": ... }`
   *     - "update_pipe_parameters":   `{ "name": ..., "parameters": {...} }`
   *
   * All structural mutations (add_pipe, delete_pipe, add_connection, delete_connection,
   * update_pipe_parameters) require the pipeline to be stopped; the server returns
   * `pipeline_running` on attempts to mutate a running pipeline.
   *
   * @param req Request object.
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

  /**
   * @brief Serve the discovered set of program ledgers referenced by storage pipes
   *
   * Scans every loaded pipeline for `ProgramStorageSinkPipe` / `ProgramStorageSourcePipe`
   * instances, collects the union of their `path` parameters (deduplicated), and for
   * each path reports which pipelines/pipes reference it along with whether the file
   * currently exists and how many entries it contains.
   *
   * Used by the Program Collection page in the UI to populate the per-ledger sections
   * without needing the user to know which pipelines persist where.
   */
  [[nodiscard]] crow::json::wvalue serveProgramCollection() const;

  /**
   * @brief Serve the parsed contents (programs) of a single ledger by path
   *
   * Reads the ledger JSON from disk (best effort -- returns an empty list if the file
   * is missing/unreadable), and for each program disassembles it inline so the UI
   * can render the bytecode view without an extra round-trip per program.
   *
   * The `path` query parameter MUST exactly match one of the paths returned by
   * `serveProgramCollection`; we deliberately don't allow reading arbitrary files
   * (the UI presents a fixed list, not a free-form picker), which keeps this endpoint
   * from being a generic file-read primitive.
   *
   * @param req Crow request; the `path` is taken from the `path` query parameter.
   */
  [[nodiscard]] crow::json::wvalue serveLedger(const crow::request& req) const;

  /**
   * @brief Generate self-contained C source for a program byte sequence
   *
   * POST body shape: `{ "data": [int byte values 0-255 OR -128..127], "source": "..." }`.
   * The `source` field is an optional human-readable description that's threaded into
   * the generated file's top-comment banner (e.g. "from /tmp/foo.json, score 0.886").
   *
   * Returns: `{ "status": "success", "c_code": "..." }`. The C code is a complete C
   * file that compiles with `cc program.c -o program` and runs as a standalone CLI
   * tool; see `ProgramCCodeGenerator` for the runtime details.
   */
  [[nodiscard]] static crow::json::wvalue serveCCodeForProgram(const crow::request& req);

 private:
  // Update-action handlers. All of them write into the supplied `value` (status/error/etc.)
  // and translate exceptions thrown by the pipeline manager into structured failure
  // responses. Kept private because they're only ever called from `servePipelineAction`.
  // They take an nlohmann::json body (already parsed once at the dispatch site) instead of
  // Crow's rvalue so they can splice fragments straight into the pipeline-manager JSON
  // without re-quoting through Crow's type system.
  void handleAddPipe(crow::json::wvalue& value, uint32_t pipeline_id,
                     const nlohmann::json& req_body);
  void handleDeletePipe(crow::json::wvalue& value, uint32_t pipeline_id,
                        const nlohmann::json& req_body);
  void handleAddConnection(crow::json::wvalue& value, uint32_t pipeline_id,
                           const nlohmann::json& req_body);
  void handleDeleteConnection(crow::json::wvalue& value, uint32_t pipeline_id,
                              const nlohmann::json& req_body);
  void handleUpdatePipeParameters(crow::json::wvalue& value, uint32_t pipeline_id,
                                  const nlohmann::json& req_body);

  beast::PipelineManager pipeline_manager_;
};

} // namespace beast

#endif // BEAST_PIPELINE_SERVER_HPP_
