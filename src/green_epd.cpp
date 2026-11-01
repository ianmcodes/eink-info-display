#include "green_epd.h"

namespace {
constexpr uint16_t WIDTH_BYTES = 400 / 8;
constexpr uint16_t HEIGHT = 300;
constexpr uint32_t BUSY_TIMEOUT_MS = 10000;

// Elecrow's green-sticker example uses this GC waveform before a refresh.
const uint8_t LUT_GC[5][42] = {
    {0x01, 0x14, 0x0A, 0x14, 0x00, 0x01, 0x01},
    {0x01, 0x54, 0x0A, 0x94, 0x00, 0x01, 0x01},
    {0x01, 0x54, 0x0A, 0x94, 0x00, 0x01, 0x01},
    {0x01, 0x94, 0x0A, 0x54, 0x00, 0x01, 0x01},
    {0x01, 0x94, 0x0A, 0x54, 0x00, 0x01, 0x01},
};
}

GreenEpd::GreenEpd(uint8_t sck, uint8_t mosi, uint8_t rst, uint8_t dc,
                   uint8_t cs, uint8_t busy)
    : _sck(sck), _mosi(mosi), _rst(rst), _dc(dc), _cs(cs), _busy(busy) {}

void GreenEpd::begin()
{
  pinMode(_sck, OUTPUT);
  pinMode(_mosi, OUTPUT);
  pinMode(_rst, OUTPUT);
  pinMode(_dc, OUTPUT);
  pinMode(_cs, OUTPUT);
  pinMode(_busy, INPUT);
  digitalWrite(_sck, LOW);
  digitalWrite(_mosi, LOW);
  digitalWrite(_cs, HIGH);
  digitalWrite(_dc, HIGH);
}

void GreenEpd::writeByte(uint8_t value)
{
  digitalWrite(_cs, LOW);
  for (uint8_t bit = 0; bit < 8; ++bit) {
    digitalWrite(_sck, LOW);
    digitalWrite(_mosi, (value & 0x80) ? HIGH : LOW);
    digitalWrite(_sck, HIGH);
    value <<= 1;
  }
  digitalWrite(_cs, HIGH);
}

void GreenEpd::command(uint8_t value)
{
  digitalWrite(_dc, LOW);
  writeByte(value);
  digitalWrite(_dc, HIGH);
}

void GreenEpd::data(uint8_t value)
{
  digitalWrite(_dc, HIGH);
  writeByte(value);
}

void GreenEpd::reset()
{
  digitalWrite(_rst, HIGH);
  delay(10);
  digitalWrite(_rst, LOW);
  delay(100);
  digitalWrite(_rst, HIGH);
  delay(100);
}

void GreenEpd::waitBusy(const char* operation)
{
  const uint32_t started = millis();
  while (digitalRead(_busy) != LOW) {
    if (millis() - started >= BUSY_TIMEOUT_MS) {
      Serial.printf("EPD busy timeout during %s (BUSY=%d)\n", operation,
                    digitalRead(_busy));
      return;
    }
    delay(1);
  }
}

void GreenEpd::initController()
{
  command(0x00);
  data(0x3F); data(0x4D);
  command(0x01);
  data(0x03); data(0x10); data(0x3F); data(0x3F); data(0x03);
  command(0x06);
  data(0x96); data(0x96); data(0x29);
  command(0x30); data(0x09);
  command(0x61);
  data(0x01); data(0x90); data(0x01); data(0x2C);
  command(0x82); data(0x05);
  command(0x50); data(0x97);
  command(0x60); data(0x22);
  command(0xE3); data(0x88);
}

void GreenEpd::loadGcLut()
{
  for (uint8_t row = 0; row < 5; ++row) {
    command(0x20 + row);
    for (uint8_t i = 0; i < 42; ++i) data(LUT_GC[row][i]);
  }
}

void GreenEpd::update()
{
  command(0x17);
  data(0xA5);
  waitBusy("refresh");
}

void GreenEpd::writeWhiteBuffer(uint8_t commandValue)
{
  command(commandValue);
  for (uint16_t i = 0; i < WIDTH_BYTES * HEIGHT; ++i) data(0xFF);
}

void GreenEpd::clear()
{
  reset();
  initController();
  writeWhiteBuffer(0x10);
  writeWhiteBuffer(0x13);
  loadGcLut();
  update();
}

void GreenEpd::display(const uint8_t* image)
{
  command(0x50);
  data(0xD7);
  command(0x13);
  for (uint16_t i = 0; i < WIDTH_BYTES * HEIGHT; ++i) data(image[i]);
  loadGcLut();
  update();
}
