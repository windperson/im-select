#include <CppUnitTest.h>

#define IM_SELECT_TEST_EXPLORER
#include "im-select-tests.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ImSelectTests {
TEST_CLASS(WindowsUnitTests) {
public:
	TEST_METHOD_INITIALIZE(ResetState) {
		state = FakeWindows{};
	}

#define IM_SELECT_TEST(name) TEST_METHOD(name) { ::name(); }
	IM_SELECT_TEST(readsForegroundThreadLayout)
	IM_SELECT_TEST(extractsOnlyLowLanguageBits)
	IM_SELECT_TEST(handlesZeroLayout)
	IM_SELECT_TEST(handlesMissingForegroundWhenReading)
	IM_SELECT_TEST(handlesMissingWindowThread)
	IM_SELECT_TEST(postsLanguageChangeToForeground)
	IM_SELECT_TEST(preservesLocaleRangeWhenSwitching)
	IM_SELECT_TEST(cliWithoutArgumentsPrintsLanguage)
	IM_SELECT_TEST(cliPrintsZeroForNonNullLayoutWithZeroLanguageId)
	IM_SELECT_TEST(cliWithoutForegroundReportsError)
	IM_SELECT_TEST(cliWithLocaleSwitchesWithoutOutput)
	IM_SELECT_TEST(cliIgnoresAdditionalArguments)
	IM_SELECT_TEST(cliAcceptsSignedDecimalAndWhitespace)
	IM_SELECT_TEST(cliRejectsInvalidLocales)
	IM_SELECT_TEST(cliAcceptsIntegerBoundaries)
	IM_SELECT_TEST(cliSwitchWithoutForegroundReportsError)
	IM_SELECT_TEST(cliReportsPostFailure)
	IM_SELECT_TEST(cliReportsQueryFailures)
#undef IM_SELECT_TEST
};
}
