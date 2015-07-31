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

#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "linkedlist.h"
#include "file_sync_functions.h"

static void default_free(void* data);

LIST_NODE* append(LIST_NODE** listptr, void* value) {

    LIST_NODE* prev_ptr = NULL;
    LIST_NODE* current_ptr = *listptr;

    LIST_NODE* new_ptr = (LIST_NODE*)malloc(sizeof(LIST_NODE));
    new_ptr->data = value;
    new_ptr->next_ptr = NULL;

    while(current_ptr != NULL) {
        prev_ptr = current_ptr;
        current_ptr = prev_ptr->next_ptr;
    }

    //listptr is empty.
    if(prev_ptr == NULL) {
        new_ptr->prev_ptr = NULL;
        *listptr = new_ptr;
    }
    else {
        prev_ptr->next_ptr = new_ptr;
        new_ptr->prev_ptr = prev_ptr;
    }

    return new_ptr;
}

LIST_NODE* prepend(LIST_NODE** listptr, void* value) {

    LIST_NODE* new_ptr = (LIST_NODE*)malloc(sizeof(LIST_NODE));
    new_ptr->data = value;
    new_ptr->next_ptr = *listptr;
    new_ptr->prev_ptr = NULL;

    if(*listptr != NULL) {
        (*listptr)->prev_ptr = new_ptr;
    }

    *listptr = new_ptr;

    return new_ptr;
}

void no_free(void* data) {
    //do nothing.
}

void free_list(LIST_NODE* listptr, void(free_func)(void*)) {

    if(free_func == NULL) {
        free_func = default_free;
    }

    LIST_NODE* currentptr = listptr;

    while(currentptr != NULL) {
        LIST_NODE* prev = currentptr;
        currentptr = currentptr->next_ptr;
        free_func(prev->data);
        SAFE_FREE(prev);
    }
}

void remove_node(LIST_NODE** listptr, LIST_NODE* remove_node, void(free_func)(void*)) {

    if(free_func == NULL) {
        free_func = default_free;
    }
    LIST_NODE* next = remove_node->next_ptr;
    LIST_NODE* prev = remove_node->prev_ptr;

    if(*listptr == remove_node) {
        *listptr = next;
    }
    else {
        //if remove_node is not listptr, prev is always not NULL.
        prev->next_ptr = next;
    }
    if (next != NULL) {
        next->prev_ptr = prev;
    }

    free_func(remove_node->data);
    SAFE_FREE(remove_node);
}

static void default_free(void* data) {
    SAFE_FREE(data);
}

void remove_first(LIST_NODE** listptr, void(free_func)(void*)) {

    if(free_func == NULL) {
        free_func = default_free;
    }

    if(*listptr != NULL) {
        LIST_NODE* curptr = (*listptr)->next_ptr;
        if(curptr != NULL) {
            curptr->prev_ptr = NULL;
        }
        LIST_NODE* removeptr = *listptr;
        *listptr = curptr;
        free_func(removeptr->data);
        SAFE_FREE(removeptr);
    }
}
