#pragma once

#include <stdint.h>
#include <fstream>
#include <span>

class WavWriter {
public:
  WavWriter(std::string filename);
  bool IsOpen();
  bool IsGood();
  void WriteHeader();
  void WriteData(std::span<uint16_t> data);
  void Close();

private:
  std::ofstream wav_file_;
};
