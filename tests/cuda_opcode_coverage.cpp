// Coverage contract for the CUDA backend: every BEAST `OpCode` value must have an
// explicit `DeviceCoverage` decision recorded in `deviceCoverage()`. The day someone
// adds a new opcode to `OpCode` without updating the coverage table, this test fails
// loudly -- which is the only way to keep the CPU and GPU VMs from silently drifting
// apart the way they did pre-Tier 3 (the SHA-256 candidate that scored 0.95 on CPU
// but 0.58 on GPU was a direct consequence of that drift).
//
// The test is intentionally cheap: it walks the entire opcode range and counts how
// many fall into each bucket. The size of the `DeviceFallbackToCpu` bucket is itself
// asserted at the end; if it grows unexpectedly, that's a regression in coverage.

#include <beast/cuda/opcode_coverage.hpp>
#include <beast/opcodes.hpp>

#include <catch2/catch.hpp>

#include <cstdint>

TEST_CASE("cuda_opcode_coverage_contract") {
  using beast::cuda::DeviceCoverage;
  using beast::cuda::deviceCoverage;

  uint32_t impl_count = 0;
  uint32_t noop_equiv_count = 0;
  uint32_t fallback_count = 0;

  // OpCode values live in [0, OpCode::Size). The switch in `deviceCoverage` covers
  // every named entry; we iterate the integer range so that, if someone introduces a
  // gap in the enum (e.g. by removing an opcode mid-sequence), the test reports the
  // gap rather than silently passing.
  for (int8_t value = 0; value < static_cast<int8_t>(beast::OpCode::Size); ++value) {
    const auto opcode = static_cast<beast::OpCode>(value);
    const auto coverage = deviceCoverage(opcode);
    switch (coverage) {
      case DeviceCoverage::DeviceImpl:           ++impl_count;       break;
      case DeviceCoverage::DeviceNoOpEquivalent: ++noop_equiv_count; break;
      case DeviceCoverage::DeviceFallbackToCpu:  ++fallback_count;   break;
    }
  }

  // Sentinel `Size` always returns FallbackToCpu in the switch (placeholder for
  // unreachable). It isn't a real opcode and isn't counted in the loop above.
  REQUIRE(deviceCoverage(beast::OpCode::Size) == DeviceCoverage::DeviceFallbackToCpu);

  // Concrete counts -- these are the canonical breakdown as of the Tier-3 port. If
  // these change, either a new opcode was added (update the expected number AND add
  // a parity test for it) or coverage regressed.
  //
  // Total = `OpCode::Size` (currently 0x4e = 78).
  // - DeviceImpl: everything except the print family and (today) zero fallbacks.
  // - DeviceNoOpEquivalent: 3 -- PerformSystemCall, PrintVariable,
  //   PrintStringFromStringTable, PrintVariableStringFromStringTable.
  //   (Actually 4; the count below reflects that.)
  // - DeviceFallbackToCpu: 0 -- after Tier 3, no opcode requires CPU fallback.
  REQUIRE(impl_count + noop_equiv_count + fallback_count ==
          static_cast<uint32_t>(beast::OpCode::Size));
  REQUIRE(noop_equiv_count == 4);
  REQUIRE(fallback_count == 0);
  // Implementation count is therefore Size - 4. Asserted via the sum invariant above,
  // but spelled out here as a smoke check on the table editing -- if you accidentally
  // mark an opcode as NoOpEquivalent when it should be DeviceImpl, this one will fire.
  REQUIRE(impl_count == static_cast<uint32_t>(beast::OpCode::Size) - 4U);
}

TEST_CASE("cuda_opcode_coverage_specific_opcodes") {
  using beast::cuda::DeviceCoverage;
  using beast::cuda::deviceCoverage;

  // Pin a handful of representative decisions so the policy is testable in isolation.
  // If somebody flips PrintVariable to DeviceImpl without thinking through what
  // "implement print on device" means, this section flags the mistake.
  REQUIRE(deviceCoverage(beast::OpCode::PrintVariable) ==
          DeviceCoverage::DeviceNoOpEquivalent);
  REQUIRE(deviceCoverage(beast::OpCode::PerformSystemCall) ==
          DeviceCoverage::DeviceNoOpEquivalent);
  REQUIRE(deviceCoverage(beast::OpCode::PrintStringFromStringTable) ==
          DeviceCoverage::DeviceNoOpEquivalent);

  // Things that MUST be device-implemented after Tier 3 -- the bug we were trying to
  // fix was the GA selecting candidates that depended on these.
  REQUIRE(deviceCoverage(beast::OpCode::CallSubroutine) == DeviceCoverage::DeviceImpl);
  REQUIRE(deviceCoverage(beast::OpCode::PushVariableOnStack) == DeviceCoverage::DeviceImpl);
  REQUIRE(deviceCoverage(beast::OpCode::PopVariableFromStack) == DeviceCoverage::DeviceImpl);
  REQUIRE(deviceCoverage(beast::OpCode::AbsoluteJumpToVariableAddressIfVariableLt0) ==
          DeviceCoverage::DeviceImpl);
  REQUIRE(deviceCoverage(beast::OpCode::LoadRandomValueIntoVariable) ==
          DeviceCoverage::DeviceImpl);
  REQUIRE(deviceCoverage(beast::OpCode::DeclareVariable) == DeviceCoverage::DeviceImpl);
  REQUIRE(deviceCoverage(beast::OpCode::SetStringTableEntry) == DeviceCoverage::DeviceImpl);
}
