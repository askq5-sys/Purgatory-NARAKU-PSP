/* Walks compiled event bytecode without running it, command by command.
 * Same layout as tools/update_window_styles_v174.py decode(); the full test
 * run checks both agree on every installed page. Used to look ahead in a
 * running script (picture prefetch). */
#ifndef NARAKU_VM_SCAN_H
#define NARAKU_VM_SCAN_H

#include <stddef.h>
#include <stdint.h>

static inline unsigned vm_scan_u16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

/* Fixed operand bytes after the opcode, or -1 for variable-size and
 * unknown commands. */
static inline int vm_scan_fixed(int op)
{
    switch (op) {
    case 0: case 13: case 31: case 35: return 0;
    case 8: case 14: case 28: case 36: case 37: case 39: return 1;
    case 3: case 4: case 12: case 24: case 27: case 41: return 2;
    case 38: return 3;
    case 7: case 20: case 29: case 42: return 4;
    case 2: case 6: case 11: case 19: case 23: case 30: return 5;
    case 18: return 6;
    case 16: return 7;
    case 5: return 8;
    case 15: return 9;
    case 9: case 10: case 17: return 11;
    case 25: return 16;
    case 26: return 17;
    case 32: return 21;
    case 33: return 22;
    default: return -1;
    }
}

/* Returns the opcode at pos and stores where the next command starts, or
 * returns -1 for an unknown or truncated command. */
static inline int vm_scan_op(const uint8_t *b, size_t size, size_t pos, size_t *next)
{
    size_t p = pos;
    int op, kind, fixed;
    if (pos >= size) return -1;
    op = b[p++];
    if (op == 44 || op == 45) p += 2;
    kind = (op == 1 || op == 40 || op == 44) ? 1 : (op == 22 || op == 45) ? 22 : op;
    if (kind == 1) {
        int i;
        size_t text = 0;
        if (p + 16 > size) return -1;
        for (i = 0; i < 8; ++i) text += vm_scan_u16(b + p + 2 * i);
        p += 16 + text;
    } else if (kind == 22) {
        int n, i, j;
        if (p + 3 > size) return -1;
        n = b[p];
        p += 3;
        for (i = 0; i < n; ++i) {
            size_t text = 0;
            if (p + 8 > size) return -1;
            for (j = 0; j < 4; ++j) text += vm_scan_u16(b + p + 2 * j);
            p += 8 + text;
        }
    } else if (kind == 21 || kind == 34) {
        if (p + 4 > size) return -1;
        p += (kind == 34 ? 5u : 4u) + (size_t)vm_scan_u16(b + p + 2) * 5u;
    } else if (kind == 43) {
        int j;
        size_t text = 0;
        if (p + 10 > size) return -1;
        for (j = 0; j < 4; ++j) text += vm_scan_u16(b + p + 2 + 2 * j);
        p += 10 + text;
    } else {
        fixed = vm_scan_fixed(kind);
        if (fixed < 0) return -1;
        p += (size_t)fixed;
    }
    if (p > size) return -1;
    *next = p;
    return op;
}

/* Resource id of the first Show Picture at or after pos within `limit`
 * commands, or 0. Stops at the end of the page or an unknown command. */
static inline int vm_scan_next_picture(const uint8_t *b, size_t size, size_t pos, int limit)
{
    while (limit-- > 0) {
        size_t next;
        int op = vm_scan_op(b, size, pos, &next);
        if (op <= 0) return 0;
        if ((op == 25 || op == 32) && pos + 5 <= size) {
            int rid = (int)vm_scan_u16(b + pos + 3);
            if (rid > 0) return rid;
        }
        pos = next;
    }
    return 0;
}

#endif
