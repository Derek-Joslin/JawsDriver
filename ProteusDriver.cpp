//
// Created by derek on 11/15/2023.
//

#include "ProteusDriver.h"

ProteusDriver::ProteusDriver() {
    // TODO: allocate necessary windows resources
   printf("Windows resources allocated\n");

   m_nColumns = 0;
   m_nRows = 0;

}

ProteusDriver::~ProteusDriver() {
    // TODO: deallocate necessary windows resources
    printf("Windows resources deallocated\n");

}

BOOL ProteusDriver::Open(LPCSTR comString) {
    // TODO: Package nicely

    // boolean value to track successful operations
    BOOL isSuccesful;

    //  Open a handle to the specified com port.
    m_comPort = CreateFileA(comString,
                       GENERIC_READ | GENERIC_WRITE,
                       0,      //  must be opened with exclusive-access
                       NULL,   // a default security attributes
                       OPEN_EXISTING, //  must use OPEN_EXISTING
                       0,      //  not overlapped I/O
                       NULL); //  hTemplate must be NULL for comm devices

    // handle error if fail to connect to the comport
    if (m_comPort == INVALID_HANDLE_VALUE) {
        //  Handle the error.
        printf("Windows to connect to comport with error %d.\n", GetLastError());
        printf("on port %s\n", comString);
        return FALSE;
    } else {
        strcpy(m_PortInfo, comString);
        printf("comport %s opened to braille display\n", m_PortInfo);
    }

    //  DCB is an abstract device control settings object in windows
    //  it will hold the settings of the comport in m_comSettings

    // set its initial value to nothing
    memset(&m_comSettings,0x00, sizeof(DCB));
    m_comSettings.DCBlength = sizeof(DCB);

    //  place the current comport settings into the comsettings object from the comport
    isSuccesful = GetCommState(m_comPort, &m_comSettings);

    if (!isSuccesful) {
        //  Handle the error.
        printf("Windows failed to load the comport settings with error %d.\n", GetLastError());
        return FALSE;
    }

    //  set the comstate according to braille display settings
    //  115,200 bps, 8 data bits, no parity, and 1 stop bit.
    m_comSettings.BaudRate = CBR_115200;     //  baud rate
    m_comSettings.ByteSize = 8;             //  data size, xmit and rcv
    m_comSettings.Parity = NOPARITY;      //  parity bit
    m_comSettings.StopBits = ONESTOPBIT;    //  stop bit

    isSuccesful = SetCommState(m_comPort, &m_comSettings);

    if (!isSuccesful) {
        //  Handle the error.
        printf("SetCommState failed with error %d.\n", GetLastError());
        return (3);
    }

    //  Get the comm config again.
    isSuccesful = GetCommState(m_comPort, &m_comSettings);

    if (!isSuccesful) {
        //  Handle the error.
        printf("GetCommState failed with error %d.\n", GetLastError());
        return (2);
    }

    //  Print some of the DCB structure values
    printf("%s settings:\n", m_PortInfo);
    printf("BaudRate = %d, ByteSize = %d, Parity = %d, StopBits = %d\n",
             m_comSettings.BaudRate,
             m_comSettings.ByteSize,
             m_comSettings.Parity,
             m_comSettings.StopBits);

    return TRUE;
}



void ProteusDriver::Close() {
    // TODO: deallocate windows comport resources
    printf("comport %s closed\n", m_PortInfo);

}

void ProteusDriver::write(char outputBuffer[]) {
    // send the output buffer by looking for the sentinel value
    for (size_t i = 0; outputBuffer[i] != '\0'; i++) {
        TransmitCommChar(m_comPort,outputBuffer[i]);
    }
    printf("write data: %s\n", outputBuffer);

}

void ProteusDriver::write(BYTE* outputBuffer, int nLength) {
    // Send each byte in the lpDots array to the serial port
    for (int i = 0; i < nLength; i++) {
        TransmitCommChar(m_comPort, outputBuffer[i]);
    }

    // Print the data in hexadecimal format for debugging
    printf("write data: ");
    for (int i = 0; i < nLength; i++) {
        printf("%02X ", outputBuffer[i]);
    }
    printf("\n");

}
BYTE* ProteusDriver::read(UINT64 nBytes) {
    // TODO: loop through m_comPort and read byte by byte into buffer and return
    BYTE* rxBuffer = (BYTE*)malloc(nBytes * sizeof(BYTE));

    if (rxBuffer == NULL) {
        // Handle memory allocation error
        printf("Error allocating memory for read buffer.\n");
        return NULL;
    }

    DWORD bytesRead;
    BOOL success = ReadFile(m_comPort, rxBuffer, nBytes, &bytesRead, NULL);

    if (!success) {
        // Handle read error
        printf("Read from comport failed with error %d.\n", GetLastError());
        free(rxBuffer);
        return NULL;
    }

    printf("Read %d bytes from rx pin\n", bytesRead);

    return rxBuffer;

}

UINT64 ProteusDriver::bytesAvailable() {

    DWORD errors;
    COMSTAT status;
    // use the clear comm error to get the status of the port
    ClearCommError(m_comPort, &errors, &status);

    return status.cbInQue;
}


BOOL ProteusDriver::updateInputQueue() {

    // dump all available bytes into a char*
    char* inputBuffer = (char*)read(bytesAvailable());

    // add the input buffer to the input queue
    m_inputQueue.push(inputBuffer);

    if (inputBuffer != NULL) {
        return TRUE;
    } else {
        return FALSE;
    }

}

void ProteusDriver::loadInputBuffer() {

    // get the first buffer at the front of the queue
    char* queueBuffer = m_inputQueue.front();

    // load the queueBuffer into our "safe" static input Buffer
    for (int iByte = 0; iByte < 100; iByte++) {
        // static so no funny business
        // load 100 elements into our input buffer
        m_inputBuffer[iByte] = queueBuffer[iByte];
    }

    // pop the element off the queue
    m_inputQueue.pop();
    free(queueBuffer);

}

