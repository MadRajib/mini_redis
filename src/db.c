#include "db.h"
#include "stdio.h"
#include <stdint.h>

struct list_head in_mem_db = LIST_INIT(in_mem_db);
unsigned int db_item_count = 0;

enum {
    ERR_KEY_PRESENT=1,
    ERR_EMPTY_DB=2,
    ERR_DB_FULL=3,
    ERR_DEFAULT=4,
    ERR_INVALID_KEY=5,
};

void print_db_items() {
    db_item_t *db_item;
    struct list_head *item = NULL; 
    list_for_each(item , &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        printf("key: %d val: %d ; ", db_item->key, db_item->val);
    }
    printf("1\n");
}

Result_t add_to_db(uint32_t key, uint32_t val , int id) {
    printf("%s\n",__func__);

    Result_t result = {-1};
    db_item_t *item = NULL;

    /*check if key is alread present*/
    result =  get_from_db(key, id);
    if (!result.ret) { 
        result.ret = -ERR_KEY_PRESENT;
        goto RET;
    }

    /*add to db*/
    item = malloc(sizeof(db_item_t));
    item->id = id;
    item->key = key;
    item->val = val;

    list_add(&in_mem_db, &item->node);

    result.ret = 0;
    print_db_items();

RET:
    return result;
}

void remove_all_recod(int fd) {
    
    struct list_head *item;
    struct list_head *next;
    db_item_t *db_item;

    list_for_each_safe(item, next, &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        if (db_item->id ==  fd) {
            list_del(&db_item->node);
            free(db_item);
            db_item = NULL;
        }
    }
}

Result_t get_from_db(uint32_t key, int fd) {
    printf("%s\n",__func__);

    struct list_head *item;
    struct list_head *next;
    db_item_t *db_item;
    
    Result_t result = {-1};

    list_for_each_safe(item, next, &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        if (db_item->id == fd && db_item->key == key) {
            result.val = db_item->val;
            result.id = db_item->id;
            result.ret = 0;
            goto RET;
        }
    }

    result.ret = -ERR_INVALID_KEY;
RET:
    print_db_items();
    return result;
}

Result_t del_from_db(uint32_t key, int fd) {

    printf("%s\n",__func__);

    struct list_head *item;
    struct list_head *next;
    db_item_t *db_item;

    Result_t result = {-1};

    list_for_each_safe(item, next, &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        if (db_item->id == fd && db_item->key == key) {
            list_del(&db_item->node);
            free(db_item);
            db_item = NULL;
            result.ret = 0;
            goto RET;
        }
    }

    result.ret = -ERR_INVALID_KEY;
RET:
    print_db_items();
    return result;
}

Result_t mod_in_db(uint32_t key, uint32_t val, int fd) {
    printf("%s\n",__func__);

    struct list_head *item;
    struct list_head *next;
    db_item_t *db_item;
    Result_t result = {-1};

    list_for_each_safe(item, next, &in_mem_db) {
        db_item = container_of(item, db_item_t, node);
        if (db_item->id == fd && db_item->key == key) {
            db_item->val = val;
            result.ret = 0;
            goto RET;
        }
    }

    result.ret = -ERR_INVALID_KEY;
RET:
    print_db_items();
    return result;
}
