# Usage
At the top level of the sparc-dft-api project, run `make` to compile server
code. `make clean` will previously compiled code.
# Socket Description



# Protocols
To add more protocols, define const char* signatures, then create the handling function protocol_xxx, then add it to protocol_dispatch.

# Future Work
* call to shut down the server and/or close socket connection - serv_int_abort
* expand protocols
    * more headers for different calculations and results
* connect to sparc backend
* what are the return codes for dispatch protocol
* allow socket to stay open for multiple interactions
* allow the buffer to store more that a static number of bytes
    * probably requires a malloc
    * probably requires that you pass the length of the object you are going to send beforehand
    * need to correct parse_request to accept more bytes
* evaluate inet ports vs unix ports (or is it sockets)
* should client send header and length first? or should it just be pulled from an initialization string?