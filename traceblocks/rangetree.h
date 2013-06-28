#ifndef	_RANGETREE_H
#define	_RANGETREE_H

#include "rbtree.h"
#include <unistd.h>

/* start is inclusive, end is exclusive */
struct range {
	long start;
	struct rb_node range_node;
	long end;
};

/* Add a single value to the heap-allocated range tree.
 *
 * If the value is already in the tree, the range it is
 * in is returned. Otherwise, the new range is returned.
 *
 * If two ranges overlap after the addition, the ranges
 * are merged.
 */
extern struct range *add_value(long, struct rb_root *);

#endif
