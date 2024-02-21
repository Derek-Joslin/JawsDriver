//
// Created by derek on 11/15/2023.
//

#ifndef C__BRAILLEDRIVER_PROTEUSDRIVER_H
#define C__BRAILLEDRIVER_PROTEUSDRIVER_H

#include <windows.h>
//#include <tchar.h>
#include <stdio.h>
#include <queue>

class ProteusDriver {
public:

    // constructor
    ProteusDriver();

    // destructor
    ~ProteusDriver();

    // connects to a comport and adds it as a member
    BOOL Open(LPCSTR comPort);

    // closes the comport and deallocates all resources
    void Close();

    // send bytes of data over the comport
    // printing strings
    void write(char outputBuffer[]);
    // sending windows BYTE type
    void write(BYTE* outputBuffer, int nLength);


    // prints the comport buffer
    BYTE* read(UINT64 nBytes);

    // function to check bytes available
    UINT64 bytesAvailable();



    // Composite Driver functions
    // TODO: Output processor (this should be super fast)
//    BOOL updateOutputBuffer(void);

    // Input processor (this should be super fast)
    BOOL updateInputQueue();


    // async functions (at windows leisure)

    // helpers (slow)
    void loadInputBuffer(void);

//    void printKeyCombos();
//    UINT8 whichCombo(const char*, int);
//    void runInputCombos();
//
//    // router executer (slow)
//    DWORD routerExecuter(const char*);
//
//    // character executer (slow)
//    BYTE characterExecuter(const char*);
//
//    // jkm executer (slow)
//    LPCSTR jkmExecuter(const char*);
//


//    BOOL NHPressKey(LPCSTR keyCombo);


    // input and output queues (long term storage)
    std::queue<char*> m_inputQueue;
    std::queue<char*> m_outputQueue;

    // short term storage (static)
    char m_inputBuffer[100];

    char m_outputBuffer[100];

private:
    // comport connection
    HANDLE m_comPort;
    DCB m_comSettings;


    char m_PortInfo[80];
    char m_nRows;

    char m_nColumns;

    // JAWS callbacks
    void* m_Callbacks = nullptr;

};


#endif //C__BRAILLEDRIVER_PROTEUSDRIVER_H
