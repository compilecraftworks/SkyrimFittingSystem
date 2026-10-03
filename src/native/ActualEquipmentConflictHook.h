#pragma once

#include <xbyak/xbyak.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sfs::native::equip_conflict {
// This is the engine's physical armor-slot loop, NOT BIPED_OBJECTS::kTotal
// (which also contains non-armor entries). Verified in decoded SE 1.5.97 and
// AE 1.6.1170 code; installation checks the same contract on every runtime.
inline constexpr std::uint32_t kArmorSlotCount = 32;
inline constexpr std::size_t kItemReadOffset = 0xC9;
inline constexpr std::size_t kContractSize = 0x24A;

[[nodiscard]] inline bool MatchesInputContract(
    const std::span<const std::uint8_t> a_code) {
  if (a_code.size() < kContractSize) { return false; }
  const auto matches = [&](std::size_t offset, const auto &bytes) {
    for (std::size_t i = 0; i < bytes.size(); ++i) {
      if (a_code[offset + i] != bytes[i]) { return false; }
    }
    return true;
  };
  // Actor=rdi, requested physical slot=ebx, inner byte index=r13.
  // Leave the +0x97 predicate CALL alone: DAV/DAVE may own that hook.
  return matches(0x35, std::array<std::uint8_t, 3>{0x48, 0x8B, 0xF9}) &&
         matches(0x92, std::array<std::uint8_t, 5>{0x8B, 0xD3, 0x48, 0x8B, 0xCD}) &&
         matches(0xA4, std::array<std::uint8_t, 5>{0x33, 0xC9, 0x44, 0x8B, 0xE9}) &&
         matches(0xB2, std::array<std::uint8_t, 3>{0x49, 0x8B, 0x07}) &&
         matches(kItemReadOffset, std::array<std::uint8_t, 8>{
             0x4E, 0x8B, 0x64, 0x28, 0x10, 0x49, 0x8B, 0xCC}) &&
         matches(0x23F, std::array<std::uint8_t, 11>{
             0x49, 0x83, 0xC5, 0x78, 0x49, 0x81, 0xFD, 0x00, 0x0F, 0x00, 0x00});
}

class CandidateReadCode final : public Xbyak::CodeGenerator {
public:
  explicit CandidateReadCode(const std::uintptr_t a_resolveCandidate) {
    Xbyak::Label done, resolve;
    // The only replaced engine instruction. No biped fields are written.
    mov(r12, ptr[rax + r13 + 0x10]);
    pushfq();
    test(r13, r13);
    jnz(done, T_NEAR);
    // Supplement the first candidate only. All remaining native iterations,
    // the original conflict predicate, quest checks and unequip stay intact.
    push(rax); push(rcx); push(rdx); push(r8); push(r9); push(r10); push(r11);
    sub(rsp, 0x88); // aligned CALL, shadow space and six volatile XMM registers
    for (int i = 0; i < 6; ++i) {
      movdqu(ptr[rsp + 0x20 + i * 16], Xbyak::Xmm(i));
    }
    mov(rcx, rdi);
    mov(rdx, rax);
    mov(r8d, ebx);
    mov(r9, r12);
    call(ptr[rip + resolve]);
    mov(r12, rax);
    for (int i = 0; i < 6; ++i) {
      movdqu(Xbyak::Xmm(i), ptr[rsp + 0x20 + i * 16]);
    }
    add(rsp, 0x88);
    pop(r11); pop(r10); pop(r9); pop(r8); pop(rdx); pop(rcx); pop(rax);
    L(done);
    popfq();
    ret();
    L(resolve); dq(a_resolveCandidate);
  }
};
} // namespace sfs::native::equip_conflict
