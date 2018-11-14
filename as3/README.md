# CMPUT 379 - Assignment 3

## Objectives

The objective of this assignment can provide hands-on experience in using Linux system API, such as, nonblocking I/O with `poll()`, using `FIFO` and `socket` for process communication and developed peer-to-peer communication protocol.

## Design Overview

- Heavily rely on the code developed in assignment 2.
- Each component (controller and switcher) has its own handler that are invoking from the main function, keep code modular.
- Keep the code as dry as possible by seperating repeated code into small helper function
- Use heartbeat mechanism to check if switch is alive (for the sake of `delay` feature)

## Project Status

All required features are implemented with error handling for most important functions, however, some edge cases may not be handled properly.

There is some minor issue with the heartbeat implementation. With some weaking of the interval to send heartbeat packet and check missing heartbeat count, now the server will take around 2 to 3 seconds to detech a swtich has entered disconnected state. I believe this is the best setting to help debug and grade this assignment.

## Testing and Results

1. Start controller with `a23dn cont 2 3000`
1. Start `sw1` with `a3sdn sw1 t2.dat null sw2 100-110 127.0.0.1 3000`
1. Observe `controller` and `sw1 have properly handshaken, and observered `sw1` has entered detached mode
1. Start `sw2` with `a3sdn sw2 t2.dat sw1 null 200-210 127.0.0.1 3000` immeditately
1. Observe `sw2`, `controller`, and `sw1` has properly exchanged `OPEN`, `ACK`, `RELAY`, and `QUERY` packets
1. The result shold be identical with the sample supplied by the professor

## Acknowledgments

- <https://stackoverflow.com/questions/2784500/how-to-send-a-simple-string-between-two-programs-using-pipes>
- <https://stackoverflow.com/a/1488815>
- <https://www.geeksforgeeks.org/socket-programming-cc/>
- <https://www.geeksforgeeks.org/socket-programming-cc/>

## Packet payload design

### OPEN
`OPEN <switch_num> <left_switch> <right switch> <range_low> <range_low>`

### QUERY
`QUERY <switch_num> <src_ip> <dest_ip>`

### ADD
`ADD <switch_num> <left_switch> <right switch> <range_low> <range_low> <src_ip> <dest_ip>`
OR
`ADD <switch_num> <ip> <src_ip> <dest_ip>` for no result

### RELAY
`RELAY <switch_num> <src_ip> <dest_ip>`
