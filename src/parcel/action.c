/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Actions
  action.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>                             /* assert() */
#include <stddef.h>                             /* NULL */
#include <stdlib.h>                             /* calloc/free */

#include "hpx/action.h"
#include "hpx/globals.h"                        /* __hpx_global_ctx (yuck) */
#include "debug.h"                              /* dbg_assert(_precondition) */
#include "hashstr.h"                            /* hashstr() */
#include "network.h"                            /* hpx_network_barrier() */

/**
 * Some constants that we use to govern the behavior of the action table.
 * @{
 */
static const int ACTIONS_INITIAL_HT_SIZE = 256; /**< initial table size */
static const int ACTIONS_PROBE_LIMIT = 2;       /**< when to expand table */
/**
 * @}
 */

/**
 * The hashtable entry type.
 *
 * Our hashtable is just an array of key-value pairs, this is the type of that
 * array element.
 */
struct entry {
  hpx_action_t  key;
  hpx_func_t value;
};

/**
 * The action hashtable is a linear probed hashtable, i.e., an array.
 */
static struct hashtable {
  size_t        size;
  struct entry *table;
} actions;

static void         expand(struct hashtable *);
static hpx_action_t insert(struct hashtable *, const hpx_action_t, const hpx_func_t);
static hpx_func_t   lookup(const struct hashtable *, const hpx_action_t);

/**
 * Expand a hashtable.
 *
 * The performance of this routine isn't important because it only happens once
 * per node, while actions are being inserted. It may be called recursively (via
 * the insert() routine) when it encounters a collision so that lookups never
 * collide.
 *
 * @param[in] ht - the hashtable to expand
 */
void
expand(struct hashtable *ht)
{
  dbg_assert_precondition(ht != NULL);
  
  /* remember the previous state of the table */
  const int e = ht->size;
  struct entry *copy = ht->table;

  /* double the size of the table */
  ht->size = 2 * ht->size;
  ht->table = calloc(ht->size, sizeof(ht->table[0]));

  /* iterate through the previous table, and insert all of the values that
     aren't empty---i.e., anything where entry->value isn't NULL. This could
     trigger recursive expansion, but that's ok, because this loop will insert
     anything that wasn't inserted in the inner expansion. */
  for (int i = 0; i < e; ++i)
    if (copy[i].value != NULL)
      insert(ht, copy[i].key, copy[i].value);

  /* don't need copy anymore */
  free(copy);
}

/**
 * Insert a key-value pair into a hashtable.
 *
 * We don't really care about the performance of this operation because of the
 * two-phased approach to the way that we use the hashtable, all inserts happen
 * once, during table initialization, and then this is read-only.
 *
 * @param[in] ht    - the hashtable
 * @param[in] key   - the action key to insert
 * @param[in] value - the local function pointer for the action
 *
 * @returns key
 */
hpx_action_t
insert(struct hashtable *ht, const hpx_action_t key, const hpx_func_t value)
{
  dbg_assert_precondition(ht != NULL);
  dbg_assert_precondition(key != 0);
  dbg_assert_precondition(value != NULL);
  
  /* lazy initialization of the action table */
  if (!ht->table) {
    ht->size = ACTIONS_INITIAL_HT_SIZE;
    ht->table = calloc(ACTIONS_INITIAL_HT_SIZE, sizeof(ht->table[0]));
  }
  
  size_t i = key % ht->size;
  size_t j = 0;

  /* search for the correct bucket, which is just a linear search, bounded by
     ACTIONS_PROBE_LIMIT */ 
  while (ht->table[i].key != 0) {
    assert(((ht->table[i].key != key) || (ht->table[i].value == value)) && 
           "attempting to overwrite key during registration");
    
    i = (i + 1) % ht->size;                     /* linear probing */
    j = j + 1;
    
    if (ACTIONS_PROBE_LIMIT < j) {              /* stop probing */
      expand(ht);
      i = key % ht->size;
      j = 0;
    }
  }

  /* insert the entry */
  ht->table[i].key = key;
  ht->table[i].value = value;
  return key;
}

/**
 * Hashtable lookup.
 *
 * Implement a simple, linear probed table. Entries with a key of 0 or value of
 * NULL are considered invalid, and terminate the search.
 */
hpx_func_t
lookup(const struct hashtable *ht, const hpx_action_t key)
{
  dbg_assert_precondition(ht != NULL);
  dbg_assert_precondition(key != 0);

  /* We just keep probing here until we hit an invalid entry, because we know
   * that the probe limit was enforced during insert. */
  size_t i = key % ht->size;
  while ((ht->table[i].key != 0) && (ht->table[i].key != key))
    i = (i + 1) % ht->size;
  return ht->table[i].value;
}

/************************************************************************/
/* ADK: There are a few ways to handle action registration--The         */
/* simplest is under the naive assumption that we are executing in a    */
/* homogeneous, SPMD environment and parcels simply carry function      */
/* pointers around. The second is to have all interested localities     */
/* register the required functions and then simply pass tags            */
/* around. Finally, a simpler, yet practical alternative, is to have a  */
/* local registration scheme for exported functions. Eventually, we     */
/* would want to have a distributed namespace for parcels that provides */
/* all three options.                                                   */
/************************************************************************/

/**
 * Register an HPX action.
 * 
 * @param[in] name - Action Name
 * @param[in] func - The HPX function that is represented by this action.
 * 
 * @return HPX error code
 */
hpx_action_t
hpx_action_register(const char *name, hpx_func_t func)
{
  dbg_assert_precondition(name != NULL);
  dbg_assert_precondition(func != NULL);
  return insert(&actions, hashstr(name), func);
}

/**
 * Called after all action registration is complete.
 */
void
hpx_action_registration_complete(void)
{
  /* currently we perform a full network barrier in order to make sure that the
   * action table has been installed globally, so that we don't wind up with an
   * hpx_action_invoke() request before local action invocation is complete. */
  hpx_network_barrier();
}

/**
 * Call to invoke an action locally.
 *
 * @param[in]  action - the action id we want to perform
 * @param[in]  args   - the argument buffer for the action
 * @param[out] out    - a future to wait on
 *
 * @returns an error code
 */
hpx_error_t
hpx_action_invoke(hpx_action_t action, void *args, struct hpx_future **out)
{
  hpx_func_t f = lookup(&actions, action);
  dbg_assert(f && "Failed to find action");
  return hpx_thread_create(__hpx_global_ctx, 0, f, args, out, NULL);
}
