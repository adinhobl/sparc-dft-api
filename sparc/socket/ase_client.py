import socket
import pickle
from sparc.sparc_core import SPARC
from ase.build import bulk

req_sig = "ASE CALC "
eor_sig = "\r\n\r\n"

def main():
    calc = SPARC(h=0.2)
    atoms = bulk('Si', cubic=True)
    atoms.set_cell([True] * 3)
    atoms.set_calculator(calc)
    print('Starting Client')
    print('Create a TCP/IP socket')
    sock = socket.socket(socket.AF_INET,
                         socket.SOCK_STREAM)
    
    print('Connect the socket to the server port')
    server_address = (socket.gethostname(), 8084)

    print('Connecting to:', server_address)
    sock.connect(server_address)
    print('Connected to server')
    try:
        print('Send data')
        data_string = pickle.dumps(atoms)
        print('Sending: ', data_string)
        sock.send("ASE CALC " + data_string + eor_sig)
        data = float(sock.recv(4096).decode())
        print('Received from server: ', data)
    finally:
        print('Closing socket')
        sock.close()

if __name__ == '__main__':
    main()