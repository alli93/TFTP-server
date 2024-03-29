# Trivial File Transfer Protocol server
tftp.c is the source code for a trivial file transfer server following the RFC1350 standard implemented using the C socket programing interface.

Our implementation supports simultaneous downloads, the maximum amount of concurrent downloads can be set with the max_clients variable 
at the start of the main function. The idea behind this is to keep two arrays, an array of file descriptors called downloads and an array 
of client sockets called client_socket, where a client and the file he has requested share an index. 

With every iteration of the main body while loop, we check to see whether someone has made a request through the main socket and if so we 
assign him a file descriptor and a socket. We then send one block of data at a time to every active socket, guaranteeing equal distribution of resources
to all tftp requests.

Authors: 
Alexander Björnsson <alexanderbjorns@gmail.com>
Snorri Hjörvar Jóhannsson <snorri13@ru.is>
