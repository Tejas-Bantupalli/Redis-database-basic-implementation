#pragma once

struct DList{
    DList *prev = NULL;
    DList *next = NULL;
};
void dList_init(DList *node);
bool dList_empty(DList *node);
void dlist_detach(DList *node);
void list_insert_before(DList *target, DList *node);