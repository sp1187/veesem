#include "vsmile.h"

/* V.Smile BIOS regions:
 * 0x0: no screen
 * 0x4: UK/US
 * 0x7: China
 * 0x8: Mexico
 * 0xa: Italy
 * 0xb: Germany
 * 0xd: France
 */

#define VSMILE_REGION 0x4
#define VSMILE_LOGO true

VSmile::VSmile(std::unique_ptr<SysRomType> sys_rom, std::unique_ptr<CartRomType> cart_rom,
               bool has_art_nvram, std::unique_ptr<ArtNvramType> initial_art_nvram,
               VideoTiming video_timing)
    : io_(std::move(sys_rom), std::move(cart_rom), has_art_nvram, std::move(initial_art_nvram),
          *this),
      spg200_(video_timing, io_),
      joy_send_(*this, 0) {}

void VSmile::RunFrame() {
  spg200_.RunFrame();
}

void VSmile::Step() {
  spg200_.Step();
}

void VSmile::Reset() {
  spg200_.Reset();

  spg200_.SetExt1Irq(true);
  spg200_.SetExt2Irq(true);

  io_.rts_[0] = io_.rts_[1] = true;
  io_.cts_[0] = io_.cts_[1] = false;

  io_.joy_.Reset();

  io_.on_button_pressed_ = false;
  io_.off_button_pressed_ = false;
  io_.restart_button_pressed_ = false;
}

std::span<uint8_t> VSmile::GetPicture() const {
  return spg200_.GetPicture();
}

std::span<uint16_t> VSmile::GetAudio() {
  return spg200_.GetAudio();
}

const VSmile::ArtNvramType* VSmile::GetArtNvram() {
  return io_.art_nvram_.get();
}

void VSmile::SetPpuViewSettings(PpuViewSettings& ppu_view_settings) {
  spg200_.SetPpuViewSettings(ppu_view_settings);
}

VSmile::JoyLedStatus VSmile::GetControllerLed() {
  return io_.joy_.GetLeds();
}

void VSmile::UpdateJoystick(const JoyInput& joy_input) {
  io_.joy_.UpdateJoystick(joy_input);
}

void VSmile::UpdateOnButton(bool pressed) {
  io_.on_button_pressed_ = pressed;
}

void VSmile::UpdateOffButton(bool pressed) {
  io_.off_button_pressed_ = pressed;
}

void VSmile::UpdateRestartButton(bool pressed) {
  io_.restart_button_pressed_ = pressed;
}

VSmile::Io::Io(std::unique_ptr<SysRomType> sys_rom, std::unique_ptr<CartRomType> cart_rom,
               bool has_art_nvram, std::unique_ptr<ArtNvramType> initial_art_nvram, VSmile& vsmile)
    : sys_rom_(std::move(sys_rom)),
      cart_rom_(std::move(cart_rom)),
      has_art_nvram_(has_art_nvram),
      joy_(vsmile.joy_send_) {
  if (has_art_nvram_) {
    if (!initial_art_nvram) {
      die("Art Studio NVRAM enabled but no initial value sent");
    }
    art_nvram_ = std::move(initial_art_nvram);
  }
}

void VSmile::Io::RunCycles(int cycles) {
  joy_.RunCycles(cycles);
}

unsigned VSmile::Io::GetAdc0() {
  return 0x0;
}

unsigned VSmile::Io::GetAdc1() {
  return 0x3ff;  // full battery
}

unsigned VSmile::Io::GetAdc2() {
  return 0x0;
}

unsigned VSmile::Io::GetAdc3() {
  return 0x0;
}

word_t VSmile::Io::GetPortA() {
  return 0x0;
}

word_t VSmile::Io::GetPortB() {
  return (!off_button_pressed_ << 7) | (!on_button_pressed_ << 6) | (!restart_button_pressed_ << 3);
}

word_t VSmile::Io::GetPortC() {
  word_t val = VSMILE_REGION | (VSMILE_LOGO << 4) | 0x0020 | (cts_[0] << 8) | (cts_[1] << 9) |
               (rts_[0] << 10) | (rts_[1] << 12) | 0x6000;
  return val;
}

void VSmile::Io::SetPortA(word_t value, word_t mask) {}

void VSmile::Io::SetPortB(word_t value, word_t mask) {}

void VSmile::Io::SetPortC(word_t value, word_t mask) {
  if (mask & 0x0100) {
    cts_[0] = (value & 0x0100);
    joy_.SetCts(cts_[0]);
  }
  if (mask & 0x0200) {
    cts_[1] = (value & 0x0200);
  }
}

word_t VSmile::Io::ReadRomCsb(addr_t addr) {
  return (*cart_rom_)[addr];
}

void VSmile::Io::WriteRomCsb(addr_t addr, word_t value) {}

word_t VSmile::Io::ReadCsb1(addr_t addr) {
  return (*cart_rom_)[addr + 0x100000];
}

void VSmile::Io::WriteCsb1(addr_t addr, word_t value) {}

word_t VSmile::Io::ReadCsb2(addr_t addr) {
  if (has_art_nvram_) {
    // In-cartridge ROM used for the drawing area buffer in V.Smile Art Studio
    return (*art_nvram_)[addr & 0x1ffff];
  }
  // Some games have a dual-ROM cartridge configuration with a larger 4 MiB ROM
  // connected to ROMCSB and CSB1, and a smaller 2 MiB ROM connected to CSB2.
  // This code handles combined ROM dumps where the CSB2 rom is appended to the
  // larger ROM.
  return (*cart_rom_)[addr + 0x200000];
}

void VSmile::Io::WriteCsb2(addr_t addr, word_t value) {
  if (has_art_nvram_) {
    (*art_nvram_)[addr & 0x1ffff] = value;
  }
}

word_t VSmile::Io::ReadCsb3(addr_t addr) {
  return (*sys_rom_)[addr];
}

void VSmile::Io::WriteCsb3(addr_t addr, word_t value) {}

void VSmile::Io::TxUart(uint8_t value) {
  if (cts_[0])
    joy_.Rx(value);
}

void VSmile::Io::RxUartDone() {
  joy_.TxDone();
}

VSmile::JoySend::JoySend(VSmile& vsmile, const int num) : vsmile_(vsmile), num_(num) {}

void VSmile::JoySend::SetRts(bool value) {
  bool old_value = vsmile_.io_.rts_[num_];
  vsmile_.io_.rts_[num_] = value;
  // TODO: this seems to be affected by GPIO pull configuration
  if (old_value && !value) {
    if (num_ == 0)
      vsmile_.spg200_.SetExt1Irq(true);
    if (num_ == 1)
      vsmile_.spg200_.SetExt2Irq(true);
  }
}

void VSmile::JoySend::Tx(uint8_t value) {
  vsmile_.spg200_.UartTx(value);
}