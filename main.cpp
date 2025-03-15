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

HWND hwnd; // Global window handle

void UpdateSensorData()
{
	int handles[1];
	int count = JslGetConnectedDeviceHandles(handles, 1);
	int joyConHandle = handles[0];

	while (true)
	{
		MOTION_STATE motionState = JslGetMotionState(joyConHandle);
		accel_x = motionState.accelX;
		accel_y = motionState.accelY;
		accel_z = motionState.accelZ;

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
		std::cerr << L"Aucun peripherique detecte." << std::endl;
		return -1;
	}

	int handles[1];
	int count = JslGetConnectedDeviceHandles(handles, 1);
	int joyConHandle = handles[0];
	MOTION_STATE motionState = JslGetMotionState(joyConHandle);
	accel_x = motionState.accelX;
	accel_y = motionState.accelY;
	accel_z = motionState.accelZ;

	// Window class registration
	constexpr wchar_t CLASS_NAME[] = L"AccelerometerWindow";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc; // Set window procedure
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = CLASS_NAME;

	if (!RegisterClassW(&wc))
	{
		// Use RegisterClassW for wide strings
		std::cerr << "Window class registration failed!" << std::endl;
		return -1;
	}

	// Create the window
	hwnd = CreateWindowEx(
		0, CLASS_NAME, L"Accelerometer Data", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1500, 300, nullptr, nullptr, wc.hInstance, nullptr);

	if (!hwnd)
	{
		std::cerr << "Window creation failed!" << std::endl;
		return -1;
	}

	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);

	// Start a thread to read IMU data
	std::thread sensorThread(UpdateSensorData);

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

/*
int main() {
	int deviceCount = JslConnectDevices();
	if (deviceCount == 0) {
		std::cerr << "Aucun peripherique detecte." << std::endl;
		return -1;
	}

	int handles[1];
	int count = JslGetConnectedDeviceHandles(handles, 1);
	int joyConHandle = handles[0]; // Supposons que le premier périphérique soit le Joy-Con
	IMU_STATE motionState = JslGetIMUState(joyConHandle);
	std::cout << R"(Acceleration :)" << std::endl;
	std::cout << R"(Acc X : )" << motionState.accelX << std::endl;
	std::cout << R"(Acc Y : )" << motionState.accelY << std::endl;
	std::cout << R"(Acc Z : )" << motionState.accelZ << std::endl;
	std::cout << R"(Gyro X : )" << motionState.gyroX << std::endl;
	std::cout << R"(Gyro Y : )" << motionState.gyroY << std::endl;
	std::cout << R"(Gyro Z : )" << motionState.gyroZ << std::endl;

	while (true) {
		motionState = JslGetIMUState(joyConHandle);

		std::cout << R"(Acc X : )" << motionState.accelX << std::endl;
		std::cout << R"(Acc Y : )" << motionState.accelY << std::endl;
		std::cout << R"(Acc Z : )" << motionState.accelZ << std::endl;
		std::cout << R"(Gyro X : )" << motionState.gyroX << std::endl;
		std::cout << R"(Gyro Y : )" << motionState.gyroY << std::endl;
		std::cout << R"(Gyro Z : )" << motionState.gyroZ << std::endl;

		// Traitez les données ici
		std::this_thread::sleep_for(std::chrono::milliseconds(15)); // Ajustez selon vos besoins
		// Notify the window to repaint itself
		InvalidateRect(hwnd, nullptr, TRUE);
	}
}
*/
