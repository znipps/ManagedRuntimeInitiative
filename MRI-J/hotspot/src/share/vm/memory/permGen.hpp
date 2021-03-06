/*
 * Copyright 2000-2005 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *  
 */
// This file is a derivative work resulting from (and including) modifications
// made by Azul Systems, Inc.  The date of such changes is 2010.
// Copyright 2010 Azul Systems, Inc.  All Rights Reserved.
//
// Please contact Azul Systems, Inc., 1600 Plymouth Street, Mountain View, 
// CA 94043 USA, or visit www.azulsystems.com if you need additional information 
// or have any questions.
#ifndef PERMGEN_HPP
#define PERMGEN_HPP

#include "generation.hpp"

// All heaps contains a "permanent generation," containing permanent
// (reflective) objects.  This is like a regular generation in some ways,
// but unlike one in others, and so is split apart.

class GenRemSet;
class CSpaceCounters;
class GenerationCounters;

// PermGen models the part of the heap 

class PermGen : public CHeapObj {
 protected:
  size_t _capacity_expansion_limit;  // maximum expansion allowed without a
				     // full gc occuring

 public:
  enum Name {
MarkSweepCompact,MarkSweep
  };

  // Permanent allocation (initialized)  
  virtual HeapWord* mem_allocate(size_t size) = 0;

  // Mark sweep support
  virtual void compute_new_size() = 0;

  // Ideally, we would use MI (IMHO) but we'll do delegation instead.
  virtual Generation* as_gen() const = 0;

  virtual void oop_iterate(OopClosure* cl) {
    Generation* g = as_gen();
    assert(g != NULL, "as_gen() NULL");
    g->oop_iterate(cl);
  }

  virtual void object_iterate(ObjectClosure* cl) {
    Generation* g = as_gen();
    assert(g != NULL, "as_gen() NULL");
    g->object_iterate(cl);
  }

  // Performance Counter support
  virtual void update_counters() {
    Generation* g = as_gen();
    assert(g != NULL, "as_gen() NULL");
    g->update_counters();
  }
};

#endif // PERMGEN_HPP
