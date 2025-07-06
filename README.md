# Remote Administration System

A comprehensive client-server system for remote administration and monitoring. This system provides secure remote access, file management, and command execution capabilities through a web-based interface.

## Features

### C++ Client
- **Reverse Shell Connection**: Establishes persistent connection to the server
- **Cross-Platform Support**: Compatible with Windows and Linux
- **File Transfer**: Bidirectional file upload/download capabilities
- **Command Execution**: Execute system commands and stream output
- **Auto-Reconnection**: Automatic reconnection logic for persistent connectivity
- **Secure Communication**: Socket-based encrypted communication

### Python Flask Server
- **Web Management Interface**: Modern, responsive web interface
- **Multi-Client Support**: Manage multiple connected clients simultaneously
- **Real-time Monitoring**: Live client status and activity logs
- **File Management**: Upload files to clients and download received files
- **Command Execution**: Execute commands on remote clients via web interface
- **Session Management**: Secure authentication and session handling
- **RESTful API**: JSON-based API for all operations

### Security Features
- **Basic Authentication**: Login system with secure password hashing
- **Input Validation**: Comprehensive input sanitization and validation
- **Command Injection Prevention**: Protection against dangerous commands
- **Secure File Handling**: Safe file upload/download with size limits
- **Session Security**: Secure session management with timeouts

## File Structure

```
├── client/                 # C++ Client Code
│   ├── client.cpp         # Main client implementation
│   └── Makefile          # Build configuration
├── server/                # Python Flask Server
│   ├── app.py            # Main Flask application
│   ├── requirements.txt  # Python dependencies
│   ├── templates/        # HTML templates
│   │   ├── base.html
│   │   ├── login.html
│   │   └── dashboard.html
│   ├── static/          # CSS/JS files
│   │   ├── css/
│   │   │   └── style.css
│   │   └── js/
│   │       └── app.js
│   └── uploads/         # File storage directory
└── README.md           # This documentation
```

## Quick Start

### Prerequisites

**For the Server:**
- Python 3.7 or higher
- pip (Python package installer)

**For the Client:**
- C++ compiler (g++, Visual Studio, etc.)
- Make build tool (for Unix-like systems)

### Server Setup

1. **Navigate to the server directory:**
   ```bash
   cd server/
   ```

2. **Install Python dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

3. **Start the server:**
   ```bash
   python app.py
   ```

   The server will start on:
   - Web Interface: http://localhost:5000
   - Client Connections: Port 9999

4. **Access the web interface:**
   - Open http://localhost:5000 in your browser
   - Login with default credentials: `admin` / `admin123`

### Client Setup

**For Linux/Unix:**

1. **Navigate to the client directory:**
   ```bash
   cd client/
   ```

2. **Build the client:**
   ```bash
   make
   ```

3. **Run the client:**
   ```bash
   ./client [server_ip] [server_port]
   # Example: ./client 192.168.1.100 9999
   # Default: ./client 127.0.0.1 9999
   ```

**For Windows:**

1. **Compile using Visual Studio or MinGW:**
   ```cmd
   g++ -std=c++17 -o client.exe client.cpp -lws2_32
   ```

2. **Run the client:**
   ```cmd
   client.exe [server_ip] [server_port]
   ```

## Usage Guide

### Web Interface

1. **Dashboard**: View all connected clients and server statistics
2. **Client Management**: Click "Manage" on any client to:
   - Execute commands via terminal interface
   - Upload files to the client
   - Request files from the client
   - View real-time activity logs
   - Disconnect the client

### Terminal Commands

The terminal interface supports any system command available on the target system:

**Windows Examples:**
```cmd
dir C:\
systeminfo
tasklist
ipconfig
```

**Linux Examples:**
```bash
ls -la /
ps aux
ifconfig
uname -a
```

### File Operations

**Upload to Client:**
1. Go to the Files tab in client management
2. Select a file to upload
3. Click "Upload" - the file will be sent to the client

**Download from Client:**
1. Enter the full path of the file you want to retrieve
2. Click "Request" - the client will send the file to the server
3. Downloaded files appear in the "Received Files" list

## Configuration

### Server Configuration

Edit the following variables in `server/app.py`:

```python
# Server ports
SERVER_PORT = 9999      # Port for client connections
WEB_PORT = 5000        # Port for web interface

# File upload settings
MAX_FILE_SIZE = 100 * 1024 * 1024  # 100MB max file size
ALLOWED_EXTENSIONS = {'txt', 'pdf', 'png', 'jpg', 'jpeg', 'gif', 'doc', 'docx', 'zip', 'rar'}

# Authentication (CHANGE THESE!)
ADMIN_USERNAME = 'admin'
ADMIN_PASSWORD_HASH = generate_password_hash('your_secure_password')
app.secret_key = 'your-secret-key-here'
```

### Client Configuration

The client accepts command-line arguments:

```bash
./client <server_ip> <server_port>
```

Default values:
- Server IP: 127.0.0.1
- Server Port: 9999

## Security Considerations

### Important Security Notes

1. **Change Default Credentials**: Update the admin username and password before deployment
2. **Use HTTPS**: Configure HTTPS for the web interface in production
3. **Firewall Configuration**: Ensure proper firewall rules are in place
4. **Network Security**: Use VPN or secure networks for client-server communication
5. **Regular Updates**: Keep all components updated with security patches

### Security Features

- **Input Sanitization**: All user inputs are sanitized to prevent injection attacks
- **Command Filtering**: Dangerous commands are blocked automatically
- **File Type Validation**: Only allowed file types can be uploaded
- **Session Management**: Secure session handling with timeouts
- **Error Handling**: Comprehensive error handling to prevent information disclosure

## API Reference

### Authentication Endpoints

- `POST /login` - User authentication
- `GET /logout` - End user session

### Client Management Endpoints

- `GET /api/clients` - List all connected clients
- `POST /api/client/{id}/command` - Execute command on client
- `POST /api/client/{id}/upload` - Upload file to client
- `POST /api/client/{id}/request_file` - Request file from client
- `GET /api/client/{id}/logs` - Get client activity logs
- `GET /api/client/{id}/files` - List files received from client
- `GET /api/client/{id}/download/{filename}` - Download received file
- `POST /api/client/{id}/disconnect` - Disconnect client

## Troubleshooting

### Common Issues

**Client Cannot Connect:**
- Verify server is running and listening on the correct port
- Check firewall settings on both client and server
- Ensure network connectivity between client and server

**Web Interface Not Loading:**
- Check if Flask server is running on port 5000
- Verify no other application is using port 5000
- Check browser console for JavaScript errors

**File Upload/Download Issues:**
- Verify file permissions on server
- Check available disk space
- Ensure file size is within limits

**Permission Denied Errors:**
- Run client with appropriate privileges
- Check file/directory permissions
- Verify user has necessary access rights

### Logging

Server logs are displayed in the console and include:
- Client connection/disconnection events
- Command execution results
- File transfer status
- Error messages and debugging information

Client activity is logged per-client and accessible via the web interface.

## Development

### Building from Source

**Client Dependencies:**
- C++17 compatible compiler
- Standard C++ libraries
- Platform-specific networking libraries (Winsock2 on Windows)

**Server Dependencies:**
- Flask web framework
- Werkzeug for utilities
- Python standard libraries

### Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is licensed under the MIT License. See the LICENSE file for details.

## Disclaimer

This tool is intended for legitimate system administration and authorized security testing only. Users are responsible for ensuring compliance with applicable laws and regulations. The authors are not responsible for any misuse of this software.

## Support

For issues, questions, or contributions, please:
1. Check the troubleshooting section
2. Review existing issues on GitHub
3. Create a new issue with detailed information
4. Include system information and error messages

---

**Warning**: This system provides powerful remote access capabilities. Ensure proper security measures are in place before deployment in production environments.
