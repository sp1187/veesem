#include "cpu.h"

#include <bit>
#include <iostream>

#include "bus_interface.h"

#define SR (reinterpret_cast<StatusReg&>(regs_[REG_SR]))

union StatusReg {
  word_t raw;

  Bitfield<10, 6> ds;
  Bitfield<9, 1> n;
  Bitfield<8, 1> z;
  Bitfield<7, 1> s;
  Bitfield<6, 1> c;
  Bitfield<0, 6> cs;
};

union Instruction {
  word_t raw;

  Bitfield<12, 4> op0;
  Bitfield<9, 3> rd;
  Bitfield<6, 3> op1;
  Bitfield<3, 6> op1n;
  Bitfield<3, 4> muls_n;
  Bitfield<3, 3> opn;
  Bitfield<0, 3> rs;
  Bitfield<0, 6> imm6;
};

enum cpu_reg {
  REG_SP = 0,
  REG_R1 = 1,
  REG_R2 = 2,
  REG_R3 = 3,
  REG_R4 = 4,
  REG_BP = 5,
  REG_SR = 6,
  REG_PC = 7,
};

enum cpu_aluop {
  ALUOP_ADD = 0,
  ALUOP_ADC = 1,
  ALUOP_SUB = 2,
  ALUOP_SBC = 3,
  ALUOP_CMP = 4,
  ALUOP_NEG = 6,
  ALUOP_XOR = 8,
  ALUOP_LOAD = 9,
  ALUOP_OR = 10,
  ALUOP_AND = 11,
  ALUOP_TEST = 12,
  ALUOP_STORE = 13
};

enum cpu_branchop {
  BRANCHOP_JB = 0,
  BRANCHOP_JAE = 1,
  BRANCHOP_JGE = 2,
  BRANCHOP_JL = 3,
  BRANCHOP_JNE = 4,
  BRANCHOP_JE = 5,
  BRANCHOP_JPL = 6,
  BRANCHOP_JMI = 7,
  BRANCHOP_JBE = 8,
  BRANCHOP_JA = 9,
  BRANCHOP_JLE = 10,
  BRANCHOP_JG = 11,
  BRANCHOP_JVC = 12,
  BRANCHOP_JVS = 13,
  BRANCHOP_JMP = 14,
};

Cpu::Cpu(BusInterface& bus) : bus_(bus) {}

void Cpu::Reset() {
  regs_.fill(0);
  sb_.fill(0);
  irq_ = false;
  fiq_ = false;
  irq_enable_ = false;
  fiq_enable_ = false;
  irq_signal_.reset();
  fiq_signal_ = false;
  fir_mov_ = true;

  regs_[REG_PC] = bus_.ReadWord(0xfff7);
}

void Cpu::PrintRegisterState() {
  std::printf(
      "SP: %04x, R1: %04x, R2: %04x, R3: %04x, "
      "R4: %04x, BP: %04x, SR: %04x, PC: %04x\n",
      regs_[REG_SP], regs_[REG_R1], regs_[REG_R2], regs_[REG_R3], regs_[REG_R4], regs_[REG_BP],
      regs_[REG_SR], regs_[REG_PC]);
  std::printf(
      "  Full PC: %06x, DS: %02x, SB: %01x, Flags: %c%c%c%c, Interrupt mode: "
      "%s\n",
      GetCsPc(), (int)SR.ds, sb_[fiq_ ? 2 : irq_], SR.n ? 'N' : '-', SR.z ? 'Z' : '-',
      SR.s ? 'S' : '-', SR.c ? 'C' : '-',
      fiq_   ? "FIQ"
      : irq_ ? "IRQ"
             : "Normal");
}

bool Cpu::CheckInterrupts() {
  if (fiq_signal_ && !fiq_ && fiq_enable_) {
    fiq_ = true;

    PushWord(regs_[REG_SP], regs_[REG_PC]);
    PushWord(regs_[REG_SP], regs_[REG_SR]);
    regs_[REG_PC] = bus_.ReadWord(0xfff6);
    regs_[REG_SR] = 0;
    return true;
  } else if (irq_signal_.any() && !irq_ && irq_enable_) {
    const int irq_to_run = std::countr_zero(irq_signal_.to_ullong());
    irq_ = true;

    PushWord(regs_[REG_SP], regs_[REG_PC]);
    PushWord(regs_[REG_SP], regs_[REG_SR]);
    regs_[REG_PC] = bus_.ReadWord(0xfff8 + irq_to_run);
    regs_[REG_SR] = 0;
    return true;
  }
  return false;
}

int Cpu::Step() {
  if (CheckInterrupts()) {
    return 10;
  }

  const Instruction iw{ReadWordFromPc()};

  if (iw.op0 == 0xf) {
    switch (iw.op1) {
      case 0:  // mul us
        if (iw.rd != REG_PC && iw.rs != REG_PC && iw.opn == 1) {
          const unsigned result = regs_[iw.rd] * static_cast<int16_t>(regs_[iw.rs]);
          regs_[REG_R3] = result & 0xffff;
          regs_[REG_R4] = (result >> 16) & 0xffff;
          return 12;
        } else {
          die("Unknown instruction");
        }
      case 1:  // call
      {
        const addr_t new_pc = (iw.imm6 << 16) | ReadWordFromPc();
        PushWord(regs_[REG_SP], regs_[REG_PC]);
        PushWord(regs_[REG_SP], regs_[REG_SR]);
        SetCsPc(new_pc);
        return 9;
      }
      case 2:  // goto or muls us
        if (iw.rd == REG_PC) {
          const addr_t new_pc = (iw.imm6 << 16) | ReadWordFromPc();
          SetCsPc(new_pc);
          return 5;
        }
        [[fallthrough]];
      case 3:  // muls us
        if (iw.rd != REG_PC && iw.rs != REG_PC) {
          const int n = iw.muls_n ? iw.muls_n : 16;
          int32_t sum = 0;

          word_t old_val2 = 0;
          for (int i = 0; i < n; i++) {
            word_t val1 = bus_.ReadWord(regs_[iw.rd]++);
            word_t val2 = bus_.ReadWord(regs_[iw.rs]++);
            sum += val1 * static_cast<int16_t>(val2);

            if (fir_mov_) {
              if (i > 0)
                bus_.WriteWord(regs_[iw.rd - 1], old_val2);
              old_val2 = val2;
            }
          }

          regs_[REG_R3] = sum & 0xffff;
          regs_[REG_R4] = (sum >> 16) & 0xffff;
          return 10 * n + 6;
        } else {
          die("Unknown instruction");
        }
      case 4:  // mul ss
      {
        if (iw.rd != REG_PC && iw.rs != REG_PC && iw.opn == 1) {
          const unsigned result =
              static_cast<int16_t>(regs_[iw.rd]) * static_cast<int16_t>(regs_[iw.rs]);

          regs_[REG_R3] = result & 0xffff;
          regs_[REG_R4] = (result >> 16) & 0xffff;
          return 12;
        } else {
          die("Unknown instruction");
        }
      }
      case 5:  // irq control, break, other settings
      {
        switch (iw.imm6) {
          case 0 ... 3:
            irq_enable_ = (iw.imm6 & 1);
            fiq_enable_ = (iw.imm6 & 2);
            return 2;
          case 4 ... 5:
            fir_mov_ = !(iw.imm6 & 1);
            return 2;
          case 8 ... 9:
            irq_enable_ = (iw.imm6 & 1);
            return 2;
          case 12:
          case 14:
            fiq_enable_ = (iw.imm6 & 2);
            return 2;
          case 32:
          case 40:
          case 48:
          case 56:  // break
            PushWord(regs_[REG_SP], regs_[REG_PC]);
            PushWord(regs_[REG_SP], regs_[REG_SR]);
            regs_[REG_PC] = bus_.ReadWord(0xfff5);
            regs_[REG_SR] = 0;
            return 10;
          case 37:  // nop
            return 2;
          default:
            die("Unknown instruction");
            return 0;
        }
      }
      case 6:
      case 7:  // muls ss
        if (iw.rd != REG_PC && iw.rs != REG_PC) {
          const int n = iw.muls_n ? iw.muls_n : 16;
          int32_t sum = 0;

          word_t old_val2 = 0;
          for (int i = 0; i < n; i++) {
            word_t val1 = bus_.ReadWord(regs_[iw.rd]++);
            word_t val2 = bus_.ReadWord(regs_[iw.rs]++);
            sum += static_cast<int16_t>(val1) * static_cast<int16_t>(val2);

            if (fir_mov_) {
              if (i > 0)
                bus_.WriteWord(regs_[iw.rd - 1], old_val2);
              old_val2 = val2;
            }
          }

          regs_[REG_R3] = sum & 0xffff;
          regs_[REG_R4] = (sum >> 16) & 0xffff;
          return 10 * n + 6;
        } else {
          die("Unknown instruction");
        }
      default:
        die("Unknown instruction");
    }
  } else {  // ALU and branch operations
    switch (iw.op1n) {
      case 0 ... 7:  // [bp+imm6] or branch forward if rd = PC
        if (iw.rd == REG_PC) {
          const bool do_branch = CheckBranch(iw.op0);
          if (do_branch) {
            const addr_t cs_pc = GetCsPc();
            SetCsPc(cs_pc + iw.imm6);
          }
          return do_branch ? 4 : 2;
        } else {
          const addr_t addr = regs_[REG_BP] + iw.imm6;
          if (iw.op0 != ALUOP_STORE) {
            const word_t value = bus_.ReadWord(addr);
            AluOp(regs_[iw.rd], regs_[iw.rd], value, iw.op0, iw.rd != REG_PC);
          } else {
            bus_.WriteWord(addr, regs_[iw.rd]);
          }
          return 6;
        }
      case 8 ... 15:  // imm6 or branch backward if rd = PC
        if (iw.rd == REG_PC) {
          const bool do_branch = CheckBranch(iw.op0);
          if (do_branch) {
            const addr_t cs_pc = GetCsPc();
            SetCsPc(cs_pc - iw.imm6);
          }
          return do_branch ? 4 : 2;
        } else {
          if (iw.op0 != ALUOP_STORE) {
            AluOp(regs_[iw.rd], regs_[iw.rd], iw.imm6, iw.op0, iw.rd != REG_PC);
          } else {
            die("Attempting to store using immediate addressing mode");
          }
          return 2;
        }
      case 16 ... 23: {  // push and pop
        int n = iw.opn;
        int reg = iw.rd;

        if (iw.op0 == ALUOP_LOAD)  // pop / reti (pseudo-op)
        {
          // special encoding for reti
          if (iw.rd == REG_BP && iw.opn == 3 && iw.rs == REG_SP) {
            if (fiq_)
              fiq_ = false;
            else if (irq_)
              irq_ = false;
            n = 2;  // otherwise handle like usual
          }

          int left = n;
          while (left-- && (reg + 1) <= 7) {
            regs_[++reg] = PopWord(regs_[iw.rs]);
          }
        } else if (iw.op0 == ALUOP_STORE) {
          int left = n;
          while (left-- && reg >= 0) {
            PushWord(regs_[iw.rs], regs_[reg--]);
          }
        } else {
          die("Attempting to push/pop using invalid alu op");
        }
        return 2 * n + 4;
      }
      case 24 ... 31: {
        addr_t addr = 0;
        switch (iw.op1n) {
          case 24:
            addr = regs_[iw.rs];
            break;
          case 25:
            addr = regs_[iw.rs]--;
            break;
          case 26:
            addr = regs_[iw.rs]++;
            break;
          case 27:
            addr = ++regs_[iw.rs];
            break;
          case 28:
            addr = (SR.ds << 16) | regs_[iw.rs];
            break;
          case 29:
            addr = (SR.ds << 16) | regs_[iw.rs]--;
            if (regs_[iw.rs] == 0xFFFF)
              SR.ds--;
            break;
          case 30:
            addr = (SR.ds << 16) | regs_[iw.rs]++;
            if (regs_[iw.rs] == 0x0000)
              SR.ds++;
            break;
          case 31:
            if (++regs_[iw.rs] == 0x0000)
              SR.ds++;
            addr = SR.ds << 16 | regs_[iw.rs];
            break;
        }

        if (iw.op0 != ALUOP_STORE) {
          word_t value = bus_.ReadWord(addr);
          AluOp(regs_[iw.rd], regs_[iw.rd], value, iw.op0, iw.rd != REG_PC);
        } else {
          bus_.WriteWord(addr, regs_[iw.rd]);
        }

        return iw.rd == REG_PC ? 7 : 6;
      }
      case 32:  // register
      {
        if (iw.op0 != ALUOP_STORE) {
          AluOp(regs_[iw.rd], regs_[iw.rd], regs_[iw.rs], iw.op0, iw.rd != REG_PC);
        } else {
          die("Attempting to store using register mode");
        }
        return iw.rd == REG_PC ? 5 : 3;
      }
      case 33:  // imm16
      {
        const word_t rs_val = regs_[iw.rs];
        const word_t imm = ReadWordFromPc();

        if (iw.op0 != ALUOP_STORE) {
          AluOp(regs_[iw.rd], rs_val, imm, iw.op0, iw.rd != REG_PC);
        } else {
          die("Attempting to store using immediate mode");
        }
        return iw.rd == REG_PC ? 5 : 4;
      }
      case 34:  // [imm16]
      {
        const word_t rs_val = regs_[iw.rs];
        const word_t addr = ReadWordFromPc();

        if (iw.op0 != ALUOP_STORE) {
          const word_t value = bus_.ReadWord(addr);
          AluOp(regs_[iw.rd], rs_val, value, iw.op0, iw.rd != REG_PC);
        } else {
          die("Attempts to store using [imm16] read mode");
        }
        return iw.rd == REG_PC ? 8 : 7;
      }
      case 35:  // [imm16] store
      {
        const word_t rs_val = regs_[iw.rs];
        const word_t rd_val = regs_[iw.rd];
        const word_t addr = ReadWordFromPc();

        if (iw.op0 != ALUOP_STORE) {
          word_t result = 0;
          AluOp(result, rs_val, rd_val, iw.op0, iw.rd != REG_PC);
          bus_.WriteWord(addr, result);
        } else {
          bus_.WriteWord(addr, rs_val);
        }

        return iw.rd == REG_PC ? 8 : 7;
      }
      case 36 ... 39:  // register with arithmetic shift right
      {
        uint8_t& cur_sb = sb_[fiq_ ? 2 : irq_];
        const int n = (iw.opn & 0x03) + 1;

        const int shift = sext<20>((regs_[iw.rs] << 4) | cur_sb) >> n;
        const word_t value = (shift >> 4) & 0xffff;
        cur_sb = shift & 0xf;

        if (iw.op0 != ALUOP_STORE) {
          AluOp(regs_[iw.rd], regs_[iw.rd], value, iw.op0, iw.rd != REG_PC);
        } else {
          die("Attempts to store using shift mode");
        }
        return iw.rd == REG_PC ? 5 : 3;
      }
      case 40 ... 43:  // register with logical shift left
      {
        uint8_t& cur_sb = sb_[fiq_ ? 2 : irq_];
        const int n = (iw.opn & 0x03) + 1;

        const unsigned shift = ((cur_sb << 16) | regs_[iw.rs]) << n;
        const word_t value = shift & 0xffff;
        cur_sb = (shift >> 16) & 0xf;

        if (iw.op0 != ALUOP_STORE) {
          AluOp(regs_[iw.rd], regs_[iw.rd], value, iw.op0, iw.rd != REG_PC);
        } else {
          die("Attempts to store using shift mode");
        }
        return iw.rd == REG_PC ? 5 : 3;
      }
      case 44 ... 47:  // register with logical shift right
      {
        uint8_t& cur_sb = sb_[fiq_ ? 2 : irq_];
        const int n = (iw.opn & 0x03) + 1;

        const unsigned shift = ((regs_[iw.rs] << 4) | cur_sb) >> n;
        const word_t value = (shift >> 4) & 0xffff;
        cur_sb = shift & 0xf;

        if (iw.op0 != ALUOP_STORE) {
          AluOp(regs_[iw.rd], regs_[iw.rd], value, iw.op0, iw.rd != REG_PC);
        } else {
          die("Attempts to store using shift mode");
        }
        return iw.rd == REG_PC ? 5 : 3;
      }
      case 48 ... 51:  // register with rotate left
      {
        uint8_t& cur_sb = sb_[fiq_ ? 2 : irq_];
        const int n = (iw.opn & 0x03) + 1;

        const unsigned shift = rotl<20>((cur_sb << 16) | regs_[iw.rs], n);
        const word_t value = shift & 0xffff;
        cur_sb = (shift >> 16) & 0xf;

        if (iw.op0 != ALUOP_STORE) {
          AluOp(regs_[iw.rd], regs_[iw.rd], value, iw.op0, iw.rd != REG_PC);
        } else {
          die("Attempts to store using shift mode");
        }
        return iw.rd == REG_PC ? 5 : 3;
      }
      case 52 ... 55:  // register with rotate right
      {
        uint8_t& cur_sb = sb_[fiq_ ? 2 : irq_];
        const int n = (iw.opn & 0x03) + 1;

        const unsigned shift = rotr<20>((regs_[iw.rs] << 4) | cur_sb, n);
        const word_t value = (shift >> 4) & 0xffff;
        cur_sb = shift & 0xf;

        if (iw.op0 != ALUOP_STORE) {
          AluOp(regs_[iw.rd], regs_[iw.rd], value, iw.op0, iw.rd != REG_PC);
        } else {
          die("Attempts to store using shift mode");
        }
        return iw.rd == REG_PC ? 5 : 3;
      }
      case 56 ... 63:  // [A6]
      {
        if (iw.op0 != ALUOP_STORE) {
          const word_t value = bus_.ReadWord(iw.imm6);
          AluOp(regs_[iw.rd], regs_[iw.rd], value, iw.op0, iw.rd != REG_PC);
        } else {
          bus_.WriteWord(iw.imm6, regs_[iw.rd]);
        }
        return iw.rd == REG_PC ? 6 : 5;
      }
    }
  }

  return 1;
}

void Cpu::SetIrq(int irq, bool val) {
  irq_signal_[irq] = val;
}

void Cpu::SetFiq(bool val) {
  fiq_signal_ = val;
}

void Cpu::UpdateNz(unsigned result) {
  SR.n = result & 0x8000;
  SR.z = (result & 0xffff) == 0;
}

void Cpu::UpdateNzsc(unsigned result, int result_signed) {
  SR.n = result & 0x8000;
  SR.z = (result & 0xffff) == 0;
  SR.s = result_signed < 0;
  SR.c = result & 0x10000;
}

void Cpu::AluOp(word_t& save, word_t val1, word_t val2, int alu_op, bool update_flags) {
  switch (alu_op) {
    case ALUOP_ADD:
    case ALUOP_ADC: {
      const bool carry = alu_op == ALUOP_ADC ? SR.c : 0;
      const unsigned result = val1 + val2 + carry;
      const signed result_signed = static_cast<int16_t>(val1) + static_cast<int16_t>(val2) + carry;
      if (update_flags)
        UpdateNzsc(result, result_signed);
      save = result & 0xffff;
      return;
    }
    case ALUOP_SUB:
    case ALUOP_SBC:
    case ALUOP_CMP: {
      const bool carry = alu_op == ALUOP_SBC ? SR.c : 1;
      const unsigned result = val1 + static_cast<uint16_t>(~val2) + carry;
      const signed result_signed = static_cast<int16_t>(val1) + static_cast<int16_t>(~val2) + carry;
      if (update_flags)
        UpdateNzsc(result, result_signed);
      if (alu_op != ALUOP_CMP)
        save = result & 0xffff;
      return;
    }
    case ALUOP_NEG: {
      const unsigned result = ~val2 + 1;
      if (update_flags)
        UpdateNz(result);
      save = result & 0xffff;
      return;
    }
    case ALUOP_XOR: {
      const word_t result = val1 ^ val2;
      if (update_flags)
        UpdateNz(result);
      save = result;
      return;
    }
    case ALUOP_LOAD: {
      const word_t result = val2;
      if (update_flags)
        UpdateNz(result);
      save = result;
      return;
    }
    case ALUOP_OR: {
      const word_t result = val1 | val2;
      if (update_flags)
        UpdateNz(result);
      save = result;
      return;
    }
    case ALUOP_AND:
    case ALUOP_TEST: {
      const word_t result = val1 & val2;
      if (update_flags)
        UpdateNz(result);
      if (alu_op != ALUOP_TEST)
        save = result;
      return;
    }
    default:
      die("Unknown ALU mode");
  }
}

bool Cpu::CheckBranch(int branchop) {
  switch (branchop) {
    case BRANCHOP_JB:
      return !SR.c;  // jump below (unsigned)
    case BRANCHOP_JAE:
      return SR.c;  // jump above or equal (unsigned)
    case BRANCHOP_JGE:
      return !SR.s;  // jump greater or equal (signed)
    case BRANCHOP_JL:
      return SR.s;  // jump less (signed)
    case BRANCHOP_JNE:
      return !SR.z;  // jump not equal
    case BRANCHOP_JE:
      return SR.z;  // jump equal
    case BRANCHOP_JPL:
      return !SR.n;  // jump plus
    case BRANCHOP_JMI:
      return SR.n;  // jump minus
    case BRANCHOP_JBE:
      return !(!SR.z && SR.c);  // jump below or equal (unsigned)
    case BRANCHOP_JA:
      return !SR.z && SR.c;  // jump above (unsigned)
    case BRANCHOP_JLE:
      return !(!SR.z && !SR.s);  // jump less or equal (signed)
    case BRANCHOP_JG:
      return !SR.z && !SR.s;  // jump greater (signed)
    case BRANCHOP_JVC:
      return SR.n == SR.s;  // jump overflow clear
    case BRANCHOP_JVS:
      return SR.n != SR.s;  // jump overflow set
    case BRANCHOP_JMP:
      return true;  // jump
    default:
      die("Unknown jump/branch mode");
  }
}

inline word_t Cpu::ReadWordFromPc() {
  const addr_t cs_pc = GetCsPc();
  const word_t val = bus_.ReadWord(cs_pc);
  SetCsPc(cs_pc + 1);

  return val;
}

inline void Cpu::PushWord(word_t& sp, word_t val) {
  bus_.WriteWord(sp--, val);
}

inline word_t Cpu::PopWord(word_t& sp) {
  return bus_.ReadWord(++sp);
}

word_t Cpu::GetDs() {
  return SR.ds;
}

void Cpu::SetDs(word_t val) {
  SR.ds = val;
}

addr_t Cpu::GetCsPc() {
  return (SR.cs << 16) | regs_[REG_PC];
}

inline void Cpu::SetCsPc(addr_t val) {
  regs_[REG_PC] = val & 0xffff;
  SR.cs = val >> 16;
}
