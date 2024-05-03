# IN2140-home-exam

The d1_create_client() function initializes a UDP socket and allocates a corresponding D1Peer structure, handling errors by returning NULL if socket creation fails. The d1_get_peer_info() function resolves a hostname to an IPv4 address and configures the socket's destination properties only supporting IPv4. d1_delete() which safely frees the client structure and closes its socket. 


The d1_send_data function constructs a data packet for transmission over a network, ensuring adherence to maximum size constraints. It initializes a packet header with necessary flags and a sequence number, then zeroes the checksum field to eliminate residual data influence. The checksum is calculated to include every byte, and once validated, it is inserted back into the packet. The finalized packet, comprising the data payload immediately following the header, is transmitted. Successful delivery is confirmed through an acknowledgment sent by the server.

Checksum employs a bitwise XOR operation, deliberately excluding the checksum field to prevent interference. This checksum, when validated by the recipient, confirms the absence of data corruption during transit. 


The d1_wait_ack() function is designed to receive acknowledge from the server, after the client send a data packet. If the knowledgemant is received with incorrect information the package will be resend until the correct acknowledge is received by the client. However I found that the implementation of this function was crucial, as the acknowledgment is always sent by the server, thus when we cannot recieve the data packet with  "pong" until the acknowledgement on the clients side has been received. Moreover, the sequence number management within this function assumes a specific pattern of alternating ACKs (0, 1, 0, 1, ...), which does not align with all server behaviours, that is toggled only when a packet is successfully sent by the server. And is thus toggled after successful recieval and sending of acknowledgment on the client side to the server, this is also noted in the code by comments.


The d1_recv_data()  listens for packets, checks their size against the header information, computes and compares the checksum to ensure data integrity, and if any discrepancies are found, it sends an ACK with the opposite sequence number to request a retransmission.The function d1_send_ack simply constructs and sends an acknowledgment packet with the correct sequence number flag, depending on whether the acknowledgment is confirming or requesting a retransmission.
I faced an issue for a long time regarding being able to move past the first "ping-pong" exchange, due to confusion when next_seqno should be toggled. I was sending the acknowledgement with the wrong seqno, I got to resolve it by figuring out the expected behaviour by output from the server. The next_seqno for the first time  is toggled after successful recieval for the first time.


The functions in d2 heavily relies on d1 for communication and new interpretation based on the flags of d2 level. The array of pointers to NetNodes in LocalTreeStore is useful because it allows direct access to any node based on its index, which corresponds to its ID since the nodes are added in a consistent order.It facilitates the reconstruction of the tree structure when printing. Therefore when printing I can access each node by its id, prints its depth with its value.
However this part of the code does not work as expected. The problem with the tree is that when the nodes are received it seems like some bytes are being lost. Since it shows that 5 nodes are being sent per time. But each node must have 32 bytes (size of the header), but the server sends a total size that varies but is often around 84 bytes, but is always under 160bytes + the Header (which would be the expected size for 5 netnodes). So initiation and storing of nodes is successful for the first two eleements then everything is shifted and fields are filled with wrong values. This can be observed by the debug print statements that I left commented out. 
When printing recursively I add "--" for each depth level to print it correctly. I first print the node and then move on to printing its children. 

Also I had to include local structures in mod header files, as it was not possible to include header files into eachother circularly.
