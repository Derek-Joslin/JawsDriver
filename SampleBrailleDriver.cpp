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

extern "C" BRLDECLSPEC void GetBrailleInterface(IBrailleDriver * *ppIBrailleDriver) {
	*ppIBrailleDriver = &TheBrailleDriver;
}

SampleBrailleDriver::SampleBrailleDriver() {
	// _Module.Init(NULL, s_hInstance);
	// Initially, the display is cleared.

	m_hBrailleThread = 0;
	m_heventShutdown = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_heventRefresh = CreateEvent(NULL, TRUE, FALSE, NULL);

	m_IsSleeping = FALSE;

    // create Proteus Driver
    m_NhDriver = ProteusDriver();
    m_routerLocation = 0;
}

SampleBrailleDriver::~SampleBrailleDriver() {
	SampleBrailleDriver::Close();
	if (m_heventShutdown)
		CloseHandle(m_heventShutdown);
	if (m_heventRefresh)
		CloseHandle(m_heventRefresh);
	// Unregister the  class that was created when the hidden window
	// was created.  
	::UnregisterClass(GetWndClassInfo().m_wc.lpszClassName, GetWndClassInfo().m_wc.hInstance);

}

void SampleBrailleDriver::Close() {
	SetEvent(m_heventShutdown);

	if (m_hBrailleThread) {
		WaitForSingleObject(m_hBrailleThread, INFINITE);
		CloseHandle(m_hBrailleThread);
		m_hBrailleThread = 0;
	}

	if (m_hDevice) {
		// ToDo: Stop communications with your display.
		m_hDevice = 0;
	}

	if (m_hWnd)
		DestroyHiddenWindow();
}

BOOL SampleBrailleDriver::Open(
	LPCSTR lpszPort,
	IBrailleDriverCallbacks* callbacks,
	LPCSTR dummy, LPCSTR dummy2) {
	strcpy_s(m_PortInfo, lpszPort);

    // begine new haptics driver comport communication
    m_NhDriver.Open(lpszPort);


    // ToDo: Retrieve the total number of cells on the display and set
    // hardcoded
    m_nTotalCells = 72;
    m_nCells = m_nTotalCells;
    m_nTextDisplayStart = 0;
    m_nStatusCellStart = 0;
    m_nStatusCells = 0;

    // create the Jaws interrupt callbacks
	m_Callbacks = callbacks;

    // send info about the proper .jkm file
    InitKeymapName();

    // begin the NH JAWS Driver thread
	BeginBrailleThread();

    // "clear" the display
	Clear();

    // create a gui object to make callbacks to JAWS
    m_hWnd = CreateHiddenWindow(); // Implement this method to create a hidden window

	return TRUE;
}

void SampleBrailleDriver::InitKeymapName() {
	USES_CONVERSION;
	char displayName[] = "Proteus";
	// ToDo: Set the name of the JAWS keymap section that will be used
	// to hold the mappings between the key combinations you pass to
	// ProcessBrailleCommandKeys and the JAWS scripts to  be run in
	// response. JAWS will concatenate a space and the  word Keys to the
	// name you return here and use that as the name of the ini section
	// to load.
	// Typically you'd use the name of your display as the keymap
	// name. If you have a different keymap for different types of
	// displays, then make this name unique to each.
    // Setting the keymap name
    // Assuming m_keymapName is a member variable of sufficient size
    sprintf_s(m_keymapName, "%s", displayName, m_nTotalCells);
    char funcIdentifier[] = "InitKeymapName";

//    m_NhDriver.write(funcIdentifier);
//    m_NhDriver.write(m_keymapName);
}

BOOL SampleBrailleDriver::Update() {
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
	// ToDo: Implement a standard communication method to the display
    char interruptor[] = "INT";
    m_NhDriver.write(interruptor);

    m_NhDriver.write(m_OldLine,m_nTotalCells);

    char terminator[] = "END";
    m_NhDriver.write(terminator);

	m_LastDisplayTime = dwTime;

	m_CritSect.Unlock();

	return TRUE;
}

// Braille Thread functions
unsigned int SampleBrailleDriver::BrailleThreadProc() {
	BOOL bTerminate = FALSE;
	HANDLE hEvents[2];
	hEvents[0] = m_heventRefresh;
	hEvents[1] = m_heventShutdown;
	while (!bTerminate) {
        if (m_NhDriver.bytesAvailable() > 0) {
            m_NhDriver.updateInputQueue();

            // get a char
            PostMessageW(m_hWnd, WM_BRL_KEYS, 0, 0);

        }
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

// just used to start the main loop of driver
unsigned int __stdcall  SampleBrailleDriver::_BrailleThreadProc(LPVOID lpv) {
	return ((SampleBrailleDriver*)lpv)->BrailleThreadProc();
}

// called and starts main loop of driver with certain settings in windows
BOOL SampleBrailleDriver::BeginBrailleThread() {
	DWORD dwThreadId = 0;

	m_hBrailleThread = (HANDLE)_beginthreadex(NULL, 0, _BrailleThreadProc, this, 0, (unsigned int*)&dwThreadId);
	if (m_hBrailleThread)
		::SetThreadPriority(m_hBrailleThread, THREAD_PRIORITY_ABOVE_NORMAL);

	return (m_hBrailleThread != 0);
}

// creates a gui element which will run in the main jaws thread
HWND SampleBrailleDriver::CreateHiddenWindow() {
	// Note: Create() first registers the window class if it has not yet
	// been registered. The newly created window is automatically attached
	// to the CWindowImpl object.
	RECT r = { 0, 0, 0, 0 };
	return Create(0, r, TEXT("BrailleDriverHiddenWindow"));
}
// destroys the gui
void SampleBrailleDriver::DestroyHiddenWindow() {
	DestroyWindow();
	m_hWnd = 0;
}

// this function is the main curious one IDK
LRESULT SampleBrailleDriver::OnKeyPress(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) {

    // determine the message being received
    switch (uMsg) {
        // Interpret wParam and lParam to get the key combination
        case WM_BRL_KEYS:
            if (!m_NhDriver.m_inputQueue.empty()) {

                // load that data into a static input buffer
                m_NhDriver.loadInputBuffer();

                // TODO: parse the data received and run the associated functions
                char postMessage[] = "Key Combos Recognized\n";
                m_NhDriver.write(postMessage);
                // print the inputBuffer
                printKeyCombos();
                runInputCombos();
            }
        break;

    }


    return 0;
}

// IDK why these are here

// IDK test these or something
int SampleBrailleDriver::GetDisplayName(LPSTR pszName, int nMaxChars) {
	// ToDo:  Copy the name of your display to pszName.
    // Define the display name
    const char* displayName = "Proteus"; // Replace with your actual display name

    // Copy the name into pszName, ensuring not to exceed nMaxChars
    strncpy_s(pszName, nMaxChars, displayName, _TRUNCATE);
    char funcIdentifier[] = "GetDisplayName";

//    m_NhDriver.write(funcIdentifier);
    // print to see if there is anything going on here
    // m_NhDriver.write(pszName);

    // Return the length of the copied string
    return (int)strlen(pszName);

}

int SampleBrailleDriver::GetDisplayKeyMapName(LPSTR pszName, int nMaxChars) {
	// ToDo: Copy the name of your display's keymap to pszName. See
    // Assuming m_keymapName is the member variable set in InitKeymapName
    // Copy the keymap name into pszName, ensuring not to exceed nMaxChars
    strncpy_s(pszName, nMaxChars, m_keymapName, _TRUNCATE);
    char funcIdentifier[] = "GetDisplayKeyMapName";

//    m_NhDriver.write(funcIdentifier);
    // m_NhDriver.write(pszName);

    // Return the length of the copied string
    return (int)strlen(pszName);
}


BOOL SampleBrailleDriver::GetManufacturerInfo(LPSTR lpszName, int cbMaxNameLen, LPGUID lpguidUnique) {

	if (!lpszName || !lpguidUnique)
		return FALSE;
	// ToDo: Copy your company name to  lpszName. The GUID is copied
	//	from the __declspec(uuid) part of the class declaration.


	_tcscpy_s(lpszName, cbMaxNameLen, "New Haptics");

    char funcIdentifier[] = "GetManufacturerInfo";

//    m_NhDriver.write(funcIdentifier);

    // confirm the the manufacturer name is registered
    // m_NhDriver.write(lpszName);

	*lpguidUnique = __uuidof(SampleBrailleDriver);



	return TRUE;
}

// I don't think this matters
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

/* IDK what these do yet */
IBrailleDriverCallbacks* SampleBrailleDriver::SetCallbacks(IBrailleDriverCallbacks* callbacks)
{

    auto oldCallbacks = m_Callbacks;
    m_Callbacks = callbacks;
    return oldCallbacks;
}

void SampleBrailleDriver::Timer()
{
}

BOOL SampleBrailleDriver::Sleep(BOOL fGotoSleep)
{
    m_IsSleeping = fGotoSleep;
    return TRUE;
}

/* FUNCTIONS USED BY JAWS */

// called by JAWS
BOOL SampleBrailleDriver::Clear() {
    m_CritSect.Lock();
    FillMemory(m_NewLine, m_nTotalCells, 0);
    m_FullUpdateRequired = TRUE;
    SetEvent(m_heventRefresh);
    m_CritSect.Unlock();
    return TRUE;
}

// called by JAWS
BOOL SampleBrailleDriver::Display(const LPBYTE lpDots, int nLength) {
    if (m_IsSleeping)
        return TRUE;
    m_CritSect.Lock();

    memcpy(m_NewLine + m_nTextDisplayStart, lpDots, nLength);

    SetEvent(m_heventRefresh);
    m_CritSect.Unlock();
    return TRUE;
}

// called by JAWS
BOOL SampleBrailleDriver::DisplayStatus(const LPBYTE lpDots, int nLength) {
    if (m_IsSleeping)
        return TRUE;
    if (!nLength || m_nStatusCells <= 0) return FALSE;
    if (nLength > m_nStatusCells) return FALSE;			//error check
    m_CritSect.Lock();
    memcpy(m_NewLine + m_nStatusCellStart, lpDots, nLength);
    SetEvent(m_heventRefresh);
    m_CritSect.Unlock();
    return TRUE;
}

// called by JAWS (I think)
// Be sure that the space not used for status cells and text cells is blank
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

/* IRRELEVANT TO OUR DISPLAY (so far) */

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

void SampleBrailleDriver::printKeyCombos() {

    int stringLen = 0; // (int) (sizeof(char) * strlen(m_inputBuffer));
    int stringPosition = 0;

    // determine the keyCombo for the current Buffer
    UINT8 KeyCombo = whichCombo(&m_NhDriver.m_inputBuffer[stringPosition], INPUT_BUFFER_SIZE - stringPosition);

    // traverse while there is a string
    while (KeyCombo != NO_COMBO) {

        // get the length of the string at the current position
        stringLen = (int) (sizeof(char) * strlen(&m_NhDriver.m_inputBuffer[stringPosition]));

        // print the length of the string and it's value
        m_NhDriver.write(&m_NhDriver.m_inputBuffer[stringPosition]);
        char newLine[] = "\n";
        m_NhDriver.write(newLine);
        // move the position
        stringPosition = stringPosition + stringLen + 1;

        // record the next keyCombo
        KeyCombo = whichCombo(&m_NhDriver.m_inputBuffer[stringPosition], INPUT_BUFFER_SIZE - stringPosition);

    }

}

void SampleBrailleDriver::runInputCombos() {

    int stringLen = 0; // (int) (sizeof(char) * strlen(m_inputBuffer));
    int stringPosition = 0;

    // determine the keyCombo for the current Buffer
    UINT8 KeyCombo = whichCombo(&m_NhDriver.m_inputBuffer[stringPosition], INPUT_BUFFER_SIZE - stringPosition);

    // traverse while there is a string
    while (KeyCombo != NO_COMBO) {

        // get the length of the string at the current position
        stringLen = (int) (sizeof(char) * strlen(&m_NhDriver.m_inputBuffer[stringPosition]));

        // run the associated combo function
        switch (KeyCombo) {
            case ROUTER_COMBO:
                routerExecuter(&m_NhDriver.m_inputBuffer[stringPosition + 1]);
                Update();
                break;
            case CHARACTER_COMBO:
                characterExecuter(&m_NhDriver.m_inputBuffer[stringPosition + 1]);
                Update();
                break;
            case JKM_COMBO:
                jkmExecuter(&m_NhDriver.m_inputBuffer[stringPosition + 1]);
                Update();
                break;
            default:
                break;

        }

        // move the position
        stringPosition = stringPosition + stringLen + 1;

        // record the next keyCombo
        KeyCombo = whichCombo(&m_NhDriver.m_inputBuffer[stringPosition], INPUT_BUFFER_SIZE - stringPosition);

    }

}

UINT8 SampleBrailleDriver::whichCombo(const char* arr, int size) {

    // start with no combo
    UINT8 comboType = NO_COMBO;

    for (int i = 0; i < size && arr[i] != '\0'; ++i) {
        switch (arr[i]) {
            case 'r':
                comboType = ROUTER_COMBO;
                break;
            case 'c':
                comboType = CHARACTER_COMBO;
                break;
            case 'j':
                comboType = JKM_COMBO;
                break;
            default:
                break;
        }
    }

    return comboType;
}

DWORD SampleBrailleDriver::routerExecuter(const char* routerString) {
    LPCSTR moveRouterCommand = "router";
    DWORD routerLocation = static_cast<DWORD>(routerString[0]);

    m_Callbacks->ProcessBrailleCommandKeys(this, moveRouterCommand, routerLocation);

    return routerLocation;
}

BYTE SampleBrailleDriver::characterExecuter(const char* characterString) {
    BYTE brailleBinary = static_cast<BYTE>(characterString[0]);


    char charToPrint[] = {(char) brailleBinary, '\0'};
    m_NhDriver.write(charToPrint);

    WCHAR BrailleChar;
    BrailleChar = m_Callbacks->DotsToChar(this, brailleBinary);

    if (brailleBinary == 0) {
        BrailleChar = L' ';
    }

    char UTF8String[10] = {0};
    char outString[15] = "\xEF\xBB\xBF";
    WCHAR inString[2] = {BrailleChar, L'\0'};

    int charsWritten = WideCharToMultiByte(CP_UTF8, 0, inString, -1, UTF8String, sizeof(UTF8String), NULL,
                                           NULL);

    strcat_s(outString, UTF8String);
    m_Callbacks->TypeKeys(this, outString, (int) strlen(outString));

    return brailleBinary;
}

LPCSTR SampleBrailleDriver::jkmExecuter(const char* jkmString) {
    LPCSTR keyCombo = jkmString;

    BOOL succesfulKeyPress;

    succesfulKeyPress = m_Callbacks->ProcessBrailleCommandKeys(this, keyCombo, 0);

    // print if this key press is successfully recognized
    if (succesfulKeyPress) {
        char pressSuccess[] = "\nSuccessful Keypress\n";
        m_NhDriver.write(pressSuccess);
    } else {
        char pressNotSuccess[] = "\nUnsuccessful Keypress\n";
        m_NhDriver.write(pressNotSuccess);
    }

    return keyCombo;
}
