/*
 * A small user-space model of a capability table.
 *
 * The slot number is only a local lookup key. The object and rights stored in
 * the table are what make an operation possible. This is a teaching model,
 * not the seL4 kernel implementation or its exact revoke algorithm.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define CAP_READ  0x1u
#define CAP_WRITE 0x2u
#define MAX_SLOTS 8

struct object {
    const char *name;
    unsigned value;
};

struct capability {
    struct object *object;
    unsigned rights;
    unsigned lineage;
    bool root;
    bool valid;
};

static void require_cap(const struct capability *table, unsigned slot,
                        unsigned required, const char *operation) {
    const struct capability *cap = &table[slot];
    if (!cap->valid || cap->object == NULL || (cap->rights & required) != required) {
        printf("slot %u: DENY %s\n", slot, operation);
        return;
    }
    printf("slot %u: ALLOW %s on %s\n", slot, operation, cap->object->name);
    if (required == CAP_WRITE) cap->object->value++;
}

static void copy_cap(struct capability *table, unsigned from, unsigned to) {
    table[to] = table[from];
    table[to].rights &= CAP_READ; /* grant a deliberately weaker copy */
    table[to].root = false;
    printf("copied slot %u -> slot %u with read-only rights\n", from, to);
}

static void revoke_lineage(struct capability *table, unsigned lineage) {
    for (unsigned i = 0; i < MAX_SLOTS; ++i) {
        if (table[i].valid && table[i].lineage == lineage && !table[i].root) table[i].valid = false;
    }
    printf("revoked lineage %u\n", lineage);
}

int main(void) {
    struct object document = {"document", 0};
    struct capability cspace[MAX_SLOTS] = {0};

    cspace[0] = (struct capability){&document, CAP_READ | CAP_WRITE, 7, true, true};
    printf("slot 0 is a local name for a read/write capability\n");
    require_cap(cspace, 0, CAP_READ, "read");
    require_cap(cspace, 0, CAP_WRITE, "write");

    copy_cap(cspace, 0, 1);
    require_cap(cspace, 1, CAP_READ, "read");
    require_cap(cspace, 1, CAP_WRITE, "write");
    printf("document value=%u\n", document.value);

    printf("an integer from another table does not grant authority here\n");
    require_cap(cspace, 6, CAP_READ, "read");

    revoke_lineage(cspace, 7);
    require_cap(cspace, 0, CAP_READ, "read after revoke (root remains)");
    require_cap(cspace, 1, CAP_READ, "read after revoke");
    return EXIT_SUCCESS;
}
