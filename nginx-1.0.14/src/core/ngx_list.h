
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_LIST_H_INCLUDED_
#define _NGX_LIST_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_list_part_s  ngx_list_part_t;

struct ngx_list_part_s {
    void             *elts;   // 当前节点中存放元素的起始地址（连续内存块）
    ngx_uint_t        nelts;  // 当前节点中已使用的元素个数
    ngx_list_part_t  *next;   // 指向下一个节点（链表结构）
};


typedef struct {
    ngx_list_part_t  *last;   // 指向当前链表的最后一个节点（用于追加新元素）
    ngx_list_part_t   part;   // 链表的第一个节点（内嵌结构体）
    size_t            size;   // 每个元素的大小（bytes）
    ngx_uint_t        nalloc; // 每个节点最多能容纳的元素个数
    ngx_pool_t       *pool;   // 内存池指针，用于分配新节点
} ngx_list_t;


ngx_list_t *ngx_list_create(ngx_pool_t *pool, ngx_uint_t n, size_t size);

static ngx_inline ngx_int_t
ngx_list_init(ngx_list_t *list, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    list->part.elts = ngx_palloc(pool, n * size);
    if (list->part.elts == NULL) {
        return NGX_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return NGX_OK;
}


/*
 *
 *  the iteration through the list:
 *
 *  part = &list.part;
 *  data = part->elts;
 *
 *  for (i = 0 ;; i++) {
 *
 *      if (i >= part->nelts) {
 *          if (part->next == NULL) {
 *              break;
 *          }
 *
 *          part = part->next;
 *          data = part->elts;
 *          i = 0;
 *      }
 *
 *      ...  data[i] ...
 *
 *  }
 */


void *ngx_list_push(ngx_list_t *list);


#endif /* _NGX_LIST_H_INCLUDED_ */
