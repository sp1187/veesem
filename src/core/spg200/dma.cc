#include "dma.h"

#include "bus_interface.h"

Dma::Dma(BusInterface& bus) : bus_(bus){};

void Dma::Reset() {
  source_ = 0;
  target_ = 0;
  length_ = 0;
}

word_t Dma::GetSourceLo() {
  return source_ & 0xffff;
}

void Dma::SetSourceLo(word_t value) {
  source_ = (source_ & ~0xffff) | value;
}

word_t Dma::GetSourceHi() {
  return (source_ >> 16) & 0x3f;
}

void Dma::SetSourceHi(word_t value) {
  source_ = ((value & 0x3f) << 16) | (source_ & 0xffff);
}

word_t Dma::GetLength() {
  return length_;
}

void Dma::StartDma(word_t value) {
  length_ = value;
  while (length_) {
    word_t word = bus_.ReadWord(source_++);
    bus_.WriteWord(target_++, word);
    target_ &= 0x3fff;
    length_--;
  }
}

word_t Dma::GetTarget() {
  return target_;
}

void Dma::SetTarget(word_t value) {
  target_ = value & 0x3fff;
}