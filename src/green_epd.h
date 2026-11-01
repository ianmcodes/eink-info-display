#pragma once

#include <Arduino.h>

class GreenEpd {
 public:
  GreenEpd(uint8_t sck, uint8_t mosi, uint8_t rst, uint8_t dc,
           uint8_t cs, uint8_t busy);

  void begin();
  void clear();
  void display(const uint8_t* image);

 private:
  void writeByte(uint8_t value);
  void command(uint8_t value);
  void data(uint8_t value);
  void reset();
  void waitBusy(const char* operation);
  void initController();
  void update();
  void loadGcLut();
  void writeWhiteBuffer(uint8_t commandValue);

  uint8_t _sck;
  uint8_t _mosi;
  uint8_t _rst;
  uint8_t _dc;
  uint8_t _cs;
  uint8_t _busy;
};
