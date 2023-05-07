#ifndef _DB_H
#define _DB_H
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "utils.h"
#include "list.h"

typedef struct {
    uint32_t id;
    uint32_t key;
    uint32_t val;
    struct list_head node;
} db_item_t;

void print_db_items();
void add_to_db(uint32_t key, uint32_t val);
db_item_t * get_from_db(uint32_t key);
void del_from_db(uint32_t key);
void mod_in_db(uint32_t key, uint32_t val);

#endif
