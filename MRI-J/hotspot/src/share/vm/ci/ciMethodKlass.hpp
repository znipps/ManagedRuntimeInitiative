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
#ifndef CIMETHODKLASS_HPP
#define CIMETHODKLASS_HPP


#include "ciKlass.hpp"
#include "ciSymbol.hpp"

// ciMethodKlass
//
// This class represents a klassOop in the HotSpot virtual machine
// whose Klass part in a methodKlass.
class ciMethodKlass : public ciKlass {
  CI_PACKAGE_ACCESS

protected:
  ciMethodKlass(KlassHandle h_k) :
         ciKlass(h_k, ciSymbol::make("unique_methodKlass")) {
    assert(get_Klass()->oop_is_method(), "wrong type");
  }

  ciMethodKlass(FAMPtr old_cimk);
  virtual void fixupFAMPointers() { 
    ciKlass::fixupFAMPointers();
  }

  methodKlass* get_methodKlass() { return (methodKlass*)get_Klass(); }
  
  const char* type_string() { return "ciMethodKlass"; }

public:
  // What kind of ciObject is this?
  bool is_method_klass() { return true; }

  // Return the distinguished ciMethodKlass instance.
  static ciMethodKlass* make();
};

#endif // CIMETHODKLASS_HPP
