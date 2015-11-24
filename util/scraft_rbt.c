#include    "./scraft_rbt.h"


static struct scraft_rbtree_node sentinel_common = { .color = SCRAFT_RBT_BLACK, .key_ref = NULL, .left = NULL, .right = NULL, .parent = NULL };

static inline
void rbt_bst_insert(struct scraft_rbtree *tree, struct scraft_rbtree_node *node) {
    struct scraft_rbtree_node **target = NULL, *pos = tree->root;
    do {
        if (*(target = (tree->key_compare(node->key_ref, pos->key_ref) < 0) ? &(pos->left) : &(pos->right)) != tree->sentinel)
            pos = *target;
    } while (*target != tree->sentinel);
    (*target = node), (node->parent = pos), (node->left = node->right = tree->sentinel), (node->color = SCRAFT_RBT_RED);
}

static inline
void rbt_left_rotate(struct scraft_rbtree_node **root, struct scraft_rbtree_node *node, struct scraft_rbtree_node *sentinel) {
    struct scraft_rbtree_node *tmp = node->right;
    (node->right = tmp->left), ((tmp->left != sentinel) && (tmp->left->parent = node)), (tmp->parent = node->parent);
    if (node == *root)
        *root = tmp;
    else if (node == node->parent->left)
        node->parent->left = tmp;
    else
        node->parent->right = tmp;
    (tmp->left = node), (node->parent = tmp);
}

static inline
void rbt_right_rotate(struct scraft_rbtree_node **root, struct scraft_rbtree_node *node, struct scraft_rbtree_node *sentinel) {
    struct scraft_rbtree_node *tmp = node->left;
    (node->left = tmp->right), ((tmp->right != sentinel) && (tmp->right->parent = node)), (tmp->parent = node->parent);
    if (node == *root)
        *root = tmp;
    else if (node == node->parent->right)
        node->parent->right = tmp;
    else
        node->parent->left = tmp;
    (tmp->right = node), (node->parent = tmp);
}

void scraft_rbt_insert(struct scraft_rbtree *tree, struct scraft_rbtree_node *node) {
    struct scraft_rbtree_node **root = &(tree->root), *tmp = NULL;
    if (unlikely(tree->root == tree->sentinel)) {
        (tree->root = node), (tree->root->parent = NULL), (tree->root->left = tree->root->right = tree->sentinel), (tree->root->color = SCRAFT_RBT_BLACK);
        return;
    }
    rbt_bst_insert(tree, node);
    while ((node != *root) && (node->parent->color == SCRAFT_RBT_RED)) {
        if (node->parent == node->parent->parent->left) {
            tmp = node->parent->parent->right;
            if (tmp->color == SCRAFT_RBT_RED) {
                (node->parent->color = SCRAFT_RBT_BLACK), (tmp->color = SCRAFT_RBT_BLACK), (node->parent->parent->color = SCRAFT_RBT_RED);
                node = node->parent->parent;
            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    rbt_left_rotate(root, node, tree->sentinel);
                }
                (node->parent->color = SCRAFT_RBT_BLACK), (node->parent->parent->color = SCRAFT_RBT_RED);
                rbt_right_rotate(root, node->parent->parent, tree->sentinel);
            }
        } else {
            tmp = node->parent->parent->left;
            if (tmp->color == SCRAFT_RBT_RED) {
                (node->parent->color = SCRAFT_RBT_BLACK), (tmp->color = SCRAFT_RBT_BLACK), (node->parent->parent->color = SCRAFT_RBT_RED);
                node = node->parent->parent;
            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    rbt_right_rotate(root, node, tree->sentinel);
                }
                (node->parent->color = SCRAFT_RBT_BLACK), (node->parent->parent->color = SCRAFT_RBT_RED);
                rbt_left_rotate(root, node->parent->parent, tree->sentinel);
            }
        }
    }
    (*root)->color = SCRAFT_RBT_BLACK;
}


static inline
struct scraft_rbtree_node *scraft_rbtree_getmin(struct scraft_rbtree_node *node, struct scraft_rbtree_node *sentinel) {
    while (node->left != sentinel)
        node = node->left;
    return node;
}

static inline
struct scraft_rbtree_node *scraft_rbtree_getmax(struct scraft_rbtree_node *node, struct scraft_rbtree_node *sentinel) {
    while (node->right != sentinel)
        node = node->right;
    return node;
}

struct scraft_rbtree_node *scraft_rbtree_min(struct scraft_rbtree *tree) {
    return scraft_rbtree_getmin(tree->root, tree->sentinel);
}

struct scraft_rbtree_node *scraft_rbtree_max(struct scraft_rbtree *tree) {
    return scraft_rbtree_getmax(tree->root, tree->sentinel);
}

void scraft_rbt_delete(struct scraft_rbtree *tree, struct scraft_rbtree_node *node) {
    struct scraft_rbtree_node **root = &(tree->root), *tmp = NULL, *subst = NULL;
    if (node->left == tree->sentinel)
        (tmp = node->right), (subst = node);
    else if (node->right == tree->sentinel)
        (tmp = node->left), (subst = node);
    else 
        (subst = scraft_rbtree_getmin(node->right, tree->sentinel)), (tmp = (subst->left != tree->sentinel) ? subst->left : subst->right);
    if (subst == *root) {
        (*root = tmp), (tmp->color = SCRAFT_RBT_BLACK);
        return;
    }
    uint8_t subst_color = subst->color;
    (subst == subst->parent->left) ? (subst->parent->left = tmp) : (subst->parent->right = tmp);
    if (subst == node) {
        tmp->parent = subst->parent;
    } else {
        (subst->parent == node) ? (tmp->parent = subst) : (tmp->parent = subst->parent);
        (subst->left = node->left), (subst->right = node->right), (subst->parent = node->parent), (subst->color = node->color);
        if (node == *root)
            *root = subst;
        else
            (node == node->parent->left) ? (node->parent->left = subst) : (node->parent->right = subst);
        ((subst->left != tree->sentinel) && (subst->left->parent = subst)), ((subst->right != tree->sentinel) && (subst->right->parent = subst));
    }
    if (subst_color == SCRAFT_RBT_RED) return;
    struct scraft_rbtree_node *fix_target = NULL;
    while ((tmp != *root) && (tmp->color == SCRAFT_RBT_BLACK)) {
        if (tmp == tmp->parent->left) {
            fix_target = tmp->parent->right;
            if (fix_target->color == SCRAFT_RBT_RED) {
                (fix_target->color = SCRAFT_RBT_BLACK), (tmp->parent->color = SCRAFT_RBT_RED);
                rbt_left_rotate(root, tmp->parent, tree->sentinel);
                fix_target = tmp->parent->right;
            }
            if ((fix_target->left->color == SCRAFT_RBT_BLACK) && (fix_target->right->color == SCRAFT_RBT_BLACK)) {
                (fix_target->color = SCRAFT_RBT_RED), (tmp = tmp->parent);
            } else {
                if (fix_target->right->color == SCRAFT_RBT_BLACK) {
                    (fix_target->left->color = SCRAFT_RBT_BLACK), (fix_target->color = SCRAFT_RBT_RED);
                    rbt_right_rotate(root, fix_target, tree->sentinel);
                    fix_target = tmp->parent->right;
                }
                (fix_target->color = tmp->parent->color), (tmp->parent->color = SCRAFT_RBT_BLACK), (fix_target->right->color = SCRAFT_RBT_BLACK);
                rbt_left_rotate(root, tmp->parent, tree->sentinel);
                tmp = *root;
            }
        } else {
            fix_target = tmp->parent->left;
            if (fix_target->color == SCRAFT_RBT_RED) {
                (fix_target->color = SCRAFT_RBT_BLACK), (tmp->parent->color = SCRAFT_RBT_RED);
                rbt_right_rotate(root, tmp->parent, tree->sentinel);
                fix_target = tmp->parent->left;
            }
            if ((fix_target->left->color == SCRAFT_RBT_BLACK) && (fix_target->right->color == SCRAFT_RBT_BLACK)) {
                (fix_target->color = SCRAFT_RBT_RED), (tmp = tmp->parent);
            } else {
                if (fix_target->left->color == SCRAFT_RBT_BLACK) {
                    (fix_target->right->color = SCRAFT_RBT_BLACK), (fix_target->color = SCRAFT_RBT_RED);
                    rbt_left_rotate(root, fix_target, tree->sentinel);
                    fix_target = tmp->parent->left;
                }
                (fix_target->color = tmp->parent->color), (tmp->parent->color = SCRAFT_RBT_BLACK), (fix_target->left->color = SCRAFT_RBT_BLACK);
                rbt_right_rotate(root, tmp->parent, tree->sentinel);
                tmp = *root;
            }
        }
    }
    tmp->color = SCRAFT_RBT_BLACK;
}

void scraft_rbt_init(struct scraft_rbtree *tree, int (*key_compare)(const void *, const void *)) {
    (tree->sentinel = tree->root = &sentinel_common), (tree->key_compare = key_compare);
}

