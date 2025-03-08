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

// Global variables to store accelerometer data
float accelerometer_x = 0.0f;
float accelerometer_y = 0.0f;
float accelerometer_z = 0.0f;

HWND hwnd;  // Global window handle

// Send command to the device
void sendCommand(hid_device* handle, const std::vector<unsigned char>& data) {
    hid_write(handle, data.data(), data.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Read IMU data from Joy-Con
void readIMUData(hid_device* handle) {
    // Enable IMU
    sendCommand(handle, { 0x40, 0x01 });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sendCommand(handle, { 0x40, 0x01 });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Set IMU sensitivity
    sendCommand(handle, { 0x41, 0x03, 0x00, 0x01, 0x02 }); // Accel ±8G, Gyro ±2000dps
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const float ACCEL_SCALE = 0.000244f;  // Correct scale for ±8G

    // Simulate data update
    for (int i = 0; i < 20; ++i)
    {
        unsigned char data[49];
        int bytesRead = hid_read(handle, data, sizeof(data));

        if (bytesRead < 49) {
            std::cerr << "Failed to read IMU data.\n";
            continue;
        }

        // Check if it's a valid IMU report (packet ID should be 0x30 or 0x31)
        if (data[0] != 0x30 && data[0] != 0x31) {
            std::cerr << "Unexpected packet ID: " << std::hex << (int)data[0] << std::dec << "\n";
            continue;
        }

        // Read accelerometer data
        int16_t accel_x_raw = (data[13] << 8) | data[12]; // Little-endian byte order
        int16_t accel_y_raw = (data[15] << 8) | data[14];
        int16_t accel_z_raw = (data[17] << 8) | data[16];

        float ax = accel_x_raw * ACCEL_SCALE;
        float ay = accel_y_raw * ACCEL_SCALE;
        float az = accel_z_raw * ACCEL_SCALE;

        // Update global accelerometer data
        accelerometer_x = ax;
        accelerometer_y = ay;
        accelerometer_z = az;

        // Notify the window to repaint itself
        InvalidateRect(hwnd, nullptr, TRUE);

        std::this_thread::sleep_for(std::chrono::milliseconds(15)); // Match 15ms report rate
    }
}

// Window procedure for handling window messages
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
        ss << L"Accelerometer:\n"
            << L"X: " << accelerometer_x << L" G\n"
            << L"Y: " << accelerometer_y << L" G\n"
            << L"Z: " << accelerometer_z << L" G";

        std::wstring str = ss.str();
        int yPos = 10;

        TextOutW(hdc, 10, yPos, L"Accelerometer Data", 19);
        yPos += 20;
        TextOutW(hdc, 10, yPos, (L"X: " + std::to_wstring(accelerometer_x) + L" G").c_str(), (L"X: " + std::to_wstring(accelerometer_x) + L" G").size());
        yPos += 20;
        TextOutW(hdc, 10, yPos, (L"Y: " + std::to_wstring(accelerometer_y) + L" G").c_str(), (L"Y: " + std::to_wstring(accelerometer_y) + L" G").size());
        yPos += 20;
        TextOutW(hdc, 10, yPos, (L"Z: " + std::to_wstring(accelerometer_z) + L" G").c_str(), (L"Y: " + std::to_wstring(accelerometer_y) + L" G").size());

        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// Main function to initialize HID, create window and read IMU data
int main() {
    // Initialize HIDAPI
    if (hid_init()) {
        std::cerr << "Failed to initialize HIDAPI\n";
        return -1;
    }

    // Open the Joy-Con device
    hid_device* handle = hid_open(JOYCON_VENDOR, JOYCON_PRODUCT, NULL);
    if (!handle) {
        std::cerr << "Failed to open Joy-Con\n";
        return -1;
    }

    // Window class registration
    const wchar_t CLASS_NAME[] = L"AccelerometerWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;  // Set window procedure
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassW(&wc)) {  // Use RegisterClassW for wide strings
        std::cerr << "Window class registration failed!" << std::endl;
        return -1;
    }

    // Create the window
    hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Accelerometer Data", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd) {
        std::cerr << "Window creation failed!" << std::endl;
        return -1;
    }

    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    // Start a thread to read IMU data
    std::thread imuThread(readIMUData, handle);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    imuThread.join();  // Wait for the IMU thread to finish

    hid_close(handle);
    hid_exit();
    return 0;
}
