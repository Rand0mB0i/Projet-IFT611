#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <hidapi.h>
#include <windows.h>
#include <sstream>

#define JOYCON_VENDOR  0x057E
#define JOYCON_PRODUCT 0x2007  // 0x2007 for Left Joy-Con
using namespace std;

// Global variables to store accelerometer data
float accelerometer_x = 0.0f;
float accelerometer_y = 0.0f;
float accelerometer_z = 0.0f;

HWND hwnd; // Global window handle
unsigned char raw_data[65] = {};
constexpr float accel_scale = 0.000244f; // Correct scale for ±8G

// Send command to the device
void sendCommand(hid_device* handle, const std::vector<unsigned char>& data)
{
	hid_write(handle, data.data(), data.size());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Read IMU data from Joy-Con
void read_imu_data(hid_device* handle)
{
	// Enable IMU
	sendCommand(handle, {0x01, 0x40});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	sendCommand(handle, {0x01, 0x40});
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Set IMU sensitivity
	sendCommand(handle, { 0x41, 0x03, 0x00, 0x00, 0x02 }); // Accel ±8G, Gyro ±2000dps
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Simulate data update
	while (true)
	{
		int bytesRead = hid_read(handle, raw_data, sizeof(raw_data));

		if (bytesRead < 5)
		{
			std::cerr << "Failed to read IMU data.\n";
			std::cout << "Nombre de byte lus: " << bytesRead << "\n";
			this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}

		// If we get IMU data (0x30), ignore and keep reading
		if (raw_data[0] == 0x30)
		{
			//std::cout << "Received IMU data..." << std::endl;
		}


		// Read accelerometer data
		int16_t accel_x_raw = (raw_data[13] << 8) | raw_data[12]; // Little-endian byte order
		int16_t accel_y_raw = (raw_data[15] << 8) | raw_data[14];
		int16_t accel_z_raw = raw_data[17] << 8 | raw_data[16];

		float ax = accel_x_raw * accel_scale;
		float ay = accel_y_raw * accel_scale;
		float az = accel_z_raw * accel_scale;

		// Update global accelerometer data
		accelerometer_x = ax;
		accelerometer_y = ay;
		accelerometer_z = az;

		cout << "Acceleration:\nX: " << accelerometer_x << "\nY: " << accelerometer_y << "\nZ: " << accelerometer_z << "\n";

		// Notify the window to repaint itself
		InvalidateRect(hwnd, nullptr, TRUE);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

			// Display accelerometer data on the window
			std::wstringstream ss;
			/*ss << L"Accelerometer:\n"
			    << L"X: " << accelerometer_x << L" G\n"
			    << L"Y: " << accelerometer_y << L" G\n"
			    << L"Z: " << accelerometer_z << L" G";*/

			// Display the raw byte data in hex
			for (int i = 0; i < 65; ++i)
			{
				ss << std::setw(2) << std::setfill(L'0') << std::hex << raw_data[i] << L" ";
				if ((i + 1) % 10 == 0)
				{
					ss << L"\n"; // New line after every 10 bytes for readability
				}
			}

			std::wstring str = ss.str();
			TextOutW(hdc, 10, 10, str.c_str(), str.length());

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
	// Initialize HIDAPI
	if (hid_init())
	{
		std::cerr << "Failed to initialize HIDAPI\n";
		return -1;
	}

	// Open the Joy-Con device
	hid_device* handle = hid_open(JOYCON_VENDOR, JOYCON_PRODUCT, nullptr);
	if (!handle)
	{
		std::cerr << "Failed to open Joy-Con\n";
		return -1;
	}

	// Set non-blocking mode
	hid_set_nonblocking(handle, 1);

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
	std::thread imuThread(read_imu_data, handle);

	// Message loop
	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	imuThread.join(); // Wait for the IMU thread to finish

	hid_close(handle);
	hid_exit();
	return 0;
}
