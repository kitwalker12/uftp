uftp
====

Fast Reliable UDP transfer

This program, made as project for CSCI 558L Laboratory, performs File Transfer at fast speeds for lossy networks with high latency.
TCP file transfer suffers from really low throughputs in lossy networks as eventually when losses accumulate the TCP window size reduces to a single packet. This program overcome that limitation by using UDP as the transfer protocol. 
The client transfers the whole file over UDP to the server, and then waits for NAKs from the server. On receiving the file, when the server detects the UDP socket has been inactive for a short amount of time, it checks its list of received packets and sends out a UDP packet containing the lost packet sequence numbers. The client receives the NAKs and retransmits the lost packets and waits for further NAKs until it receives a UDP Finish packet. Otherwise the cycle starts over till the file transfer is complete. File integrity is performed by MD5 sum.
By using this approach, we download a major portion of the file without ever decreasing a virtual UDP window, and the rest is transferred by using the selective NAK mechanism. High throughput is obtained by sending multiple NAKs in a single UDP packets. Memory maps are used for reading and writing to the file to be transferred which drastically decreases the I/O time.

Install:
-make server
-make client

To Run:
Server-	./server <portno>
Client-	./client <serveraddr> <portno> <filename>

