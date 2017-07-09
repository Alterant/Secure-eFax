/* Part of this file reproduces source code in sigc++/slot.h, sigc++/object_slot.h
   and sigc++/bind.h to which Karl Einar Nelson owns the copyright.  The remainder
   of this file is copyright (C) Chris Vine 2004 to 2006

   This program is distributed under the General Public Licence, version 2.
   For particulars of this and relevant disclaimers see the file
   COPYING distributed with the source files.
*/


// this file enables the libsigc++-2.0 syntax used in this program to compile with
// libsigc++-1.2

#ifndef SIGC_COMPATIBILIY_H
#define SIGC_COMPATIBILIY_H

#include <sigc++/sigc++.h>

namespace sigc {

  //sigc::connection --> SigC::Connection
  typedef SigC::Connection connection;

  // sigc::trackable --> Sigc::Object
  typedef SigC::Object trackable;

  //sigc::signal0 --> Sigc::Signal0 (with default marshaller)
  template <class R>
    class signal0 : public SigC::Signal0<R> {};

  //sigc::signal1 --> Sigc::Signal1 (with default marshaller)
  template <class R,class P1>
    class signal1 : public SigC::Signal1<R, P1> {};

  //sigc::signal2 --> Sigc::Signal2 (with default marshaller)
  template <class R,class P1,class P2>
    class signal2 : public SigC::Signal2<R, P1, P2> {};

  // this is necessary for the next five template functions
  // sigc::slot<R> --> SigC::Slot0<R> (user declared slot taking member function with no argument)
  template <class R>
    struct slot : public SigC::Slot0<R> {
    slot(SigC::SlotNode* s): SigC::Slot0<R>(s) {}
    slot(R (*f)()): SigC::Slot0<R>(f) {}
    slot(const SigC::Slot0<R>& s): SigC::Slot0<R>(s) {}
  };

  /*
    In these bind functions it would be much easier to do it by
     uncommenting the return statements in each of these functions by
     calling SigC::bind directly.  However although gcc-2.95 and gcc-3
     accepts this, for some reason gcc-4 does not.  Accordingly the
     relevant implementations of SigC::bind in sigc++/bind.h are
     reproduced 'in extenso'
  */
    
  // sigc::bind --> SigC::bind (for passing one argument to a slot connected
  // to a signal passing no arguments)
  template <class A1,class R,class C1>
    slot<R> 
    bind(const SigC::Slot1<R,C1>& s, A1 a1) 
    {
      //return SigC::bind<A1, R, C1> (s, a1);
      typedef typename SigC::AdaptorBindData1_<C1> Data;
      typedef typename SigC::AdaptorBindSlot0_1_<R,C1> Adaptor;
      return reinterpret_cast<SigC::SlotNode*>(
	new Data((SigC::FuncPtr)(&Adaptor::proxy),s,
		 (SigC::FuncPtr)(&Data::dtor),a1));
    }

  // sigc::bind --> SigC::bind (for passing two arguments to a slot connected
  // to a signal passing no arguments)
  template <class A1,class A2,class R,class C1,class C2>
    slot<R> 
    bind(const SigC::Slot2<R,C1,C2>& s, A1 a1, A2 a2)
    { 
      //return SigC::bind<A1, A2, R, C1, C2> (s, a1, a2);
      typedef typename SigC::AdaptorBindData2_<C1,C2> Data;
      typedef typename SigC::AdaptorBindSlot0_2_<R,C1,C2> Adaptor;
      return reinterpret_cast<SigC::SlotNode*>(
	new Data((SigC::FuncPtr)(&Adaptor::proxy),s,
		 (SigC::FuncPtr)(&Data::dtor),a1,a2));
    }

  /* The remainder of these templated functions implement sigc::ptr_fun() and
     sigc::mem_fun() in terms of SigC::slot.  For the mem_fun functions it
     would be much easier to do it by uncommenting the return statements in each
     of these functions by calling SigC::slot directly.  However gcc-2.95 chokes
     on this with an internal compiler error.  Accordingly the relevant
     implementations of SigC::slot in sigc++/slot.h and sigc++/object_slot.h
     are reproduced 'in extenso' 
  */

  //sigc::ptrfun() --> SigC::slot (for function taking no arguments)
  template <class R>
    slot<R> 
    ptr_fun(R (*func)())
    { return func; }

  //sigc::memfun() --> SigC::slot (for class member function taking no arguments)
  template <class R,class O1,class O2>
    slot<R>
    mem_fun(O1& obj,R (O2::*method)())
    {
      //return slot<R, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot0_<R,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }

  //sigc::memfun() const --> SigC::slot const (for class member function taking no arguments)
  template <class R,class O1,class O2>
    slot<R>
    mem_fun(O1& obj,R (O2::*method)() const)
    {
      //return slot<R, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot0_<R,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }

  //sigc::memfun() --> SigC::slot (for class member function taking one argument)
  template <class R,class P1,class O1,class O2>
    SigC::Slot1<R,P1>
    mem_fun(O1& obj,R (O2::*method)(P1)) 
    {
      //return SigC::slot<R, P1, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot1_<R,P1,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);

    }
 
  //sigc::memfun() const --> SigC::slot const (for class member function taking one argument)
  template <class R,class P1,class O1,class O2>
    SigC::Slot1<R,P1>
    mem_fun(O1& obj,R (O2::*method)(P1) const) 
    {
      //return SigC::slot<R, P1, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot1_<R,P1,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);

    }
 
  //sigc::memfun() --> SigC::slot (for class member function taking two arguments)
  template <class R,class P1,class P2,class O1,class O2>
    SigC::Slot2<R,P1,P2>
    mem_fun(O1& obj,R (O2::*method)(P1,P2))
    {
      //return SigC::slot<R, P1, P2, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot2_<R,P1,P2,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }
 
  //sigc::memfun() --> SigC::slot const (for class member function taking two arguments)
  template <class R,class P1,class P2,class O1,class O2>
    SigC::Slot2<R,P1,P2>
    mem_fun(O1& obj,R (O2::*method)(P1,P2) const)
    {
      //return SigC::slot<R, P1, P2, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot2_<R,P1,P2,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }

  //sigc::memfun() --> SigC::slot (for class member function taking three arguments)
  template <class R,class P1,class P2,class P3,class O1,class O2>
    SigC::Slot3<R,P1,P2,P3>
    mem_fun(O1& obj,R (O2::*method)(P1,P2,P3))
    {
      //return SigC::slot<R, P1, P2, P3, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot3_<R,P1,P2,P3,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }
 
  //sigc::memfun() --> SigC::slot const (for class member function taking three arguments)
  template <class R,class P1,class P2,class P3,class O1,class O2>
    SigC::Slot3<R,P1,P2,P3>
    mem_fun(O1& obj,R (O2::*method)(P1,P2,P3) const)
    {
      //return SigC::slot<R, P1, P2, P3, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot3_<R,P1,P2,P3,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }
 
  //sigc::memfun() --> SigC::slot (for class member function taking four arguments)
  template <class R,class P1,class P2,class P3,class P4,class O1,class O2>
    SigC::Slot4<R,P1,P2,P3,P4>
    mem_fun(O1& obj,R (O2::*method)(P1,P2,P3,P4))
    {
      //return SigC::slot<R, P1, P2, P3, P4, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot4_<R,P1,P2,P3,P4,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }
 
  //sigc::memfun() --> SigC::slot const (for class member function taking four arguments)
  template <class R,class P1,class P2,class P3,class P4,class O1,class O2>
    SigC::Slot4<R,P1,P2,P3,P4>
    mem_fun(O1& obj,R (O2::*method)(P1,P2,P3,P4) const)
    {
      //return SigC::slot<R, P1, P2, P3, P4, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot4_<R,P1,P2,P3,P4,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }
 
  //sigc::memfun() --> SigC::slot (for class member function taking five arguments)
  template <class R,class P1,class P2,class P3,class P4,class P5,class O1,class O2>
    SigC::Slot5<R,P1,P2,P3,P4,P5>
    mem_fun(O1& obj,R (O2::*method)(P1,P2,P3,P4,P5))
    {
      //return SigC::slot<R, P1, P2, P3, P4, P5, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot5_<R,P1,P2,P3,P4,P5,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }
 
  //sigc::memfun() --> SigC::slot const (for class member function taking five arguments)
  template <class R,class P1,class P2,class P3,class P4,class P5,class O1,class O2>
    SigC::Slot5<R,P1,P2,P3,P4,P5>
    mem_fun(O1& obj,R (O2::*method)(P1,P2,P3,P4,P5) const)
    {
      //return SigC::slot<R, P1, P2, P3, P4, P5, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot5_<R,P1,P2,P3,P4,P5,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }
 
  //sigc::memfun() --> SigC::slot (for class member function taking six arguments)
  template <class R,class P1,class P2,class P3,class P4,class P5,class P6,class O1,class O2>
    SigC::Slot6<R,P1,P2,P3,P4,P5,P6>
    mem_fun(O1& obj,R (O2::*method)(P1,P2,P3,P4,P5,P6))
    {
      //return SigC::slot<R, P1, P2, P3, P4, P5, P6, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot6_<R,P1,P2,P3,P4,P5,P6,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }
 
  //sigc::memfun() --> SigC::slot const (for class member function taking six arguments)
  template <class R,class P1,class P2,class P3,class P4,class P5,class P6,class O1,class O2>
    SigC::Slot6<R,P1,P2,P3,P4,P5,P6>
    mem_fun(O1& obj,R (O2::*method)(P1,P2,P3,P4,P5,P6) const)
    {
      //return SigC::slot<R, P1, P2, P3, P4, P5, P6, O1, O2> (obj, method);
      typedef typename SigC::ObjectSlot6_<R,P1,P2,P3,P4,P5,P6,O2> SType;
      O2& obj_of_method = obj;
      return new SigC::ObjectSlotNode((SigC::FuncPtr)(&SType::proxy),
				      &obj,
				      &obj_of_method,
				      method);
    }

}

#endif
