#pragma once

class VSmileJoySend {
public:
  virtual ~VSmileJoySend(){};

  virtual void SetRts(bool value) = 0;
  virtual void Tx(uint8_t byte) = 0;
};