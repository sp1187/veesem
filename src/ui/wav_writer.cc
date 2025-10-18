#include "wav_writer.h"

#include <SDL.h>
#include <cassert>

namespace {
unsigned char wav_header[] = {  // RIFF magic number
    'R', 'I', 'F', 'F',
    // file size
    0x00, 0x00, 0x00, 0x00,
    // WAVE magic number
    'W', 'A', 'V', 'E',

    // start of fmt chunk
    'f', 'm', 't', ' ',
    // chunk size
    0x10, 0x00, 0x00, 0x00,
    // PCM format
    0x01, 0x00,
    // 2 channels
    0x02, 0x00,
    // 281250 Hz sample rate
    0xa2, 0x4a, 0x04, 0x00,
    // 281250 * 2 * 2 bytes per second
    0x88, 0x2a, 0x11, 0x00,
    // 4 bytes per block
    0x04, 0x00,
    // 16 bits per sample
    0x10, 0x00,

    // start of data chunk
    'd', 'a', 't', 'a',
    // chunk size
    0x00, 0x00, 0x00, 0x00};
}

WavWriter::WavWriter(std::string filename) : wav_file_(filename, std::ios::binary) {}

void WavWriter::WriteHeader() {
  assert(IsOpen());

  wav_file_.write(reinterpret_cast<const char*>(&wav_header), sizeof wav_header);
}

bool WavWriter::IsOpen() {
  return wav_file_.is_open();
}

bool WavWriter::IsGood() {
  return wav_file_.good();
}

void WavWriter::WriteData(std::span<uint16_t> data) {
  assert(IsOpen());

  for (size_t i = 0; i < data.size(); i++) {
    uint16_t swapped = SDL_SwapLE16(data[i] ^ 0x8000);
    wav_file_.write(reinterpret_cast<const char*>(&swapped), sizeof swapped);
  }
}

void WavWriter::Close() {
  assert(IsOpen());

  long size = wav_file_.tellp();
  uint32_t swapped;

  wav_file_.seekp(4);
  swapped = SDL_SwapLE32(size - 8);
  wav_file_.write(reinterpret_cast<const char*>(&swapped), sizeof swapped);

  wav_file_.seekp(40);
  swapped = SDL_SwapLE32(size - sizeof wav_header);
  wav_file_.write(reinterpret_cast<const char*>(&swapped), sizeof swapped);

  wav_file_.close();
}
