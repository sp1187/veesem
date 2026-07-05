#include "dma.h"

#include "bus_interface.h"

Dma::Dma(BusInterface& bus) : bus_(bus) {};

void Dma::Reset() {
  source_ = 0;
  target_ = 0;
  length_ = 0;
}

Word Dma::GetSourceLo() {
  return source_ & 0xffff;
}

void Dma::SetSourceLo(Word value) {
  source_ = (source_ & ~0xffff) | value;
}

Word Dma::GetSourceHi() {
  return (source_ >> 16) & 0x3f;
}

void Dma::SetSourceHi(Word value) {
  source_ = ((value & 0x3f) << 16) | (source_ & 0xffff);
}

Word Dma::GetLength() {
  return length_;
}

void Dma::StartDma(Word value) {
  length_ = value;
  while (length_) {
    Word word = bus_.ReadWord(source_++);
    bus_.WriteWord(target_++, word);
    target_ &= 0x3fff;
    length_--;
  }
}

Word Dma::GetTarget() {
  return target_;
}

void Dma::SetTarget(Word value) {
  target_ = value & 0x3fff;
}