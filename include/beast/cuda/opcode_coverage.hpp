#ifndef BEAST_CUDA_OPCODE_COVERAGE_HPP_
#define BEAST_CUDA_OPCODE_COVERAGE_HPP_

// Internal
#include <beast/opcodes.hpp>

namespace beast::cuda {

/**
 * @brief Device-coverage status for a single BEAST opcode.
 *
 * The whole point of this enum is to make the GPU port a *contract* rather than a
 * historical accident: every value in `beast::OpCode` must have an explicit decision
 * recorded in `deviceCoverage()` below. Adding a new BEAST opcode without updating
 * the coverage table fails the `cuda_coverage_contract` unit test loudly at CI time,
 * which is the only way to keep the CPU and GPU VMs from silently drifting apart.
 *
 * The three values capture the only honest design decisions available for a given
 * opcode:
 *
 *   - `DeviceImpl`: the device VM executes this opcode and (modulo bounded
 *     per-thread storage like the string-table size) reproduces the CPU semantics.
 *     This is the default we aim for.
 *
 *   - `DeviceNoOpEquivalent`: the device VM treats this opcode as a no-op, and that
 *     produces the *same scoring outcome* as the CPU. Today this covers exactly the
 *     opcodes whose CPU side-effects are not observable through variable values --
 *     `PrintVariable`, `PrintStringFromStringTable*`, `PerformSystemCall`. The
 *     evaluators score programs by reading output variables; they never look at the
 *     print buffer or syscall results, so dropping these on the device gives bit-
 *     for-bit identical scores. NOT a shortcut: this is the correct semantic for
 *     the use cases the GPU backend supports.
 *
 *   - `DeviceFallbackToCpu`: the device VM cannot execute this opcode bit-exactly
 *     with bounded per-thread state. A genome that contains it must be routed to
 *     the CPU thread pool by the hybrid dispatcher (`HybridBatchEvaluator`).
 *     Reserved for things like the `Link` variable type, where supporting it would
 *     mean unbounded per-access indirection chains on the device.
 *
 * IMPORTANT: There is no "DeviceFolded" status anymore. The old behaviour --
 * silently folding to `NoOp` on the device while CPU did real work -- was the root
 * of the SHA-256 candidate that scored 0.95 on CPU but 0.58 on GPU. Every entry in
 * the table is now an explicit, defensible decision.
 */
enum class DeviceCoverage {
  DeviceImpl = 0,           ///< Device VM executes; bit-exact (or doc'd bounded) parity with CPU.
  DeviceNoOpEquivalent = 1, ///< Device VM no-ops; scoring is observably identical to CPU.
  DeviceFallbackToCpu = 2,  ///< Genome must be routed to the CPU batch path.
};

/**
 * @brief Lookup the device-coverage decision for a BEAST opcode.
 *
 * Implementation is a switch over `OpCode`, intentionally NOT a table -- the switch
 * gets `-Wswitch` warnings from the compiler when a new `OpCode` value is added
 * without a matching case, which is a second layer of safety on top of the unit
 * test in `tests/cuda_opcode_coverage.cpp`.
 */
[[nodiscard]] DeviceCoverage deviceCoverage(beast::OpCode opcode) noexcept;

} // namespace beast::cuda

#endif // BEAST_CUDA_OPCODE_COVERAGE_HPP_
