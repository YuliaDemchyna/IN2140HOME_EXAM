/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "d1_udp.h"
#include "d1_udp_mod.h"

D1Peer* d1_create_client( )
{
    // Allocate memory for the D1Peer structure
    D1Peer* peer = (D1Peer*) malloc(sizeof(D1Peer));
    if (!peer) {
        perror("Failed to allocate D1Peer");
        return NULL;
    }

    // Create a UDP socket
    peer->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (peer->socket < 0) {
        perror("Failed to create UDP socket");
        free(peer);  // Clean up allocated memory if socket creation fails
        return NULL;
    }

    // Initialize the sockaddr_in struct to zeros
    memset(&(peer->addr), 0, sizeof(peer->addr));


    // Initialize the sequence number to zero
    peer->next_seqno = 0;

    return peer;

}

D1Peer* d1_delete( D1Peer* peer )
{
    if (peer != NULL) {
        close(peer->socket);
        free(peer);
    }
    return NULL;
}

int d1_get_peer_info( struct D1Peer* peer, const char* peername, uint16_t server_port )
{// need to call DNS service with the host name, i need to resolve host name to ip
    struct hostent *he; // storing host info
    struct in_addr **addr_list;  // Pointer to an array of IP addresses, for storing host IP

    if ((he = gethostbyname(peername)) == NULL) {
        herror("gethostbyname failed");  // Report error
        return 0;  // Return 0 on error
    }

    addr_list = (struct in_addr **) he->h_addr_list;

    memset(&(peer->addr), 0, sizeof(peer->addr));  // Clear the structure
    peer->addr.sin_family = AF_INET;  // Set the family to IPv4
    peer->addr.sin_port = htons(server_port);  // Set the port, converting to network byte order
    peer->addr.sin_addr = *addr_list[0];  // Set the first IP address

    return 1;
}

int d1_recv_data( struct D1Peer* peer, char* buffer, size_t sz )
{
    printf("entered the function\n");
    char packet_buffer[1024];
    ssize_t received_len;
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    printf("Waiting for data...\n");

    // Receive a packet
    received_len = recvfrom(peer->socket, packet_buffer, sizeof(packet_buffer), 0,
                            (struct sockaddr *)&from, &from_len);


    printf("Received %zd bytes.\n", received_len);
    if (received_len < 0) {
        perror("recvfrom failed");
        printf("exit -1 ");
        return -1;
    }



    // Validate received length against minimum header size
    if (received_len < sizeof(D1Header)) {
        fprintf(stderr, "Packet too short to contain header\n");
        printf("exit -2 \n");
        return -2;
    }

    // Interpret the beginning of the buffer as a header.
    D1Header *header = (D1Header *)packet_buffer;

    //verifying the packets size
    if (ntohl(header->size) != received_len) {
        fprintf(stderr, "Mismatched packet size\n");
        d1_send_ack(peer, !(header->flags & SEQNO)); // Send ACK with the opposite sequence number
        printf("exit -3 \n");
        return -3;
    }

    // Compute checksum
    uint16_t computed_checksum = calculate_checksum(header, packet_buffer + sizeof(D1Header), received_len - sizeof(D1Header));
    if (ntohs(header->checksum) != computed_checksum) {
        fprintf(stderr, "Checksum error\n");
        printf("exit at checksum\n");
        d1_send_ack(peer, !(header->flags & SEQNO)); // Send ACK with the opposite sequence number
        return -4;
    }

    // Calculate payload size
    size_t payload_size = received_len - sizeof(D1Header);
    if (payload_size > sz) {
        printf("exit at payload size\n");
        fprintf(stderr, "Buffer size too small for payload\n");
        return -5;
    }

    // Copy payload to buffer
    memcpy(buffer, packet_buffer + sizeof(D1Header), payload_size);

    d1_send_ack(peer, peer->next_seqno);
    peer->next_seqno = peer->next_seqno ? 0 : 1;

    printf("Payload size: %zu, Returning normally\n", payload_size);
    printf("size: %zu \n", sz);

    return payload_size;
}

int d1_wait_ack(D1Peer* peer, char* buffer, size_t sz) {
    char packet_buffer[1024];
    ssize_t received_len;
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    printf("Waiting for ACK...\n");

    while (1) {
        received_len = recvfrom(peer->socket, packet_buffer, sizeof(packet_buffer), 0,
                                (struct sockaddr *)&from, &from_len);

        if (received_len < 0) {
            perror("recvfrom failed");
            return -1;  // Error in receiving data
        }

        if (received_len < sizeof(D1Header)) {
            fprintf(stderr, "Received packet too short to contain header\n");
            continue;  // Wait for next packet
        }

        D1Header *header = (D1Header *)packet_buffer;

        // Check if the packet is an ACK
        if ((ntohs(header->flags) & FLAG_ACK) == FLAG_ACK) {
            uint16_t seqno = ntohs(header->flags) & ACKNO;


            // Check if the sequence number matches
            if (seqno == peer->next_seqno) {
                printf("Received correct ACK with seqno %d\n", peer->next_seqno);
                // peer->next_seqno = peer->next_seqno ? 0 : 1;  // Toggle the sequence number
                return 1;  // Success
            } else {
                fprintf(stderr, "Received incorrect ACK seqno %d, expected %d. Resending data.\n", seqno, peer->next_seqno);
                // Resend the data
                if (d1_send_data(peer, buffer, sz) < 0) {
                    fprintf(stderr, "Failed to resend data\n");
                    return -2;  // Error in resending data
                }
                // Continue to wait for the correct ACK
            }
        } else {
            fprintf(stderr, "Received non-ACK packet during ACK wait\n");
            return -3 ;// Continue waiting, ignore non-ACK packets
        }
    }
}


//HELPER FUNCTION TO CALCULATE CHECKSUM
uint16_t calculate_checksum(const D1Header* header, const char* data, size_t data_size) {
    uint16_t checksum = 0;
    const unsigned char* bytes = (const unsigned char*) header;

    // XOR the header bytes, skipping the checksum field
    for (size_t i = 0; i < sizeof(D1Header); i += 2) {
        if (i != 2) { // Skip the checksum field at indices 2 and 3
            checksum ^= (bytes[i] << 8) | bytes[i + 1];
        }
    }

    // XOR the data bytes
    size_t i = 0;
    uint16_t temp = 0;
    for (; i < data_size; ++i) {
        if (i % 2 == 0) {
            temp = ((unsigned char)data[i]) << 8; // High byte for even indices
        } else {
            temp |= (unsigned char)data[i]; // Low byte for odd indices
            checksum ^= temp;
            temp = 0; // Reset temp after processing two bytes
        }
    }

    // If there's an odd byte left
    if (data_size % 2 != 0) {
        checksum ^= temp;
    }

    return checksum;
}

int d1_send_data(D1Peer* peer, char* buffer, size_t sz) {
    size_t total_packet_size = sizeof(D1Header) + sz;
    printf("Size: %zu\n", total_packet_size);

    if (total_packet_size > 1024) {
        return -1; // Error code for packet too large
    }

    char packet_buffer[1024];
    D1Header header;
    memset(&header, 0, sizeof(header));

    header.flags |= htons(FLAG_DATA); // Set data packet flag //apply htons to all things that are sent
    if (peer->next_seqno == 1) {
        header.flags |= htons(SEQNO); // Set the sequence number flag if next_seqno is 1
    }

    header.size = htonl(total_packet_size); // Convert total size to network byte order


    // Set checksum to zero before calculation
    header.checksum = 0;  // This is critical to ensure the checksum calculation starts from a clean state
    // Calculate checksum after all other header fields are set
    header.checksum = htons(calculate_checksum(&header, buffer, sz));  // isnt total size supposed to be there
    printf("Checksum: %u\n", calculate_checksum(&header, buffer, sz));
    // Copy the header and data into the packet buffer
    memcpy(packet_buffer, &header.flags, sizeof(header.flags));          // Copy 2 bytes of flags
    memcpy(packet_buffer + 2, &header.checksum, 2); // Copy 2 bytes of checksum
    memcpy(packet_buffer + 4, &header.size, 4); // Copy 4 bytes of size

    // Copy the data immediately following the header in the packet buffer
    memcpy(packet_buffer + sizeof(D1Header), buffer, sz);

    // Send the packet
    ssize_t bytes_sent = sendto(peer->socket, packet_buffer, total_packet_size, 0,
                                (struct sockaddr *)&(peer->addr), sizeof(peer->addr));


    if (bytes_sent < 0) {
        perror("Send failed");
        return -2;
    }

    // Wait for acknowledgement
    int ack_result = d1_wait_ack(peer, packet_buffer, total_packet_size);

    if (ack_result != 1) {
        return -3; // Acknowledgment error
    }

    return 0;
}


void d1_send_ack(struct D1Peer* peer, int seqno) {
    D1Header ackHeader;
    char ackPacket[sizeof(D1Header)];  // Assuming no additional data is sent with ACKs.

    printf("Sending ACK for seqno %d\n", seqno);

    memset(&ackHeader, 0, sizeof(ackHeader));
    ackHeader.flags = htons(FLAG_ACK);

    // Directly use the `seqno` argument to set the sequence number in the ACK
    if (seqno == 1) {
        ackHeader.flags |= htons(ACKNO);
    }

    // Log what flags are set for debugging
    printf("ackHeader.flags: %hu\n", ntohs(ackHeader.flags));

    ackHeader.size = htonl(sizeof(D1Header));
    ackHeader.checksum = htons(calculate_checksum(&ackHeader, NULL, 0));

    memcpy(ackPacket, &ackHeader, sizeof(ackHeader));
    ssize_t sent_bytes = sendto(peer->socket, ackPacket, sizeof(ackHeader), 0,
                                (struct sockaddr *)&(peer->addr), sizeof(peer->addr));

    if (sent_bytes < 0) {
        perror("Failed to send ACK");
    }
}


//TODO wake sure that  if wait ack gets wrong suqince number the data will must be sent again. ITS ONLY ABOUT SEQUENCES NUMBER
// so look for that error code and make a loop until  sequence nuber is resolved
