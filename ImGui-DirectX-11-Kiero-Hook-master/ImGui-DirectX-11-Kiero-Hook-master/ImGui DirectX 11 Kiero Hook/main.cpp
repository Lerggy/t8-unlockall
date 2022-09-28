#include "includes.h"
#include <TlHelp32.h>
#include <cstdint>
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Present oPresent;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;

void InitImGui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

std::uintptr_t dwProcessBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(NULL));

void initalize()
{

	SetUnhandledExceptionFilter(hookHandler);

	HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;
	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

	if (hThreadSnap)
	{
		te32.dwSize = sizeof(THREADENTRY32);

		if (!Thread32First(hThreadSnap, &te32))
		{
			CloseHandle(hThreadSnap);
			return;
		}

		do
		{
			if (te32.th32OwnerProcessID == GetCurrentProcessId() && te32.th32ThreadID != GetCurrentThreadId())
			{
				HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, 0, te32.th32ThreadID);
				if (hThread)
				{
					CONTEXT context;
					context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
					SuspendThread(hThread);

					if (GetThreadContext(hThread, &context))
					{
						context.Dr0 = (uintptr_t)(dwProcessBase + 0x37F6FD0); //BM Spoof

						context.Dr7 = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6);

						SetThreadContext(hThread, &context);
					}

					ResumeThread(hThread);
					CloseHandle(hThread);
				}
			}
		} while (Thread32Next(hThreadSnap, &te32));
		CloseHandle(hThreadSnap);
	}
}

__int64 hkLiveInventory_GetItemQuantity(LPEXCEPTION_POINTERS ex)
{
	return 1337;
}

LONG CALLBACK hookHandler(LPEXCEPTION_POINTERS ex)
{

	if (ex->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
	{
		if (ex->ContextRecord->Rip == (uintptr_t)(dwProcessBase + 0x37F6FD0))
		{
			ex->ContextRecord->Rip = (uintptr_t)hkLiveInventory_GetItemQuantity;
		}

		return EXCEPTION_CONTINUE_EXECUTION;
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

bool init = false;
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!init)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)& pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)& pBackBuffer);
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			InitImGui();
			init = true;
		}

		else
			return oPresent(pSwapChain, SyncInterval, Flags);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Render();

	pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpReserved)
{
	bool init_hook = false;
	do
	{
		if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
		{
			kiero::bind(8, (void**)& oPresent, hkPresent);
			init_hook = true;
		}
	} while (!init_hook);
	return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		initalize();
		CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		kiero::shutdown();
		break;
	}
	return TRUE;
}