# UDP file server 
This folder contains the code for a basic implementation of a UDP based file server. A simple mechanism is implemented to achieve a certain packet level reliability. 

## Compilation
Compile and run the server
   * make
   * ./server [portno]

Compile and run the client
   * make
   * ./client [server ip] [portno]

The client will display a set of commands and prompt for input as shown below.<br/>
   Please enter your commands in the format shown below<br/>
   1. get [filename]<br/>
   2. put [filename]<br/>
   3. delete [filename]<br/>
   4. ls<br/>
   5. exit<br/>

The client then receives the command and sends it to the server. 
Functions corresponding to the command get triggered both on the client and the server side.

## Reliability
For reliable file transfer, a structure which holds the following is used,
   1. packet no
   2. packet size
   3. buffer data
The sender keeps sending a packet till the receiver receives it and sends the same packet no back.<br/>
The receiver writes the data only when it receives the expected packet no and packet size.<br/> 
The receiver sends the updated packet no if the write is successful.<br/>
The sender then sends the next packet.<br/>
File transfer ends when the sender sends a packet of packet_size -1.

## get
1. Client receives the command and sends it to the server and keeps waiting for response.
2. server checks if the file is present and sends the response.
3. If the file is present, file transfer takes place.

## put
1. client receives the command and sends it to the server.
2. The server keeps waiting for response.
2. client checks if the file is present and sends the response to the server.
3. If the file is present, file transder takes place.

## delete
1. client receives the command and sends it to the server.
2. server checks if the file is present and sends the response.
3. If the file is present, the server deletes it and sends success status.

## ls
1. client receives the command and sends it to the server.
2. server queries the list of files and stores it in a file.
3. server then sends the file to the client and deletes it after sending.
4. client receives the file, displays it and then deletes it.

## exit
1. client receives the command and sends it to the server.
2. server exits.

## References
 * Starter code given with the assignment
 * https://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html
 * https://beej.us/guide/bgnet/html/single/bgnet.html
 * https://www.geeksforgeeks.org/socket-programming-cc/


 


