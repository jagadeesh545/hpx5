// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include "btt.h"

namespace {
  struct Entry {
    int32_t count;
    int32_t owner;
    void *lva;
    Entry() : count(0), owner(0), lva(NULL) {
    }
    Entry(int32_t o, void *l) : count(0), owner(o), lva(l) {
    }
  };

  typedef cuckoohash_map<uint64_t, Entry, CityHasher<uint64_t> > Map;

  class BTT : public Map {
   public:
    BTT(size_t);
    bool trypin(gva_t gva, void** lva);
    void unpin(gva_t gva);
    uint32_t getOwner(gva_t gva) const;
  };
}

BTT::BTT(size_t size) : Map(size) {
}

bool
BTT::trypin(gva_t gva, void** lva) {
  uint64_t key = gva_to_key(gva);
  return update_fn(key, [lva](Entry& entry) {
      entry.count++;
      if (lva) {
        *lva = entry.lva;
      }
    });
}

void
BTT::unpin(gva_t gva) {
  uint64_t key = gva_to_key(gva);
  bool found = update_fn(key, [](Entry& entry) {
      entry.count--;
    });
  assert(found);
}

uint32_t
BTT::getOwner(gva_t gva) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    return entry.owner;
  }
  else {
    return gva.bits.home;
  }
}

void *
btt_new(size_t size) {
  return new BTT(size);
}

void
btt_delete(void* obj) {
  BTT *btt = static_cast<BTT*>(obj);
  delete btt;
}

void
btt_insert(void *obj, gva_t gva, int32_t owner, void *lva) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool inserted = btt->insert(key, Entry(owner, lva));
  assert(inserted);
  (void)inserted;
}

void
btt_remove(void *obj, gva_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool erased = btt->erase(key);
  assert(erased);
  (void)erased;
}

bool
btt_try_pin(void* obj, gva_t gva, void** lva) {
  BTT *btt = static_cast<BTT*>(obj);
  return btt->trypin(gva, lva);
}

void
btt_unpin(void* obj, gva_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  btt->unpin(gva);
}

uint32_t
btt_owner_of(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  return btt->getOwner(gva);
}
