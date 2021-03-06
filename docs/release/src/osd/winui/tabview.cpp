// For licensing and usage information, read docs/winui_license.txt
//****************************************************************************

// standard windows headers
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wingdi.h>

// MAME/MAMEUI headers
#include "tabview.h"
#include "emu.h"
#include "mui_util.h"
#include "strconv.h"


struct TabViewInfo
{
	const struct TabViewCallbacks *pCallbacks;
	int nTabCount;
	WNDPROC pfnParentWndProc;
};



static struct TabViewInfo *GetTabViewInfo(HWND hWnd)
{
	LONG_PTR l;
	l = GetWindowLongPtr(hWnd, GWLP_USERDATA);
	return (struct TabViewInfo *) l;
}



static LRESULT CallParentWndProc(WNDPROC pfnParentWndProc,
	HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT rc;

	if (!pfnParentWndProc)
		pfnParentWndProc = GetTabViewInfo(hWnd)->pfnParentWndProc;

	if (IsWindowUnicode(hWnd))
		rc = CallWindowProcW(pfnParentWndProc, hWnd, message, wParam, lParam);
	else
		rc = CallWindowProcA(pfnParentWndProc, hWnd, message, wParam, lParam);
	return rc;
}



static LRESULT CALLBACK TabViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	struct TabViewInfo *pTabViewInfo;
	WNDPROC pfnParentWndProc;
	BOOL bHandled = FALSE;
	LRESULT rc = 0;

	pTabViewInfo = GetTabViewInfo(hWnd);
	pfnParentWndProc = pTabViewInfo->pfnParentWndProc;

	switch(message)
	{
		case WM_DESTROY:
			free(pTabViewInfo);
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) pfnParentWndProc);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			break;
	}

	if (!bHandled)
		rc = CallParentWndProc(pfnParentWndProc, hWnd, message, wParam, lParam);

	switch(message)
	{
		case WM_MOVE:
		case WM_SIZE:
			if (pTabViewInfo->pCallbacks->pfnOnMoveSize)
				pTabViewInfo->pCallbacks->pfnOnMoveSize();
			break;
	}

	return rc;
}



static int TabView_GetTabFromTabIndex(HWND hwndTabView, int tab_index)
{
	int shown_tabs = -1;
	int i;
	struct TabViewInfo *pTabViewInfo;

	pTabViewInfo = GetTabViewInfo(hwndTabView);

	for (i = 0; i < pTabViewInfo->nTabCount; i++)
	{
		if (!pTabViewInfo->pCallbacks->pfnGetShowTab || pTabViewInfo->pCallbacks->pfnGetShowTab(i))
		{
			shown_tabs++;
			if (shown_tabs == tab_index)
				return i;
		}
	}
	dprintf("invalid tab index %i\n", tab_index);
	return 0;
}



int TabView_GetCurrentTab(HWND hwndTabView)
{
	struct TabViewInfo *pTabViewInfo;
	LPCSTR pszTab = NULL;
	LPCSTR pszThatTab;
	int i, nTab = -1;

	pTabViewInfo = GetTabViewInfo(hwndTabView);

	if (pTabViewInfo->pCallbacks->pfnGetCurrentTab)
		pszTab = pTabViewInfo->pCallbacks->pfnGetCurrentTab();

	if (pszTab)
	{
		if (pTabViewInfo->pCallbacks->pfnGetTabShortName)
		{
			for (i = 0; i < pTabViewInfo->nTabCount; i++)
			{
				pszThatTab = pTabViewInfo->pCallbacks->pfnGetTabShortName(i);
				if (pszThatTab && (core_stricmp(pszTab, pszThatTab)==0))
				{
					nTab = i;
					break;
				}
			}
		}
		if (nTab < 0)
		{
			nTab = 0;
			sscanf(pszTab, "%d", &nTab);
		}
	}
	else
	{
		nTab = 0;
	}
	return nTab;
}



void TabView_SetCurrentTab(HWND hwndTabView, int nTab)
{
	struct TabViewInfo *pTabViewInfo;
	LPCSTR pszName;
	char szBuffer[16];

	pTabViewInfo = GetTabViewInfo(hwndTabView);

	if (pTabViewInfo->pCallbacks->pfnGetTabShortName)
	{
		pszName = pTabViewInfo->pCallbacks->pfnGetTabShortName(nTab);
	}
	else
	{
		snprintf(szBuffer, sizeof(szBuffer) / sizeof(szBuffer[0]), "%d", nTab);
		pszName = szBuffer;
	}

	if (pTabViewInfo->pCallbacks->pfnSetCurrentTab)
		pTabViewInfo->pCallbacks->pfnSetCurrentTab(pszName);
}



static int TabView_GetCurrentTabIndex(HWND hwndTabView)
{
	int shown_tabs = 0;
	int i;
	int nCurrentTab;
	struct TabViewInfo *pTabViewInfo;

	pTabViewInfo = GetTabViewInfo(hwndTabView);
	nCurrentTab = TabView_GetCurrentTab(hwndTabView);

	for (i = 0; i < pTabViewInfo->nTabCount; i++)
	{
		if (i == nCurrentTab)
			break;

		if (!pTabViewInfo->pCallbacks->pfnGetShowTab || pTabViewInfo->pCallbacks->pfnGetShowTab(i))
			shown_tabs++;
	}
	return shown_tabs;
}



void TabView_UpdateSelection(HWND hwndTabView)
{
	HRESULT res;
	res = TabCtrl_SetCurSel(hwndTabView, TabView_GetCurrentTabIndex(hwndTabView));
	res++;
}



BOOL TabView_HandleNotify(LPNMHDR lpNmHdr)
{
	HWND hwndTabView;
	struct TabViewInfo *pTabViewInfo;
	BOOL bResult = FALSE;
	int nTabIndex, nTab;

	hwndTabView = lpNmHdr->hwndFrom;
	pTabViewInfo = GetTabViewInfo(hwndTabView);

	switch (lpNmHdr->code)
	{
		case TCN_SELCHANGE:
			nTabIndex = TabCtrl_GetCurSel(hwndTabView);
			nTab = TabView_GetTabFromTabIndex(hwndTabView, nTabIndex);
			TabView_SetCurrentTab(hwndTabView, nTab);
			if (pTabViewInfo->pCallbacks->pfnOnSelectionChanged)
				pTabViewInfo->pCallbacks->pfnOnSelectionChanged();
			bResult = TRUE;
			break;
	}
	return bResult;
}



void TabView_CalculateNextTab(HWND hwndTabView)
{
	struct TabViewInfo *pTabViewInfo;
	int i;
	int nCurrentTab;

	pTabViewInfo = GetTabViewInfo(hwndTabView);

	// at most loop once through all options
	for (i = 0; i < pTabViewInfo->nTabCount; i++)
	{
		nCurrentTab = TabView_GetCurrentTab(hwndTabView);
		TabView_SetCurrentTab(hwndTabView, (nCurrentTab + 1) % pTabViewInfo->nTabCount);
		nCurrentTab = TabView_GetCurrentTab(hwndTabView);

		if (!pTabViewInfo->pCallbacks->pfnGetShowTab || pTabViewInfo->pCallbacks->pfnGetShowTab(nCurrentTab))
		{
			// this tab is being shown, so we're all set
			return;
		}
	}
}


void TabView_Reset(HWND hwndTabView)
{
	struct TabViewInfo *pTabViewInfo;
	TC_ITEM tci;
	int i;
	TCHAR* t_text;
	HRESULT res;
	BOOL b_res;

	pTabViewInfo = GetTabViewInfo(hwndTabView);

	b_res = TabCtrl_DeleteAllItems(hwndTabView);

	memset(&tci, 0, sizeof(tci));
	tci.mask = TCIF_TEXT;
	tci.cchTextMax = 20;

	for (i = 0; i < pTabViewInfo->nTabCount; i++)
	{
		if (!pTabViewInfo->pCallbacks->pfnGetShowTab || pTabViewInfo->pCallbacks->pfnGetShowTab(i))
		{
			t_text = ui_wstring_from_utf8(pTabViewInfo->pCallbacks->pfnGetTabLongName(i));
			if( !t_text )
				return;
			tci.pszText = t_text;
			res = TabCtrl_InsertItem(hwndTabView, i, &tci);
			free(t_text);
		}
	}
	TabView_UpdateSelection(hwndTabView);
	res++;
	b_res++;
}


BOOL SetupTabView(HWND hwndTabView, const struct TabViewOptions *pOptions)
{
	struct TabViewInfo *pTabViewInfo;
	LONG_PTR l;
	BOOL bShowTabView;

	assert(hwndTabView);

	// Allocate the list view struct
	pTabViewInfo = (struct TabViewInfo *) malloc(sizeof(struct TabViewInfo));
	if (!pTabViewInfo)
		return FALSE;

	// And fill it out
	memset(pTabViewInfo, 0, sizeof(*pTabViewInfo));
	pTabViewInfo->pCallbacks = pOptions->pCallbacks;
	pTabViewInfo->nTabCount = pOptions->nTabCount;

	// Hook in our wndproc and userdata pointer
	l = GetWindowLongPtr(hwndTabView, GWLP_WNDPROC);
	pTabViewInfo->pfnParentWndProc = (WNDPROC) l;
	SetWindowLongPtr(hwndTabView, GWLP_USERDATA, (LONG_PTR) pTabViewInfo);
	SetWindowLongPtr(hwndTabView, GWLP_WNDPROC, (LONG_PTR) TabViewWndProc);

	bShowTabView = pTabViewInfo->pCallbacks->pfnGetShowTabCtrl ?
		pTabViewInfo->pCallbacks->pfnGetShowTabCtrl() : TRUE;
	ShowWindow(hwndTabView, bShowTabView ? SW_SHOW : SW_HIDE);

	TabView_Reset(hwndTabView);
	if (pTabViewInfo->pCallbacks->pfnOnSelectionChanged)
		pTabViewInfo->pCallbacks->pfnOnSelectionChanged();
	return TRUE;
}


