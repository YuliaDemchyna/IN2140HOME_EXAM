/* ======================================================================
 * YOU ARE EXPECTED TO MODIFY THIS FILE.
 * ====================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "d2_lookup.h"

D2Client* d2_client_create( const char* server_name, uint16_t server_port )
{

    // Create the D1Peer instance
    D1Peer* peer = d1_create_client();
    if (!peer) {
        fprintf(stderr, "Failed to create D1 peer\n");
        return NULL;
    }

    if (!d1_get_peer_info(peer, server_name, server_port)) {
        fprintf(stderr, "Failed to set up peer info\n");
        d1_delete(peer);
        return NULL;
    }

    D2Client* client = (D2Client*) malloc(sizeof(D2Client));
    if (!client) {
        perror("Failed to allocate D2Client");
        d1_delete(peer);
        return NULL;
    }

    client->peer = peer;

    return client;
}

D2Client* d2_client_delete(D2Client* client) {
    if (client != NULL) {
        if (client->peer != NULL) {
            d1_delete(client->peer);
        }
        free(client);
    }
    return NULL;
}

// not sure if it is succsessful
int d2_send_request(D2Client* client, uint32_t id) {
    if (id <= 1000) {
        fprintf(stderr, "ID must be greater than 1000\n");
        return -1;
    }

    PacketRequest request;
    memset(&request, 0, sizeof(request));
    request.type = htons(TYPE_REQUEST);  // Convert TYPE_REQUEST to network byte order
    request.id = htonl(id);              // Convert id to network byte order

    // Use D1 function to send the data
    int result = d1_send_data(client->peer, (char*)&request, sizeof(request));
    if (result <= 0) {
        fprintf(stderr, "Failed to send PacketRequest using D1 function\n");
        return -1;
    }

    return result;  // Return the result from the D1 layer function
}

int d2_recv_response_size(D2Client* client) {
    char buffer[sizeof(PacketResponseSize)];
    int bytes_received = d1_recv_data(client->peer, buffer, sizeof(buffer));

    if (bytes_received <= 0) {
        return -1;  // Failure in receiving data or no data received
    }

    PacketResponseSize* responseSize = (PacketResponseSize*) buffer;

    if (ntohs(responseSize->type) != TYPE_RESPONSE_SIZE) {
        fprintf(stderr, "Received packet is not a PacketResponseSize\n");
        return -1;  // Incorrect packet type
    }

    // Convert the size from network to host byte order and return it
    return ntohs(responseSize->size);
}

int d2_recv_response(D2Client* client, char* buffer, size_t sz) {
    if (sz < sizeof(PacketResponse)) {
        fprintf(stderr, "Buffer too small for PacketResponse\n");
        return -1;  // Buffer provided is too small to hold the smallest PacketResponse
    }

    int bytes_received = d1_recv_data(client->peer, buffer, sz);
    //fprintf(stderr, "Debug d2_recv_response size %d\n", bytes_received );

    if (bytes_received <= 0) {
        return -1;  // Failure in receiving data or no data received
    }

    PacketResponse* response = (PacketResponse*) buffer;

    if (ntohs(response->type) != TYPE_RESPONSE && ntohs(response->type) != TYPE_LAST_RESPONSE) {
        fprintf(stderr, "Received packet is not a PacketResponse or LastResponse\n");
        return -1;  // Incorrect packet type
    }

    // Verify that the received data length matches the payload_size declared in the header
    if (bytes_received != ntohs(response->payload_size)) {
        fprintf(stderr, "Mismatch between declared payload size and received size\n");
        return -1;  // Data integrity issue
    }

    return bytes_received;  // Return the number of bytes stored in the buffer
}

LocalTreeStore* d2_alloc_local_tree(int num_nodes) {
    if (num_nodes < 1) {
        return NULL;
    }

    LocalTreeStore* treeStore = (LocalTreeStore*)calloc(1, sizeof(LocalTreeStore));
    if (treeStore == NULL) {
        return NULL;
    }

    // Allocate memory for the node pointers within the LocalTreeStore
    treeStore->nodes = (NetNode**)calloc(num_nodes, sizeof(NetNode*));  // array of pointers
    if (treeStore->nodes == NULL) {
        free(treeStore);
        return NULL;
    }

    treeStore->number_of_nodes = num_nodes;
    return treeStore;
}

void d2_free_local_tree(LocalTreeStore* treeStore) {
    if (treeStore != NULL) {
        if (treeStore->nodes != NULL) {
            for (int i = 0; i < treeStore->number_of_nodes; i++) {
                if (treeStore->nodes[i] != NULL) {
                    free(treeStore->nodes[i]);  // looping and freeing all NetNode
                }
            }
            free(treeStore->nodes);
        }
        free(treeStore);
    }
}

// HELPER FUNCTION Ensure proper conversion from network to host byte order
void convert_netnode_from_network_to_host(NetNode *node) {
    node->id = ntohl(node->id);
    node->value = ntohl(node->value);
    node->num_children = ntohl(node->num_children);
    for (int i = 0; i < 5; i++) {
        node->child_id[i] = ntohl(node->child_id[i]);
    }
}


void print_bytes(const char* buffer, size_t buflen) {
    for (size_t i = 0; i < buflen; i++) {
        printf("%02X ", (unsigned char)buffer[i]); // Print each byte as a two-digit hexadecimal number
    }
    printf("\n");
}
int d2_add_to_local_tree(LocalTreeStore* treeStore, int node_idx, char* buffer, int buflen) {
    if (treeStore == NULL || buffer == NULL || buflen < sizeof(NetNode) || node_idx < 0) {
        fprintf(stderr, "Invalid input parameters.\n");
        return -1;
    }
    int expected_nodes = buflen / sizeof(NetNode);
    if (expected_nodes < 1 || expected_nodes > 5) {
        fprintf(stderr, "Invalid number of NetNodes in buffer: %d.\n", expected_nodes);
        return -2;
    }

    if (node_idx + expected_nodes > treeStore->number_of_nodes) {
        fprintf(stderr, "Buffer contains more nodes than can be added at the specified index.\n");
        return -3;
    }
    // print_bytes(buffer, buflen)
    fprintf(stderr, "Debug d2_add_to_local_tree %d, %d,%d,%d\n",
            node_idx, buflen, sizeof(NetNode), expected_nodes );
    for (int i = 0; i < 1; i++) { // correct to have expected instead of 1
        NetNode* newNode = (NetNode*) calloc(1, sizeof(NetNode));
        if (!newNode) {
            fprintf(stderr, "Memory allocation failed for newNode.\n");
            return -4; // Cleanup not handled here for brevity
        }
        memcpy(newNode, buffer + i * sizeof(NetNode), sizeof(NetNode));
        convert_netnode_from_network_to_host(newNode);

        treeStore->nodes[node_idx + i] = newNode;
        fprintf(stderr, "Debug in adding to tree: Node %d -> id: %u, value: %u, children: %u",
                node_idx + i, newNode->id, newNode->value, newNode->num_children);

        if (newNode->num_children > 0 && i < 1) {
            fprintf(stderr, ", child IDs: ");
            for (int j = 0; j < newNode->num_children; j++) {
                fprintf(stderr, "%u ", newNode->child_id[j]);
            }
        }
        fprintf(stderr, "\n");
    }

    return node_idx + expected_nodes; // Return the new index after adding all nodes
}

//HELPER TO PRINT
void print_node_recursive(LocalTreeStore* treeStore, int idx, int depth) {
    if (treeStore == NULL || idx < 0 || idx >= treeStore->number_of_nodes) {
        fprintf(stderr, "Invalid node index or tree store\n");
        return;
    }

    NetNode* node = treeStore->nodes[idx];
    if (node == NULL) {
        fprintf(stderr, "Node at index %d is NULL\n", idx);
        return;
    }



    //Print indentation for current depth
    for (int i = 0; i < depth; i++) {
        printf("--");
    }

    // Print the node's data
    // printf("id %u value %u children %u\n", node->id, node->value, node->num_children);
    printf("%d\n", node->value);


    // Recursively print each child
    for (int i = 0; i < node->num_children; i++) {
        uint32_t childIdx = node->child_id[i];
        if (childIdx < 0 || childIdx >= treeStore->number_of_nodes) {
            fprintf(stderr, "Child index %u out of bounds\n", childIdx);
            continue;
        }
        print_node_recursive(treeStore, childIdx, depth + 1);
    }
}

void d2_print_tree(LocalTreeStore* treeStore) {
    if (treeStore == NULL || treeStore->nodes == NULL || treeStore->number_of_nodes == 0) {
        printf("The tree is empty or not initialized.\n");
        return;
    }

    // Assume that the root node is at index 0, adjust if your structure varies
    print_node_recursive(treeStore, 0, 0);
}

