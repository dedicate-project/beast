#ifndef BEAST_FILESYSTEM_HELPER_HPP_
#define BEAST_FILESYSTEM_HELPER_HPP_

// Standard
#include <filesystem>
#include <string>

// nlohmann
#include <nlohmann/json.hpp>

namespace beast {

/**
 * @brief A helper class for interacting with the filesystem.
 */
class FilesystemHelper {
 public:
  /**
   * @brief Constructs a FilesystemHelper with the specified model path.
   *
   * @param model_path The path to the model directory.
   * @throws std::runtime_error If the model path cannot be created or accessed.
   */
  explicit FilesystemHelper(const std::string& model_path);

  /**
   * @brief Saves a model to the model path with a unique filename based on the model identifier.
   *
   * @param model_identifier The identifier for the model.
   * @param model The model to save.
   * @return The filename used to save the model.
   */
  std::string saveModel(const std::string& model_identifier, const nlohmann::json& model) const;

  void updateModel(const std::string& filename, const std::string& model_identifier,
                   const nlohmann::json& model, const nlohmann::json& metadata) const;

  /**
   * @brief Loads all JSON files from the model path and returns their contents.
   *
   * @return A vector of JSON objects containing the filename and content of each model.
   */
  std::vector<nlohmann::json> loadModels() const;

  /**
   * @brief Deletes the model with the specified filename.
   *
   * @param filename The filename of the model to delete.
   * @throws std::runtime_error If the specified file does not exist.
   */
  void deleteModel(const std::string& filename) const;

  /**
   * @brief Checks if a model file exists with the specified filename.
   *
   * @param filename The filename to check.
   * @return true if the model file exists, false otherwise.
   */
  bool modelExists(const std::string& filename) const;

 private:
  std::filesystem::path m_model_path;

  /**
   * @brief Cleans the given filename by replacing non-alphanumeric characters with underscores.
   *
   * Consecutive underscores are removed, and underscores at the beginning or end of the filename
   * are removed.
   *
   * @param filename The filename to clean.
   * @return The cleaned filename.
   */
  static std::string cleanFilename(const std::string& filename);

  /**
   * @brief Gets a unique filename based on the given filename, by appending a counter if necessary.
   *
   * @param original_filename The original filename to use as a base.
   * @return A unique filename based on the original filename.
   */
  std::string getUniqueFilename(const std::string& original_filename) const;
};

} // namespace beast

#endif // BEAST_FILESYSTEM_HELPER_HPP_
