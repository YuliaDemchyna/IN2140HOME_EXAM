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
    D1Peer* peer = (D1Peer*) malloc(sizeof(D1Peer));
    if (!peer) {
        perror("Failed to allocate D1Peer");
        return NULL;
    }

    peer->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (peer->socket < 0) {
        perror("Failed to create UDP socket");
        free(peer);
        return NULL;
    }

    memset(&(peer->addr), 0, sizeof(peer->addr));

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
{
    struct hostent *he;
    struct in_addr **addr_list;

    if ((he = gethostbyname(peername)) == NULL) {
        herror("gethostbyname failed");
        return 0;  //error
    }

    addr_list = (struct in_addr **) he->h_addr_list;

    memset(&(peer->addr), 0, sizeof(peer->addr));
    peer->addr.sin_family = AF_INET;
    peer->addr.sin_port = htons(server_port);
    peer->addr.sin_addr = *addr_list[0];

    return 1;
}

int d1_recv_data( struct D1Peer* peer, char* buffer, size_t sz )
{
    char packet_buffer[1024];
    ssize_t received_len;
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    received_len = recvfrom(peer->socket, packet_buffer, sizeof(packet_buffer), 0,
                            (struct sockaddr *)&from, &from_len);

    //printf("Received %zd bytes.\n", received_len);
    if (received_len < 0) {
        perror("recvfrom failed");
        return -1;
    }

    if (received_len < sizeof(D1Header)) {
        fprintf(stderr, "Packet too short to contain header\n");
        return -2;
    }

    D1Header *header = (D1Header *)packet_buffer;

    if (ntohl(header->size) != received_len) {
        fprintf(stderr, "Mismatched packet size\n");
        d1_send_ack(peer, !(header->flags & SEQNO));
        printf("exit -3 \n");
        return -3;
    }

    uint16_t computed_checksum = calculate_checksum(header, packet_buffer + sizeof(D1Header), received_len - sizeof(D1Header));
    if (ntohs(header->checksum) != computed_checksum) {
        fprintf(stderr, "Checksum error\n");
        d1_send_ack(peer, !(header->flags & SEQNO));
        return -4;
    }

    size_t payload_size = received_len - sizeof(D1Header);
    if (payload_size > sz) {
        fprintf(stderr, "Buffer size too small for payload\n");
        return -5;
    }

    memcpy(buffer, packet_buffer + sizeof(D1Header), payload_size);

    d1_send_ack(peer, peer->next_seqno);
    peer->next_seqno = peer->next_seqno ? 0 : 1;

    return payload_size;
}

int d1_wait_ack(D1Peer* peer, char* buffer, size_t sz) {
    char packet_buffer[1024];
    ssize_t received_len;
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    while (1) {
        received_len = recvfrom(peer->socket, packet_buffer, sizeof(packet_buffer), 0,
                                (struct sockaddr *)&from, &from_len);

        if (received_len < 0) {
            perror("recvfrom failed");
            return -1;
        }

        if (received_len < sizeof(D1Header)) {
            fprintf(stderr, "Received packet too short to contain header\n");
            continue;
        }

        D1Header *header = (D1Header *)packet_buffer;

        if ((ntohs(header->flags) & FLAG_ACK) == FLAG_ACK) {
            uint16_t seqno = ntohs(header->flags) & ACKNO;


            if (seqno == peer->next_seqno) {
                //printf("Received correct ACK with seqno %d\n", peer->next_seqno);
                // peer->next_seqno = peer->next_seqno ? 0 : 1;  // toggling does not work here based on servers behavior
                return 1;
            } else {
                fprintf(stderr, "Received incorrect ACK seqno %d, expected %d. Resending data.\n", seqno, peer->next_seqno);
                if (d1_send_data(peer, buffer, sz) < 0) {
                    fprintf(stderr, "Failed to resend data\n");
                    return -2;
                }
            }
        } else {
            fprintf(stderr, "Received non-ACK packet during ACK wait\n");
            return -3 ;
        }
    }
}

uint16_t calculate_checksum(const D1HeaderLocal* header, const char* data, size_t data_size) {
    uint16_t checksum = 0;
    const unsigned char* bytes = (const unsigned char*) header;

    for (size_t i = 0; i < sizeof(D1Header); i += 2) {
        if (i != 2) {
            checksum ^= (bytes[i] << 8) | bytes[i + 1];
        }
    }

    size_t i = 0;
    uint16_t temp = 0;
    for (; i < data_size; ++i) {
        if (i % 2 == 0) {
            temp = ((unsigned char)data[i]) << 8;
        } else {
            temp |= (unsigned char)data[i];
            checksum ^= temp;
            temp = 0;
        }
    }

    if (data_size % 2 != 0) {
        checksum ^= temp;
    }

    return checksum;
}

int d1_send_data(D1Peer* peer, char* buffer, size_t sz) {
    size_t total_packet_size = sizeof(D1Header) + sz;
    //printf("Size: %zu\n", total_packet_size);

    if (total_packet_size > 1024) {
        return -1;
    }

    char packet_buffer[1024];
    D1Header header;
    memset(&header, 0, sizeof(header));

    header.flags |= htons(FLAG_DATA);
    if (peer->next_seqno == 1) {
        header.flags |= htons(SEQNO);
    }

    header.size = htonl(total_packet_size);


    header.checksum = 0;
    header.checksum = htons(calculate_checksum(&header, buffer, sz));
    //printf("Checksum: %u\n", calculate_checksum(&header, buffer, sz));

    memcpy(packet_buffer, &header.flags, sizeof(header.flags));
    memcpy(packet_buffer + 2, &header.checksum, 2);
    memcpy(packet_buffer + 4, &header.size, 4);

    memcpy(packet_buffer + sizeof(D1Header), buffer, sz);

    ssize_t bytes_sent = sendto(peer->socket, packet_buffer, total_packet_size, 0,
                                (struct sockaddr *)&(peer->addr), sizeof(peer->addr));

    if (bytes_sent < 0) {
        perror("Send failed");
        return -2;
    }

    int ack_result = d1_wait_ack(peer, packet_buffer, total_packet_size);

    if (ack_result != 1) {
        return -3;
    }

    return bytes_sent;
}


void d1_send_ack(struct D1Peer* peer, int seqno) {
    D1Header ackHeader;
    char ackPacket[sizeof(D1Header)];

    //printf("Sending ACK for seqno %d\n", seqno);

    memset(&ackHeader, 0, sizeof(ackHeader));
    ackHeader.flags = htons(FLAG_ACK);


    if (seqno == 1) {
        ackHeader.flags |= htons(ACKNO);
    }

    //printf("ackHeader.flags: %hu\n", ntohs(ackHeader.flags));

    ackHeader.size = htonl(sizeof(D1Header));
    ackHeader.checksum = htons(calculate_checksum(&ackHeader, NULL, 0));

    memcpy(ackPacket, &ackHeader, sizeof(ackHeader));
    ssize_t sent_bytes = sendto(peer->socket, ackPacket, sizeof(ackHeader), 0,
                                (struct sockaddr *)&(peer->addr), sizeof(peer->addr));

    if (sent_bytes < 0) {
        perror("Failed to send ACK");
    }
}
