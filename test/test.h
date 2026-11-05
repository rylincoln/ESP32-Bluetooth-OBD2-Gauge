// A small test harness, so the suite needs nothing beyond a C++ compiler.
// Define tests with TEST(name) and finish each test file with TEST_MAIN().
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>

namespace test {

struct Case {
  const char* name;
  void (*run)();
  Case* next;
};

inline Case*& firstCase() {
  static Case* first = nullptr;
  return first;
}

inline int& failures() {
  static int count = 0;
  return count;
}

struct Register {
  explicit Register(Case* c) {
    c->next = firstCase();
    firstCase() = c;
  }
};

inline void fail(const char* file, int line, const char* message) {
  std::printf("  FAIL %s:%d: %s\n", file, line, message);
  failures()++;
}

inline int runAll() {
  int total = 0;
  for (Case* c = firstCase(); c != nullptr; c = c->next) {
    int before = failures();
    c->run();
    if (failures() != before) std::printf("  ^ in %s\n", c->name);
    total++;
  }
  if (failures() != 0) {
    std::printf("%d failed checks across %d tests\n", failures(), total);
    return 1;
  }
  std::printf("%d tests passed\n", total);
  return 0;
}

}  // namespace test

#define TEST(name)                                                  \
  static void name();                                               \
  static test::Case name##_case = {#name, name, nullptr};           \
  static test::Register name##_register(&name##_case);              \
  static void name()

#define TEST_MAIN() \
  int main() { return test::runAll(); }

#define CHECK(condition)                                  \
  do {                                                    \
    if (!(condition)) test::fail(__FILE__, __LINE__, #condition); \
  } while (0)

#define CHECK_EQ(actual, expected)                                         \
  do {                                                                     \
    long long actual_ = (long long)(actual);                               \
    long long expected_ = (long long)(expected);                           \
    if (actual_ != expected_) {                                            \
      char message_[160];                                                  \
      std::snprintf(message_, sizeof message_, "%s is %lld, expected %lld", \
                    #actual, actual_, expected_);                          \
      test::fail(__FILE__, __LINE__, message_);                            \
    }                                                                      \
  } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                        \
  do {                                                                 \
    double actual_ = (double)(actual);                                 \
    double expected_ = (double)(expected);                             \
    if (std::fabs(actual_ - expected_) > (tolerance)) {                \
      char message_[160];                                              \
      std::snprintf(message_, sizeof message_, "%s is %f, expected %f", \
                    #actual, actual_, expected_);                      \
      test::fail(__FILE__, __LINE__, message_);                        \
    }                                                                  \
  } while (0)

#define CHECK_STR(actual, expected)                                         \
  do {                                                                      \
    const char* actual_ = (actual);                                         \
    const char* expected_ = (expected);                                     \
    if (std::strcmp(actual_, expected_) != 0) {                             \
      char message_[200];                                                   \
      std::snprintf(message_, sizeof message_, "%s is \"%s\", expected \"%s\"", \
                    #actual, actual_, expected_);                           \
      test::fail(__FILE__, __LINE__, message_);                             \
    }                                                                       \
  } while (0)
