# Usage
At the top level of the sparc-dft-api project, run `make` to compile server
code. `make clean` will remove previously compiled code.

Run the server in the top level by typing `./server` in the terminal. 

Run a client program. `client.ipynb` implements calls that you can use to simulate server-client interaction. Run the cells in order and watch the inputs and outputs of the server.
# Socket Description
The Sparc code is a C code used for running DFT calculations. Rather than have to write a C driver script for every DFT calculation, it is easier to write a python script that communicates with the C code to tell it what calculations to perform. There are a few ways that communcation could be carried out - either python bindings to specific C functions or through a server-client architecture where the client is a python script that can dynamically make requests of a server which is running the DFT calcualtions. This second style allows for a more dynamic workflow, so that is the architecture implemented here.

Note that in the i-PI documentation, they refer to server and client in the opposite usage as I do in all these documents. I found it weird that in the I-Pi documentation the large HPC cluster with highly tuned codes that recieves and answers requests is called the client, and the small python driver that is making requests is called the server, but what do I know.

Refer to i-Pi protocol: https://ipi-code.org/i-pi/user-guide.html#communication-protocol


# Future Work
## Near Term
* there may be issues keeping all messages ordered when running multiple commands really quickly... Not sure what to do about this... It might be that you take too much out of the stream at some point, and it messes up the eor_sigs. Might just be that it goes too fast and the socket hasn't been completely opened for proper comms...
* connect to sparc backend - implement protocol_getforce
* clean up print statements and use actual unit tests
* it might be possible to remove the cnx_status fields if return codes are better used for error handling. 
* make code better suited for molecular dynamics calculations so you don't have to recompute everything every time. See https://medfordgroup.slack.com/archives/C02CMU78X1V/p1635278357002000 for the next 6 or 7 posts terminating in https://medfordgroup.slack.com/archives/C02CMU78X1V/p1635448942026400 . This is how they implemented this in Quantum Espresso ASE codes https://github.com/gusmaogabriels/ase-espresso/blob/86b76e349162bd69f8cbb8dd93a4505b62970eba/espresso/iespresso.py#L253 
* potentially make serv_recv_arr and serv_recv_int one function that takes num_elements and a type.

## Long Term
* Handle Multiple unit types https://ipi-code.org/i-pi/introduction.html#internal-units
* evaluate whether to support both inet ports & unix ports (or is it sockets). Are both necessary? How much would have to change?


# Potentially Useful Links
## Project specific:
* https://gitlab.com/QEF/q-e/-/blob/develop/PW/src/run_driver.f90#L431 - Fortran implementation of the same code. Useful if you want to see how another group did this (and you can read Fortran)
* https://github.com/gusmaogabriels/ase-espresso/blob/86b76e349162bd69f8cbb8dd93a4505b62970eba/espresso/iespresso.py Python implementation of the i-PI server. Python is a more convenient and slower language to write this stuff in because it handles a lot of the buffer code. 
* https://github.com/i-pi/i-pi/blob/master/ipi/interfaces/sockets.py I-Pi python implementation
* https://github.com/i-pi/i-pi/blob/master/drivers/py/driver.py I-Pi driver code python implementation
* https://github.com/SPARC-X/SPARC main Sparc code. This server code will need to be tested with it at some point.
* https://ipi-code.org/i-pi/user-guide.html#communication-protocol Textual explanation of the I-Pi communucation protocol. The diagram in this dir is based on this.
* https://wiki.fysik.dtu.dk/ase/ase/calculators/socketio/socketio.html basc resource for understanding socket calculators for molecular simulations.

## Links for some questions I had about C, python, sockets, coding:
* https://stackoverflow.com/questions/15168771/pointer-to-a-string-in-c
* https://stackoverflow.com/questions/33913308/socket-module-how-to-send-integer
* https://stackoverflow.com/questions/46461236/does-realloc-free-the-existing-memory
* https://stackoverflow.com/questions/22131381/realloc-vs-malloc-in-c
* https://stackoverflow.com/questions/50494918/send-array-of-floats-over-a-socket-connection
* https://stackoverflow.com/questions/38511305/sending-float-values-on-socket-c-c
* https://stackoverflow.com/questions/37538/how-do-i-determine-the-size-of-my-array-in-c
* https://stackoverflow.com/questions/7583118/sending-float-arrays-over-c-sockets-between-windows-and-linux
* https://stackoverflow.com/questions/50494918/send-array-of-floats-over-a-socket-connection
* https://stackoverflow.com/questions/32434969/how-to-send-and-receive-an-array-of-integers-from-client-to-server-with-c-socket
* https://stackoverflow.com/questions/21017698/converting-int-to-bytes-in-python-3/54141411#54141411 
