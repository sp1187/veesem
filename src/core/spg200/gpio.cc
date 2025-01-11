#include "gpio.h"

#include "spg200_io.h"

Gpio::Gpio(Spg200Io& io) : io_(io) {}

void Gpio::Reset() {
  ports_.fill({});
}

word_t Gpio::GetMode() {
  return mode_.raw;
}

void Gpio::SetMode(word_t value) {
  mode_.raw = value & IoMode::WriteMask;
}

word_t Gpio::GetData(int port_index) {
  return ReadGpio(port_index);
}

word_t Gpio::GetBuffer(int port_index) {
  return ports_[port_index].buffer;
}

void Gpio::SetBuffer(int port_index, word_t value) {
  ports_[port_index].buffer = value;
  WriteGpio(port_index);
}

word_t Gpio::GetDir(int port_index) {
  return ports_[port_index].dir;
}

void Gpio::SetDir(int port_index, word_t value) {
  ports_[port_index].dir = value;
  WriteGpio(port_index);
}

word_t Gpio::GetAttrib(int port_index) {
  return ports_[port_index].attrib;
}

void Gpio::SetAttrib(int port_index, word_t value) {
  ports_[port_index].attrib = value;
  WriteGpio(port_index);
}

word_t Gpio::GetMask(int port_index) {
  return ports_[port_index].mask;
}

void Gpio::SetMask(int port_index, word_t value) {
  ports_[port_index].mask = value;
  WriteGpio(port_index);
}

word_t Gpio::ReadGpio(int port_index) {
  Port& port = ports_[port_index];
  uint16_t buf = (port.buffer ^ (port.dir & ~port.attrib));
  uint16_t io_out = 0;

  switch (port_index) {
    case 0:
      io_out = io_.GetPortA();
      break;
    case 1:
      io_out = io_.GetPortB();
      break;
    case 2:
      io_out = io_.GetPortC();
      break;
  }

  return (buf & (port.dir & ~port.mask)) | (io_out & (~port.dir & ~port.mask));
}

void Gpio::WriteGpio(int port_index) {
  Port& port = ports_[port_index];
  uint16_t buf = (port.buffer ^ (port.dir & ~port.attrib)) & ~port.mask;
  uint16_t bits = port.dir & ~port.mask;

  switch (port_index) {
    case 0:
      io_.SetPortA(buf, bits);
      break;
    case 1:
      io_.SetPortB(buf, bits);
      break;
    case 2:
      io_.SetPortC(buf, bits);
      break;
  }
}
