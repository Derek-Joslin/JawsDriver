#pragma once
	/*!\mainpage JAWS Braille Driver Interface Specification

\section intro Introduction

	 The IBrailleDriver interface  is the means by which JAWS communicates with Braille Displays.
	 Each type of Braille display requires that a special driver  be
	 written to expose the features of that display in a standard
	 manner.
	 The Driver must implement all of the functions of the
	 IBrailleDriver interface.
	 Compiled driver files  must be given a .JLB extension.

\section loading How a Driver Is Loaded

JAWS goes through the following steps in order to load a driver:
	-	Calls LoadLibrary on the .JLB file.
		Any helper DLLs used by the .jlb file will be found if they reside in the same directory.
	-	Calls GetProcAddress for GetBrailleInterface.
	-	Calls GetBrailleInterface to obtain an IBrailleDriver interface pointer
	-	Calls GetManufacturerInfo (Note that this step doesn't happen if
		you have the authorization flag set that allows you to test with
		unsigned drivers)
	-	Calls  OpenDriver to initialize the display

\section threading Threading Considerations
This is copied verbatim from the OpenDriver documentation but is here
as well to make sure that you follow this guidance.
The thread on which a driver receives the OpenDriver call is the only
thread on which the driver is allowed to make calls to functions of the
IBrailleDriverCallbacks interface.  The easiest way to guarantee this is to
have the OpenDriver function create a hidden window, have other threads that wish to communicate
information back to JAWS post messages to that window, and have
that window's message handlers make the actual calls to functions
of the IBrailleDriverCallbacks interface.
(This works because all messages dispatched to a window are guaranteed
to be dispatched on the same thread on which that window was created.)

Failure to obey the above rule will at best cause JAWS to crash
because you'll know you have a problem. More likely you'll get hard to
diagnose issues related to typing or pressing command key
combinations.

\section ARM Building for ARM64

The sample Braille driver  includes an ARM64 configuration. In order to build for ARM64, the ARM64 compiler must be installed as part of
Visual Studio 2022. There are two things that must be done:
<UL>
<LI> Under Workloads/Windows Desktop, in the optional components
section, make sure that
MSVC ARM64 build tools is checked.

<LI> Under Individual Compponents, make sure that ATL for ARM64 is
checked. Not doing this will result in the ARM64 compiler failing to
find atls.lib.
</UL>

\section typekeysmode What's This Thing Called TypeKeysMode

Spoiler alert: It was a failed experiment. If your display has an
integrated Braille or QWERTY Keyboard, we recommend that you return
FALSE from the SupportsTypeKeysMode function and act as if this
option is unconditionally on.

The intent was to allow Braille keys to either perform special
commands or self-insert when TypeKeys mode was off and on
respectively. The advantage of using non chorded Braille keys to issue
commands wasn't worth the added confusion to users. The functionality
is still there but we recommend against using it.  

The following
documentation is here for completeness.

On Braille displays with an
integrated Braille keyboard, individually typed Braille characters can
either be treated as commands that are attached to scripts (e.g. J
means turn on the JAWS cursor) or can be treated as if they were
characters typed from the QWERTY keyboard.  This decision is made
internally within the Braille Display driver and for the examples
above would result in either the dot string being passed to
ProcessBrailleCommandKeys or converted to a character and passed to
TypeKeys.  Since JAWS supports typing via the Braille keyboard, in
most cases a driver will choose to unconditionally treat non-chorded
dot combinations as characters to be typed.  In this case,
SupportsTypeKeysMode should return FALSE, GetTypeKeysMode should
return 0, and SetTypeKeysMode should do nothing.

In the case of a driver that wishes to allow  keys on the Braille
keypad to either be used for issuing commands or for typing, a mode of
0 means that keys on the Braille keypad should issue commands,
and a mode of 1 means that they should be used to type characters.
SupportsTypeKeysMode should return TRUE, GetTypeKeysMode
should return the current mode, and SetTypeKeysMode should make note
of the current mode and cause the driver to process keys from the
Braille keypad accordingly.

If SupportsTypeKeysMode returns TRUE, then JAWS will call
SetTypeKeysMode at startup and whenever the user changes it to inform
the driver of the current mode.
Additionally a key combination on the Braille display must be assigned
to the JAWS script ToggleTypeKeysMode to allow the user to turn the
mode on and off.
Note that when JAWS starts, TypeKeysMode will always be off.
This means that the driver's SetTypeKeysMode will be initially called
with a value of 0.

\section textinput  Processing Input

When one or more keys is pressed on the Braille display, a driver
should first call IBrailleDriverCallbacks::ProcessBrailleCommandKeys with a text string
representing that key or combination of keys. If
IBrailleDriverCallbacks::ProcessBrailleCommandKeys returns true, this means that an entry in
the display's .jkm file matched this key combination and the
associated JAWS script will be called.

If IBrailleDriverCallbacks::ProcessBrailleCommandKeys returns false, and the key sequence 
contains Braille dots only, a driver should call IBrailleDriverCallbacks::DotsToChar with that dot
pattern and then call IBrailleDriverCallbacks::TypeKeys with the result of IBrailleDriverCallbacks::DotsToChar encoded
as UTF-8.

If The initial call to IBrailleDriverCallbacks::ProcessBrailleCommandKeys returns false and the key
combination should be mapped to a qwerty key combination, then a
driver should format a buffer containing "qwerty:" followed by the key
combination that should be emulated and call IBrailleDriverCallbacks::ProcessBrailleCommandKeys
with that buffer.

The names of keys that can be
included in a qwerty: string are listed in the keycodes.ini file in the
JAWS settings directory.

When a user starts doing contracted Braille input, JAWS forward translates any
characters immediately adjacent to the caret location and adds those to
a contracted input buffer. When a driver calls DotsToChar in this
mode, JAWS actually returns the Unicode Braille dot pattern for the dots
entered, and then when the driver calls TypeKeys, that dot pattern is
inserted into the
contracted Braille input buffer. When a user presses space, backspace,
or another navigation key, JAWS back translates the entered dot
patterns and either replaces or inserts a new word. It's therefore
essential that a driver uses DotsToChar to convert dot patterns to
Unicode characters for insertion. Failure to do this will prevent JAWS
from  allowing contracted input.

\section  copyright Copyright
	 Copyright (&copy;) 2000-2022, Freedom Scientific Inc.
*/
/**
	 \brief Declaration of  IBrailleDriver & IBrailleDriverCallbacks interfaces.
*/

class ITTSDriver;

class IBrailleDriverCallbacks;

/// Abstract representation of a Braille display
/**
	 \sa IBrailleDriverCallbacks
*/
class IBrailleDriver
{
  public:
  /// The place  on the display where cells should be shown
  enum StatusCellPositions {
	scpNone,											///< Don't show status cells
    scpLeft,										///< Show status cells at left
    scpRight										///< Show status cells at right
  };
/// Open the driver
/**
Called before any other communication with the display
\param[in] lpszPort A String specifying a port descriptor.
\param[in] pCallBacks a pointer to an implementation of the
IBrailleDriverCallbacks interface which the driver must use to let
JAWS know that keys have been pressed.
\param[in] lpszDisplayName A human readable description of the Braille
Display, for example, Focus 44,70 or 84. Note this parameter maybe
left NULL.
\param[in] lpszDriverName The Displays real dll name if
appropriate. This parameter maybe left NULL.
\returns This function must return TRUE if the driver
is successfully loaded and communication with the Braille Display
may proceed. It must return FALSE if the driver did not
successfully load and thus JAWS should not attempt to communicate
with the display. If an Open is unsuccessful JAWS assumes that the
driver does not need to be closed.
\remarks
The thread on which a driver receives the OpenDriver call is the only
thread on which the driver is allowed to make calls to functions of the
IBrailleDriverCallbacks interface.  The easiest way to guarantee this is to
have the OpenDriver function create a hidden window, have other threads that wish to communicate
information back to JAWS post messages to that window, and have
that window's message handlers make the actual calls to functions
of the IBrailleDriverCallbacks interface.
(This works because all messages dispatched to a window are guaranteed
to be dispatched on the same thread on which that window was created.)
Failure to obey the above rule will at best cause JAWS to hang and at worst cause JAWS to crash.
*/
  virtual BOOL Open(LPCSTR lpszPort, IBrailleDriverCallbacks *pCallBacks, LPCSTR lpszDisplayName=0,LPCSTR lpszDriverName=0) = 0;
/// Terminates communication with the display driver
/**
After this point all interface
pointers are assumed invalid.
*/

  virtual void Close ()= 0;
  virtual IBrailleDriverCallbacks * SetCallbacks(IBrailleDriverCallbacks * pCallbacks) = 0;
/// Retrieves the name  of the Braille display
/**
Copies up to \a nMaxChars of the name
of the display to \a lpszName 
\param[out] lpszName A preallocated buffer to hold the name of the
display.
\param[in] nMaxChars The maximum length of the output buffer.
\returns The number of characters
copied to the output buffer not counting the terminating NULL.

*/
  virtual int GetDisplayName(LPSTR lpszName, int nMaxChars) = 0;
  virtual int GetDriverName(LPSTR lpszName, int cbMaxLength) = 0;
/// Retrieves the name of the keymap to be used with this display
/**
\param[out] lpszName The name of the keymap, 
\param[in] nMaxChars Length of output buffer.
\returns This function returns the number of characters
copied to the output buffer not including the terminating NULL.
\remarks
The keymap name identifies the name of the section in default.jkm
where the display's key assignments are stored.
For example, if this function returns the keymap name as "Spiffy40",
then the section in default.jkm should be [Spiffy40 keys].
This function should not include the word keys in the keymap name,
but the section in default.jkm must be suffixed with the word keys.
If a driver supports multiple different displays with widely
different key layouts, it may be helpful to return a different keymap
name for each display.
*/
  virtual int GetDisplayKeyMapName(LPSTR lpszName, int nMaxChars) = 0;
/// Retrieves the version of the interface with which this driver complies.
/**
\returns The interface version, this version should be 0x300
*/
  virtual DWORD GetInterfaceVersion() = 0;
/// Retrieves this driver's version
/**
\returns The driver version.
*/
virtual DWORD GetDriverVersion()= 0;
///GetDisplayVersion
/**retrieves the Braille Display firmware version.
\returns The version number
*/
virtual DWORD GetDisplayVersion() = 0;
virtual BOOL HasIntegratedSpeech() = 0;
  virtual BOOL GetSpeechInterface(ITTSDriver **ptts) = 0;
/// Displays dot patterns in the main display area
/**
\param[in] lpDots The dot patterns to display.
Each dot pattern is in a single byte with BIT0 representing dot1,
BIT1 representing dot2, etc.
\param[in] nLength The maximum number of cells to use.
\returns TRUE if the dot patterns were displayed, FALSE otherwise.
*/
  virtual BOOL Display (const LPBYTE lpDots, int nLength) = 0;
/// Displays dot patterns in the status area
/**
\param[in] lpDots The dot patterns to display.
\param[in] nLength The maximum number of cells to use.
\returns TRUE if the dot patterns were displayed, FALSE otherwise.
*/
  virtual BOOL DisplayStatus (const LPBYTE lpDots, int nLength) = 0;
///Clears all cells of the display
/**
\returns TRUE on success FALSE otherwise.
*/
  virtual BOOL Clear ()= 0;
  virtual BOOL DisplayVertical (const LPBYTE lpDots, int nLength)= 0;
  virtual BOOL ClearVertical()= 0;
///Puts the driver into sleep mode 
/**
\param[in] fGotoSleep TRUE to enter sleep mode, FALSE to leave it.
\returns TRUE if the call succeeded, FALSE otherwise.
\remarks
This is a legacy function that in theory no longer needs to do
anything. To be safe save  off the new state and if the current sleep
mode is enabled, do nothing  in Display and DisplayStatus.

*/
  virtual BOOL Sleep (BOOL fGotoSleep)= 0;
///Check for pending input
/**
This function does not have to be supported if the
driver relies on events for handling keyboard processing as the
events may use the IBrailleDriverCallbacks pointer to inform JAWS
about keys waiting to be processed. If however the display driver
does not use events for handling keyboard activity this function
must check for unhandled Display keystrokes and  call the
IBrailleDriverCallbacks functions when there is input available.
*/
  virtual void Timer () = 0;
///Retrieves the number of cells in the main display area
/**
\returns The number of cells.
*/
  virtual int Cells () = 0;
  virtual int VerticalCells() = 0;
///Retrieves the number of cells in the status area
/**
\returns The number of status cells.
*/
  virtual int StatusCells () = 0;
///TotalCells
/**
\returns The total number of cells.
*/
  virtual int TotalCells() = 0;

///Determines if the user can specify the location of status cells
/**
\returns TRUE if positioning is allowed, FALSE otherwise.
*/
  virtual BOOL SupportsCustomLayout ()= 0;
/// Sets positions of main and status cells
/**
\param[in] nTextDisplayStart The cell in which text should begin
\param[in] nTextCells The number of cells to use for text.
\param[in] nStatusCellStart The cell in which status cells should
begin. 
\param[in] nStatusCells The number of cells to use for Status
cells.
\returns This function returns TRUE if the layout was
successfully set or FALSE otherwise.
*/
  virtual BOOL SetCustomLayout (int nTextDisplayStart, int nTextCells, int nStatusCellStart, int nStatusCells)= 0;
  virtual BOOL SupportsCursorBlinkRate() = 0;
  virtual BOOL SetCursorBlinkRate(DWORD dwMiliseconds, BYTE byDotPattern) = 0;
  virtual BOOL DisplayWithCursor (const LPBYTE lpDots, int nLength, int nCursorColumn) = 0;
///Determines if dot firmness is adjustable
/**
\param[out] nSettings the number of different firmness settings supported
\returns TRUE if the driver and
Display supports dot firmness adjustability or FALSE otherwise. If
it returns TRUE then \a nSettings is assumed to have
been properly set to the number of adjustable dot firmnesss
settings supported by the Display. If the function returns FALSE
then the value in \a nSettings is ignored.
*/
virtual BOOL SupportsFirmnessSetting (int& nSettings)= 0;
/// Sets the display's dot firmness
/**
\a nSetting should be between 0 and the value returned by SupportsFirmnessSetting.
\param[in] nSetting the firmness setting to be used
\returns TRUE if the firmness has been set, FALSE otherwise.
*/
  	virtual BOOL SetFirmness (int nSetting) = 0;
	/// Determines if there is pending input from the Braille display
	/**
		 \returns TRUE if keys are waiting, FALSE otherwise
	*/
	virtual BOOL IsKeyWaiting() = 0;
	virtual void SetOneHandedMode(BOOL state) = 0;

	// Functions added in Interface 0x200;
	/// Determines if TypeKeys mode is enabled
	/**
		 \returns TRUE if TypeKeys is enabled, FALSE otherwise
	*/
	virtual BOOL GetTypeKeysMode() = 0;
	/// Enables or disables TypeKeys mode
	/**
		 If SupportsTypeKeysMode is TRUE, then JAWS will call this
		 function when the user presses a key attached to the "braille
		 ToggleTypeKeysMode" script.  From within this script, JAWS will first call GetTypeKeysMode
		 and then call SetTypeKeysMode to invert the value.
		 \param[in] bEnable TRUE to enable the mode, FALSE to disable it.
	*/
	virtual void SetTypeKeysMode(BOOL bEnable = TRUE) = 0;
	/// Determines if the display supports TypeKeys Mode
	/**
		 TypeKeys mode allows the driver to process the keys on the
	Braille display differently than when this mode is off.
	e.g. When the mode is on a particular key combination could be used
	to insert an alphanumeric character, and when it's off to issue a
	navigation command.
\returns TRUE if TypeKeys is supported, FALSE otherwise.
	*/
	virtual BOOL SupportsTypeKeysMode() = 0;

	// Functions added in Interface 0x300;
	
	/// Retrieves information about the manufacturer of the Braille display
	/**
		
		\param[out] lpszName Contains the name of the display's
		manufacturer
		\param[in] cbMaxNameLen  maximum size of \a lpszName
		\param[out] lpguidUnique Contains the Freedom-Scientific generated
		GUID assigned to the display manufacturer
\returns TRUE if \a lpszName and \a lpguidUnique have been filled with
		information, false otherwise.
*/
	virtual BOOL GetManufacturerInfo(LPSTR lpszName,int cbMaxNameLen,LPGUID lpguidUnique) = 0;

};

/// How a Braille driver invokes callback functions
/**
	 This interface provides a way for Braille Drivers to notify JAWS of events such as key presses as well as query
	 JAWS for information.
	 \sa IBrailleDriver
*/
class IBrailleDriverCallbacks
{
public:

/// Processes keys from the Braille display
	/**
		 Should be called to process keys attached to scripts, and keys
		 simulating QWERTY key combinations that don't result in
		 characters being inserted.
		 To insert characters, call TypeKeys instead.
\param[in] pDriver A pointer to the IBrailleDriver interface of the driver making the call.

\param[in] lpszKeyCombo The key combination
requiring processing. Note this string must match a mapping in the
Displays JKM (keymap) file.  It should describe a combination of one or
more keys pressed simultaneously.
\param[in] dwRouting A number specifying the Routing key just
pressed. Should be 0 if no routing key is involved in this combination.
\returns TRUE if the call was successful, FALSE otherwise.
\remarks
Any string passed to this function must have a corresponding entry in
the display's keymap section that maps the key combination to the
JAWS script to be executed when the combination is pressed.
Each entry in the keymap section should be preceded by the word
Braille.
So if the driver passes "UpArrow" to
ProcessBrailleCommandKeys, then the entry in the keymap section should
be "Braille UpArrow"
Note that the quotation marks should not be included.
For key combinations, we recommend that the key names be separated
with a + symbol.
For example "LeftRocker+UpARrow"
If the keys being pressed correspond to Braille dots, then something
like "Dots 1 2 3" or "Dots 1 2 3 chord"would be appropriate.
If a routing button is pressed either alone or in combination with
other keys, then the last word of lpszKeyCombo should be "routing"
and dwRouting should contain the 1-based offset of the routing button
that has been pressed.
For example "Routing" or "LeftArrow+Routing"
There is one exception to the rule that all strings sent to
ProcessBrailleCommandKeys must have an associated entry in
default.jkm and this relates to simulating QWERTY keys.
If a string beginning with "qwerty:" is sent to
ProcessBrailleCommandKeys, then everything following the  "qwerty:"
is processed as if the keys were pressed on the QWERTY keyboard.
If a JAWS script is associated with those QWERTY keys, then the script
will be run.  Otherwise the keys will be simulated.  
For example "qwerty:Alt+f." This approach should not be used to
simulate characters to be inserted into a document.
*/
	virtual   BOOL ProcessBrailleCommandKeys(IBrailleDriver *pDriver,LPCSTR lpszKeyCombo,DWORD dwRouting) = 0;
/// Simulates typing printable characters on a QWERTY keyboard
	/**
		 Although it's possible to simulate this by directly calling
		 Windows API functions, doing so will bypass JAWS features such as
		 the ability to support JAWS Quick keys and Grade II Braille input.
\param[in] pDriver A pointer to the IBrailleDriver interface of the driver
making the call.
\param[in]  lpszKeys The ANSI string of one or more characters to type.
\param[in] nCount The number of characters to type.
\returns TRUE if the call was successful, FALSE otherwise.

\remarks

The lpszKeys parameter must consist of the UTF8 BOM followed by the UTF8 encoding of a single
	character. 

The string must be:
> 	UTF8 BOM: 0xEF 0XBB 0XBF followed by the UTF8 encoding of the character.
	
\code
	void CYourDriver::UTF8TypeKey(WCHAR wKey)
{
	char UTF8String[10] = { 0 };
	char outString[15] = "\xEF\xBB\xBF";
	WCHAR inString[2] = { wKey, L'\0' };

	int charsWritten = WideCharToMultiByte(CP_UTF8, 0, inString, -1, UTF8String, sizeof(UTF8String), NULL, NULL);
	strcat_s(outString, UTF8String);
	return (pCallbacks->TypeKeys(this, outString, (int)strlen(outString)) == TRUE);
}
\endcode
*/
  virtual BOOL TypeKeys(IBrailleDriver *pDriver,LPCSTR lpszKeys,int nCount) = 0;
///Does a reverse lookup of a character based on dot pattern
/**
Enables the driver to get the
associated character from the Braille table in use for the
specified dot pattern.
\param[in] pDriver A pointer to the IBrailleDriver interface of the driver
making the call.
\param[in] byDotPattern Pattern to be translated.

\returns A Character from the Braille table in USE which maps to the
dot pattern supplied.

\remarks When a user starts doing contracted Braille input, JAWS forward translates any
characters immediately adjacent to the caret location and adds those to
a contracted input buffer. When a driver calls DotsToChar in this
mode, JAWS actually returns the Unicode Braille dot pattern for the dots
entered, and then a call to TypeKeys inserts that dot pattern into the
contracted Braille input buffer. When a user presses space, backspace,
or another navigation key, JAWS back translates the entered dot
patterns and either replaces or inserts a new word. It's therefore
essential that a driver uses DotsToChar to convert dot patterns to
Unicode characters for insertion. Failure to do this will prevent JAWS
from  allowing contracted input.
*/
  virtual WCHAR DotsToChar(IBrailleDriver *pDriver,BYTE byDotPattern)= 0;
	virtual void OnFirstKeyDown(IBrailleDriver *pDriver,DWORD dwKey) = 0;
	virtual void DeviceDisconnected(IBrailleDriver* driver) = 0;
};
