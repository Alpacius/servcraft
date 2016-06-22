#ifndef     SCRAFT_RBT_IFCE_H_
#define     SCRAFT_RBT_IFCE_H_

#include    "./scraft_rbt.h"


void scraft_rbt_insert(struct scraft_rbtree *tree, struct scraft_rbtree_node *node);
void scraft_rbt_init(struct scraft_rbtree *tree, int (*key_compare)(const void *, const void *));
void scraft_rbt_delete(struct scraft_rbtree *tree, struct scraft_rbtree_node *node);
void scraft_rbt_detach(struct scraft_rbtree_node *node);
struct scraft_rbtree_node *scraft_rbtree_min(struct scraft_rbtree *tree);
struct scraft_rbtree_node *scraft_rbtree_max(struct scraft_rbtree *tree);
struct scraft_rbtree_node *scraft_rbt_find(struct scraft_rbtree *tree, const void *key);

#endif      // SCRAFT_RBT_IFCE_H_
