#include "elm327_link.h"

#include <string.h>

namespace {

// ELM327 data sheet: https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf
// Replies are parsed on the assumption of this setup: headers and echo off, spaces on.
const char* const kSetupCommands[] = {"ATZ", "ATBRD23", "ATSP0", "ATAT2", "ATL0", "ATH0", "ATE0", "0100"};
const uint8_t kSetupCount = sizeof kSetupCommands / sizeof kSetupCommands[0];
const uint8_t kBaudRateCommand = 1;  // ATBRD23 needs a second return once it answers

const uint32_t kSetupWaitMs = 100;
const uint32_t kSearchWaitMs = 2000;  // the last command makes the adapter search for a protocol
const uint32_t kExchangeWaitMs = 500;

const size_t kCommandSize = 64;

}  // namespace

Elm327Link::Elm327Link(ElmTransport& transport)
    : transport_(transport), count_(0), slot_(0), stepDue_(false), skipping_(false), replyLength_(0) {
  reply_[0] = '\0';
  exchangeReply_[0] = '\0';
}

void Elm327Link::initialise(ReplyObserver onReply) {
  clearReply();
  for (uint8_t i = 0; i < kSetupCount; i++) {
    send(kSetupCommands[i]);
    transport_.wait(i == kSetupCount - 1 ? kSearchWaitMs : kSetupWaitMs);
    while (transport_.available() > 0) {
      char c = (char)transport_.read();
      if (c == '>') {
        transport_.flush();
        if (onReply != nullptr) onReply(reply_);
        if (i == kBaudRateCommand) transport_.write("\r");
        clearReply();
      } else {
        append(reply_, &replyLength_, c);
      }
    }
  }
}

void Elm327Link::setRequests(const char* const* codes, const uint8_t* skips, uint8_t count) {
  count_ = count < kMaxRequests ? count : kMaxRequests;
  for (uint8_t i = 0; i < count_; i++) {
    codes_[i] = codes[i];
    skips_[i] = skips[i];
    skipsLeft_[i] = skips[i];
  }
}

bool Elm327Link::stepDue() {
  while (transport_.available() > 0) {
    char c = (char)transport_.read();
    if (c == '>') {
      transport_.flush();
      stepDue_ = true;
    } else {
      append(reply_, &replyLength_, c);
    }
  }
  bool due = stepDue_;
  stepDue_ = false;
  return due;
}

bool Elm327Link::advance() {
  if (count_ == 0) return false;
  if (!skipping_) clearReply();

  bool roundEnded = false;
  slot_++;
  if (slot_ >= count_) {
    slot_ = 0;
    roundEnded = true;
  }

  if (skipsLeft_[slot_] == 0) {
    skipsLeft_[slot_] = skips_[slot_];
    skipping_ = false;
    send(codes_[slot_]);
  } else {
    skipsLeft_[slot_]--;
    skipping_ = true;
    stepDue_ = true;
  }
  return roundEnded;
}

void Elm327Link::restartRound() {
  transport_.flush();
  drain();
  clearReply();
  skipping_ = false;
  slot_ = count_ > 0 ? count_ - 1 : 0;
  stepDue_ = true;
}

const char* Elm327Link::exchange(const char* command) {
  drain();
  size_t length = 0;
  exchangeReply_[0] = '\0';
  send(command);
  transport_.wait(kExchangeWaitMs);
  while (transport_.available() > 0) {
    char c = (char)transport_.read();
    if (c == '>') {
      transport_.flush();
    } else {
      append(exchangeReply_, &length, c);
    }
  }
  return exchangeReply_;
}

void Elm327Link::send(const char* command) {
  char line[kCommandSize];
  size_t length = strlen(command);
  if (length > sizeof line - 2) length = sizeof line - 2;
  memcpy(line, command, length);
  line[length] = '\r';
  line[length + 1] = '\0';
  transport_.write(line);
}

void Elm327Link::drain() {
  while (transport_.available() > 0) transport_.read();
}

void Elm327Link::clearReply() {
  replyLength_ = 0;
  reply_[0] = '\0';
}

void Elm327Link::append(char* buffer, size_t* length, char c) {
  if (*length + 1 >= kReplySize) return;
  buffer[*length] = c;
  (*length)++;
  buffer[*length] = '\0';
}
