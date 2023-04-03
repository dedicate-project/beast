#include <catch2/catch.hpp>

// Standard
#include <filesystem>
#include <fstream>

// BEAST
#include <beast/beast.hpp>

TEST_CASE("FilesystemHelper") {
  const std::filesystem::path model_path =
      std::filesystem::temp_directory_path() / "beast_test_models";
  std::filesystem::create_directory(model_path);
  beast::FilesystemHelper fs_helper{model_path.u8string()};

  SECTION("Save and load a model") {
    const std::string model_id = "test_model";
    nlohmann::json model = {{"test_key", "test_value"}};
    nlohmann::json wrapped_model = {{"name", model_id}, {"model", model}};

    const std::string filename = fs_helper.saveModel(model_id, model);

    REQUIRE(fs_helper.modelExists(filename));

    const std::vector<nlohmann::json> loaded_models = fs_helper.loadModels();
    REQUIRE(loaded_models.size() == 1);
    REQUIRE(loaded_models[0]["filename"] == filename);
    REQUIRE(loaded_models[0]["content"] == wrapped_model);
  }

  SECTION("Save and delete a model") {
    const std::string model_id = "test_model";
    const nlohmann::json model = {{"test_key", "test_value"}};

    const std::string filename = fs_helper.saveModel(model_id, model);

    REQUIRE(fs_helper.modelExists(filename));

    fs_helper.deleteModel(filename);

    REQUIRE_FALSE(fs_helper.modelExists(filename));
  }

  SECTION("Load all models") {
    const std::string model_id1 = "test_model1";
    nlohmann::json model1 = {{"test_key", "test_value1"}};
    const std::string filename1 = fs_helper.saveModel(model_id1, model1);

    const std::string model_id2 = "test_model2";
    nlohmann::json model2 = {{"test_key", "test_value2"}};
    const std::string filename2 = fs_helper.saveModel(model_id2, model2);

    nlohmann::json wrapped_model1 = {{"name", model_id1}, {"model", model1}};
    nlohmann::json wrapped_model2 = {{"name", model_id2}, {"model", model2}};

    const std::vector<nlohmann::json> loaded_models = fs_helper.loadModels();
    REQUIRE(loaded_models.size() == 2);

    bool found_model1 = false;
    bool found_model2 = false;

    for (const auto& loaded_model : loaded_models) {
      if (loaded_model["filename"] == filename1 && loaded_model["content"] == wrapped_model1) {
        found_model1 = true;
      }
      if (loaded_model["filename"] == filename2 && loaded_model["content"] == wrapped_model2) {
        found_model2 = true;
      }
    }

    REQUIRE(found_model1);
    REQUIRE(found_model2);
  }

  SECTION("Delete non-existent model") {
    REQUIRE_THROWS_AS(fs_helper.deleteModel("non_existent_model.json"), std::invalid_argument);
  }

  std::filesystem::remove_all(model_path);
}
