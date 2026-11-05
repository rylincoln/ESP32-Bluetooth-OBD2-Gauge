#include "elm327_link.h"
#include "test.h"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace {

// Stands in for the ELM327 adapter: answers scripted commands and records what the
// link sent. Commands with no script get no answer, like an adapter that's silent.
class FakeAdapter : public ElmTransport {
 public:
  std::map<std::string, std::string> script;
  std::deque<char> incoming;
  std::vector<std::string> sent;
  std::vector<uint32_t> waits;
  int flushes = 0;

  int available() override { return (int)incoming.size(); }
  int read() override {
    char c = incoming.front();
    incoming.pop_front();
    return (unsigned char)c;
  }
  void write(const char* text) override {
    sent.push_back(text);
    auto answer = script.find(text);
    if (answer != script.end()) arrive(answer->second);
  }
  void flush() override { flushes++; }
  void wait(uint32_t ms) override { waits.push_back(ms); }

  void arrive(const std::string& bytes) {
    for (char c : bytes) incoming.push_back(c);
  }
};

const char* const kCodes[3] = {"0105", "010C", "0142"};
const uint8_t kNoSkips[3] = {0, 0, 0};

std::vector<std::string> observed;
void observe(const char* reply) { observed.push_back(reply); }

}  // namespace

// ---- initialise ----

TEST(setup_commands_go_out_in_order) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.initialise();
  const char* expected[] = {"ATZ\r", "ATBRD23\r", "ATSP0\r", "ATAT2\r", "ATL0\r", "ATH0\r", "ATE0\r", "0100\r"};
  CHECK_EQ(adapter.sent.size(), 8);
  for (size_t i = 0; i < adapter.sent.size() && i < 8; i++) CHECK_STR(adapter.sent[i].c_str(), expected[i]);
}

TEST(setup_waits_longest_after_the_search_command) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.initialise();
  CHECK_EQ(adapter.waits.size(), 8);
  for (size_t i = 0; i + 1 < adapter.waits.size(); i++) CHECK_EQ(adapter.waits[i], 100);
  CHECK_EQ(adapter.waits.back(), 2000);
}

TEST(baud_rate_change_gets_an_extra_return_once_its_prompt_arrives) {
  FakeAdapter adapter;
  adapter.script["ATBRD23\r"] = "OK\r\r>";
  Elm327Link link(adapter);
  link.initialise();
  CHECK_EQ(adapter.sent.size(), 9);
  CHECK_STR(adapter.sent[1].c_str(), "ATBRD23\r");
  CHECK_STR(adapter.sent[2].c_str(), "\r");
  CHECK_STR(adapter.sent[3].c_str(), "ATSP0\r");
}

TEST(baud_rate_change_without_a_prompt_gets_no_extra_return) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.initialise();
  CHECK_STR(adapter.sent[2].c_str(), "ATSP0\r");
}

TEST(other_setup_prompts_get_no_extra_return) {
  FakeAdapter adapter;
  adapter.script["ATZ\r"] = "ELM327 v1.5\r\r>";
  adapter.script["ATSP0\r"] = "OK\r\r>";
  Elm327Link link(adapter);
  link.initialise();
  CHECK_EQ(adapter.sent.size(), 8);
}

TEST(setup_reports_each_reply_to_the_observer) {
  FakeAdapter adapter;
  adapter.script["ATZ\r"] = "ELM327 v1.5\r\r>";
  adapter.script["ATSP0\r"] = "OK\r\r>";
  observed.clear();
  Elm327Link link(adapter);
  link.initialise(observe);
  CHECK_EQ(observed.size(), 2);
  CHECK_STR(observed[0].c_str(), "ELM327 v1.5\r\r");
  CHECK_STR(observed[1].c_str(), "OK\r\r");
}

TEST(setup_prompts_set_no_step_due) {
  FakeAdapter adapter;
  adapter.script["ATZ\r"] = "ELM327 v1.5\r\r>";
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  link.initialise();
  CHECK(!link.stepDue());
}

TEST(setup_leaves_no_finished_reply_behind) {
  FakeAdapter adapter;
  adapter.script["0100\r"] = "41 00 BE 3E B8 11 \r\r>";
  Elm327Link link(adapter);
  link.initialise();
  CHECK_STR(link.reply(), "");
}

TEST(setup_keeps_an_unfinished_reply_for_polling_to_collect) {
  FakeAdapter adapter;
  adapter.script["0100\r"] = "SEARCHING...";
  Elm327Link link(adapter);
  link.initialise();
  CHECK_STR(link.reply(), "SEARCHING...");
}

// ---- polling ----

TEST(nothing_is_due_until_asked_or_a_prompt_arrives) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  CHECK(!link.stepDue());
}

TEST(a_requested_step_is_due_once) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  link.requestStep();
  CHECK(link.stepDue());
  CHECK(!link.stepDue());
}

TEST(advance_requests_the_next_slot_with_a_return) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  link.requestStep();
  CHECK(link.stepDue());
  CHECK_EQ(link.slot(), 0);
  link.advance();
  CHECK_EQ(link.slot(), 1);
  CHECK_EQ(adapter.sent.size(), 1);
  CHECK_STR(adapter.sent[0].c_str(), "010C\r");
}

TEST(only_one_request_is_in_flight_until_its_prompt_arrives) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  link.requestStep();
  CHECK(link.stepDue());
  link.advance();
  CHECK(!link.stepDue());
  adapter.arrive("41 0C 1A F8 \r\r");  // reply text, but no prompt yet
  CHECK(!link.stepDue());
  CHECK_EQ(adapter.sent.size(), 1);
  adapter.arrive(">");
  CHECK(link.stepDue());
  CHECK(link.hasReply());
  CHECK_STR(link.reply(), "41 0C 1A F8 \r\r");
}

TEST(a_prompt_flushes_the_adapter) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  adapter.arrive("41 05 7B \r\r>");
  CHECK(link.stepDue());
  CHECK_EQ(adapter.flushes, 1);
}

TEST(advance_clears_the_reply_for_the_next_request) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  adapter.arrive("41 05 7B \r\r>");
  CHECK(link.stepDue());
  link.advance();
  CHECK_STR(link.reply(), "");
  adapter.arrive("41 0C 1A F8 \r\r>");
  CHECK(link.stepDue());
  CHECK_STR(link.reply(), "41 0C 1A F8 \r\r");
}

TEST(advance_reports_the_end_of_each_round) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  CHECK(!link.advance());  // slot 0 -> 1
  CHECK(!link.advance());  // slot 1 -> 2
  CHECK(link.advance());   // slot 2 -> 0
  CHECK_EQ(link.slot(), 0);
  CHECK_STR(adapter.sent[2].c_str(), "0105\r");
}

TEST(slow_pid_is_polled_every_other_round_when_it_skips_one) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  const uint8_t skips[3] = {0, 1, 0};
  link.setRequests(kCodes, skips, 3);

  // Slot 1 starts with one skip left, so the first pass over it sends nothing.
  link.advance();
  CHECK_EQ(link.slot(), 1);
  CHECK(!link.hasReply());
  CHECK(link.stepDue());  // a skipped step is due straight away
  CHECK_EQ(adapter.sent.size(), 0);

  link.advance();  // slot 2
  link.advance();  // slot 0
  link.advance();  // slot 1, whose skip has run out
  CHECK(link.hasReply());
  CHECK_STR(adapter.sent.back().c_str(), "010C\r");

  link.advance();  // slot 2
  link.advance();  // slot 0
  link.advance();  // slot 1 again: reset to one skip, so nothing is sent
  CHECK_EQ(link.slot(), 1);
  CHECK(!link.hasReply());
}

TEST(setting_new_requests_keeps_the_current_slot) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  link.advance();
  const char* const others[3] = {"0104", "010B", "0142"};
  link.setRequests(others, kNoSkips, 3);
  CHECK_EQ(link.slot(), 1);
  link.advance();
  CHECK_STR(adapter.sent.back().c_str(), "0142\r");
}

TEST(setting_new_requests_restarts_the_skip_counts) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  const uint8_t skips[3] = {0, 1, 0};
  link.setRequests(kCodes, skips, 3);
  link.advance();  // slot 1, using up its skip
  link.setRequests(kCodes, skips, 3);
  link.advance();  // slot 2
  link.advance();  // slot 0
  link.advance();  // slot 1 is skipped again
  CHECK(!link.hasReply());
}

TEST(restarting_a_round_discards_the_old_reply_and_bytes) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  adapter.arrive("41 05 7B \r\r>");
  CHECK(link.stepDue());
  adapter.arrive("41 0C");  // a late reply to something from the old layout

  link.restartRound();

  CHECK_EQ(adapter.available(), 0);
  CHECK(adapter.flushes >= 2);
  CHECK(link.stepDue());
  CHECK(link.hasReply());
  CHECK_STR(link.reply(), "");
  CHECK_EQ(link.slot(), 2);  // so that advancing starts again from slot 0
}

TEST(restarted_round_requests_the_first_slot_next) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  link.advance();
  link.restartRound();
  CHECK(link.stepDue());
  CHECK(link.advance());
  CHECK_EQ(link.slot(), 0);
  CHECK_STR(adapter.sent.back().c_str(), "0105\r");
}

TEST(restarting_a_round_ends_a_skipped_step) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  const uint8_t skips[3] = {0, 1, 0};
  link.setRequests(kCodes, skips, 3);
  link.advance();
  CHECK(!link.hasReply());
  link.restartRound();
  CHECK(link.hasReply());
}

TEST(advancing_with_no_requests_does_nothing) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  CHECK(!link.advance());
  CHECK_EQ(adapter.sent.size(), 0);
}

TEST(reply_longer_than_the_buffer_is_cut_without_overrunning) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  adapter.arrive(std::string(Elm327Link::kReplySize * 2, 'A'));
  adapter.arrive(">");
  CHECK(link.stepDue());
  CHECK_EQ(std::strlen(link.reply()), Elm327Link::kReplySize - 1);
}

// ---- exchange and send ----

TEST(exchange_clears_old_bytes_then_returns_the_reply_without_its_prompt) {
  FakeAdapter adapter;
  adapter.script["0101\r"] = "41 01 86 0E \r\r>";
  adapter.arrive("stale");
  Elm327Link link(adapter);
  CHECK_STR(link.exchange("0101"), "41 01 86 0E \r\r");
  CHECK_STR(adapter.sent[0].c_str(), "0101\r");
}

TEST(exchange_gives_the_adapter_half_a_second_to_answer) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.exchange("03");
  CHECK_EQ(adapter.waits.size(), 1);
  CHECK_EQ(adapter.waits[0], 500);
}

TEST(exchange_returns_empty_when_the_adapter_is_silent) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  CHECK_STR(link.exchange("0902"), "");
}

TEST(exchange_leaves_the_polling_reply_and_step_alone) {
  FakeAdapter adapter;
  adapter.script["03\r"] = "43 01 00 7D \r\r>";
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  adapter.arrive("41 05 7B \r\r>");
  CHECK(link.stepDue());
  link.exchange("03");
  CHECK_STR(link.reply(), "41 05 7B \r\r");
  CHECK(!link.stepDue());
}

TEST(exchange_reply_longer_than_the_buffer_is_cut_without_overrunning) {
  FakeAdapter adapter;
  adapter.script["03\r"] = std::string(Elm327Link::kReplySize * 2, 'B');
  Elm327Link link(adapter);
  CHECK_EQ(std::strlen(link.exchange("03")), Elm327Link::kReplySize - 1);
}

TEST(send_appends_a_return_in_one_write) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.send("ATLP");
  CHECK_EQ(adapter.sent.size(), 1);
  CHECK_STR(adapter.sent[0].c_str(), "ATLP\r");
}

TEST(send_leaves_polling_alone) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  link.setRequests(kCodes, kNoSkips, 3);
  link.advance();
  link.send("04");
  CHECK_EQ(link.slot(), 1);
  CHECK(!link.stepDue());
}

TEST(very_long_command_is_cut_but_still_ends_with_a_return) {
  FakeAdapter adapter;
  Elm327Link link(adapter);
  std::string command(200, 'A');
  link.send(command.c_str());
  CHECK(adapter.sent[0].size() < 200);
  CHECK(adapter.sent[0].back() == '\r');
}

TEST_MAIN()
