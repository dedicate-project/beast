The PipelineManager Class
=========================

The ``PipelineManager`` class is a key component of the Beast library that manages the creation,
retrieval, and organization of pipelines. A pipeline is an encapsulation of a series of processing
steps and is represented by the ``Pipeline`` class. The ``PipelineManager`` class stores pipelines
in a list and provides methods for creating new pipelines, retrieving pipelines by ID, and getting a
list of all pipelines.

Usage
-----

To create a new instance of the ``PipelineManager`` class, provide a storage path where the
pipelines will be stored:

.. code-block:: cpp

   beast::PipelineManager manager("path/to/storage");

To create a new pipeline, call the ``createPipeline`` method with a name for the pipeline:

.. code-block:: cpp

   uint32_t pipeline_id = manager.createPipeline("Example Pipeline");

To retrieve a specific pipeline by its ID, use the ``getPipelineById`` method:

.. code-block:: cpp

   beast::PipelineManager::PipelineDescriptor& pipeline = manager.getPipelineById(pipeline_id);

To get a list of all pipelines managed by the ``PipelineManager``, call the ``getPipelines`` method:

.. code-block:: cpp

   const std::list<beast::PipelineManager::PipelineDescriptor>& pipelines = manager.getPipelines();

Note that the ``PipelineManager::PipelineDescriptor`` is a structure that contains the following fields:

- ``id``: The unique identifier of the pipeline.
- ``name``: The name of the pipeline.
- ``filename``: The filename where the pipeline is stored.
- ``pipeline``: The actual ``Pipeline`` object.

.. doxygenclass:: beast::PipelineManager
   :members:
