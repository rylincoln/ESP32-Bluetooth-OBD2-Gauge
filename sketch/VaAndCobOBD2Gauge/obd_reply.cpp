#include "obd_reply.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

const size_t kTokenSize = 32;   // longer tokens are cut; no value in a reply needs this much
const int kMaxReplyBytes = 128; // the most data bytes a reply to 03 is read for

// Copies token number `index` of the space-separated text, or an empty string past
// the end. Runs of spaces count as empty tokens.
void copyToken(const char* text, int index, char* token) {
  token[0] = '\0';
  const char* start = text;
  for (int i = 0; i < index; i++) {
    start = strchr(start, ' ');
    if (start == nullptr) return;
    start++;
  }
  size_t length = strcspn(start, " ");
  if (length >= kTokenSize) length = kTokenSize - 1;
  memcpy(token, start, length);
  token[length] = '\0';
}

uint8_t parseHexByte(const char* token) {
  return (uint8_t)strtol(token, nullptr, 16);
}

uint8_t hexByteAt(const char* text, int index) {
  char token[kTokenSize];
  copyToken(text, index, token);
  return parseHexByte(token);
}

// A view of the text with surrounding whitespace removed.
struct Trimmed {
  const char* start;
  size_t length;
};

Trimmed trim(const char* text) {
  while (isspace((unsigned char)*text)) text++;
  size_t length = strlen(text);
  while (length > 0 && isspace((unsigned char)text[length - 1])) length--;
  Trimmed result = {text, length};
  return result;
}

// Fills `token` with the next run of characters not in `separators` and moves
// `cursor` past it. Returns false when the text runs out. When `isLast` is given,
// it reports whether nothing but separators remains.
bool nextToken(const char*& cursor, const char* end, const char* separators, char* token, bool* isLast) {
  while (cursor < end && strchr(separators, *cursor) != nullptr) cursor++;
  if (cursor >= end) return false;
  const char* start = cursor;
  while (cursor < end && strchr(separators, *cursor) == nullptr) cursor++;
  size_t length = (size_t)(cursor - start);
  if (length >= kTokenSize) length = kTokenSize - 1;
  memcpy(token, start, length);
  token[length] = '\0';
  if (isLast != nullptr) {
    const char* rest = cursor;
    while (rest < end && strchr(separators, *rest) != nullptr) rest++;
    *isLast = rest >= end;
  }
  return true;
}

// The system letter and first digit of a code come from the top nibble of its
// first byte.
const char* const kDtcPrefixes[16] = {"P0", "P1", "P2", "P3", "C0", "C1", "C2", "C3",
                                      "B0", "B1", "B2", "B3", "U0", "U1", "U2", "U3"};

void formatDtc(uint8_t high, uint8_t low, DtcCode* code) {
  static const char kHex[] = "0123456789ABCDEF";
  const char* prefix = kDtcPrefixes[high >> 4];
  code->text[0] = prefix[0];
  code->text[1] = prefix[1];
  code->text[2] = kHex[high & 0x0F];
  code->text[3] = kHex[low >> 4];
  code->text[4] = kHex[low & 0x0F];
  code->text[5] = '\0';
}

char vinCharacter(uint8_t byte) {
  return (byte > 0x20 && byte < 0x7F) ? (char)byte : '\0';
}

}  // namespace

ReplyKind decodeBytes(const char* reply, const char* pidCode, uint8_t* a, uint8_t* b) {
  size_t length = strlen(reply);
  if (length == 0 || reply[length - 1] == ' ') return ReplyKind::Empty;
  // The reply repeats the PID code, one byte per two hex digits, ahead of its data.
  int dataStart = (int)(strlen(pidCode) / 2);
  *a = hexByteAt(reply, dataStart);
  *b = hexByteAt(reply, dataStart + 1);
  return ReplyKind::Value;
}

float toReading(Formula formula, uint8_t a, uint8_t b, float previous) {
  float reading = 0.0f;
  switch (formula) {
    case Formula::Psi: reading = a * 0.145; break;
    case Formula::Celsius: reading = a - 40; break;
    case Formula::Percent: reading = a * 100.0 / 255; break;
    case Formula::Rpm: reading = (256 * a + b) / 4.0; break;
    case Formula::Volts: reading = (256 * a + b) / 1000.0; break;
    case Formula::TransT6: reading = (256 * a + b) / 16.0; break;
    case Formula::TransT5: reading = (a * 256 + b) * 5 / 72 - 18; break;
  }
  if ((formula == Formula::TransT6 || formula == Formula::TransT5) &&
      (reading > 150 || reading < 0)) {
    return previous;
  }
  return reading;
}

bool isWarning(float reading, int warningValue) {
  return fabsf(reading) > fabsf((float)warningValue);
}

bool decodeMil(const char* reply, uint8_t* a) {
  *a = 0xFF;
  char text[kTokenSize * 2];
  Trimmed trimmed = trim(reply);
  if (trimmed.length == 0) return false;
  // Only the header and first data byte matter, so a short copy is enough.
  size_t length = trimmed.length < sizeof text ? trimmed.length : sizeof text - 1;
  memcpy(text, trimmed.start, length);
  text[length] = '\0';

  char token[kTokenSize];
  copyToken(text, 0, token);
  if (strcmp(token, "41") != 0) return false;
  copyToken(text, 1, token);
  if (strcmp(token, "01") != 0) return false;
  *a = hexByteAt(text, 2);
  return true;
}

int decodeDtcs(const char* reply, DtcCode* codes, int maxCodes) {
  uint8_t bytes[kMaxReplyBytes];
  int byteCount = 0;
  int announcedBytes = 0;  // multi-line replies open with their length

  const char* cursor = reply;
  const char* end = reply + strlen(reply);
  char token[kTokenSize];
  while (byteCount < kMaxReplyBytes && nextToken(cursor, end, " \r\n\t", token, nullptr)) {
    if (strchr(token, ':') != nullptr) continue;  // a line number such as "0:"
    if (strlen(token) >= 3) {
      announcedBytes = (int)strtol(token, nullptr, 16);
      continue;
    }
    bytes[byteCount++] = parseHexByte(token);
    if (byteCount == announcedBytes) break;  // anything after is padding
  }

  if (byteCount == 0 || bytes[0] != 0x43) return -1;

  // Each computer's list is a 43, a count of its codes, then that many pairs of bytes.
  // A code can itself start with the byte 43, so only the count says where a list ends.
  int listed = 0;
  int i = 0;
  while (i + 1 < byteCount && bytes[i] == 0x43 && listed < maxCodes) {
    int codesLeft = bytes[i + 1];
    i += 2;
    for (; codesLeft > 0 && i + 1 < byteCount && listed < maxCodes; codesLeft--) {
      formatDtc(bytes[i], bytes[i + 1], &codes[listed]);
      listed++;
      i += 2;
    }
  }
  return listed;
}

bool decodeVin(const char* reply, char* vin, size_t size) {
  vin[0] = '\0';
  Trimmed trimmed = trim(reply);
  const char* cursor = trimmed.start;
  const char* end = trimmed.start + trimmed.length;

  const int kHeaderBytes = 3;  // 49 02 01 come ahead of the VIN
  int announcedBytes = 0;
  int byteCount = 0;
  size_t written = 0;

  char token[kTokenSize];
  bool isLast = false;
  while (nextToken(cursor, end, " ", token, &isLast)) {
    bool isByte = isLast || (strchr(token, ':') == nullptr && strlen(token) < 3);
    if (isByte) {
      if (byteCount >= kHeaderBytes && written + 1 < size) {
        vin[written++] = vinCharacter(parseHexByte(token));
        vin[written] = '\0';
      }
      byteCount++;
    } else if (strchr(token, ':') == nullptr) {
      announcedBytes = (uint8_t)strtol(token, nullptr, 16);
    }
    if (isLast) break;
  }
  if (byteCount == 0 || byteCount != announcedBytes) {
    vin[0] = '\0';
    return false;
  }
  return true;
}
