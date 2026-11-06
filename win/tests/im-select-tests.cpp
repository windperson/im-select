#include "..\im-select\im-select\stdafx.h"
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
struct FakeWindows {
    HWND foreground = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(0x1234));
    DWORD threadId = 42;
    HKL layout = reinterpret_cast<HKL>(static_cast<ULONG_PTR>(0x04090409));
    BOOL postResult = TRUE;
    int foregroundCalls = 0;
    int threadCalls = 0;
    int layoutCalls = 0;
    int postCalls = 0;
    HWND queriedWindow = nullptr;
    DWORD queriedThread = 0;
    bool nullProcessId = false;
    HWND postedWindow = nullptr;
    UINT postedMessage = 0;
    WPARAM postedWParam = 1;
    LPARAM postedLParam = -1;
    std::string output;
};

FakeWindows state;
int failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (false)

HWND fakeGetForegroundWindow() {
    ++state.foregroundCalls;
    return state.foreground;
}

DWORD fakeGetWindowThreadProcessId(HWND window, LPDWORD processId) {
    ++state.threadCalls;
    state.queriedWindow = window;
    state.nullProcessId = processId == nullptr;
    return state.threadId;
}

HKL fakeGetKeyboardLayout(DWORD threadId) {
    ++state.layoutCalls;
    state.queriedThread = threadId;
    return state.layout;
}

BOOL fakePostMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    ++state.postCalls;
    state.postedWindow = window;
    state.postedMessage = message;
    state.postedWParam = wParam;
    state.postedLParam = lParam;
    return state.postResult;
}

int fakePrintf(const char* format, int value) {
    char buffer[64] = {};
    int written = std::snprintf(buffer, sizeof(buffer), format, value);
    state.output += buffer;
    return written;
}
}

// Compile the real implementation with Win32 calls replaced only in this test.
#define GetForegroundWindow fakeGetForegroundWindow
#define GetWindowThreadProcessId fakeGetWindowThreadProcessId
#define GetKeyboardLayout fakeGetKeyboardLayout
#undef PostMessage
#define PostMessage fakePostMessage
#define printf fakePrintf
#define main imSelectMain
#include "..\im-select\im-select\im-select.cpp"
#undef main
#undef printf
#undef PostMessage
#undef GetKeyboardLayout
#undef GetWindowThreadProcessId
#undef GetForegroundWindow

namespace {
void readsForegroundThreadLayout() {
    CHECK(getInputMethod() == 1033);
    CHECK(state.foregroundCalls == 1);
    CHECK(state.threadCalls == 1);
    CHECK(state.queriedWindow == state.foreground);
    CHECK(state.nullProcessId);
    CHECK(state.layoutCalls == 1);
    CHECK(state.queriedThread == state.threadId);
    CHECK(state.postCalls == 0);
}

void extractsOnlyLowLanguageBits() {
    state.layout = reinterpret_cast<HKL>(
        static_cast<ULONG_PTR>(0x12345678F1230411ULL));
    CHECK(getInputMethod() == 1041);
}

void handlesZeroLayout() {
    state.layout = nullptr;
    CHECK(getInputMethod() == 0);
    CHECK(state.layoutCalls == 1);
}

void handlesMissingForegroundWhenReading() {
    state.foreground = nullptr;
    CHECK(getInputMethod() == 0);
    CHECK(state.foregroundCalls == 1);
    CHECK(state.threadCalls == 0);
    CHECK(state.layoutCalls == 0);
}

void postsLanguageChangeToForeground() {
    switchInputMethod(1041);
    CHECK(state.foregroundCalls == 1);
    CHECK(state.postCalls == 1);
    CHECK(state.postedWindow == state.foreground);
    CHECK(state.postedMessage == WM_INPUTLANGCHANGEREQUEST);
    CHECK(state.postedWParam == 0);
    CHECK(state.postedLParam == 1041);
    CHECK(state.threadCalls == 0);
    CHECK(state.layoutCalls == 0);
}

void preservesLocaleRangeWhenSwitching() {
    const int locales[] = {0, 65535, -1};
    for (int locale : locales) {
        switchInputMethod(locale);
        CHECK(state.postedLParam == static_cast<LPARAM>(locale));
    }
    CHECK(state.postCalls == 3);
}

void cliWithoutArgumentsPrintsLanguage() {
    char program[] = "im-select";
    char* argv[] = {program, nullptr};
    CHECK(imSelectMain(1, argv) == 0);
    CHECK(state.output == "1033\n");
    CHECK(state.postCalls == 0);
}

void cliWithoutForegroundPrintsZero() {
    state.foreground = nullptr;
    char program[] = "im-select";
    char* argv[] = {program, nullptr};
    CHECK(imSelectMain(1, argv) == 0);
    CHECK(state.output == "0\n");
    CHECK(state.threadCalls == 0);
    CHECK(state.layoutCalls == 0);
    CHECK(state.postCalls == 0);
}

void cliWithLocaleSwitchesWithoutOutput() {
    char program[] = "im-select";
    char locale[] = "1041";
    char* argv[] = {program, locale, nullptr};
    CHECK(imSelectMain(2, argv) == 0);
    CHECK(state.postCalls == 1);
    CHECK(state.postedWindow == state.foreground);
    CHECK(state.postedMessage == WM_INPUTLANGCHANGEREQUEST);
    CHECK(state.postedWParam == 0);
    CHECK(state.postedLParam == 1041);
    CHECK(state.output.empty());
}

void cliIgnoresAdditionalArguments() {
    char program[] = "im-select";
    char locale[] = "1033";
    char extra[] = "1041";
    char* argv[] = {program, locale, extra, nullptr};
    CHECK(imSelectMain(3, argv) == 0);
    CHECK(state.postCalls == 1);
    CHECK(state.postedLParam == 1033);
    CHECK(state.output.empty());
}

// Characterization tests preserve existing edge cases, not recommended policy.
void cliPreservesAtoiParsing() {
    char program[] = "im-select";
    char locale[] = " \t+1041suffix";
    char* argv[] = {program, locale, nullptr};
    CHECK(imSelectMain(2, argv) == 0);
    CHECK(state.postCalls == 1);
    CHECK(state.postedLParam == 1041);
}

void cliNonNumericLocaleCurrentlyPostsZero() {
    char program[] = "im-select";
    char locale[] = "invalid";
    char* argv[] = {program, locale, nullptr};
    CHECK(imSelectMain(2, argv) == 0);
    CHECK(state.postCalls == 1);
    CHECK(state.postedLParam == 0);
}

void switchingWithoutForegroundCurrentlyStillPosts() {
    state.foreground = nullptr;
    switchInputMethod(1033);
    CHECK(state.postCalls == 1);
    CHECK(state.postedWindow == nullptr);
    CHECK(state.postedMessage == WM_INPUTLANGCHANGEREQUEST);
    CHECK(state.postedLParam == 1033);
}

void cliCurrentlyIgnoresPostFailure() {
    state.postResult = FALSE;
    char program[] = "im-select";
    char locale[] = "1033";
    char* argv[] = {program, locale, nullptr};
    CHECK(imSelectMain(2, argv) == 0);
    CHECK(state.postCalls == 1);
    CHECK(state.output.empty());
}
}

int main() {
    struct Test {
        const char* name;
        void (*run)();
    };
    const Test tests[] = {
        {"reads foreground thread layout", readsForegroundThreadLayout},
        {"extracts only low language bits", extractsOnlyLowLanguageBits},
        {"handles zero layout", handlesZeroLayout},
        {"handles missing foreground when reading", handlesMissingForegroundWhenReading},
        {"posts language change to foreground", postsLanguageChangeToForeground},
        {"preserves locale range when switching", preservesLocaleRangeWhenSwitching},
        {"CLI without arguments prints language", cliWithoutArgumentsPrintsLanguage},
        {"CLI without foreground prints zero", cliWithoutForegroundPrintsZero},
        {"CLI with locale switches without output", cliWithLocaleSwitchesWithoutOutput},
        {"CLI ignores additional arguments", cliIgnoresAdditionalArguments},
        {"CLI preserves atoi parsing", cliPreservesAtoiParsing},
        {"CLI non-numeric locale currently posts zero", cliNonNumericLocaleCurrentlyPostsZero},
        {"switching without foreground currently still posts", switchingWithoutForegroundCurrentlyStillPosts},
        {"CLI currently ignores post failure", cliCurrentlyIgnoresPostFailure},
    };
    int passed = 0;
    int failed = 0;
    for (const Test& test : tests) {
        state = FakeWindows{};
        int before = failures;
        test.run();
        bool success = failures == before;
        std::printf("%s: %s\n", success ? "PASS" : "FAIL", test.name);
        if (success) {
            ++passed;
        } else {
            ++failed;
        }
    }
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
