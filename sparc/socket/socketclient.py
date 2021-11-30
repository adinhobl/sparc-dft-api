import socket
import sys
import numpy as np

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
            m = message + eor_sig
            print('Sending:', m)
            self.sock.send(m.encode('utf8'))
        except:
            print("There was an issue sending")

    def _recv_message(self):
        try:
            data = self.sock.recv(1024).decode()
            print('Received from server:', data)
        except:
            print("There was an issue recving")

    def echo(self, message:str="test"):
        self._send_message("ECHO")
        self._send_message(message)
        self._recv_message()

    def abort(self):
        print('Instructing server to shut down.')
        self._send_message("ABORT")
        print('Closing socket.')
        self.sock.close()

    def status(self):
        print("getting status")
        self._send_message("STATUS")
        self._recv_message()

    def init(self, bead_index=1, init_str=""):
        print("initializing")
        self._send_message("INIT")
        self._send_message(str(bead_index))
        init_str_len = len(init_str)
        self._send_message(str(init_str_len))
        if init_str_len > 0:
            self._send_message(init_str)

    def posdata(self, cell_vec_mat, inv_mat, num_atoms, atom_positions):
        print("sending position data")
        self._send_message("POSDATA")

        # to improve this code's speed, see packing and unpacking https://stackoverflow.com/a/50495001

        for i in list(cell_vec_mat):
            print(i)
            self.sock.send(i)
        print('\n')

        for i in list(inv_mat):
            print(i)
            self.sock.send(i)
        print('\n')

        na = num_atoms.to_bytes(4, byteorder=sys.byteorder)
        self.sock.send(na)
        print(na, '--->', num_atoms, '\n')

        for i in list(atom_positions.flatten()):
            print(i)
            self.sock.send(i)

        

# status_sig = "STATUS"
# init_sig = "INIT"
# posdata_sig = "POSDATA"
# getforce_sig = "GETFORCE"
# getstress_sig = "GETSTRESS"