#include <beast/filesystem_helper.hpp>

// Standard
#include <fstream>
#include <stdexcept>

namespace beast {

FilesystemHelper::FilesystemHelper(const std::string& model_path) {
  m_model_path = std::filesystem::absolute(model_path);

  if (!std::filesystem::is_directory(m_model_path)) {
    if (!std::filesystem::create_directory(m_model_path)) {
      throw std::runtime_error("Could not create model directory.");
    }
  }
}

std::string FilesystemHelper::saveModel(
    const std::string& model_identifier, const nlohmann::json& model) {
  std::string filename = cleanFilename(model_identifier) + ".json";
  std::string filepath = (m_model_path / filename).string();

  std::ofstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file for writing.");
  }

  nlohmann::json wrapper;
  wrapper["name"] = model_identifier;
  wrapper["model"] = model;

  file << wrapper.dump(4);
  file.close();

  return filename;
}

std::string FilesystemHelper::cleanFilename(const std::string& filename) {
  std::string result;
  bool last_char_was_underscore = false;
  for (char c : filename) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      result += c;
      last_char_was_underscore = false;
    } else if (!last_char_was_underscore && !result.empty()) {
      result += "_";
      last_char_was_underscore = true;
    }
  }
  if (result.back() == '_') {
    result.pop_back();
  }
  return result;
}

std::string FilesystemHelper::getUniqueFilename(const std::string& original_filename) const {
  int counter = 0;
  std::string cleaned_filename = cleanFilename(original_filename);
  std::string unique_filename = cleaned_filename + ".json";
  while (std::filesystem::exists(m_model_path / unique_filename)) {
    counter++;
    unique_filename = cleaned_filename + "_" + std::to_string(counter) + ".json";
  }
  return unique_filename;
}

std::vector<nlohmann::json> FilesystemHelper::loadModels() const {
  std::vector<nlohmann::json> models;

  for (const auto& file : std::filesystem::directory_iterator(m_model_path)) {
    if (file.path().extension() == ".json") {
      std::ifstream input(file.path());
      if (!input.is_open()) {
        throw std::runtime_error("Could not open file for reading.");
      }

      nlohmann::json model;
      input >> model;

      models.push_back({
        {"filename", file.path().filename().string()},
        {"content", model}
      });
    }
  }

  return models;
}

void FilesystemHelper::deleteModel(const std::string& filename) {
  std::filesystem::path filepath = m_model_path / filename;
  if (!std::filesystem::exists(filepath)) {
    throw std::runtime_error("Could not delete model - file does not exist.");
  }
  std::filesystem::remove(filepath);
}

bool FilesystemHelper::modelExists(const std::string& filename) const {
  return std::filesystem::exists(m_model_path / filename);
}

}  // namespace beast
