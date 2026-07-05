#pragma once

#include <array>
#include <bitset>
#include <cstdint>

#include "core/common.h"

class BusInterface;

class Cpu {
public:
  Cpu(BusInterface& bus);

  int Step();
  void SetIrq(int irq, bool value);
  void SetFiq(bool value);

  void Reset();

  Word GetDs();
  void SetDs(Word val);
  Addr GetCsPc();

  void PrintRegisterState();

private:
  void AluOp(Word& save, Word val1, Word val2, int alu_op, bool update_flags);
  void UpdateNz(uint32_t result);
  void UpdateNzsc(uint32_t result, int32_t result_signed);
  bool CheckBranch(int branchop);
  bool CheckInterrupts();

  Word ReadWordFromPc();
  void PushWord(Word& sp, Word val);
  Word PopWord(Word& sp);

  void SetCsPc(Addr val);

  BusInterface& bus_;

  std::array<uint16_t, 8> regs_;
  std::array<uint8_t, 3> sb_;  // 0 - normal, 1 - irq, 2 - fiq (fiq ? 2 : irq)

  std::bitset<8> irq_signal_;
  bool fiq_signal_ = false;

  bool irq_, fiq_;
  bool irq_enable_, fiq_enable_;
  bool fir_mov_;
};