/*
* SDB - Smart Development Bridge
*
* Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
*
* Contact:
* Ho Namkoong <ho.namkoong@samsung.com>
* Yoonki Park <yoonki.park@samsung.com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* Contributors:
* - S-Core Co., Ltd
*
*/

#include <stdlib.h>
#include "sdb_map.h"
#include "linkedlist.h"
#include "utils.h"

static LIST_NODE* find_in_list(MAP* this, LIST_NODE* list, MAP_KEY key);
static int default_hash(MAP* this, MAP_KEY key);
static int default_equal(MAP_KEY key, MAP_KEY node_key);
static void default_free(void* node);

void initialize_map(MAP* this, int size, int(*hash)(struct map* this, MAP_KEY key),
        int(*equal)(MAP_KEY key, MAP_KEY node_key), void(*freedata)(void* data)) {

    if(size < 1) {
        this->size = DEFAULT_MAP_SIZE;
    }
    else {
        this->size = size;
    }

    if(equal == NULL) {
        this->equal = default_equal;
    }
    else {
        this->equal = equal;
    }

    if(hash == NULL) {
        this->hash = default_hash;
    }
    else {
        this->hash = hash;
    }

    if(freedata == NULL) {
        this->freedata = default_free;
    }
    else {
        this->freedata = freedata;
    }

    this->map_node_list = (LIST_NODE**)calloc(this->size, sizeof(LIST_NODE*));
}

static void default_free(void* node) {
    MAP_NODE* _node = node;
    if(_node != NULL) {
        if(_node->value != NULL ) {
//            free(_node->value);
        }
        SAFE_FREE(_node);
    }
}

static int default_hash(MAP* this, MAP_KEY key) {
    return key.key_int%(this->size);
}

static int default_equal(MAP_KEY key, MAP_KEY node_key) {
    if(key.key_int != node_key.key_int) {
        return 0;
    }
    return 1;
}

static LIST_NODE* find_in_list(MAP* this, LIST_NODE* list, MAP_KEY key) {

    while(list != NULL) {
        MAP_NODE* node = list->data;
        if(this->equal(key, node->key)) {
            return list;
        }
        list = list->next_ptr;
    }

    return NULL;
}

void map_put(MAP* this, MAP_KEY key, void* value) {

    int hash_key = this->hash(this, key);
    LIST_NODE** hash_list = &(this->map_node_list[hash_key]);

    MAP_NODE* node = malloc(sizeof(MAP_NODE));
    node->key = key;
    node->value = value;

    LIST_NODE* list_node = find_in_list(this, *hash_list, key);
    if(list_node == NULL) {
        prepend(hash_list, (void*)node);
    }
    else {
        this->freedata(list_node->data);
        list_node->data = node;
    }
}

void* map_get(MAP* this, MAP_KEY key) {

    int hash_key = this->hash(this, key);
    LIST_NODE* hash_list = this->map_node_list[hash_key];
    LIST_NODE* result_node = find_in_list(this, hash_list, key);
    if(result_node == NULL) {
        return NULL;
    }

    return ((MAP_NODE*)(result_node->data))->value;
}

void map_remove(MAP* this, MAP_KEY key) {

    int hash_key = this->hash(this, key);
    LIST_NODE** hash_list = &(this->map_node_list[hash_key]);

    LIST_NODE* result_node = find_in_list(this, *hash_list, key);
    remove_node(hash_list, result_node, this->freedata);
}

void map_clear(MAP* this) {

    if(this != NULL) {
        int i =0;
        for(; i<this->size; i++) {
            free_list(this->map_node_list[i], this->freedata);
        }

        SAFE_FREE(this->map_node_list);
    }
}
