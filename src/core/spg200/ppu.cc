#include "ppu.h"

#include "bus_interface.h"
#include "irq.h"

namespace {
inline addr_t CalculateLineSegmentAddr(word_t segment_ptr, int ch, int tile_y, int tile_width,
                                       int tile_height, int bits_per_pixel) {
  return (segment_ptr << 6) + (ch * tile_height + tile_y) * tile_width * bits_per_pixel / 16;
}

inline int BlendInterpolate(int old_value, int new_value, int blend_level) {
  return (old_value * (4 - (blend_level + 1))) / 4 + (new_value * (blend_level + 1)) / 4;
}

inline unsigned DivideRoundUp(unsigned dividend, unsigned divisor) {
  return (dividend / divisor) + !!(dividend % divisor);
}
}  // namespace

Ppu::Ppu(VideoTiming video_timing, BusInterface& bus, Irq& irq)
    : video_timing_(video_timing),
      bus_(bus),
      irq_(irq),
      scanline_clock_((video_timing == VideoTiming::NTSC ? 429 : 432) * 4, 1) {}

void Ppu::Reset() {
  cur_scanline_ = 0;
  scanline_clock_.Reset();
  frame_count_ = 0;
  for (int scanline = 0; scanline < 240; scanline++)
    framebuffer_[scanline].fill({0});

  bg_data_.fill({});
  sprite_data_.fill({});
  sprite_segment_ptr_ = 0;
  blend_level_ = 0;
  vertical_compress_amount_ = 0x20;
  vertical_compress_offset_ = 0;
  fade_level_ = 0;
  line_scroll_.fill(0);
  line_compress_.fill(0);
  palette_memory_.fill(0);
  sprite_enable_ = false;
  sprite_dma_source_ = 0;
  sprite_dma_target_ = 0;
  sprite_dma_length_ = 0;
  stn_lcd_control_ = 0;
  irq_vpos_ = 0x1ff;  //???
  irq_hpos_ = 0x1ff;

  irq_ctrl_.raw = 0;
  irq_status_.raw = 0;
  UpdateIrq();
}

bool Ppu::RunCycles(int cycles) {
  if (scanline_clock_.Tick(cycles)) {
    const int scanlines = video_timing_ == VideoTiming::NTSC ? 262 : 312;
    bool frame_finished = false;

    if (cur_scanline_ == irq_vpos_ && irq_ctrl_.pos) {
      irq_status_.pos = true;
      UpdateIrq();
    }

    if (cur_scanline_ < 240) {
      DrawLine(cur_scanline_);
      if (cur_scanline_ == 239) {
        if (irq_ctrl_.vblank) {
          irq_status_.vblank = true;
          UpdateIrq();
        }
        frame_count_++;
        frame_finished = true;
      }
      cur_scanline_++;
    } else if (cur_scanline_ >= scanlines - 1) {
      irq_status_.vblank = false;
      UpdateIrq();
      cur_scanline_ = 0;
    } else {
      cur_scanline_++;
    }

    return frame_finished;
  }

  return false;
}

void Ppu::SetViewSettings(PpuViewSettings& view_settings) {
  view_settings_ = view_settings;
}

word_t Ppu::GetBgXScroll(int bg_index) {
  return bg_data_[bg_index].xscroll;
}

void Ppu::SetBgXScroll(int bg_index, word_t value) {
  bg_data_[bg_index].xscroll = value & 0x1ff;
}

word_t Ppu::GetBgYScroll(int bg_index) {
  return bg_data_[bg_index].yscroll;
}

void Ppu::SetBgYScroll(int bg_index, word_t value) {
  bg_data_[bg_index].yscroll = value & 0xff;
}

word_t Ppu::GetBgAttribute(int bg_index) {
  return bg_data_[bg_index].attr.raw;
}

void Ppu::SetBgAttribute(int bg_index, word_t value) {
  bg_data_[bg_index].attr.raw = value & BgAttribute::WriteMask;
}

word_t Ppu::GetBgControl(int bg_index) {
  return bg_data_[bg_index].ctrl.raw;
}

void Ppu::SetBgControl(int bg_index, word_t value) {
  bg_data_[bg_index].ctrl.raw = value & BgControl::WriteMask;
}

word_t Ppu::GetBgTileMapPtr(int bg_index) {
  return bg_data_[bg_index].tile_map_ptr;
}

void Ppu::SetBgTileMapPtr(int bg_index, word_t value) {
  bg_data_[bg_index].tile_map_ptr = value & 0x3fff;
}

word_t Ppu::GetBgAttributeMapPtr(int bg_index) {
  return bg_data_[bg_index].attribute_map_ptr;
}

void Ppu::SetBgAttributeMapPtr(int bg_index, word_t value) {
  bg_data_[bg_index].attribute_map_ptr = value & 0x3fff;
}

word_t Ppu::GetVerticalCompressAmount() {
  return vertical_compress_amount_;
}

void Ppu::SetVerticalCompressAmount(word_t value) {
  vertical_compress_amount_ = value & 0x1ff;
}

word_t Ppu::GetVerticalCompressOffset() {
  return vertical_compress_offset_;
}

void Ppu::SetVerticalCompressOffset(word_t value) {
  vertical_compress_offset_ = value & 0x1fff;
}

word_t Ppu::GetBgSegmentPtr(int bg_index) {
  return bg_data_[bg_index].segment_ptr;
}

void Ppu::SetBgSegmentPtr(int bg_index, word_t value) {
  bg_data_[bg_index].segment_ptr = value;
}

word_t Ppu::GetSpriteSegmentPtr() {
  return sprite_segment_ptr_;
}

void Ppu::SetSpriteSegmentPtr(word_t value) {
  sprite_segment_ptr_ = value;
}

word_t Ppu::GetBlendLevel() {
  return blend_level_;
}

void Ppu::SetBlendLevel(word_t value) {
  blend_level_ = value & 0x03;
}

word_t Ppu::GetFadeLevel() {
  return fade_level_;
}

void Ppu::SetFadeLevel(word_t value) {
  fade_level_ = value & 0xff;
}

word_t Ppu::GetSpriteDmaSource() {
  return sprite_dma_source_;
}

void Ppu::SetSpriteDmaSource(word_t value) {
  sprite_dma_source_ = value & 0x3fff;
}

word_t Ppu::GetSpriteDmaTarget() {
  return sprite_dma_target_;
}

void Ppu::SetSpriteDmaTarget(word_t value) {
  sprite_dma_target_ = value & 0x3ff;
}

word_t Ppu::GetSpriteDmaLength() {
  return sprite_dma_length_;
}

void Ppu::StartSpriteDma(word_t length) {
  sprite_dma_length_ = length;

  while (sprite_dma_length_) {
    const word_t word = bus_.ReadWord(sprite_dma_source_++);
    WriteSpriteMemory(sprite_dma_target_++, word);
    sprite_dma_target_ &= 0x3ff;
    sprite_dma_length_--;
  }

  if (irq_ctrl_.dma) {
    irq_status_.dma = true;
    UpdateIrq();
  }
}

word_t Ppu::GetStnLcdControl() {
  return stn_lcd_control_;
}

void Ppu::SetStnLcdControl(word_t value) {
  stn_lcd_control_ = value & 0x3f;
}

word_t Ppu::GetLineScroll(uint8_t offset) {
  return line_scroll_[offset & 0xff];
}

void Ppu::SetLineScroll(uint8_t offset, word_t value) {
  line_scroll_[offset & 0xff] = value & 0x1ff;
}

word_t Ppu::GetLineCompress(uint8_t offset) {
  return line_compress_[offset & 0xff];
}

void Ppu::SetLineCompress(uint8_t offset, word_t value) {
  line_compress_[offset & 0xff] = value;
}

word_t Ppu::GetPaletteColor(uint8_t offset) {
  return palette_memory_[offset & 0xff];
}

void Ppu::SetPaletteColor(uint8_t offset, word_t value) {
  palette_memory_[offset & 0xff] = value;
}

word_t Ppu::ReadSpriteMemory(word_t offset) {
  const int index = (offset & 0x3ff) >> 2;
  switch (offset & 3) {
    case 0:
      return sprite_data_[index].ch;
    case 1:
      return sprite_data_[index].xpos;
    case 2:
      return sprite_data_[index].ypos;
    case 3:
      return sprite_data_[index].attr.raw;
  }
  return 0;
}

void Ppu::WriteSpriteMemory(word_t offset, word_t value) {
  const int index = (offset & 0x3ff) >> 2;
  switch (offset & 3) {
    case 0:
      sprite_data_[index].ch = value;
      return;
    case 1:
      sprite_data_[index].xpos = value & 0x1ff;
      return;
    case 2:
      sprite_data_[index].ypos = value & 0x1ff;
      return;
    case 3:
      sprite_data_[index].attr.raw = value & SpriteAttribute::WriteMask;
      return;
  }
}

word_t Ppu::GetSpriteControl() {
  return sprite_enable_;
}

void Ppu::SetSpriteControl(word_t value) {
  sprite_enable_ = value & 0x1;
}

word_t Ppu::GetIrqControl() {
  return irq_ctrl_.raw;
}

void Ppu::SetIrqControl(word_t value) {
  irq_ctrl_.raw = value & Interrupts::WriteMask;
  UpdateIrq();
}

word_t Ppu::GetIrqStatus() {
  return irq_status_.raw;
}

void Ppu::ClearIrqStatus(word_t value) {
  irq_status_.raw &= ~(value & Interrupts::WriteMask);
  UpdateIrq();
}

word_t Ppu::GetIrqVpos() {
  return irq_vpos_;
}

void Ppu::SetIrqVpos(word_t value) {
  irq_vpos_ = value & 0x1ff;
}

word_t Ppu::GetIrqHpos() {
  return irq_hpos_;
}

void Ppu::SetIrqHpos(word_t value) {
  irq_hpos_ = value & 0x1ff;
}

word_t Ppu::GetLineCounter() {
  return cur_scanline_;
}

int64_t Ppu::GetFrameCounter() {
  return frame_count_;
}

void Ppu::UpdateIrq() {
  const Interrupts active{static_cast<word_t>(irq_ctrl_.raw & irq_status_.raw)};
  bool value = active.dma || active.pos || active.vblank;
  irq_.SetPpuIrq(value);
}

void Ppu::DrawLine(int scanline) {
  Color transparent;
  transparent.transparent = 1;
  framebuffer_[scanline].fill(transparent);

  for (unsigned layer = 0; layer < 4; layer++) {
    for (unsigned bg = 0; bg < 2; bg++) {
      if (!view_settings_.show_bg[bg])
        continue;
      if (bg_data_[bg].ctrl.enabled && bg_data_[bg].attr.depth == layer) {
        DrawBgScanline(bg, scanline);
      }
    }

    if (sprite_enable_ && view_settings_.show_sprites &&
        view_settings_.show_sprites_in_layer[layer]) {
      for (int sprite_index = 0; sprite_index < 256; sprite_index++) {
        const auto& sprite = sprite_data_[sprite_index];
        if (sprite.ch && !sprite.attr.blend && sprite.attr.depth == layer) {
          DrawSpriteScanline(sprite_index, scanline);
        }
      }

      // Draw blended sprites last
      for (int sprite_index = 0; sprite_index < 256; sprite_index++) {
        const auto& sprite = sprite_data_[sprite_index];
        if (sprite.ch && sprite.attr.blend && sprite.attr.depth == layer) {
          DrawSpriteScanline(sprite_index, scanline);
        }
      }
    }
  }

  // Replace all remaining transparent pixels with black
  for (auto& pixel : framebuffer_[scanline]) {
    if (pixel.transparent) {
      pixel = Color{};
    }
  }
}

void Ppu::DrawBgScanline(int bg_index, int screen_y) {
  const auto& bg = bg_data_[bg_index];

  int virtual_y = screen_y;
  if (bg.ctrl.vcompress) {
    const int offset = sext<13>(vertical_compress_offset_) + 128 -
                       128 * static_cast<int>(vertical_compress_amount_) / 0x20;

    virtual_y = screen_y * static_cast<int>(vertical_compress_amount_) / 0x20 + offset;
  }

  if (virtual_y < 0 || virtual_y >= 240)
    return;

  const int tilemap_y = (virtual_y + bg.yscroll) & 0xff;
  const int scroll_x = (bg.xscroll + (bg.ctrl.hmovement ? line_scroll_[tilemap_y] : 0)) & 0x1ff;

  if (bg.ctrl.bitmap_mode) {
    const word_t addr_lo = bus_.ReadWord(bg.tile_map_ptr + tilemap_y);
    const word_t addr_hi =
        bus_.ReadWord(bg.attribute_map_ptr + tilemap_y / 2) >> word_t((tilemap_y & 1) ? 8 : 0);
    const addr_t addr = addr_lo | (addr_hi << 16);
    const int bits_per_pixel = bg.ctrl.hicolor_mode ? 16 : (bg.attr.color_mode + 1) * 2;
    for (int screen_x = -scroll_x; screen_x < 320; screen_x += 512) {
      DrawTileLine(screen_y, screen_x, addr, 512, bg.attr.palette, false, bits_per_pixel,
                   bg.ctrl.blend);
    }

    return;
  }

  const int tile_width = 8 << bg.attr.hsize;
  const int tile_height = 8 << bg.attr.vsize;
  const int tilemap_ytile = tilemap_y / tile_height;
  const int tiles_per_row = 512 >> (bg.attr.hsize + 3);

  for (int screen_x = -(scroll_x % tile_width); screen_x < 320;
       screen_x += tile_width) {  // one tile

    const int tilemap_x = (screen_x + scroll_x) & 0x1ff;
    const int tilemap_xtile = tilemap_x / tile_width;

    const int tilemap_tilepos =
        bg.ctrl.wallpaper_mode ? 0 : tiles_per_row * tilemap_ytile + tilemap_xtile;

    addr_t num_addr = bg.tile_map_ptr + tilemap_tilepos;
    word_t ch = bus_.ReadWord(num_addr);

    if (!ch)
      continue;

    int palette = bg.attr.palette;
    bool vflip = bg.attr.vflip;
    bool hflip = bg.attr.hflip;
    bool blend = bg.ctrl.blend;

    if (!bg.ctrl.register_mode) {
      addr_t attr_addr = bg.attribute_map_ptr + (tilemap_tilepos >> 1);
      word_t attr_word = bus_.ReadWord(attr_addr);
      TileAttribute attr{static_cast<word_t>(attr_word >> word_t((tilemap_tilepos & 1) ? 8 : 0))};
      palette = attr.palette;
      vflip = attr.vflip;
      hflip = attr.hflip;
      blend = attr.blend;
    }

    const int tile_y =
        !vflip ? tilemap_y % tile_height : tile_height - (tilemap_y % tile_height) - 1;
    const int bits_per_pixel = (bg.attr.color_mode + 1) * 2;

    const addr_t addr = CalculateLineSegmentAddr(bg.segment_ptr, ch, tile_y, tile_width,
                                                 tile_height, bits_per_pixel);
    DrawTileLine(screen_y, screen_x, addr, tile_width, palette, hflip, bits_per_pixel, blend);
  }
}

void Ppu::DrawSpriteScanline(int sprite, int screen_y) {
  const auto& sprite_data = sprite_data_[sprite];
  const int tile_width = 8 << sprite_data.attr.hsize;
  const int tile_height = 8 << sprite_data.attr.vsize;
  const int xpos = (160 + sext<9>(sprite_data.xpos)) - tile_width / 2;
  const int ypos = (128 - sext<9>(sprite_data.ypos)) - tile_height / 2;
  const int bits_per_pixel = (sprite_data.attr.color_mode + 1) * 2;

  const int tile_y =
      !sprite_data.attr.vflip ? (screen_y - ypos) : (tile_height - 1) - (screen_y - ypos);

  if (tile_y < 0 || tile_y >= tile_height)
    return;

  addr_t addr = CalculateLineSegmentAddr(sprite_segment_ptr_, sprite_data.ch, tile_y, tile_width,
                                         tile_height, bits_per_pixel);
  DrawTileLine(screen_y, xpos, addr, tile_width, sprite_data.attr.palette, sprite_data.attr.hflip,
               bits_per_pixel, sprite_data.attr.blend);
}

void Ppu::DrawTileLine(int screen_y, int screen_x_start, addr_t line_addr, int tile_width,
                       unsigned palette, bool hflip, unsigned bits_per_pixel, bool blend) {
  int pixbuf_shift = -bits_per_pixel;
  uint32_t pixbuf = 0;
  addr_t addr = line_addr + (hflip ? ((tile_width * bits_per_pixel) / 16 - 1) : 0);

  // skip bus reads containing entirely unused pixels
  const int left_offscreen = screen_x_start < 0 ? -screen_x_start : 0;
  int skipped_pixels = 0;
  if (left_offscreen > 0) {
    int skipped_words = ((left_offscreen * bits_per_pixel) / 16);
    if (skipped_words != 0) {
      addr += hflip ? -skipped_words : skipped_words;
      skipped_pixels = DivideRoundUp(skipped_words * 16, bits_per_pixel);
      pixbuf_shift -= (skipped_pixels * bits_per_pixel) % 16;
    }
  }

  for (int screen_x = screen_x_start + skipped_pixels;
       screen_x < screen_x_start + tile_width && screen_x < 320; screen_x++) {
    if (pixbuf_shift < 0) {
      word_t val = bus_.ReadWord(addr);
      addr += hflip ? -1 : 1;
      if (bits_per_pixel != 16)
        val = (val >> 8) | (val << 8);
      pixbuf = hflip ? (val << 16) | (pixbuf >> 16) : (pixbuf << 16) | val;
      pixbuf_shift += 16;
    }

    const int pixbuf_shift_flip =
        hflip ? ((16 - bits_per_pixel) - pixbuf_shift) + 16 : pixbuf_shift;
    const int pixdata = (pixbuf >> pixbuf_shift_flip) & ((1 << bits_per_pixel) - 1);
    pixbuf_shift -= bits_per_pixel;

    if (screen_x < 0)
      continue;

    Color newpixel;
    switch (bits_per_pixel) {
      case 2:
      case 4:
        newpixel.raw = palette_memory_[palette * 16 + pixdata];
        break;
      case 6:
        newpixel.raw = palette_memory_[(palette >> 2) * 64 + pixdata];
        break;
      case 8:
        newpixel.raw = palette_memory_[pixdata];
        break;
      case 16:
        newpixel.raw = pixdata;
        break;
      default:
        __builtin_unreachable();
    }

    if (newpixel.transparent)
      continue;

    if (blend) {
      Color oldpixel = framebuffer_[screen_y][screen_x];
      if (!oldpixel.transparent) {
        newpixel.r = BlendInterpolate(oldpixel.r, newpixel.r, blend_level_);
        newpixel.g = BlendInterpolate(oldpixel.g, newpixel.g, blend_level_);
        newpixel.b = BlendInterpolate(oldpixel.b, newpixel.b, blend_level_);
      }
    }

    framebuffer_[screen_y][screen_x] = newpixel;
  }
}

std::span<uint8_t> Ppu::GetFramebuffer() const {
  return {(uint8_t*)&framebuffer_, sizeof(framebuffer_)};
}
