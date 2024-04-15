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
    /* implement this */
    return NULL;
}

D2Client* d2_client_delete( D2Client* client )
{
    /* implement this */
    return NULL;
}

int d2_send_request( D2Client* client, uint32_t id )
{
    /* implement this */
    return 0;
}

int d2_recv_response_size( D2Client* client )
{
    /* implement this */
    return 0;
}

int d2_recv_response( D2Client* client, char* buffer, size_t sz )
{
    /* implement this */
    return 0;
}

LocalTreeStore* d2_alloc_local_tree( int num_nodes )
{
    /* implement this */
    return NULL;
}

void  d2_free_local_tree( LocalTreeStore* nodes )
{
    /* implement this */
}

int d2_add_to_local_tree( LocalTreeStore* nodes_out, int node_idx, char* buffer, int buflen )
{
    /* implement this */
    return 0;
}

void d2_print_tree( LocalTreeStore* nodes_out )
{
    /* implement this */
}

