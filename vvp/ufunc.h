#ifndef __ufunc_H
#define __ufunc_H
/*
 * Copyright (c) 2002-2005 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ifdef HAVE_CVS_IDENT
#ident "$Id: ufunc.h,v 1.4 2005/03/18 02:56:04 steve Exp $"
#endif

# include  "pointers.h"

/*
 * The .ufunc statement creates functors to represent user defined
 * functions within the netlist (as opposed to within behavioral
 * code.) The function device itself is implemented as a thread with a
 * bunch of functors to receive input and a functor to deliver the
 * output. (Functions have exactly one output.) The input functors
 * detect that a change has occurred, and invoke the thread to process
 * the new values. The thread then uses the output functor to deliver
 * the result. The relationships look like this:
 *
 *  ufunc_input_functor --+--> ufunc_core --> ...
 *                        |
 *  ufunc_input_functor --+
 *                        |
 *  ufunc_input_functor --+
 *
 * There are enough input functors to take all the function inputs, 4
 * per functor. These inputs deliver the changed input value to the
 * ufunc_core, which carries the infastructure for the thread. The
 * ufunc_core is also a functor whose output is connected to the rest
 * of the netlist. This is where the result is delivered back to the
 * netlist.
 */

class ufunc_core : public vvp_net_fun_t {

    public:
      ufunc_core(unsigned ow, vvp_net_t*ptr,
		 unsigned nports, vvp_net_t**ports,
		 vvp_code_t start_address,
		 struct __vpiScope*run_scope,
		 char*result_label);
      ~ufunc_core();

      struct __vpiScope*scope() { return scope_; }

      void assign_bits_to_ports(void);
      void finish_thread(vthread_t thr);

    private:
      friend class ufunc_input_functor;
      void recv_vec4_from_inputs(unsigned port, vvp_vector4_t bit);

    private:
	// output width of the function node.
      unsigned owid_;
	// Pointer to the "self" vvp_net_t that points to this functor.
      vvp_net_t*onet_;
	// Structure to track the input values from the input functors.
      unsigned nports_;
      vvp_net_t**ports_;
      vvp_vector4_t*port_values_;

	// This is a thread to execute the behavioral portion of the
	// function.
      vthread_t thread_;
      struct __vpiScope*scope_;
      vvp_code_t code_;

	// Where the result will be.
      vvp_net_t*result_;
};

/*
 * The job of the input functor is only to monitor inputs to the
 * function and pass them to the ufunc_core object. Each functor takes
 * up to 4 inputs, with the base the port number for the first
 * function input that this functor represents.
 */
class ufunc_input_functor : public vvp_net_fun_t {

    public:
      ufunc_input_functor(ufunc_core*c, unsigned base);
      ~ufunc_input_functor();

      void recv_vec4(vvp_net_ptr_t port, vvp_vector4_t bit);

    private:
      ufunc_core*core_;
      unsigned port_base_;
};

/*
 * $Log: ufunc.h,v $
 * Revision 1.4  2005/03/18 02:56:04  steve
 *  Add support for LPM_UFUNC user defined functions.
 *
 */
#endif
