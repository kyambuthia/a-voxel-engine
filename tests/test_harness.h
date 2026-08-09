#pragma once

// Minimal zero-dependency test harness. Register cases with TEST(name); the
// runner prints per-case results and returns non-zero if any assertion failed.
// Kept header-only so new test files only need to #include it and link
// tests/test_main.cpp.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace test {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back(Case{name, std::move(fn)});
    }
};

inline int& failureCount() {
    static int n = 0;
    return n;
}

inline void reportFailure(const char* file, int line, const std::string& msg) {
    std::fprintf(stderr, "    FAIL %s:%d: %s\n", file, line, msg.c_str());
    ++failureCount();
}

template <typename A, typename B>
void checkEq(const char* file, int line, const char* ea, const char* eb,
             const A& a, const B& b) {
    if (a == b) {
        return;
    }
    char buf[512];
    std::snprintf(buf, sizeof(buf), "CHECK_EQ(%s, %s) failed: %lld != %lld",
                  ea, eb, static_cast<long long>(a), static_cast<long long>(b));
    reportFailure(file, line, buf);
}

template <typename A, typename B>
void checkNear(const char* file, int line, const char* ea, const char* eb,
               const A& a, const B& b, double tol) {
    const double da = static_cast<double>(a);
    const double db = static_cast<double>(b);
    if (std::abs(da - db) <= tol) {
        return;
    }
    char buf[512];
    std::snprintf(buf, sizeof(buf), "CHECK_NEAR(%s, %s, tol) failed: %.9g != %.9g",
                  ea, eb, da, db);
    reportFailure(file, line, buf);
}

inline int runAll() {
    int failedCases = 0;
    const std::size_t total = registry().size();
    for (const Case& c : registry()) {
        const int before = failureCount();
        c.fn();
        if (failureCount() > before) {
            std::fprintf(stderr, "  [FAILED] %s\n", c.name.c_str());
            ++failedCases;
        } else {
            std::printf("  [ok] %s\n", c.name.c_str());
        }
    }
    std::printf("%zu/%zu cases passed, %d assertion failures\n",
                total - static_cast<std::size_t>(failedCases), total,
                failureCount());
    return failureCount() == 0 ? 0 : 1;
}

}  // namespace test

#define TEST(name)                                                        \
    static void test_##name();                                            \
    static ::test::Registrar registrar_##name(#name, &test_##name);       \
    static void test_##name()

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            ::test::reportFailure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b) ::test::checkEq(__FILE__, __LINE__, #a, #b, (a), (b))

#define CHECK_NEAR(a, b, tol) \
    ::test::checkNear(__FILE__, __LINE__, #a, #b, (a), (b), (tol))
