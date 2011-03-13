/*
 * Copyright 1999-2001 Sun Microsystems, Inc.  All Rights Reserved.
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
#ifndef CIOBJARRAY_HPP
#define CIOBJARRAY_HPP


#include "ciArray.hpp"
#include "ciEnv.hpp"
#include "ciUtilities.hpp"
#include "freezeAndMelt.hpp"

// ciObjArray
//
// This class represents a ObjArrayOop in the HotSpot virtual
// machine.
class ciObjArray : public ciArray {
  CI_PACKAGE_ACCESS

protected:
  ciObjArray(objArrayHandle h_o) : ciArray(h_o) {}
  ciObjArray(FAMPtr old_cioa) : ciArray(old_cioa) {
    FAM->mapNewToOldPtr(this, old_cioa);
  }

  ciObjArray(ciKlass* klass, int len) : ciArray(klass, len) {}

  objArrayOop get_objArrayOop() {
    return (objArrayOop)get_oop();
  }

  const char* type_string() { return "ciObjArray"; }

public:
  // What kind of ciObject is this?
  bool is_obj_array() { return true; }

  ciObject* obj_at(int index);
};

#endif // CIOBJARRAY_HPP
