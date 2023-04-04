#ifndef BEAST_BEAST_HPP_
#define BEAST_BEAST_HPP_

// Standard
#include <array>

// Internal
#include <beast/cpu_virtual_machine.hpp>
#include <beast/evaluator.hpp>
#include <beast/filesystem_helper.hpp>
#include <beast/opcodes.hpp>
#include <beast/pipe.hpp>
#include <beast/pipeline.hpp>
#include <beast/pipeline_manager.hpp>
#include <beast/pipeline_server.hpp>
#include <beast/program.hpp>
#include <beast/random_program_factory.hpp>
#include <beast/time_functions.hpp>
#include <beast/version.hpp>
#include <beast/vm_session.hpp>

#include <beast/evaluators/aggregation_evaluator.hpp>
#include <beast/evaluators/maze_evaluator.hpp>
#include <beast/evaluators/operator_usage_evaluator.hpp>
#include <beast/evaluators/random_serial_data_passthrough_evaluator.hpp>
#include <beast/evaluators/runtime_statistics_evaluator.hpp>

#include <beast/pipes/evaluator_pipe.hpp>
#include <beast/pipes/evolution_pipe.hpp>
#include <beast/pipes/null_sink_pipe.hpp>
#include <beast/pipes/program_factory_pipe.hpp>

#endif // BEAST_BEAST_HPP_
