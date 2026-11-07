#include <Windows.h>
#include <cstdio>
#include <string>
#include <vector>

namespace {
struct WindowState {
	unsigned int requests = 0;
	WPARAM wParam = 0;
	LPARAM lParam = 0;
};

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	auto state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message == WM_NCCREATE) {
		auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		state = static_cast<WindowState*>(create->lpCreateParams);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
	}
	if (message == WM_INPUTLANGCHANGEREQUEST && state) {
		++state->requests;
		state->wParam = wParam;
		state->lParam = lParam;
		// Do not forward the request: the test must not change this window's layout.
		return 0;
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

bool runCommand(const std::wstring& executable, const std::wstring& argument,
				HWND window, std::string& output) {
	if (GetForegroundWindow() != window) {
		std::fprintf(stderr, "FAIL: test window is no longer foreground; do not switch windows during the test\n");
		return false;
	}

	SECURITY_ATTRIBUTES security = { sizeof(security), nullptr, TRUE };
	HANDLE readPipe = nullptr;
	HANDLE writePipe = nullptr;
	if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
		std::fprintf(stderr, "FAIL: CreatePipe failed (%lu)\n", GetLastError());
		return false;
	}
	if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
		std::fprintf(stderr, "FAIL: SetHandleInformation failed (%lu)\n", GetLastError());
		CloseHandle(readPipe);
		CloseHandle(writePipe);
		return false;
	}

	HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
								  &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (nullInput == INVALID_HANDLE_VALUE) {
		std::fprintf(stderr, "FAIL: could not open NUL (%lu)\n", GetLastError());
		CloseHandle(readPipe);
		CloseHandle(writePipe);
		return false;
	}

	STARTUPINFOW startup = {};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = nullInput;
	startup.hStdOutput = writePipe;
	startup.hStdError = writePipe;
	PROCESS_INFORMATION process = {};
	std::wstring command = L"\"" + executable + L"\"";
	if (!argument.empty()) {
		command += L" " + argument;
	}
	std::vector<wchar_t> commandLine(command.begin(), command.end());
	commandLine.push_back(L'\0');
	BOOL started = CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr,
								  TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
	DWORD startError = started ? ERROR_SUCCESS : GetLastError();
	CloseHandle(nullInput);
	CloseHandle(writePipe);
	if (!started) {
		std::fprintf(stderr, "FAIL: CreateProcess failed (%lu)\n", startError);
		CloseHandle(readPipe);
		return false;
	}

	DWORD wait = WaitForSingleObject(process.hProcess, 10000);
	if (wait != WAIT_OBJECT_0) {
		std::fprintf(stderr, "FAIL: executable did not finish (wait result %lu)\n", wait);
		TerminateProcess(process.hProcess, 1);
		WaitForSingleObject(process.hProcess, INFINITE);
	}
	DWORD exitCode = 1;
	BOOL gotExitCode = GetExitCodeProcess(process.hProcess, &exitCode);
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);

	char buffer[256];
	DWORD bytesRead = 0;
	while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead != 0) {
		output.append(buffer, bytesRead);
	}
	CloseHandle(readPipe);
	if (wait != WAIT_OBJECT_0 || !gotExitCode || exitCode != 0) {
		std::fprintf(stderr, "FAIL: executable exited unsuccessfully (exit code %lu): %s\n", exitCode, output.c_str());
		return false;
	}
	if (GetForegroundWindow() != window) {
		std::fprintf(stderr, "FAIL: foreground changed during execution; result is unreliable\n");
		return false;
	}
	return true;
}

bool waitForRequest(WindowState& state) {
	ULONGLONG deadline = GetTickCount64() + 3000;
	while (state.requests == 0 && GetTickCount64() < deadline) {
		MSG message;
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		if (state.requests == 0) {
			MsgWaitForMultipleObjectsEx(0, nullptr, 50, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		}
	}
	return state.requests != 0;
}
}

int wmain(int argc, wchar_t** argv) {
	if (argc != 2) {
		std::fprintf(stderr, "Usage: im-select-integration-tests.exe <path-to-im-select.exe>\n");
		return 1;
	}

	HINSTANCE instance = GetModuleHandleW(nullptr);
	const wchar_t* className = L"ImSelectIntegrationTestWindow";
	WNDCLASSW windowClass = {};
	windowClass.lpfnWndProc = windowProc;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = className;
	if (!RegisterClassW(&windowClass)) {
		std::fprintf(stderr, "FAIL: RegisterClass failed (%lu)\n", GetLastError());
		return 1;
	}

	HWND previousForeground = GetForegroundWindow();
	WindowState state;
	HWND window = CreateWindowExW(0, className, L"im-select integration test - please keep focused",
								  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 140,
								  nullptr, nullptr, instance, &state);
	if (!window) {
		std::fprintf(stderr, "FAIL: CreateWindow failed (%lu); an interactive desktop is required\n", GetLastError());
		UnregisterClassW(className, instance);
		return 1;
	}
	ShowWindow(window, SW_SHOW);
	SetForegroundWindow(window);

	bool passed = false;
	if (GetForegroundWindow() != window) {
		std::fprintf(stderr, "FAIL: could not focus test window; run from an active interactive desktop\n");
	} else {
		DWORD thread = GetWindowThreadProcessId(window, nullptr);
		HKL layout = GetKeyboardLayout(thread);
		unsigned int language = static_cast<unsigned int>(reinterpret_cast<ULONG_PTR>(layout) & 0xFFFFu);
		std::string queryOutput;
		if (!layout) {
			std::fprintf(stderr, "FAIL: could not obtain the test window's keyboard layout\n");
		} else if (runCommand(argv[1], L"", window, queryOutput)) {
			std::string expected = std::to_string(language) + "\r\n";
			if (queryOutput != expected) {
				std::fprintf(stderr, "FAIL: expected query output %s, got %s\n", expected.c_str(), queryOutput.c_str());
			} else {
				std::printf("PASS: executable reads the foreground window's language ID\n");
				std::string switchOutput;
				if (runCommand(argv[1], std::to_wstring(language), window, switchOutput)) {
					if (!waitForRequest(state)) {
						std::fprintf(stderr, "FAIL: language-change request was not delivered\n");
					} else if (state.requests != 1 || state.wParam != 0 || state.lParam != static_cast<LPARAM>(language)) {
						std::fprintf(stderr, "FAIL: unexpected language-change request parameters\n");
					} else if (!switchOutput.empty()) {
						std::fprintf(stderr, "FAIL: switch command produced unexpected output: %s\n", switchOutput.c_str());
					} else {
						std::printf("PASS: executable posts a language-change request to the foreground window\n");
						passed = true;
					}
				}
			}
		}
	}

	if (GetForegroundWindow() == window && previousForeground && IsWindow(previousForeground)) {
		SetForegroundWindow(previousForeground);
	}
	DestroyWindow(window);
	UnregisterClassW(className, instance);
	return passed ? 0 : 1;
}
