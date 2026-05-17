The FilesystemHelper Class
==========================

The `FilesystemHelper` class is a C++ class that provides functionality for working with files in a
specific directory. It is designed to be used for saving and loading BEAST pipelines that are stored
as JSON objects.

The class provides the following main functions:

- `saveModel`: Saves a given JSON model object to a unique filename in the specified directory.
- `loadModels`: Loads all JSON model objects in the specified directory.
- `deleteModel`: Deletes a specific JSON model object from the specified directory.

For more detailed information on each of these functions, please refer to the Doxygen-generated
documentation below.

Example usage:

.. code-block:: cpp

    #include <beast/filesystem_helper.hpp>

    int main() {
      // Create a FilesystemHelper instance for a directory called "models"
      beast::FilesystemHelper fs_helper("models");

      // Create a JSON model object
      const nlohmann::json model = {{"key1", "value1"}, {"key2", 2}};

      // Save the model to a file in the "models" directory
      const std::string filename = fs_helper.saveModel("my_model", model);

      // Load all models in the "models" directory
      const std::vector<nlohmann::json> models = fs_helper.loadModels();

      // Delete the previously saved model
      fs_helper.deleteModel(filename);

      return 0;
    }

Note that this example shows only a subset of the functionality provided by the `FilesystemHelper`
class, and the actual usage may vary depending on the specific needs of the application.

.. doxygenclass:: beast::FilesystemHelper
   :members:
