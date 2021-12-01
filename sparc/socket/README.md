# Usage
At the top level of the sparc-dft-api project, run `make` to compile server
code. `make clean` will previously compiled code.
# Socket Description



# Protocols
To add more protocols, define const char* signatures, then create the handling function protocol_xxx, then add it to protocol_dispatch.

Refer to i-Pi protocol: https://ipi-code.org/i-pi/user-guide.html#communication-protocol


# Future Work
## Near Term
* there may be issues keeping all messages ordered when running multiple commands really quickly... Not sure what to do about this... It might be that you take too much out of the stream at some point, and it messes up the eor_sigs. Might just be that it goes too fast and the socket hasn't been completely opened for proper comms...
* do I need to handle cases where the end of response sentinal value isn't found while processing comms?
* expand protocols
    * more headers for different calculations and results
* connect to sparc backend
* what are the return codes for dispatch protocol
* evaluate inet ports vs unix ports (or is it sockets)
* clean up print statements and use actual unit tests
* it might be possible to remove the cnx_status fields if return codes are better used.
* make code better suited for molecular dynamics calculations so you don't have to recompute everything every time. See https://medfordgroup.slack.com/archives/C02CMU78X1V/p1635278357002000 for the next 6 or 7 posts terminating in https://medfordgroup.slack.com/archives/C02CMU78X1V/p1635448942026400 
* Sending floats is a pain in the butt. Is the best we can do converting the floats to strings,   joining the strings on the eor_sig, and then passing the whole thing to the other side to be string decoded?
    * TCP makes no guarantees (and in testing it was true) that the values would be sent one at a time
    * all the strings aren't the same length (e.g. 0.123 vs 2345.56753567), and it's not trivial to get padding correct. 
    * https://stackoverflow.com/questions/40763897/sending-raw-binary-data-over-sockets-in-python-3
    * https://stackoverflow.com/questions/3264828/change-default-float-print-format
    * https://stackoverflow.com/questions/8885663/how-to-format-a-floating-number-to-fixed-width-in-python
    * https://stackoverflow.com/questions/20037379/how-to-convert-python-socket-stream-string-value-to-float
    * https://stackoverflow.com/questions/50494918/send-array-of-floats-over-a-socket-connection
* potentially make serv_recv_arr and serv_recv_int one function that takes num_elements and a type.

## Long Term
* Handle Multiple unit types https://ipi-code.org/i-pi/introduction.html#internal-units