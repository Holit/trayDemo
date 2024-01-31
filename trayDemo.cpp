#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define WM_PRINT_MOUSEWHEEL WM_USER + 1

static RECT applyRect;

HWND FindTrayToolbarWindow()
{
	HWND hWnd = FindWindow(L"Shell_TrayWnd", NULL);
	if (hWnd)
	{
		hWnd = FindWindowEx(hWnd, NULL, L"TrayNotifyWnd", NULL);
		if (hWnd)
		{
			hWnd = FindWindowEx(hWnd, NULL, L"SysPager", NULL);
			if (hWnd)
			{
				hWnd = FindWindowEx(hWnd, NULL, L"ToolbarWindow32", NULL);
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
		printf("failed to OpenProcess, %d\n" ,GetLastError());
		return area;
	}
	LPVOID lAddress = VirtualAllocEx(hProcess, 0, 4096, MEM_COMMIT, PAGE_READWRITE);
	if (lAddress == NULL) {
		printf("failed to VirtualAllocEx, %d\n", GetLastError());
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
			ReadProcessMemory(hProcess, (LPCVOID)lTextAdr, buff, 1024, 0);
			hwnd = (HWND)(*((DWORD*)buff));
			uid = (UINT)(*((UINT*)buff + 1));
			printf("uid: 0x%x\n", uid);
			uCallbackMessage = (UINT)(*((DWORD*)buff + 2));
			printf("uCallbackMessage: 0x%x\n", uid);

			printf("strPath: %ls\n", (WCHAR*)buff + nStrOffset);
			printf("strTitle: %ls\n", (WCHAR*)buff + nStrOffset + MAX_PATH);
			
			if (wcsstr((WCHAR*)buff + nStrOffset + MAX_PATH,L"Speaker"))
			{
				iSpeakerIndex = i;
			}
		}

	}

	//使用此方法的缺陷是，如果tray更新，需要重新计算，因此需要Hook相关更新函数
	area.left += (((area.right - area.left) / lButton) * iSpeakerIndex );
	area.right -= (((area.right - area.left) / lButton) * (lButton - iSpeakerIndex));


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
		//printf("mouse_wheel: %d\n", delta);

		INPUT ip = { 0 };
		ip.type = INPUT_KEYBOARD;
		ip.ki.wVk = delta > 0 ? VK_VOLUME_UP : VK_VOLUME_DOWN;

		SendInput(1, &ip, sizeof(INPUT));
		ip.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &ip, sizeof(INPUT));

		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}
int WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
){

	SetProcessDPIAware();
	// 创建窗口并获取窗口句柄
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = MainWndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = L"MouseWheelPrintWndClass";
	
	RegisterClass(&wc);

	HWND hWnd = CreateWindowW(wc.lpszClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
	applyRect = EnumNotifyWindow(FindTrayToolbarWindow());
	// 安装鼠标钩子
	g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);

	if (g_hMouseHook == NULL)
	{
//TODO: 将printf替换为OutputDebugStringA
		printf("Failed to install mouse hook\n");
		return ERROR_HOOK_NOT_INSTALLED;
	}
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
		// 卸载鼠标钩子
	UnhookWindowsHookEx(g_hMouseHook);

	return ERROR_SUCCESS;
}
