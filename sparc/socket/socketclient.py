import socket
from ase import Atoms
eor_sig = "\r\n\r\n"

class SocketClient:
    
    def __init__(self, sock_num:int=20801, atoms):
        self.sock_num = sock_num
        self.sock = None
        self.atoms = atoms

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

    def status(self):
        print("getting status")
        self._send_message("STATUS" + eor_sig)
        self._recv_message()

    def init(self, bead_index=1, init_str="hello"):
        print("initializing")
        self._send_message("INIT" + eor_sig)
        self._send_message(str(bead_index).encode('utf8') + eor_sig)
        init_str_len = len(init_str)
        self._send_message(str(init_str_len).encode('utf8') + eor_sig)
        if init_str_len > 0:
            self._send_message(init_str + eor_sig)

    def posdata(self):
        print("sending data")
        self._send_message("POSDATA" + eor_sig)
        matrix = self.get_matrix
        for row in matrix:
            for item in row:
                self._send_message(item)
        self._send_message(self.get_num_atoms.encode('utf8') + eor_sig)
        coords = self.get_coords
        for row in coords:
            for item in row:
                self._send_message(item)

    def getforce(self):
        print("getting force")
        self._send_message("GETFORCE" + eor_sig)
        reply = ""
        while True:
            try:
                reply = self.recv()
            except:
                print("Error while receiving message: &s" % reply)
            if reply == "FORCEREADY":
                break
        
        if reply != "":
            pot = np.float64()
            pot = self.recv()

            mlen = np.int32()
            mlen = self.recv()

            force = np.zeros(3 * mlen, np.float64)
            force = self.recv()

            vir = np.zeros((3, 3), np.float64)
            vir = self.recv()

            return [pot, force, vir]
        
    def get_coords(self):
        return self.atoms.get_positions()

    def get_matrix(self):
        return self.atoms.cell[:]

    def get_num_atoms(self):
        return str(len(self.atoms))