/* ======================================================================
 * YOU CAN MODIFY THIS FILE.
 * ====================================================================== */

#ifndef D2_LOOKUP_MOD_H
#define D2_LOOKUP_MOD_H

#include "d1_udp.h"

struct D2Client
{
    D1Peer* peer;
};

typedef struct D2Client D2Client;

struct NetNodeLocal
{
    uint32_t id;
    uint32_t value;
    uint32_t num_children;
    uint32_t child_id[5];
};
typedef struct NetNodeLocal NetNodeLocal;
struct LocalTreeStore
{
    int number_of_nodes;
    NetNodeLocal** nodes;
};

typedef struct LocalTreeStore LocalTreeStore;

#endif /* D2_LOOKUP_MOD_H */
