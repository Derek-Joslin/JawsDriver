#pragma once
#include "targetver.h"
#include <windows.h>
#include "ibrldrv.h"
#include "ProteusDriver.h"
#define WM_BRL_KEYS (WM_USER+1)
//#define WM_CUSTOM_KEYPRESS (WM_APP + 1) // WM_APP is safe base value for custom messages

// ToDo: Replace the following dummy GUID with your Freedom Scientific display GUID
class
	__declspec(uuid("BE59E613-051A-49C3-B40B-D9A4ED44D4EB"))
	SampleBrailleDriver : public
	IBrailleDriver
	, public CWindowImpl< SampleBrailleDriver, CWindow,
	CWinTraits <WS_OVERLAPPEDWINDOW, 0> >
{
static const int INPUT_BUFFER_SIZE = 100;

// combo types
static const UINT8 NO_COMBO = 0u;
static const UINT8 CHARACTER_COMBO = 1u;
static const UINT8 ROUTER_COMBO = 2u;
static const UINT8 JKM_COMBO = 3u;

public:
	SampleBrailleDriver();
	~SampleBrailleDriver();
public:
	BEGIN_MSG_MAP(SampleBrailleDriver)
		MESSAGE_HANDLER(WM_BRL_KEYS, OnKeyPress)
	END_MSG_MAP()

	BOOL Open(LPCSTR lpszPort, IBrailleDriverCallbacks* pCallbacks,
		LPCSTR dummy, LPCSTR dummy2) override;
	void Close() override;
	BOOL Sleep(BOOL newSleepValue)  override;
	// --- Status info
	DWORD GetInterfaceVersion() override
	{
		return 0x300;
	}
	int GetDisplayName(LPSTR pszName, int cbMaxSize) override;
	int GetDisplayKeyMapName(LPSTR pszName, int cbMaxSize) override;
	int TotalCells() override
	{
		return m_nTotalCells;
	}
	void InitKeymapName();

public:
	BOOL DisplayStatus(const LPBYTE lpDots, int nLength) override;
	BOOL Display(const LPBYTE lpDots, int nLength) override;
	BOOL  Clear() override;
	void Timer() override;

	// Functions added in Interface 0x200;
	BOOL GetTypeKeysMode()  override { return 0; }
	void SetTypeKeysMode(BOOL bEnable = TRUE) override { UNREFERENCED_PARAMETER(bEnable); }
	BOOL SupportsTypeKeysMode() override { return FALSE; }
	BOOL GetManufacturerInfo(LPSTR lpszName, int cbMaxNameLen, LPGUID lpguidUnique) override;

	//Firmness and custom layout
public:
	BOOL SupportsFirmnessSetting(int& nSettings) override;
	BOOL SetFirmness(int nSetting) override;
	BOOL SupportsCustomLayout() override
	{
		return TRUE;
	}
	BOOL SetCustomLayout(int nTextDisplayStart, int nTextCells, int nStatusCellStart, int nStatusCells) override;
	int Cells() override
	{
		return m_nCells;
	}
	int StatusCells() override
	{
		return m_nStatusCells;
	}
	IBrailleDriverCallbacks* SetCallbacks(IBrailleDriverCallbacks* callbacks) override;

	//  Interface functions that either aren't used or have default values that should always be returned
	BOOL SupportsCursorBlinkRate() override { return FALSE; }
	BOOL SetCursorBlinkRate(DWORD dwMiliseconds, BYTE byDotPattern) override { return FALSE; }
	BOOL DisplayWithCursor(const LPBYTE pDots, int nLength, int nCursorColumn) override { return FALSE; }
	BOOL IsKeyWaiting() override { return FALSE; }
	void SetOneHandedMode(BOOL state) override { }
	int GetDriverName(LPSTR lpszName, int cbMaxLength) override { return 0; }
	DWORD GetDriverVersion() override { return 0; }
	DWORD GetDisplayVersion() override { return 0; }
	BOOL HasIntegratedSpeech() override { return FALSE; }
	BOOL GetSpeechInterface(ITTSDriver** ptts) override { return FALSE; }
	BOOL DisplayVertical(const LPBYTE pText, int nLength)  override { return FALSE; }
	BOOL ClearVertical()  override { return TRUE; }
	int VerticalCells() override { return 0; }
private:
	LRESULT OnKeyPress(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	bool UTF8TypeKey(WCHAR wKey);

private:
	BOOL BeginBrailleThread();
	// hidden window so keys are processed in the main thread
	HWND CreateHiddenWindow();
	void DestroyHiddenWindow();

	BOOL Update();
	void CleanDisplay();
	unsigned int BrailleThreadProc();
	static unsigned int __stdcall  _BrailleThreadProc(LPVOID lpv);

	char m_PortInfo[80]{ 0 };
	int m_nCells = 0;
	int m_nStatusCells = 0;
	int m_nTextDisplayStart = 0;
	int m_nStatusCellStart = 0;
	BYTE m_OldLine[85]{ 0 };
	BYTE m_NewLine[85]{ 0 };
	BOOL m_DisplayChanged = TRUE;
	BOOL m_FullUpdateRequired = TRUE;
	CComAutoCriticalSection m_CritSect;
	HANDLE m_heventShutdown = nullptr;
	HANDLE m_heventRefresh = nullptr;

	IBrailleDriverCallbacks* m_Callbacks = nullptr;

	HANDLE m_hBrailleThread = nullptr;
	int m_nTotalCells;

    // DJ custom functions
    void printKeyCombos();
    UINT8 whichCombo(const char*, int);
    void runInputCombos();

    // router executer (slow)
    DWORD routerExecuter(const char*);

    // character executer (slow)
    BYTE characterExecuter(const char*);

    // jkm executer (slow)
    LPCSTR jkmExecuter(const char*);


private:
	char m_keymapName[80]{ 0 };
	DWORD m_LastDisplayTime = 0;
	DWORD m_LastFullDisplayTime = 0;
	DWORD m_ConsecutiveWriteFailures = 0;
	HANDLE m_hDevice = 0;
	BOOL m_IsSleeping = FALSE;
    ProteusDriver m_NhDriver;
    BYTE* m_keyInputBuffer;
    BYTE m_currentDots;
    int m_nKeyInputBits;
    DWORD m_routerLocation;
    HWND m_hWnd;
    char m_inputType;
};
