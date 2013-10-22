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

#ifndef SDB_MAP_H_
#define SDB_MAP_H_

#include "linkedlist.h"

#define DEFAULT_MAP_SIZE 30

union map_key {
    int key_int;
    void* key_void;
};

typedef union map_key MAP_KEY;

struct map_node {
    MAP_KEY key;
    void* value;
};
typedef struct map_node MAP_NODE;

struct map {
    int size;
    int(*hash)(struct map* this, MAP_KEY key);
    int(*equal)(MAP_KEY key, MAP_KEY node_key);
    void(*freedata)(void* data);
    LIST_NODE** map_node_list;
};
typedef struct map MAP;

void initialize_map(MAP* map, int size, int(*hash)(struct map* this, MAP_KEY key),
        int(*equal)(MAP_KEY key, MAP_KEY node_key), void(*freedata)(void* data));
void map_put(MAP* map, MAP_KEY key, void* value);
void* map_get(MAP* map, MAP_KEY key);
void map_remove(MAP* this, MAP_KEY key);

#endif /* SDB_MAP_H_ */
