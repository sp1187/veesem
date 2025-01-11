#pragma once

#include <array>
#include <memory>

#include "core/common.h"
#include "types.h"

class BusInterface;
class Cpu;
class Irq;

class Ppu {
public:
  Ppu(VideoTiming video_timing, BusInterface& bus, Irq& irq);

  bool RunCycles(int cycles);
  void Reset();

  word_t GetBgXScroll(int bg_index);
  void SetBgXScroll(int bg_index, word_t value);
  word_t GetBgYScroll(int bg_index);
  void SetBgYScroll(int bg_index, word_t value);
  word_t GetBgAttribute(int bg_index);
  void SetBgAttribute(int bg_index, word_t value);
  word_t GetBgControl(int bg_index);
  void SetBgControl(int bg_index, word_t value);
  word_t GetBgTileMapPtr(int bg_index);
  void SetBgTileMapPtr(int bg_index, word_t value);
  word_t GetBgAttributeMapPtr(int bg_index);
  void SetBgAttributeMapPtr(int bg_index, word_t value);
  word_t GetVerticalCompressAmount();
  void SetVerticalCompressAmount(word_t value);
  word_t GetVerticalCompressOffset();
  void SetVerticalCompressOffset(word_t value);
  word_t GetBgSegmentPtr(int bg_index);
  void SetBgSegmentPtr(int bg_index, word_t value);
  word_t GetSpriteSegmentPtr();
  void SetSpriteSegmentPtr(word_t value);
  word_t GetBlendLevel();
  void SetBlendLevel(word_t value);
  word_t GetFadeLevel();
  void SetFadeLevel(word_t value);
  word_t GetLineScroll(uint8_t offset);
  void SetLineScroll(uint8_t offset, word_t value);
  word_t GetLineCompress(uint8_t offset);
  void SetLineCompress(uint8_t offset, word_t value);
  word_t GetPaletteColor(uint8_t offset);
  void SetPaletteColor(uint8_t offset, word_t value);
  word_t ReadSpriteMemory(word_t offset);
  void WriteSpriteMemory(word_t offset, word_t value);
  word_t GetSpriteControl();
  void SetSpriteControl(word_t value);
  word_t GetSpriteDmaSource();
  void SetSpriteDmaSource(word_t value);
  word_t GetSpriteDmaTarget();
  void SetSpriteDmaTarget(word_t value);
  word_t GetSpriteDmaLength();
  void StartSpriteDma(word_t length);
  word_t GetStnLcdControl();
  void SetStnLcdControl(word_t value);
  word_t GetIrqControl();
  void SetIrqControl(word_t value);
  word_t GetIrqStatus();
  void ClearIrqStatus(word_t value);
  word_t GetIrqVpos();
  void SetIrqVpos(word_t value);
  word_t GetIrqHpos();
  void SetIrqHpos(word_t value);
  word_t GetLineCounter();
  int64_t GetFrameCounter();
  std::span<uint8_t> GetFramebuffer() const;

private:
  void UpdateIrq();
  void DrawLine(int y);
  void DrawBgScanline(int bg_index, int y);
  void DrawSpriteScanline(int sprite_index, int y);
  void DrawTileLine(int screen_y, int screen_x_start, addr_t addr, int tile_width, unsigned palette,
                    bool hflip, unsigned bits_per_pixel, bool blend);
  union Color {
    uint16_t raw;
    Bitfield<15, 1> transparent;
    Bitfield<10, 5> r;
    Bitfield<5, 5> g;
    Bitfield<0, 5> b;
  };

  using Scanline = std::array<Color, 320>;
  using Framebuffer = std::array<Scanline, 240>;

  Framebuffer framebuffer_;
  const VideoTiming video_timing_;
  BusInterface& bus_;
  Irq& irq_;
  int cur_scanline_ = 0;
  SimpleConfigurableClock scanline_clock_;
  int cycles_left_ = 0;

  int64_t frame_count_ = 0;

  union Interrupts {
    word_t raw;
    Bitfield<2, 1> dma;
    Bitfield<1, 1> pos;
    Bitfield<0, 1> vblank;

    static const word_t WriteMask = 0x0007;
  } irq_ctrl_, irq_status_;

  union BgAttribute {
    word_t raw;
    Bitfield<12, 2> depth;
    Bitfield<8, 4> palette;
    Bitfield<6, 2> vsize;
    Bitfield<4, 2> hsize;
    Bitfield<3, 1> vflip;
    Bitfield<2, 1> hflip;
    Bitfield<0, 2> color_mode;

    static const word_t WriteMask = 0x3fff;
  };

  // TODO: separate 8-bit bitfield type?
  union TileAttribute {
    word_t raw;
    Bitfield<6, 1> blend;
    Bitfield<5, 1> vflip;
    Bitfield<4, 1> hflip;
    Bitfield<0, 4> palette;
  };

  union BgControl {
    word_t raw;
    Bitfield<8, 1> blend;
    Bitfield<7, 1> hicolor_mode;
    Bitfield<6, 1> vcompress;
    Bitfield<5, 1> hcompress;
    Bitfield<4, 1> hmovement;
    Bitfield<3, 1> enabled;
    Bitfield<2, 1> wallpaper_mode;
    Bitfield<1, 1> register_mode;
    Bitfield<0, 1> bitmap_mode;

    static const word_t WriteMask = 0x01ff;
  };

  union SpriteAttribute {
    word_t raw;
    Bitfield<14, 1> blend;
    Bitfield<12, 2> depth;
    Bitfield<8, 4> palette;
    Bitfield<6, 2> vsize;
    Bitfield<4, 2> hsize;
    Bitfield<3, 1> vflip;
    Bitfield<2, 1> hflip;
    Bitfield<0, 2> color_mode;

    static const word_t WriteMask = 0x7fff;
  };

  struct BgData {
    word_t xscroll = 0;
    word_t yscroll = 0;
    BgAttribute attr{0};
    BgControl ctrl{0};
    word_t tile_map_ptr = 0;
    word_t attribute_map_ptr = 0;
    word_t segment_ptr = 0;
  };

  struct SpriteData {
    uint16_t ch = 0;
    uint16_t xpos = 0;
    uint16_t ypos = 0;
    SpriteAttribute attr{0};
  };

  std::array<BgData, 2> bg_data_;
  std::array<SpriteData, 256> sprite_data_;
  uint16_t sprite_segment_ptr_ = 0;
  uint8_t stn_lcd_control_ = 0;  // TODO: document and create union
  uint8_t blend_level_ = 0;
  uint8_t fade_level_ = 0;
  uint16_t vertical_compress_amount_ = 0x20;
  uint16_t vertical_compress_offset_ = 0;
  std::array<uint16_t, 256> line_scroll_ = {0};
  std::array<uint16_t, 256> line_compress_ = {0};
  std::array<uint16_t, 256> palette_memory_ = {0};

  bool sprite_enable_ = false;
  uint16_t sprite_dma_source_ = 0;
  uint16_t sprite_dma_target_ = 0;
  uint16_t sprite_dma_length_ = 0;
  uint16_t irq_vpos_ = 0x1ff;
  uint16_t irq_hpos_ = 0x1ff;
};