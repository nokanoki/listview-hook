#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include <sstream>
#include <vector>


struct Context
{
	HANDLE hProc;
	HWND hMainWnd;
	HWND hListWnd;
	std::vector<std::vector<std::wstring>> items;
	std::vector<std::wstring> columns;
	std::wstringstream error;
};


extern "C" __declspec(dllexport)  int __stdcall CreateContext(
	wchar_t *strFindByWndTitle,
	wchar_t *strOpenProcces,
	wchar_t *strListViewClassName,
	void **pContext
	);
extern "C" __declspec(dllexport)  int __stdcall Read(Context *pContext);

extern "C" __declspec(dllexport) int __stdcall GetColumnCount(Context *pContex);
extern "C" __declspec(dllexport) int __stdcall GetRowCount(Context *pContext);
extern "C" __declspec(dllexport) int __stdcall GetStringAt(Context *pContext, int row, int column, wchar_t *buff, int maxSize);
extern "C" __declspec(dllexport) int __stdcall GetError(Context *pContext, wchar_t *str,int count);


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
	)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}


int __stdcall CreateContext(
	wchar_t *strFindByWndTitle,
	wchar_t *strOpenProcces,
	wchar_t *strListViewClassName,
	void **pContext
	)
{
	Context *context = new Context;
	context->hListWnd = 0;
	context->hMainWnd = 0;
	context->hProc = INVALID_HANDLE_VALUE;

	*pContext = context;

	if (!strOpenProcces && wcslen(strOpenProcces))
	{
		PROCESS_INFORMATION info = { 0 };
		STARTUPINFO startUp = { 0 };
		startUp.cb = sizeof(startUp);

		if (!CreateProcess(
				strOpenProcces,
				0,
				0,
				0,
				TRUE,
				0,
				0,
				0,
				&startUp,
				&info))
		{
			context->error << L"cannot open procces. (GetLastError: " << GetLastError() << L")";
			return 1;
		}
		context->hProc = info.hProcess;
		Sleep(1000);
	}
	context->hMainWnd = FindWindowEx(0, 0, 0, strFindByWndTitle);
	if (context->hMainWnd == NULL)
	{
		context->error << L"Cannot find window with title: " << strFindByWndTitle;
		return 1;
	}
	context->hListWnd = FindWindowEx(context->hMainWnd, 0, strListViewClassName, 0);
	if (context->hListWnd == NULL)
	{
		context->error << L"Cannot find list control with class name: " << strListViewClassName;
		return 1;
	}
	if (context->hProc == INVALID_HANDLE_VALUE)
	{
		DWORD dwProcId;
		GetWindowThreadProcessId(context->hMainWnd, &dwProcId);
		context->hProc = OpenProcess(
			PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
			FALSE,
			dwProcId
			);
		if (context->hProc == INVALID_HANDLE_VALUE)
		{
			context->error << L"cannot open process. GetLastError:" << GetLastError();
			return 1;
		}
	}
	return 0;
}


int __stdcall Read(Context *pContext)
{
	if (!pContext)
		return -1;
	if (pContext->hProc == INVALID_HANDLE_VALUE || pContext->hListWnd == NULL)
	{
		pContext->error << L"Invalid handle";
		return -1;
	}
	pContext->items.clear();

	LV_ITEM lvItem = { 0 };
	LV_COLUMN lvColumn = { 0 };
	LV_ITEM *pCrossLvItem = 0;
	LV_COLUMN *pCrossLvColumn = 0;
	wchar_t buff[256];
	
	lvItem.cchTextMax = 256;
	lvItem.mask = LVIF_TEXT;

	lvColumn.mask = LVCF_TEXT;
	lvColumn.cchTextMax = 256;


	LPWSTR crossBuff = (LPWSTR)VirtualAllocEx(pContext->hProc, NULL, 256 * sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE);
	pCrossLvItem = (LV_ITEM*)VirtualAllocEx(pContext->hProc, NULL, sizeof(LV_ITEM), MEM_COMMIT, PAGE_READWRITE);
	pCrossLvColumn = (LV_COLUMN*)VirtualAllocEx(pContext->hProc, NULL, sizeof(LV_COLUMN), MEM_COMMIT, PAGE_READWRITE);

	int columnCount = 0;
	for (;;)
	{
		lvColumn.pszText = crossBuff;
		WriteProcessMemory(pContext->hProc, pCrossLvColumn, &lvColumn, sizeof(lvColumn), NULL);
		if (!ListView_GetColumn(pContext->hListWnd, columnCount, pCrossLvColumn))
			break;
		columnCount++;
	}
	
	for (
		int idx = ListView_GetNextItem(pContext->hListWnd,-1,LVNI_ALL);
		idx != -1 ;
		idx = ListView_GetNextItem(pContext->hListWnd,idx,LVNI_ALL)
		)
	{
		std::vector<std::wstring> row;
		for (int subIdx = 0; subIdx < columnCount; subIdx++)
		{
			lvItem.iItem = idx;
			lvItem.iSubItem = subIdx;
			lvItem.pszText = crossBuff;
			WriteProcessMemory(pContext->hProc, pCrossLvItem, &lvItem, sizeof(lvItem), NULL);

			if (!ListView_GetItem(pContext->hListWnd, pCrossLvItem))
				break;
			ReadProcessMemory(pContext->hProc, pCrossLvItem, &lvItem, sizeof(lvItem), NULL);
			ReadProcessMemory(pContext->hProc, lvItem.pszText, buff, sizeof(buff), NULL);
			row.push_back(std::wstring(buff));

		}
		pContext->items.push_back(row);
	}
	VirtualFreeEx(pContext->hProc, pCrossLvItem, sizeof(LV_ITEM), MEM_RELEASE);
	VirtualFreeEx(pContext->hProc, crossBuff, 256, MEM_RELEASE);
	VirtualFreeEx(pContext->hProc, pCrossLvColumn, sizeof(LV_COLUMN), MEM_RELEASE);

	return 0;

}

int __stdcall GetColumnCount(Context *pContex)
{
	if (pContex)
	{
		if (pContex->items.size())
		{
			return pContex->items[0].size();
		}
	}
	return -1;
}

int __stdcall GetRowCount(Context *pContext)
{
	if (pContext)
	{
		return pContext->items.size();
	}
	return -1;
}
int __stdcall GetStringAt(Context *pContext, int row, int column, wchar_t *buff, int maxSize)
{
	if (!pContext)
		return -1;
	if (row < 0 || row > pContext->items.size())
	{
		pContext->error << L"row overflow";
		return -1;
	}
	if (column < 0 || column > pContext->items[row].size())
	{
		pContext->error << L"column overflow";
		return -1;
	}
	if (pContext->items[row][column].size() > maxSize)
	{
		pContext->error << L"data overflow";
		return -1;
	}
	wcscpy(buff, pContext->items[row][column].c_str());
	return 0;
}
int __stdcall GetError(Context *pContext,wchar_t *str, int count)
{
	if (pContext && str)
	{
		int sz = pContext->error.str().size();
		if (sz > count)
			return -1;
		wcscpy(str, pContext->error.str().c_str());
	}
	return 0;

}