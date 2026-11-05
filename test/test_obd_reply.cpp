#include "obd_reply.h"
#include "test.h"

namespace {

struct Bytes {
  ReplyKind kind;
  uint8_t a;
  uint8_t b;
};

// Starts from a recognisable value so a test can see whether the decoder wrote.
Bytes decode(const char* reply, const char* pidCode) {
  Bytes result;
  result.a = 0x5A;
  result.b = 0xA5;
  result.kind = decodeBytes(reply, pidCode, &result.a, &result.b);
  return result;
}

}  // namespace

// ---- decodeBytes ----

TEST(one_byte_reply_gives_a_and_zero_b) {
  Bytes r = decode("41 05 7B \r\r", "0105");
  CHECK(r.kind == ReplyKind::Value);
  CHECK_EQ(r.a, 0x7B);
  CHECK_EQ(r.b, 0);
}

TEST(two_byte_reply_gives_a_and_b) {
  Bytes r = decode("41 0C 1A F8 \r\r", "010C");
  CHECK(r.kind == ReplyKind::Value);
  CHECK_EQ(r.a, 0x1A);
  CHECK_EQ(r.b, 0xF8);
}

TEST(three_byte_pid_code_skips_three_echo_bytes) {
  Bytes r = decode("62 1E 1C 0A 3C \r\r", "221E1C");
  CHECK(r.kind == ReplyKind::Value);
  CHECK_EQ(r.a, 0x0A);
  CHECK_EQ(r.b, 0x3C);
}

TEST(reply_without_trailing_space_still_decodes) {
  Bytes r = decode("41 05 7B\r\r", "0105");
  CHECK_EQ(r.a, 0x7B);
  CHECK_EQ(r.b, 0);
}

TEST(no_data_decodes_as_zero) {
  Bytes r = decode("NO DATA\r\r", "0142");
  CHECK(r.kind == ReplyKind::NonReply);
  CHECK_EQ(r.a, 0);
  CHECK_EQ(r.b, 0);
}

TEST(can_error_decodes_as_zero) {
  Bytes r = decode("CAN ERROR\r\r", "0142");
  CHECK(r.kind == ReplyKind::NonReply);
  CHECK_EQ(r.a, 0);
  CHECK_EQ(r.b, 0);
}

TEST(question_mark_decodes_as_zero) {
  Bytes r = decode("?\r\r", "0105");
  CHECK(r.kind == ReplyKind::NonReply);
  CHECK_EQ(r.a, 0);
  CHECK_EQ(r.b, 0);
}

TEST(unable_to_connect_decodes_as_zero) {
  // Reading the third word as hex used to turn the C of CONNECT into a data byte.
  Bytes r = decode("UNABLE TO CONNECT\r\r", "0105");
  CHECK(r.kind == ReplyKind::NonReply);
  CHECK_EQ(r.a, 0);
  CHECK_EQ(r.b, 0);
}

TEST(negative_reply_decodes_as_zero) {
  Bytes r = decode("7F 01 12 \r\r", "0105");
  CHECK(r.kind == ReplyKind::NonReply);
  CHECK_EQ(r.a, 0);
  CHECK_EQ(r.b, 0);
}

TEST(hex_that_is_not_a_positive_reply_decodes_as_zero) {
  Bytes r = decode("12 34 56 \r\r", "0105");
  CHECK(r.kind == ReplyKind::NonReply);
  CHECK_EQ(r.a, 0);
  CHECK_EQ(r.b, 0);
}

TEST(positive_start_followed_by_text_decodes_as_zero) {
  Bytes r = decode("41 0C ERROR\r\r", "0105");
  CHECK(r.kind == ReplyKind::NonReply);
  CHECK_EQ(r.a, 0);
  CHECK_EQ(r.b, 0);
}

TEST(empty_reply_leaves_the_bytes_alone) {
  Bytes r = decode("", "0105");
  CHECK(r.kind == ReplyKind::Empty);
  CHECK_EQ(r.a, 0x5A);
  CHECK_EQ(r.b, 0xA5);
}

TEST(reply_of_only_line_breaks_leaves_the_bytes_alone) {
  Bytes r = decode(" \r\r", "0105");
  CHECK(r.kind == ReplyKind::Empty);
  CHECK_EQ(r.a, 0x5A);
  CHECK_EQ(r.b, 0xA5);
}

TEST(reply_to_another_pid_leaves_the_bytes_alone) {
  Bytes r = decode("41 0C 1A F8 \r\r", "0105");
  CHECK(r.kind == ReplyKind::OtherPid);
  CHECK_EQ(r.a, 0x5A);
  CHECK_EQ(r.b, 0xA5);
}

TEST(reply_to_another_three_byte_pid_leaves_the_bytes_alone) {
  Bytes r = decode("62 16 74 0A 3C \r\r", "221E1C");
  CHECK(r.kind == ReplyKind::OtherPid);
  CHECK_EQ(r.a, 0x5A);
  CHECK_EQ(r.b, 0xA5);
}

TEST(reply_in_another_mode_leaves_the_bytes_alone) {
  Bytes r = decode("41 05 7B \r\r", "221E1C");
  CHECK(r.kind == ReplyKind::OtherPid);
  CHECK_EQ(r.a, 0x5A);
  CHECK_EQ(r.b, 0xA5);
}

TEST(text_ahead_of_the_reply_is_skipped) {
  Bytes r = decode("SEARCHING...\r41 05 7B \r\r", "0105");
  CHECK(r.kind == ReplyKind::Value);
  CHECK_EQ(r.a, 0x7B);
}

TEST(reply_ending_in_a_space_decodes) {
  Bytes r = decode("41 05 7B ", "0105");
  CHECK(r.kind == ReplyKind::Value);
  CHECK_EQ(r.a, 0x7B);
}

TEST(header_with_no_data_decodes_as_zero) {
  Bytes r = decode("41 05 \r\r", "0105");
  CHECK(r.kind == ReplyKind::Value);
  CHECK_EQ(r.a, 0);
  CHECK_EQ(r.b, 0);
}

TEST(data_bytes_that_match_the_header_stay_data) {
  Bytes r = decode("41 0C 41 0C \r\r", "010C");
  CHECK(r.kind == ReplyKind::Value);
  CHECK_EQ(r.a, 0x41);
  CHECK_EQ(r.b, 0x0C);
}

TEST(a_broken_off_header_does_not_hide_the_real_one) {
  Bytes r = decode("41 41 05 7B \r\r", "0105");
  CHECK(r.kind == ReplyKind::Value);
  CHECK_EQ(r.a, 0x7B);
}

TEST(reply_of_many_lines_does_not_overrun) {
  // Two engine computers answering: more bytes than any one reply should hold.
  Bytes r = decode("41 0C 1A F8 \r41 0C 1A F8 \r41 0C 1A F8 \r41 0C 1A F8 \r\r", "010C");
  CHECK_EQ(r.a, 0x1A);
  CHECK_EQ(r.b, 0xF8);
}

// ---- toReading ----

TEST(psi_reading) { CHECK_NEAR(toReading(Formula::Psi, 100, 0, 0), 14.5, 0.001); }
TEST(celsius_reading) { CHECK_NEAR(toReading(Formula::Celsius, 0x7B, 0, 0), 83.0, 0.001); }
TEST(celsius_reading_of_zero_bytes) { CHECK_NEAR(toReading(Formula::Celsius, 0, 0, 0), -40.0, 0.001); }
TEST(percent_reading) { CHECK_NEAR(toReading(Formula::Percent, 255, 0, 0), 100.0, 0.001); }
TEST(rpm_reading) { CHECK_NEAR(toReading(Formula::Rpm, 0x1A, 0xF8, 0), 1726.0, 0.001); }
TEST(volts_reading) { CHECK_NEAR(toReading(Formula::Volts, 0x36, 0xB0, 0), 14.0, 0.001); }
TEST(volts_reading_of_zero_bytes) { CHECK_NEAR(toReading(Formula::Volts, 0, 0, 0), 0.0, 0.001); }

TEST(t6_transmission_reading) { CHECK_NEAR(toReading(Formula::TransT6, 0x05, 0x00, 7), 80.0, 0.001); }
TEST(t6_transmission_keeps_previous_above_150) {
  CHECK_NEAR(toReading(Formula::TransT6, 0x20, 0x00, 77), 77.0, 0.001);
}

TEST(t5_transmission_uses_whole_number_arithmetic) {
  // 1000 * 5 / 72 truncates to 69, and 69 - 18 is 51. Floating point would give 51.44.
  CHECK_NEAR(toReading(Formula::TransT5, 0x03, 0xE8, 7), 51.0, 0.001);
}
TEST(t5_transmission_keeps_previous_below_zero) {
  CHECK_NEAR(toReading(Formula::TransT5, 0, 0, 33), 33.0, 0.001);
}
TEST(t5_transmission_keeps_previous_above_150) {
  CHECK_NEAR(toReading(Formula::TransT5, 0x0F, 0xA0, 33), 33.0, 0.001);
}

// ---- isWarning ----

TEST(warns_above_warning_value) { CHECK(isWarning(100.0f, 99)); }
TEST(no_warning_at_warning_value) { CHECK(!isWarning(99.0f, 99)); }
TEST(no_warning_below_warning_value) { CHECK(!isWarning(98.0f, 99)); }
TEST(no_warning_at_zero_reading) { CHECK(!isWarning(0.0f, 99)); }
TEST(no_warning_when_reading_and_warning_value_are_both_zero) { CHECK(!isWarning(0.0f, 0)); }
TEST(warns_on_any_reading_when_warning_value_is_zero) { CHECK(isWarning(1.0f, 0)); }
TEST(cold_reading_past_the_warning_value_warns) {
  // A cold -40 C reading beats a warning value of 35 by size, as the firmware has
  // always compared them.
  CHECK(isWarning(-40.0f, 35));
}
TEST(cold_reading_within_the_warning_value_does_not_warn) { CHECK(!isWarning(-40.0f, 99)); }

// ---- decodeMil ----

TEST(mil_reply_gives_first_data_byte) {
  uint8_t a = 0;
  CHECK(decodeMil("41 01 86 0E 88 88 \r\r", &a));
  CHECK_EQ(a, 0x86);
}

TEST(mil_off_reply_gives_zero) {
  uint8_t a = 0xFF;
  CHECK(decodeMil("41 01 00 00 00 00 \r\r", &a));
  CHECK_EQ(a, 0);
}

TEST(mil_reply_from_two_computers_uses_the_first) {
  uint8_t a = 0;
  CHECK(decodeMil("41 01 86 0E 88 88 41 01 81 0C 00 00 ", &a));
  CHECK_EQ(a, 0x86);
}

TEST(mil_reply_with_wrong_header_fails) {
  uint8_t a = 0;
  CHECK(!decodeMil("41 05 86 0E \r\r", &a));
  CHECK_EQ(a, 0xFF);
}

TEST(mil_non_reply_fails) {
  uint8_t a = 0;
  CHECK(!decodeMil("NO DATA\r\r", &a));
  CHECK_EQ(a, 0xFF);
}

TEST(mil_empty_reply_fails) {
  uint8_t a = 0;
  CHECK(!decodeMil("", &a));
  CHECK_EQ(a, 0xFF);
}

TEST(mil_very_long_reply_does_not_overrun) {
  uint8_t a = 0;
  CHECK(decodeMil("41 01 81 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00", &a));
  CHECK_EQ(a, 0x81);
}

// ---- decodeDtcs ----

TEST(multi_line_reply_lists_every_code) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs(
      "00E 0: 43 06 00 7D C6 93 1: 01 08 C6 0F 02 E9 02 2: E0 CC CC CC CC CC CC 43 01 C4 01",
      codes, kMaxDtcs);
  CHECK_EQ(count, 6);
  CHECK_STR(codes[0].text, "P007D");
  CHECK_STR(codes[1].text, "U0693");
  CHECK_STR(codes[2].text, "P0108");
  CHECK_STR(codes[3].text, "U060F");
  CHECK_STR(codes[4].text, "P02E9");
  CHECK_STR(codes[5].text, "P02E0");
}

TEST(multi_line_reply_with_carriage_returns_lists_every_code) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("00A \r0: 43 04 00 7D C6 93 \r1: 01 08 C6 0F \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 4);
  CHECK_STR(codes[3].text, "U060F");
}

TEST(reply_with_one_code_lists_it) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 01 00 7D \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 1);
  CHECK_STR(codes[0].text, "P007D");
}

TEST(reply_with_two_codes_lists_both) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 02 00 7D C6 93 \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 2);
  CHECK_STR(codes[1].text, "U0693");
}

TEST(two_computers_on_separate_lines_list_all_their_codes) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 01 00 7D \r43 01 C4 01 \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 2);
  CHECK_STR(codes[0].text, "P007D");
  CHECK_STR(codes[1].text, "U0401");
}

TEST(code_prefix_covers_all_four_systems) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 04 13 00 53 00 93 00 D3 00 \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 4);
  CHECK_STR(codes[0].text, "P1300");
  CHECK_STR(codes[1].text, "C1300");
  CHECK_STR(codes[2].text, "B1300");
  CHECK_STR(codes[3].text, "U1300");
}

TEST(reply_cut_short_lists_only_what_arrived) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("00E 0: 43 06 00 7D C6 93 \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 2);
  CHECK_STR(codes[1].text, "U0693");
}

TEST(code_starting_with_the_byte_43_stays_a_code) {
  // 43 01 is C0301. Reading every 43 as the start of a list used to lose these.
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 02 00 7D 43 01 \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 2);
  CHECK_STR(codes[0].text, "P007D");
  CHECK_STR(codes[1].text, "C0301");
}

TEST(first_code_starting_with_the_byte_43_stays_a_code) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 01 43 01 \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 1);
  CHECK_STR(codes[0].text, "C0301");
}

TEST(padding_after_the_last_code_is_not_a_code) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 01 C4 01 00 00 \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 1);
  CHECK_STR(codes[0].text, "U0401");
}

TEST(padding_that_could_pass_for_a_count_is_not_a_code) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 01 C4 01 AA AA AA AA \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 1);
  CHECK_STR(codes[0].text, "U0401");
}

TEST(computer_with_no_codes_leaves_the_next_computers_list) {
  DtcCode codes[kMaxDtcs];
  int count = decodeDtcs("43 00 \r43 01 C4 01 \r\r", codes, kMaxDtcs);
  CHECK_EQ(count, 1);
  CHECK_STR(codes[0].text, "U0401");
}

TEST(no_data_is_not_a_dtc_reply) {
  DtcCode codes[kMaxDtcs];
  CHECK_EQ(decodeDtcs("NO DATA\r\r", codes, kMaxDtcs), -1);
}

TEST(empty_reply_is_not_a_dtc_reply) {
  DtcCode codes[kMaxDtcs];
  CHECK_EQ(decodeDtcs("", codes, kMaxDtcs), -1);
}

TEST(reply_with_no_codes_lists_none) {
  DtcCode codes[kMaxDtcs];
  CHECK_EQ(decodeDtcs("43 00 \r\r", codes, kMaxDtcs), 0);
}

TEST(list_stops_at_the_callers_limit) {
  DtcCode codes[2];
  CHECK_EQ(decodeDtcs("43 03 00 7D C6 93 01 08 \r\r", codes, 2), 2);
  CHECK_STR(codes[1].text, "U0693");
}

// ---- decodeVin ----

TEST(vin_from_multi_line_reply) {
  char vin[kVinSize];
  CHECK(decodeVin("014 \r0: 49 02 01 4D 50 42 \r1: 41 4D 46 30 36 30 4E \r2: 58 34 33 37 30 39 33 \r\r",
                  vin, sizeof vin));
  CHECK_STR(vin, "MPBAMF060NX437093");
}

TEST(vin_with_missing_bytes_fails) {
  char vin[kVinSize];
  CHECK(!decodeVin("014 \r0: 49 02 01 4D 50 42 \r1: 41 4D 46 30 36 30 4E \r\r", vin, sizeof vin));
}

TEST(vin_non_reply_fails) {
  char vin[kVinSize];
  CHECK(!decodeVin("NO DATA\r\r", vin, sizeof vin));
}

TEST(vin_empty_reply_fails) {
  char vin[kVinSize];
  CHECK(!decodeVin("", vin, sizeof vin));
}

TEST(vin_is_cut_to_the_callers_buffer) {
  char vin[5];
  CHECK(decodeVin("014 \r0: 49 02 01 4D 50 42 \r1: 41 4D 46 30 36 30 4E \r2: 58 34 33 37 30 39 33 \r\r",
                  vin, sizeof vin));
  CHECK_STR(vin, "MPBA");
}

TEST_MAIN()
