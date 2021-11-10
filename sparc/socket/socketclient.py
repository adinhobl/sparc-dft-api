import socket

eor_sig = "\r\n\r\n"

class SocketClient:
    
    def __init__(self, sock_num:int=20801):
        self.sock_num = sock_num
        self.sock = None

    def connect(self):
        print('Starting Client')
        print('Create a TCP/IP socket')
        self.sock = socket.socket(socket.AF_INET,
                            socket.SOCK_STREAM)
        
        print('Connect the socket to the server port')
        server_address = (socket.gethostname(), self.sock_num)

        print('Connecting to:', server_address)
        self.sock.connect(server_address)
        print('Connected to server')

    def _send_message(self, message:str="test"):
        if self.sock is None:
            raise TypeError("Must call `connect` before `send_message`.")
        try:
            print('Sending:', message)
            self.sock.send(message.encode())
        except:
            print("There was an issue sending")

    def _recv_message(self):
        try:
            data = self.sock.recv(1024).decode()
            print('Received from server:', data)
        except:
            print("There was an issue recving")

    def echo(self, message:str="test"):
        self._send_message("ECHO" + eor_sig)
        self._send_message(message + eor_sig)
        self._recv_message()

    def abort(self):
        print('Instructing server to shut down.')
        self._send_message("ABORT" + eor_sig)
        print('Closing socket.')
        self.sock.close()

# status_sig = "STATUS"
# init_sig = "INIT"
# posdata_sig = "POSDATA"
# getforce_sig = "GETFORCE"
# getstress_sig = "GETSTRESS"