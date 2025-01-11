#include "extmem.h"

#include "spg200_io.h"

Extmem::Extmem(Spg200Io& io) : io_(io) {}

void Extmem::Reset() {
  ctrl_.raw = 0;
  ctrl_.bus_arbiter = 5;
}

void Extmem::SetControl(uint16_t value) {
  ctrl_.raw = value & ExternalMemControl::WriteMask;
}

word_t Extmem::GetControl() {
  return ctrl_.raw;
}

word_t Extmem::ReadWord(addr_t addr) {
  switch (ctrl_.address_decode) {
    case 0:
      return io_.ReadRomCsb(addr);
    case 1:
      switch (addr >> 21) {
        case 0:
          return io_.ReadRomCsb(addr & 0x1fffff);
        case 1:
          return io_.ReadCsb1(addr & 0x1fffff);
        default:
          __builtin_unreachable();
      }
      break;
    case 2:
    case 3:
      switch (addr >> 20) {
        case 0:
          return io_.ReadRomCsb(addr & 0x0fffff);
        case 1:
          return io_.ReadCsb1(addr & 0x0fffff);
        case 2:
          return io_.ReadCsb2(addr & 0x0fffff);
        case 3:
          return io_.ReadCsb3(addr & 0x0fffff);
        default:
          __builtin_unreachable();
      }
    default:
      __builtin_unreachable();
      break;
  }
  return 0;
}

void Extmem::WriteWord(addr_t addr, word_t value) {
  switch (ctrl_.address_decode) {
    case 0:
      return io_.WriteRomCsb(addr, value);
    case 1:
      switch (addr >> 21) {
        case 0:
          return io_.WriteRomCsb(addr & 0x1ffff, value);
        case 1:
          return io_.WriteCsb1(addr & 0x1fffff, value);
        default:
          __builtin_unreachable();
      }
      break;
    case 2:
    case 3:
      switch (addr >> 20) {
        case 0:
          return io_.WriteRomCsb(addr & 0x0fffff, value);
        case 1:
          return io_.WriteCsb1(addr & 0x0fffff, value);
        case 2:
          return io_.WriteCsb2(addr & 0x0fffff, value);
        case 3:
          return io_.WriteCsb3(addr & 0x0fffff, value);
        default:
          __builtin_unreachable();
      }
      break;
  }
}