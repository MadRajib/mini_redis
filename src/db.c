#include "db.h"
#include "stdio.h"

struct list_head in_mem_db = LIST_INIT(in_mem_db);
unsigned int db_item_count = 0;

void print_db_items() {
    db_item_t *db_item;
    struct list_head *item = NULL; 
    list_for_each(item , &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        printf("key: %d val: %d ; ", db_item->key, db_item->val);
    }
    printf("1\n");
}

void add_to_db(uint32_t key, uint32_t val) {

    printf("%s\n",__func__);
    
    db_item_t *item = malloc(sizeof(db_item_t));
    item->id = ++db_item_count;
    item->key = key;
    item->val = val;
    list_add(&in_mem_db, &item->node);
    print_db_items();
}

db_item_t * get_from_db(uint32_t key) {
    printf("%s\n",__func__);

    struct list_head *item;
    struct list_head *next;
    db_item_t *db_item;


    list_for_each_safe(item, next, &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        if (db_item->key == key) {
            return db_item;
        }
    }
    print_db_items();
    return NULL;
}

void del_from_db(uint32_t key) {

    printf("%s\n",__func__);

    struct list_head *item;
    struct list_head *next;
    db_item_t *db_item;


    list_for_each_safe(item, next, &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        if (db_item->key == key) {
            list_del(&db_item->node);
            free(db_item);
            db_item = NULL;
        }
    }
    
    print_db_items();

}

void mod_in_db(uint32_t key, uint32_t val) {
    printf("%s\n",__func__);

    struct list_head *item;
    struct list_head *next;
    db_item_t *db_item;


    list_for_each_safe(item, next, &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        if (db_item->key == key) {
            db_item->val = val;
            break;
        }
    }
    
    print_db_items();
}
