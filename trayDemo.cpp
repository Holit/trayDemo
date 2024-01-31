#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define WM_PRINT_MOUSEWHEEL WM_USER + 1

static RECT applyRect;
HWND FindTrayToolbarWindow()
{
	HWND hWnd = ::FindWindow(L"Shell_TrayWnd", NULL);
	if (hWnd)
	{
		hWnd = ::FindWindowEx(hWnd, NULL, L"TrayNotifyWnd", NULL);
		if (hWnd)
		{
			hWnd = ::FindWindowEx(hWnd, NULL, L"SysPager", NULL);
			if (hWnd)
			{
				hWnd = ::FindWindowEx(hWnd, NULL, L"ToolbarWindow32", NULL);
			}
		}
	}
	return hWnd;
}
RECT EnumNotifyWindow(HWND hWnd)
{
	RECT area = { 0 };
	DWORD dwProcessId = 0;
	GetWindowThreadProcessId(hWnd, &dwProcessId);

	HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, dwProcessId);
	if (hProcess == NULL) {
		printf("failed to OpenProcess, %d" ,GetLastError());
		return area;
	}
	LPVOID lAddress = VirtualAllocEx(hProcess, 0, 4096, MEM_COMMIT, PAGE_READWRITE);
	if (lAddress == NULL) {
		printf("failed to VirtualAllocEx, %d", GetLastError());
		return area;
	}

	DWORD lTextAdr = 0;
	BYTE buff[1024] = { 0 };

	HWND hwnd = NULL;
	UINT uid;
	UINT uCallbackMessage;
	int nDataOffset = sizeof(TBBUTTON) - sizeof(INT_PTR) - sizeof(DWORD_PTR);
	int nStrOffset = 18;
	//是否是64位
#ifdef _WIN64
#else
	nDataOffset += 4;
	nStrOffset += 6;
#endif

	GetWindowRect(hWnd, &area);
	/*area = {LT(3232, 2080) RB(3472, 2160)  [240 x 80]}*/

	//得到圖標個數
	int lButton = SendMessage(hWnd, TB_BUTTONCOUNT, 0, 0);
	
	int iSpeakerIndex = 0;
	for (int i = 0; i < lButton; i++)
	{
		//得到TBBUTTON的地址
		SendMessage(hWnd, TB_GETBUTTON, i, (LPARAM)lAddress);
		TBBUTTON tb;
		//读TBBUTTON的地址
		ReadProcessMemory(hProcess, lAddress, &tb, sizeof(TBBUTTON), 0);

		//得到TRAYDATA的地址
		ReadProcessMemory(hProcess, (LPVOID)((DWORD)lAddress + nDataOffset), &lTextAdr, 4, 0);
		if (lTextAdr != -1) {
			//讀TRAYDATA的地址
			ReadProcessMemory(hProcess, (LPCVOID)lTextAdr, buff, 1024, 0);
			hwnd = (HWND)(*((DWORD*)buff));
			uid = (UINT)(*((UINT*)buff + 1));
			printf("uid: 0x%x\n", uid);
			//qInfo() << uid;
			uCallbackMessage = (UINT)(*((DWORD*)buff + 2));
			printf("uCallbackMessage: 0x%x\n", uid);

			printf("strPath: %ls\n", (WCHAR*)buff + nStrOffset);
			printf("strTitle: %ls\n", (WCHAR*)buff + nStrOffset + MAX_PATH);
			
			if (wcsstr((WCHAR*)buff + nStrOffset + MAX_PATH,L"Speaker"))
			{
				printf("Hit!\n");
				iSpeakerIndex = i;
			}
			//hIcon = (HICON)(*((DWORD*)buff + 6));
		}

	}
	//使用此方法的缺陷是，如果tray更新，需要重新计算，因此需要Hook相关更新函数
	area.left += (((area.right - area.left) / lButton) * iSpeakerIndex );
	area.right -= (((area.right - area.left) / lButton) * (lButton - iSpeakerIndex) );


	VirtualFreeEx(hProcess, lAddress, 4096, MEM_RELEASE);
	CloseHandle(hProcess);

	return area;
}
// 全局变量，用于存储鼠标钩子的句柄
HHOOK g_hMouseHook = NULL;

// 鼠标事件回调函数
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0)
	{
		if (wParam == WM_MOUSEWHEEL)
		{
			
			MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
			if (PtInRect(&applyRect, pMouseStruct->pt))
			{
				// 打印滚轮信息
				int delta = GET_WHEEL_DELTA_WPARAM(pMouseStruct->mouseData);

				PostMessageW(FindWindow(L"MouseWheelPrintWndClass", NULL), WM_PRINT_MOUSEWHEEL, 0, (LPARAM)delta);
			}
		}
	}

	// 传递事件给下一个钩子或默认处理程序
	return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PRINT_MOUSEWHEEL:
	{
		// 接收并打印鼠标滚轮信息
		int delta = (int)lParam;
		printf("mouse_wheel: %d\n", delta);
		if (delta > 0)
		{
			INPUT ip = { 0 };
			ip.type = INPUT_KEYBOARD;
			ip.ki.wVk = VK_VOLUME_UP;   //or VOLUME_DOWN or MUTE
			SendInput(1, &ip, sizeof(INPUT));
			ip.ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(1, &ip, sizeof(INPUT));
		}
		else
		{

			INPUT ip = { 0 };
			ip.type = INPUT_KEYBOARD;
			ip.ki.wVk = VK_VOLUME_DOWN;   //or VOLUME_DOWN or MUTE
			SendInput(1, &ip, sizeof(INPUT));
			ip.ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(1, &ip, sizeof(INPUT));
		}
		//查找下述内容：
		/*
			uid: 0x0
			uCallbackMessage: 0x0
			strPath: {F38BF404-1D43-42F2-9305-67DE0B28FC23}\explorer.exe
			strTitle: Speakers: 34%
		*/

		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}
int main() {
	/*
	// 获取系统中扬声器设备的数量
	UINT numDevices = waveOutGetNumDevs();

	if (numDevices == 0) {
		// 没有可用的扬声器设备
		printf("No audio devices found.\n");
		return 1;
	}

	printf("Number of audio devices: %u\n", numDevices);

	for (UINT deviceID = 0; deviceID < numDevices; ++deviceID) {

		// 获取扬声器设备的名称
		WAVEOUTCAPS waveOutCaps;
		MMRESULT result = waveOutGetDevCaps(deviceID, &waveOutCaps, sizeof(WAVEOUTCAPS));

		// 获取当前扬声器音量
		DWORD currentVolume;
		result = waveOutGetVolume((HWAVEOUT)deviceID, &currentVolume);

		if (result != MMSYSERR_NOERROR) {
			// 处理错误
			printf("Error getting volume for device %u\n", deviceID);
			continue;
		}

		// 获取左右声道的音量值
		WORD leftVolume = LOWORD(currentVolume);
		WORD rightVolume = HIWORD(currentVolume);

		printf("Device %u - Name: %ls, Left: 0x%04x, Right: 0x%04x\n", deviceID, waveOutCaps.szPname, leftVolume, rightVolume);
	}

	// 获取当前扬声器音量
	UINT deviceID = WAVE_MAPPER; // 使用WAVE_MAPPER表示默认音频设备
	DWORD currentVolume;
	MMRESULT result = waveOutGetVolume((HWAVEOUT)0, &currentVolume);
	if (result != MMSYSERR_NOERROR) {
		// 处理错误
		return 1;
	}
	// 获取左右声道的音量值
	WORD leftVolume = LOWORD(currentVolume);
	WORD rightVolume = HIWORD(currentVolume);

	printf("Current volume - Left: 0x%04x, Right: 0x%04x\n", leftVolume, rightVolume);

	// 计算新的左声道音量值（加1%）
	int newLeftVolume = (int)leftVolume + (int)(0.01 * 0x7FFF);

	// 确保音量在合理范围内（0x0000 到 0x7FFF）
	newLeftVolume = max(0, min(0x7FFF, newLeftVolume));

	DWORD newVolume = MAKELONG(newLeftVolume, newLeftVolume);
	result = waveOutSetVolume((HWAVEOUT)deviceID, newVolume);


	return 0;
	*/
	SetProcessDPIAware();
	// 创建窗口并获取窗口句柄
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = MainWndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = L"MouseWheelPrintWndClass";

	RegisterClass(&wc);

	HWND hWnd = CreateWindow(wc.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
	applyRect = EnumNotifyWindow(FindTrayToolbarWindow());
	// 安装鼠标钩子
	g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);

	if (g_hMouseHook == NULL)
	{
		printf("Failed to install mouse hook\n");
		return 1;
	}
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	printf("Mouse hook installed. Press Enter to exit...\n");

	// 等待用户按Enter键，然后卸载钩子
	getchar();

	// 卸载鼠标钩子
	UnhookWindowsHookEx(g_hMouseHook);

	return 0;
	return 0;
}
