#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <windows.h>
#include <sstream>
#include <random>
#include <atomic>
#include <JoyShockLibrary.h>

using namespace std;

// Global variables to store accelerometer data
float accel_x = 0.0f;
float accel_y = 0.0f;
float accel_z = 0.0f;

// Offsets for reset
float offset_x = 0.0f;
float offset_y = 0.0f;
float offset_z = 0.0f;

HWND hwnd; // Global window handle

enum Direction { NONE, UP, DOWN, LEFT, RIGHT };

Direction currentInstruction = NONE;
std::chrono::steady_clock::time_point instructionTime;
std::atomic<bool> waitingForInput = false;
std::atomic<bool> correctInputReceived = false;
std::string feedback = "";

enum Mode { GAME, DEBUG };
std::atomic<Mode> currentMode = GAME;


Direction GetRandomDirection() {
	static std::default_random_engine engine(std::random_device{}());
	std::uniform_int_distribution<int> dist(0, 3);
	return static_cast<Direction>(dist(engine) + 1);
}

const wchar_t* DirectionToText(Direction dir) {
	switch (dir) {
	case UP: return L"Haut";
	case DOWN: return L"Bas";
	case LEFT: return L"Gauche";
	case RIGHT: return L"Droite";
	default: return L"Aucun";
	}
}


Direction DetectDirection(float x, float y, float threshold = 1.0f)
{
	if (abs(x) < threshold && abs(y) < threshold)
		return NONE;

	if (abs(x) > abs(y))
		return (x > 0) ? LEFT : RIGHT;
	else
		return (y > 0) ? UP : DOWN;
}

void UpdateSensorData()
{
	int handles[1];
	int count = JslGetConnectedDeviceHandles(handles, 1);
	int joyConHandle = handles[0];

	while (true)
	{
		MOTION_STATE motionState = JslGetMotionState(joyConHandle);
		float buttonState = JslGetButtons(joyConHandle);
		
		bool resetPressed = (buttonState == 0x00400 || buttonState == 0x00800);
		bool modeTogglePressed = (buttonState == 0x00010 || buttonState == 0x00020); // "Plus" ou "Moins"
		static bool lastToggle = false;

		if (modeTogglePressed && !lastToggle)
		{
			currentMode = (currentMode == GAME) ? DEBUG : GAME;
		}
		lastToggle = modeTogglePressed;

		if (resetPressed)
		{
			offset_x = motionState.accelX;
			offset_y = motionState.accelY;
			offset_z = motionState.accelZ;
		}

		accel_x = motionState.accelX - offset_x;
		accel_y = motionState.accelY - offset_y;
		accel_z = motionState.accelZ - offset_z;

		Direction detected = DetectDirection(accel_x, accel_y);

		if (waitingForInput && detected == currentInstruction)
		{
			auto reactionTime = chrono::duration_cast<chrono::milliseconds>(
				chrono::steady_clock::now() - instructionTime).count();
			feedback = "Réaction : " + to_string(reactionTime) + " ms";
			correctInputReceived = true;
			waitingForInput = false;
		}

		InvalidateRect(hwnd, nullptr, TRUE);
		std::this_thread::sleep_for(std::chrono::milliseconds(15));
	}
}

// Window procedure for handling window messages
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			RECT rect;
			GetClientRect(hwnd, &rect);
			HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
			FillRect(hdc, &rect, brush);
			DeleteObject(brush);

			if (currentMode == DEBUG)
			{
				// Affiche les données brutes
				wchar_t buffer[100];
				swprintf(buffer, 100, L"X: %.3f\nY: %.3f\nZ: %.3f", accel_x, accel_y, accel_z);
				DrawTextW(hdc, buffer, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				Direction dir = DetectDirection(accel_x, accel_y);
				const wchar_t* dirText = DirectionToText(dir);

				RECT dirRect = rect;
				dirRect.top += 50;
				DrawTextW(hdc, dirText, -1, &dirRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
			else
			{
				// Mode jeu : instruction + feedback
				RECT instrRect = rect;
				instrRect.top += 30;
				wstring instrText = L"Instruction : ";
				instrText += DirectionToText(currentInstruction);
				DrawTextW(hdc, instrText.c_str(), -1, &instrRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				RECT feedRect = rect;
				feedRect.top += 80;
				wstring wfeedback(feedback.begin(), feedback.end());
				DrawTextW(hdc, wfeedback.c_str(), -1, &feedRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}

			RECT helpRect;
			helpRect.left = rect.right - 250;
			helpRect.top = rect.top + 10;
			helpRect.right = rect.right - 10;
			helpRect.bottom = rect.top + 100;

			std::wstring helpText =
				L"Contrôles :\n"
				L"ZL/ZR : Reset capteurs\n"
				L"+/- : Changer mode Debug/Jeu";

			DrawTextW(hdc, helpText.c_str(), -1, &helpRect, DT_LEFT | DT_TOP);
			EndPaint(hwnd, &ps);
			return 0;
		}

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

// Main function to initialize HID, create window and read IMU data
int main()
{
	int deviceCount = JslConnectDevices();
	if (deviceCount == 0)
	{
		cerr << "Aucun peripherique detecte." << endl;
		return -1;
	}

	int handles[1];
	int count = JslGetConnectedDeviceHandles(handles, 1);
	int joyConHandle = handles[0];
	MOTION_STATE motionState = JslGetMotionState(joyConHandle);
	accel_x = motionState.accelX;
	accel_y = motionState.accelY;
	accel_z = motionState.accelZ;

	// Initial offset (optionnel)
	offset_x = accel_x;
	offset_y = accel_y;
	offset_z = accel_z;

	// Window class registration
	constexpr wchar_t CLASS_NAME[] = L"AccelerometerWindow";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc; // Set window procedure
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = CLASS_NAME;

	if (!RegisterClassW(&wc))
	{
		// Use RegisterClassW for wide strings
		cerr << "Window class registration failed!" << endl;
		return -1;
	}

	// Create the window
	hwnd = CreateWindowEx(
		0, CLASS_NAME, L"Super Mario Party 2025", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 800, nullptr, nullptr, wc.hInstance, nullptr);

	if (!hwnd)
	{
		cerr << "Window creation failed!" << endl;
		return -1;
	}

	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);

	// Start a thread to read IMU data
	thread sensorThread(UpdateSensorData);

	// Start a thread for the game
	thread gameLoop([] {
		while (true)
		{
			if (currentMode == GAME)
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));

				currentInstruction = GetRandomDirection();
				instructionTime = std::chrono::steady_clock::now();
				waitingForInput = true;
				correctInputReceived = false;
				feedback = "";

				while (currentMode == GAME && !correctInputReceived)
					std::this_thread::sleep_for(std::chrono::milliseconds(10));

				std::this_thread::sleep_for(std::chrono::seconds(2));
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
		});

	// Message loop
	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	sensorThread.join();
	gameLoop.join();

	return 0;
}