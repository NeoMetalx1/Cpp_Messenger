import socket
import json
import threading

class MessengerClient:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False
        
    def connect(self):
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            self.connected = True
            
            # Запуск потока для приема сообщений
            receive_thread = threading.Thread(target=self.receive_messages)
            receive_thread.daemon = True
            receive_thread.start()
            
            return True
        except Exception as e:
            print(f"Ошибка подключения: {e}")
            return False
    
    def send_message(self, message_data):
        if not self.connected:
            return False
            
        try:
            # Сериализация сообщения в JSON
            message_json = json.dumps(message_data)
            # Добавление заголовка с длиной сообщения
            message_bytes = message_json.encode('utf-8')
            header = len(message_bytes).to_bytes(4, byteorder='big')
            
            self.socket.sendall(header + message_bytes)
            return True
        except Exception as e:
            print(f"Ошибка отправки: {e}")
            self.connected = False
            return False
    
    def receive_messages(self):
        while self.connected:
            try:
                # Чтение заголовка (4 байта - длина сообщения)
                header = self.socket.recv(4)
                if not header:
                    break
                    
                message_length = int.from_bytes(header, byteorder='big')
                
                # Чтение сообщения указанной длины
                message_bytes = b''
                while len(message_bytes) < message_length:
                    chunk = self.socket.recv(message_length - len(message_bytes))
                    if not chunk:
                        break
                    message_bytes += chunk
                
                if message_bytes:
                    message_json = message_bytes.decode('utf-8')
                    message_data = json.loads(message_json)
                    # Обработка полученного сообщения
                    print(f"Получено: {message_data}")
                    # Здесь вы бы вызвали функцию обратного вызова или сигнал для UI
            except Exception as e:
                print(f"Ошибка приема: {e}")
                self.connected = False
                break
    
    def disconnect(self):
        if self.socket:
            self.socket.close()
        self.connected = False
