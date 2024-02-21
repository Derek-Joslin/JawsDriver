#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <atlbase.h>

extern CComModule _Module;
#include <atlwin.h>
#include "SampleBrailleDriver.h"
#define BRLDECLSPEC __declspec(dllexport)

SampleBrailleDriver TheBrailleDriver;
CComModule _Module;

extern "C" BRLDECLSPEC void GetBrailleInterface(IBrailleDriver * *ppIBrailleDriver)
{
	*ppIBrailleDriver = &TheBrailleDriver;
}

SampleBrailleDriver::SampleBrailleDriver()
{
	// _Module.Init(NULL, s_hInstance);
	//Initially, the display is cleared.

	m_hBrailleThread = 0;
	m_heventShutdown = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_heventRefresh = CreateEvent(NULL, TRUE, FALSE, NULL);

	m_IsSleeping = FALSE;
}

SampleBrailleDriver::~SampleBrailleDriver()
{
	SampleBrailleDriver::Close();
	if (m_heventShutdown)
		CloseHandle(m_heventShutdown);
	if (m_heventRefresh)
		CloseHandle(m_heventRefresh);
	// Unregister the  class that was created when the hidden window
	// was created.  
	::UnregisterClass(GetWndClassInfo().m_wc.lpszClassName, GetWndClassInfo().m_wc.hInstance);
}

void SampleBrailleDriver::Close()
{
	SetEvent(m_heventShutdown);

	if (m_hBrailleThread)
	{
		WaitForSingleObject(m_hBrailleThread, INFINITE);
		CloseHandle(m_hBrailleThread);
		m_hBrailleThread = 0;
	}

	if (m_hDevice)
	{
		// ToDo: Stop communications with your display.
		m_hDevice = 0;
	}

	if (m_hWnd)
		DestroyHiddenWindow();
}

BOOL SampleBrailleDriver::Open(
	LPCSTR lpszPort,
	IBrailleDriverCallbacks* callbacks,
	LPCSTR dummy, LPCSTR dummy2)
{
	USES_CONVERSION;
	m_ConsecutiveWriteFailures = 0;
	ATLASSERT(callbacks);
	if (callbacks == nullptr)
	{
		return FALSE;
	}
	ATLASSERT(lpszPort);
	ATLASSERT(lpszPort[0]);
	if ((lpszPort == nullptr) || (lpszPort[0] == 0))
	{
		return FALSE;
	}

	strcpy_s(m_PortInfo, lpszPort);


    //	// ToDo: Begin communication with your display. Try USB first
	// followed by Bluetooth/serial. A device handle is just an
		// Always try USB first.
        // connect to com port
        m_hDevice = //Serial(*lpszPort);

	// ToDo: Retrieve the total number of cells on the display and set
	// the number of regular cells and status cells, if any.

    // grab data from display
    uint8_t nRows = m_hDevice.getNRows();
    uint8_t nColumns = m_hDevice.getNColumns();

    // init braille display size
    m_nCells = nRows * nColumns;//m_nTotalCells - 5;
    m_nTextDisplayStart = 0;
    m_nStatusCellStart = (nRows-1)*nColumns;
    m_nStatusCells = nColumns;

	m_Callbacks = callbacks;

//	InitKeymapName();

    // kinda whatever

//	BeginBrailleThread(); // run New_Haptics.exe

//	Clear();

	return TRUE;
}

void SampleBrailleDriver::InitKeymapName()
{
	USES_CONVERSION;
	char displayName[80];
	// ToDo: Set the name of the JAWS keymap section that will be used
	// to hold the mappings between the key combinations you pass to
	// ProcessBrailleCommandKeys and the JAWS scripts to  be run in
	// response. JAWS will concatenate a space and the  word Keys to the
	// name you return here and use that as the name of the ini section
	// to load.
	// Typically you'd use the name of your display as the keymap
	// name. If you have a different keymap for different types of
	// displays, then make this name unique to each.
	//		sprintf_s(m_keymapName, "%s%d", "PMDisplay", m_nTotalCells);
}
BOOL SampleBrailleDriver::Update()
{
	// ToDo: Update the display with the most recent dot patterns sent
	// by JAWS. This function will be called from the Braille worker
	// thread created when the driver was opened. It's up to you to
	// have stored off the dot patterns sent by JAWS to the Display and
	// DisplayStatus functions. Those ar always called on the main JAWS
	// thread and should not block while you communicate with your
	// display. If you have another asynchronous way of communicating
	// with the display, then you can send the data through that method
	// and not use this worker thread.
	// The following is one way of minimizing the data sent to your
	// display. It's up to you whether or not to use this logic or not. 
	int start, size;
	DWORD dwTime = GetTickCount();
	ResetEvent(m_heventRefresh);

	//If 10 seconds expired since last full update
	//update entire display (don't just update changing cells)
	if (dwTime - m_LastFullDisplayTime > 1000)
		m_FullUpdateRequired = TRUE;

	m_CritSect.Lock();

	if (m_FullUpdateRequired)
	{
		CleanDisplay();
		start = 0;
		size = m_nTotalCells;
		m_FullUpdateRequired = FALSE;
		m_LastFullDisplayTime = dwTime;
	}
	else
	{
		//Identify a section of the display that has changed: we don't identify multiple sections
		int i;
		for (i = 0; i < m_nTotalCells; i++)
			if (m_OldLine[i] != m_NewLine[i]) break;
		//Handle no change
		if (i >= m_nTotalCells)
		{
			m_CritSect.Unlock();
			return TRUE;
		}
		start = i;
		for (i = m_nTotalCells - 1; i > start; i--)
			if (m_OldLine[i] != m_NewLine[i]) break;
		size = i - start + 1;
	}
	memcpy(m_OldLine + start, m_NewLine + start, size);
	// ToDo: Send data to the display
	m_LastDisplayTime = dwTime;
	m_CritSect.Unlock();

	return TRUE;
}

BOOL SampleBrailleDriver::Clear()
{
	m_CritSect.Lock();
	FillMemory(m_NewLine, m_nTotalCells, 0);
	m_FullUpdateRequired = TRUE;
	SetEvent(m_heventRefresh);
	m_CritSect.Unlock();
	return TRUE;
}

BOOL SampleBrailleDriver::Display(const LPBYTE brailleCharactersToDisplay, int nCharacters) {

    // convert to multiline
    // [[0,0,0,0,0,0,0,0],
    //  [0,0,0,0,0,0,0,0],
    //  [0,0,0,0,0,0,0,0]]
    const LPBYTE multiLineBraille[nCharacters/3][3];

//	if (m_IsSleeping)
//		return TRUE;
//	m_CritSect.Lock();
//	memcpy(m_NewLine + m_nTextDisplayStart, lpDots, nLength);
//
//	SetEvent(m_heventRefresh);
//	m_CritSect.Unlock();
	return TRUE;
}

BOOL SampleBrailleDriver::DisplayStatus(const LPBYTE lpDots, int nLength)
{
    // this will display the status text on the bottom row of the braille display


//	if (m_IsSleeping)
//		return TRUE;
//	if (!nLength || m_nStatusCells <= 0) return FALSE;
//	if (nLength > m_nStatusCells) return FALSE;			//error check
//	m_CritSect.Lock();
//	memcpy(m_NewLine + m_nStatusCellStart, lpDots, nLength);
//	SetEvent(m_heventRefresh);
//	m_CritSect.Unlock();
	return TRUE;
}

//Be sure that the space not used for status cells and text cells is blank
void SampleBrailleDriver::CleanDisplay()
{
	m_CritSect.Lock();
	//Simplify the problem by using temporary vars
	int a1s = 0;
	int a1l = 0;
	int a2s = 0;
	int a2l = 0;
	if (m_nStatusCells > 0)
	{
		if (m_nTextDisplayStart > m_nStatusCellStart)
		{
			a1s = m_nStatusCellStart;
			a1l = m_nStatusCells;
			a2s = m_nTextDisplayStart;
			a2l = m_nCells;
		}
		else
		{
			a2s = m_nStatusCellStart;
			a2l = m_nStatusCells;
			a1s = m_nTextDisplayStart;
			a1l = m_nCells;
		}
	}
	else // no status cells
	{
		a1s = m_nTextDisplayStart;
		a1l = m_nCells;
	}

	//Fill blanks at display left edge
	if (a1s)
		FillMemory(m_NewLine, a1s, 0);
	if (m_nStatusCells)
	{
		//Space between first used- and second used-blocks
		if (a1s + a1l < a2s)
			FillMemory(m_NewLine + a1s + a1l, a2s - (a1s + a1l), 0);
		//Space at right end of display
		if (a2s + a2l < m_nTotalCells)
			FillMemory(m_NewLine + a2s + a2l, m_nTotalCells - (a2s + a2l), 0);
	}
	else// no status cells
	{
		//Space at right end of display
		if (a1s + a1l < m_nTotalCells)
			FillMemory(m_NewLine + a1s + a1l, m_nTotalCells - (a1s + a1l), 0);

	}
	m_CritSect.Unlock();

}

BOOL SampleBrailleDriver::SetFirmness(int f)
{
	// ToDo: If your display supports multiple firmness settings, update
	// the driver here.
	return TRUE;
}

BOOL SampleBrailleDriver::SupportsFirmnessSetting(int& countOfSettings)
{
	// ToDo: If your driver supports firmenss settings, indicate how
	// many values there are.
	countOfSettings = 0;
	return FALSE;
}

BOOL SampleBrailleDriver::SetCustomLayout(int nTextDisplayStart, int nTextCells,
	int nStatusCellStart, int nStatusCells)
{
	// ToDo: If you allow custom placement of status cells, this call is
	//JAWS telling you the user's preference.
	//Check passed data
	int a1s = 0;
	int a1l = 0;
	int a2s = 0;
	int a2l = 0;

	if (nTextDisplayStart < 1 || nTextDisplayStart > m_nTotalCells)
		return FALSE;
	if (nTextCells <= 0)
		return FALSE;

	if (nStatusCells)
	{
		if (nTextDisplayStart > nStatusCellStart)
		{
			a1s = nStatusCellStart - 1;
			a1l = nStatusCells;
			a2s = nTextDisplayStart - 1;
			a2l = nTextCells;
		}
		else
		{
			a2s = nStatusCellStart - 1;
			a2l = nStatusCells;
			a1s = nTextDisplayStart - 1;
			a1l = nTextCells;
		}
	}
	else
	{
		a1s = nTextDisplayStart - 1;
		a1l = nTextCells;
	}
	// outside world calls with start offsets 1-based,
	// internally they're 0-based and have now been adjusted
	if (a1s < 0 || a2s < 0)
		return FALSE; // invalid start offsets
	if (a1s + a1l > a2s && a2l > 0)
		return FALSE;// two segments overlap and they shouldn't
	if (a1s + a1l > m_nTotalCells || a2s + a2l > m_nTotalCells)
		return FALSE;// segments too long

	m_nStatusCells = nStatusCells;
	m_nCells = nTextCells;
	m_nTextDisplayStart = nTextDisplayStart - 1;
	if (nStatusCells > 0)
		m_nStatusCellStart = nStatusCellStart - 1;
	else
		m_nStatusCellStart = 0;

	m_FullUpdateRequired = TRUE;
	return TRUE;
}

IBrailleDriverCallbacks* SampleBrailleDriver::SetCallbacks(IBrailleDriverCallbacks* callbacks) {

	auto oldCallbacks = m_Callbacks;
	m_Callbacks = callbacks;
	return oldCallbacks;
}

void SampleBrailleDriver::Timer() {
}

// Braille Thread functions
unsigned int SampleBrailleDriver::BrailleThreadProc() {
	BOOL bTerminate = FALSE;
	HANDLE hEvents[2];
	hEvents[0] = m_heventRefresh;
	hEvents[1] = m_heventShutdown;
	while (!bTerminate) {
		switch (WaitForMultipleObjects(2, hEvents, FALSE, INFINITE)) {
		case WAIT_OBJECT_0:
			Update();
			break;
		case WAIT_OBJECT_0 + 1:
			bTerminate = TRUE;
			break;
		default:
			break;
		}
	}
	return 0;
}

unsigned int __stdcall  SampleBrailleDriver::_BrailleThreadProc(LPVOID lpv) {
	return ((SampleBrailleDriver*)lpv)->BrailleThreadProc();
}


BOOL SampleBrailleDriver::BeginBrailleThread() {
	DWORD dwThreadId = 0;

	m_hBrailleThread =
		(HANDLE)_beginthreadex(NULL, 0, _BrailleThreadProc, this, 0, (unsigned int*)&dwThreadId);
	if (m_hBrailleThread)
		::SetThreadPriority(m_hBrailleThread, THREAD_PRIORITY_ABOVE_NORMAL);

	return (m_hBrailleThread != 0);
}


HWND SampleBrailleDriver::CreateHiddenWindow() {
	// Note: Create() first registers the window class if it has not yet
	// been registered. The newly created window is automatically attached
	// to the CWindowImpl object.
	RECT r = { 0, 0, 0, 0 };
	return Create(0, r, TEXT("BrailleDriverHiddenWindow"));
}

void SampleBrailleDriver::DestroyHiddenWindow() {
	DestroyWindow();
	m_hWnd = 0;
}

BOOL SampleBrailleDriver::Sleep(BOOL fGotoSleep) {
	m_IsSleeping = fGotoSleep;
	return TRUE;
}

LRESULT   SampleBrailleDriver::OnKeyPress(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {
	// ToDo: Call either ProcessBrailleCommandKeys or Utf8TypeKey
    ProcessBrailleCommandKeys("D1+D2+D3");

    // depending on which keys have been pressed. It's up to you to
	// receive  key presses from the display on a background worker
	// thread and then post them to the driver's hidden window using the
	// two DWORD message parameters and decode them here into a string
	// representing the single key or key combination that's been pressed.
	return 0;
}


int SampleBrailleDriver::GetDisplayName(LPSTR pszName, int nMaxChars)
{
	// ToDo:  Copy the name of your display to pszName.
	*pszName = '\0';
	return (int)strlen(pszName);
}
int SampleBrailleDriver::GetDisplayKeyMapName(LPSTR pszName, int nMaxChars)
{
	// ToDo: Copy the name of your display's keymap to pszName. See
	// InitKeymapName for details.
	return (int)strlen(pszName);
}
bool SampleBrailleDriver::UTF8TypeKey(WCHAR wKey)
{
	char UTF8String[10] = { 0 };
	char outString[15] = "\xEF\xBB\xBF";
	WCHAR inString[2] = { wKey, L'\0' };

	int charsWritten = WideCharToMultiByte(CP_UTF8, 0, inString, -1, UTF8String, sizeof(UTF8String), NULL, NULL);
	if (!charsWritten)
		OutputDebugString("We have some kind of conversion problem\n");

	strcat_s(outString, UTF8String);
	return (m_Callbacks->TypeKeys(this, outString, (int)strlen(outString)) == TRUE);
}
BOOL SampleBrailleDriver::GetManufacturerInfo(LPSTR lpszName, int cbMaxNameLen, LPGUID lpguidUnique)
{
	if (!lpszName || !lpguidUnique)
		return FALSE;
	// ToDo: Copy your company name to  lpszName. The GUID is copied
	//	from the __declspec(uuid) part of the class declaration.
	//	_tcscpy_s(lpszName,cbMaxNameLen,FREEDOM_MANUFACTURER_NAME);
	*lpguidUnique = __uuidof(SampleBrailleDriver);
	return TRUE;
}


