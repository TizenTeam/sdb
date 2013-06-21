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

#ifndef LINKEDLIST_H_
#define LINKEDLIST_H_

struct list_node {
    void* data;
    struct list_node* next_ptr;
};

typedef struct list_node LIST_NODE;

void no_free();
void append( LIST_NODE** listptr, void* value);
void free_list(LIST_NODE* listptr, void(free_func)(void*));
void remove_first(LIST_NODE** listptr, void(free_func)(void*));

#endif /* LINKEDLIST_H_ */
