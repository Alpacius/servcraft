// The implementation comes from ngx.

#ifndef     SCRAFT_RBT_H_
#define     SCRAFT_RBT_H_

#include    <stddef.h>
#include    <stdint.h>
#include    "../include/miscutils.h"

#define     SCRAFT_RBT_RED      1
#define     SCRAFT_RBT_BLACK    0

struct scraft_rbtree;

struct scraft_rbtree_node {
    void *key_ref;
    struct scraft_rbtree_node *left, *right, *parent;
    struct scraft_rbtree *meta;
    uint8_t color;
};

struct scraft_rbtree {
    struct scraft_rbtree_node *root, *sentinel, sentinel_;
    int (*key_compare)(const void *, const void *);
};

#endif      // SCRAFT_RBT_H_
