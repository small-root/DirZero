import paramiko
import os

class SFTPManager:
    def __init__(self, host, port=22, username=None, password=None, key_filepath=None):
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.key_filepath = key_filepath
        self.transport = None
        self.sftp = None

    def connect(self):
        """SFTP server se connection establish karta hai."""
        try:
            self.transport = paramiko.Transport((self.host, self.port))
            if self.key_filepath:
                key = paramiko.RSAKey.from_private_key_file(self.key_filepath)
                self.transport.connect(username=self.username, pkey=key)
            else:
                self.transport.connect(username=self.username, password=self.password)
            
            self.sftp = paramiko.SFTPClient.from_transport(self.transport)
            print(f"Successfully connected to SFTP server at {self.host}")
        except Exception as e:
            print(f"Failed to connect to {self.host}: {e}")
            raise

    def upload_file(self, local_path, remote_path):
        """Local file ko remote SFTP server par upload karta hai."""
        if not self.sftp:
            raise Exception("Connection is not active. Call connect() first.")
        try:
            self.sftp.put(local_path, remote_path)
            print(f"Uploaded: {local_path} -> {remote_path}")
        except Exception as e:
            print(f"Failed to upload {local_path}: {e}")

    def download_file(self, remote_path, local_path):
        """Remote SFTP server se file local machine par download karta hai."""
        if not self.sftp:
            raise Exception("Connection is not active. Call connect() first.")
        try:
            self.sftp.get(remote_path, local_path)
            print(f"Downloaded: {remote_path} -> {local_path}")
        except Exception as e:
            print(f"Failed to download {remote_path}: {e}")

    def disconnect(self):
        """SFTP session aur transport connection close karta hai."""
        if self.sftp:
            self.sftp.close()
        if self.transport:
            self.transport.close()
        print("SFTP Connection closed.")
