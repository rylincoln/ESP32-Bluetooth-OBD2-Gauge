// Turns ELM327 reply text into values. It takes text and returns values, with no
// globals, screen or Bluetooth, so the same code runs in test/.
#pragma once

#include <stddef.h>
#include <stdint.h>

// How a reply's data bytes become a reading.
enum class Formula : uint8_t {
  Psi,
  Celsius,
  Percent,
  Rpm,
  Volts,
  TransT6,  // Ford T6+ transmission temperature (221E1C)
  TransT5,  // Ford T5 transmission temperature (221674)
};

enum class ReplyKind : uint8_t {
  Value,  // *a and *b hold the decoded bytes
  Empty,  // nothing to decode, so *a and *b were left alone
};

// Finds the data bytes in a reply to pidCode. Text that holds no data bytes, such
// as NO DATA, decodes as zero, which auto-off relies on: a silent engine computer
// has to read as 0 volts.
ReplyKind decodeBytes(const char* reply, const char* pidCode, uint8_t* a, uint8_t* b);

// Works out a reading. The transmission formulas return previous when the result
// is outside 0-150, which is how they ride out a bad reply.
float toReading(Formula formula, uint8_t a, uint8_t b, float previous);

// True when a reading is further from zero than its warning value.
bool isWarning(float reading, int warningValue);

// Reads the first data byte of a reply to 0101. On failure, returns false and sets
// *a to 0xFF.
bool decodeMil(const char* reply, uint8_t* a);

struct DtcCode {
  char text[6];  // such as "U060F"
};
const int kMaxDtcs = 16;

// Lists the codes in a reply to 03, up to maxCodes. Returns how many it listed, or
// -1 when the text isn't a reply to 03 at all.
int decodeDtcs(const char* reply, DtcCode* codes, int maxCodes);

const size_t kVinSize = 18;

// Reads the VIN from a reply to 0902, cut to fit the buffer. Returns false when the
// reply is incomplete or isn't a reply to 0902.
bool decodeVin(const char* reply, char* vin, size_t size);
