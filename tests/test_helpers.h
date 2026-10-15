#pragma once

// Minimal assertion helpers shared by all raster tests.
//
//   CHECK(cond)                    fails if cond is false
//   CHECK_MSG(cond, message)       same, and also prints message (std::string)
//   CHECK_NEAR(actual, expected, tolerance)
//
// A failed check prints "file:line: ..." to stderr and the test keeps going,
// so one run reports every failure. End main() with
//   return test::summary("test_name");
// which prints the totals and returns a non-zero exit code if anything failed
// (that is what makes ctest report the test as failed).

#include <cmath>
#include <cstdio>
#include <string>

namespace test {

inline int checks = 0;
inline int failures = 0;

inline bool check(bool ok, const char* expression, const std::string& message,
                  const char* file, int line) {
    ++checks;
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "%s:%d: CHECK failed: %s", file, line, expression);
        if (!message.empty()) {
            std::fprintf(stderr, "  [%s]", message.c_str());
        }
        std::fprintf(stderr, "\n");
    }
    return ok;
}

inline bool checkNear(double actual, double expected, double tolerance,
                      const char* actualText, const char* expectedText,
                      const char* file, int line) {
    ++checks;
    const bool ok = std::fabs(actual - expected) <= tolerance;
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s = %.6f, expected %s = %.6f (tolerance %g)\n",
                     file, line, actualText, actual, expectedText, expected, tolerance);
    }
    return ok;
}

inline int summary(const char* testName) {
    std::printf("%s: %d checks, %d failed\n", testName, checks, failures);
    return failures == 0 ? 0 : 1;
}

} // namespace test

#define CHECK(cond) ::test::check(static_cast<bool>(cond), #cond, std::string(), __FILE__, __LINE__)
#define CHECK_MSG(cond, message) ::test::check(static_cast<bool>(cond), #cond, (message), __FILE__, __LINE__)
#define CHECK_NEAR(actual, expected, tolerance) \
    ::test::checkNear((actual), (expected), (tolerance), #actual, #expected, __FILE__, __LINE__)
