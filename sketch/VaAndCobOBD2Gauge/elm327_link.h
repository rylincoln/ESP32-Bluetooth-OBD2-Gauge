// The gauge's one connection to the ELM327 adapter. It owns the setup sequence, the
// order requests go out in, the one request allowed in flight and the reply being
// collected. Nothing here knows about Bluetooth, the screen or the layouts.
#pragma once

#include <stddef.h>
#include <stdint.h>

// The byte stream to the adapter. The firmware's version is Bluetooth; tests script
// their own.
class ElmTransport {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual void write(const char* text) = 0;
  virtual void flush() = 0;
  virtual void wait(uint32_t ms) = 0;

 protected:
  ~ElmTransport() {}
};

class Elm327Link {
 public:
  static const uint8_t kMaxRequests = 8;
  static const size_t kReplySize = 512;  // longer replies are cut to fit
  typedef void (*ReplyObserver)(const char* reply);

  explicit Elm327Link(ElmTransport& transport);

  // Sends the adapter's setup commands in order, with the wait each one needs.
  // Blocks for about 2.7 seconds. The observer, if any, sees each reply.
  void initialise(ReplyObserver onReply = nullptr);

  // Sets what a round polls, in slot order, and restarts the skip counts. Keeps the
  // current slot. The codes must stay valid until the next call.
  void setRequests(const char* const* codes, const uint8_t* skips, uint8_t count);

  // Polling. Call stepDue() often, which also collects any bytes that have arrived.
  // When it returns true, use the reply if hasReply() is true, then call advance().
  bool stepDue();
  bool hasReply() const { return !skipping_; }  // false when this slot is skipped this round
  uint8_t slot() const { return slot_; }
  const char* reply() const { return reply_; }

  // Moves to the next slot and requests it, or skips it for this round. Returns true
  // when a round has just ended.
  bool advance();

  // Makes a step due without waiting for a prompt.
  void requestStep() { stepDue_ = true; }

  // Throws away anything in progress so the next advance() requests the first slot.
  void restartRound();

  // Sends one command and returns whatever the adapter answers within half a second,
  // without its prompt. Blocks. The text stays valid until the next exchange().
  const char* exchange(const char* command);

  // Sends one command without waiting for an answer.
  void send(const char* command);

 private:
  void drain();
  void clearReply();
  static void append(char* buffer, size_t* length, char c);

  ElmTransport& transport_;
  const char* codes_[kMaxRequests];
  uint8_t skips_[kMaxRequests];
  uint8_t skipsLeft_[kMaxRequests];
  uint8_t count_;
  uint8_t slot_;
  bool stepDue_;
  bool skipping_;
  char reply_[kReplySize];
  size_t replyLength_;
  char exchangeReply_[kReplySize];
};
