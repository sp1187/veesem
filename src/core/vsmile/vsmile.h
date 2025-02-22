#pragma once

#include "core/common.h"

#include "core/spg200/settings.h"
#include "core/spg200/spg200.h"
#include "core/spg200/spg200_io.h"
#include "core/spg200/types.h"
#include "vsmile_common.h"
#include "vsmile_joy.h"

class IoIrq;

class VSmile {
public:
  using CartRomType = std::array<word_t, 4 * 1024 * 1024>;
  using SysRomType = std::array<word_t, 1024 * 1024>;
  using ArtNvramType = std::array<word_t, 128 * 1024>;

  using JoyInput = VSmileJoy::JoyInput;
  using JoyLedStatus = VSmileJoy::JoyLedStatus;

  VSmile(std::unique_ptr<SysRomType> sys_rom, std::unique_ptr<CartRomType> cart_rom,
         bool has_art_nvram, std::unique_ptr<ArtNvramType> initial_art_nvram,
         VideoTiming video_timing);

  void RunFrame();
  void Step();
  void Reset();

  std::span<uint8_t> GetPicture() const;
  std::span<uint16_t> GetAudio();
  const ArtNvramType* GetArtNvram();

  void SetPpuViewSettings(PpuViewSettings& ppu_view_settings);

  void UpdateJoystick(const JoyInput& joy_input);
  JoyLedStatus GetControllerLed();

  void UpdateOnButton(bool pressed);
  void UpdateOffButton(bool pressed);
  void UpdateRestartButton(bool pressed);

private:
  class Io : public Spg200Io {
  public:
    Io(std::unique_ptr<SysRomType> sys_rom, std::unique_ptr<CartRomType> cart_rom,
       bool has_art_nvram, std::unique_ptr<ArtNvramType> initial_art_nvram, VSmile& vsmile);

    void RunCycles(int cycles) override;

    unsigned GetAdc0() override;
    unsigned GetAdc1() override;
    unsigned GetAdc2() override;
    unsigned GetAdc3() override;

    word_t GetPortA() override;
    void SetPortA(word_t value, word_t mask) override;
    word_t GetPortB() override;
    void SetPortB(word_t value, word_t mask) override;

    word_t GetPortC() override;
    void SetPortC(word_t value, word_t mask) override;

    void TxUart(uint8_t value) override;
    void RxUartDone() override;

    word_t ReadRomCsb(addr_t addr) override;
    void WriteRomCsb(addr_t addr, word_t value) override;
    word_t ReadCsb1(addr_t addr) override;
    void WriteCsb1(addr_t addr, word_t value) override;
    word_t ReadCsb2(addr_t addr) override;
    void WriteCsb2(addr_t addr, word_t value) override;
    word_t ReadCsb3(addr_t addr) override;
    void WriteCsb3(addr_t addr, word_t value) override;

    std::unique_ptr<SysRomType> sys_rom_;
    std::unique_ptr<CartRomType> cart_rom_;
    bool has_art_nvram_ = false;
    std::unique_ptr<ArtNvramType> art_nvram_;
    VSmileJoy joy_;

    bool rts_[2] = {true};
    bool cts_[2] = {false};

    bool on_button_pressed_ = false;
    bool off_button_pressed_ = false;
    bool restart_button_pressed_ = false;
  } io_;

  class JoySend : public VSmileJoySend {
  public:
    JoySend(VSmile& vsmile, const int num);

    void SetRts(bool value) override;
    void Tx(uint8_t value) override;

  private:
    VSmile& vsmile_;
    const int num_;
  };

  Spg200 spg200_;
  JoySend joy_send_;
};
