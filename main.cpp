#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <windows.h>
#include <sstream>
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

Direction DetectDirection(float x, float y, float threshold = 0.7f)
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
		if (resetPressed)
		{
			offset_x = motionState.accelX;
			offset_y = motionState.accelY;
			offset_z = motionState.accelZ;
		}

		accel_x = motionState.accelX - offset_x;
		accel_y = motionState.accelY - offset_y;
		accel_z = motionState.accelZ - offset_z;

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

			// Efface l'ancienne valeur en remplissant le fond en blanc
			RECT rect;
			GetClientRect(hwnd, &rect);
			HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
			FillRect(hdc, &rect, brush);
			DeleteObject(brush);

			// Display accelerometer data on the window
			wchar_t buffer[100];
			swprintf(buffer, 100, L"X: %.3f\nY: %.3f\nZ: %.3f", accel_x, accel_y, accel_z);
			DrawTextW(hdc, buffer, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

			// Détecte et affiche la direction
			Direction dir = DetectDirection(accel_x, accel_y);
			const wchar_t* dirText = L"Aucun mouvement";
			switch (dir)
			{
			case UP: dirText = L"Haut"; break;
			case DOWN: dirText = L"Bas"; break;
			case LEFT: dirText = L"Gauche"; break;
			case RIGHT: dirText = L"Droite"; break;
			default: break;
			}

			RECT dirRect = rect;
			dirRect.top += 50;
			DrawTextW(hdc, dirText, -1, &dirRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

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
		cerr << L"Aucun peripherique detecte." << endl;
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
		0, CLASS_NAME, L"Accelerometer Data", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 300, 300, nullptr, nullptr, wc.hInstance, nullptr);

	if (!hwnd)
	{
		cerr << "Window creation failed!" << endl;
		return -1;
	}

	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);

	// Start a thread to read IMU data
	thread sensorThread(UpdateSensorData);

	// Message loop
	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	sensorThread.join();

	return 0;
}