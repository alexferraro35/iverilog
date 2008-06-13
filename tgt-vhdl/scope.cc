/*
 *  VHDL code generation for scopes.
 *
 *  Copyright (C) 2008  Nick Gasson (nick@nickg.me.uk)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "vhdl_target.h"
#include "vhdl_element.hh"

#include <iostream>
#include <sstream>
#include <cassert>

/*
 * Given a nexus and an architecture, find the first signal
 * that is connected to the nexus, if there is one.
 */
static vhdl_var_ref *nexus_to_var_ref(vhdl_arch *arch, ivl_nexus_t nexus)
{
   int nptrs = ivl_nexus_ptrs(nexus);
   for (int i = 0; i < nptrs; i++) {
      ivl_nexus_ptr_t nexus_ptr = ivl_nexus_ptr(nexus, i);

      ivl_signal_t sig;
      if ((sig = ivl_nexus_ptr_sig(nexus_ptr))) {
         const char *signame = get_renamed_signal(sig).c_str();
         
         vhdl_decl *decl = arch->get_decl(signame);
         assert(decl);

         vhdl_type *type = new vhdl_type(*(decl->get_type()));
         return new vhdl_var_ref(signame, type);
      }
      else {
         // Ignore other types of nexus pointer
      }
   }
   
   assert(false);
}

/*
 * Convert the inputs of a logic gate to a binary expression.
 */
static vhdl_expr *inputs_to_expr(vhdl_arch *arch, vhdl_binop_t op,
                                 ivl_net_logic_t log)
{
   // Not always std_logic but this is probably OK since
   // the program has already been type checked
   vhdl_binop_expr *gate =
      new vhdl_binop_expr(op, vhdl_type::std_logic());
   
   int npins = ivl_logic_pins(log);
   for (int i = 1; i < npins; i++) {
      ivl_nexus_t input = ivl_logic_pin(log, i);
      gate->add_expr(nexus_to_var_ref(arch, input));
   }

   return gate;
}

/*
 * Convert a gate intput to an unary expression.
 */
static vhdl_expr *input_to_expr(vhdl_arch *arch, vhdl_unaryop_t op,
                                ivl_net_logic_t log)
{
   ivl_nexus_t input = ivl_logic_pin(log, 1);
   assert(input);

   vhdl_expr *operand = nexus_to_var_ref(arch, input);
   return new vhdl_unaryop_expr(op, operand, vhdl_type::std_logic()); 
}

/*
 * Translate all the primitive logic gates into concurrent
 * signal assignments.
 */
static void declare_logic(vhdl_arch *arch, ivl_scope_t scope)
{
   int nlogs = ivl_scope_logs(scope);
   for (int i = 0; i < nlogs; i++) {
      ivl_net_logic_t log = ivl_scope_log(scope, i);

      // The output is always pin zero
      ivl_nexus_t output = ivl_logic_pin(log, 0);
      vhdl_var_ref *lhs = nexus_to_var_ref(arch, output);

      vhdl_expr *rhs = NULL;
      switch (ivl_logic_type(log)) {
      case IVL_LO_NOT:
         rhs = input_to_expr(arch, VHDL_UNARYOP_NOT, log);
         break;
      case IVL_LO_AND:
         rhs = inputs_to_expr(arch, VHDL_BINOP_AND, log);
         break;
      case IVL_LO_OR:
         rhs = inputs_to_expr(arch, VHDL_BINOP_OR, log);
         break;
      default:
         error("Don't know how to translate logic type = %d",
               ivl_logic_type(log));
         continue;
      }
      assert(rhs);

      arch->add_stmt(new vhdl_cassign_stmt(lhs, rhs));
   }
}

/*
 * Declare all signals for a scope in an architecture.
 */
static void declare_signals(vhdl_arch *arch, ivl_scope_t scope)
{
   int nsigs = ivl_scope_sigs(scope);
   for (int i = 0; i < nsigs; i++) {
      ivl_signal_t sig = ivl_scope_sig(scope, i);

      int width = ivl_signal_width(sig);
      vhdl_type *sig_type;
      if (width > 0)
         sig_type = vhdl_type::std_logic();
      else
         sig_type = vhdl_type::std_logic_vector(width-1, 0);
      
      remember_signal(sig, arch->get_parent());

      const char *name = ivl_signal_basename(sig);
      ivl_signal_port_t mode = ivl_signal_port(sig);
      switch (mode) {
      case IVL_SIP_NONE:
         arch->add_decl(new vhdl_signal_decl(name, sig_type));
         break;
      case IVL_SIP_INPUT:
         arch->get_parent()->add_port
            (new vhdl_port_decl(name, sig_type, VHDL_PORT_IN));
         break;
      case IVL_SIP_OUTPUT:
         arch->get_parent()->add_port
            (new vhdl_port_decl(name, sig_type, VHDL_PORT_OUT));

         if (ivl_signal_type(sig) == IVL_SIT_REG) {
            // A registered output
            // In Verilog the output and reg can have the
            // same name: this is not valid in VHDL
            // Instead a new signal foo_Reg is created
            // which represents the register
            std::string newname(name);
            newname += "_Reg";
            rename_signal(sig, newname.c_str());

            vhdl_type *reg_type = new vhdl_type(*sig_type);
            arch->add_decl(new vhdl_signal_decl(newname.c_str(), reg_type));            

            // Create a concurrent assignment statement to
            // connect the register to the output
            arch->add_stmt
               (new vhdl_cassign_stmt
                (new vhdl_var_ref(name, NULL),
                 new vhdl_var_ref(newname.c_str(), NULL)));
         }
         break;
      case IVL_SIP_INOUT:
         arch->get_parent()->add_port
            (new vhdl_port_decl(name, sig_type, VHDL_PORT_INOUT));
         break;
      }
   }
}

/*
 * Create a VHDL entity for scopes of type IVL_SCT_MODULE.
 */
static vhdl_entity *create_entity_for(ivl_scope_t scope)
{
   assert(ivl_scope_type(scope) == IVL_SCT_MODULE);

   // The type name will become the entity name
   const char *tname = ivl_scope_tname(scope);

   // Remember the scope name this entity was derived from so
   // the correct processes can be added later
   const char *derived_from = ivl_scope_name(scope);
   
   // Verilog does not have the entity/architecture distinction
   // so we always create a pair and associate the architecture
   // with the entity for convenience (this also means that we
   // retain a 1-to-1 mapping of scope to VHDL element)
   vhdl_arch *arch = new vhdl_arch(tname, "FromVerilog");
   vhdl_entity *ent = new vhdl_entity(tname, derived_from, arch);

   // Locate all the signals in this module and add them to
   // the architecture
   declare_signals(arch, scope);

   // Similarly, add all the primitive logic gates
   declare_logic(arch, scope);
   
   // Build a comment to add to the entity/architecture
   std::ostringstream ss;
   ss << "Generated from Verilog module " << ivl_scope_tname(scope);
   
   arch->set_comment(ss.str());
   ent->set_comment(ss.str());
   
   remember_entity(ent);
   return ent;
}

/*
 * Map two signals together in an instantiation.
 * The signals are joined by a nexus.
 */
static void map_signal(ivl_signal_t to, vhdl_entity *parent,
                       vhdl_comp_inst *inst)
{
   // TODO: Work for multiple words
   ivl_nexus_t nexus = ivl_signal_nex(to, 0);
   
   int nptrs = ivl_nexus_ptrs(nexus);
   for (int i = 0; i < nptrs; i++) {
      ivl_signal_t sig;
      vhdl_decl *decl;
      
      ivl_nexus_ptr_t ptr = ivl_nexus_ptr(nexus, i);
      if ((sig = ivl_nexus_ptr_sig(ptr)) != NULL) {
         const char *basename = ivl_signal_basename(sig);
         if (sig == to) {
            // Don't map a signal to itself!
            continue;
         }
         else if ((decl = parent->get_arch()->get_decl(basename))) {
            // It's a signal declared in the parent
            // Pick this one (any one will do as they're all
            // connected together if there's more than one)
            vhdl_var_ref *ref =
               new vhdl_var_ref(basename, vhdl_type::std_logic());
            inst->map_port(ivl_signal_basename(to), ref);

            return;
         }
      }
   }

   error("Failed to find signal to connect to port %s",
         ivl_signal_basename(to));
}

/*
 * Find all the port mappings of a module instantiation.
 */
static void port_map(ivl_scope_t scope, vhdl_entity *parent,
                     vhdl_comp_inst *inst)
{
   // Find all the port mappings
   int nsigs = ivl_scope_sigs(scope);
   for (int i = 0; i < nsigs; i++) {
      ivl_signal_t sig = ivl_scope_sig(scope, i);
      
      ivl_signal_port_t mode = ivl_signal_port(sig);
      switch (mode) {
      case IVL_SIP_NONE:
         // Internal signals don't appear in the port map
         break;
      case IVL_SIP_INPUT:
      case IVL_SIP_OUTPUT:
      case IVL_SIP_INOUT:
         map_signal(sig, parent, inst);
         break;         
      }
   }
}

/*
 * Instantiate an entity in the hierarchy, and possibly create
 * that entity if it hasn't been encountered yet.
 */
static int draw_module(ivl_scope_t scope, ivl_scope_t parent)
{
   assert(ivl_scope_type(scope) == IVL_SCT_MODULE);

   // Maybe we need to create this entity first?
   vhdl_entity *ent = find_entity(ivl_scope_tname(scope)); 
   if (NULL == ent)
      ent = create_entity_for(scope);
   assert(ent);

   // Is this module instantiated inside another?
   if (parent != NULL) {
      vhdl_entity *parent_ent = find_entity(ivl_scope_tname(parent));
      assert(parent_ent != NULL);

      // Make sure we only collect instantiations from *one*
      // example of this module in the hieararchy
      if (parent_ent->get_derived_from() == ivl_scope_name(parent)) {

         vhdl_arch *parent_arch = parent_ent->get_arch();
         assert(parent_arch != NULL);
         
         // Create a forward declaration for it
         if (!parent_arch->have_declared_component(ent->get_name())) {
            vhdl_decl *comp_decl = vhdl_component_decl::component_decl_for(ent);
            parent_arch->add_decl(comp_decl);
         }
         
         // And an instantiation statement
         const char *inst_name = ivl_scope_basename(scope);
         vhdl_comp_inst *inst =
            new vhdl_comp_inst(inst_name, ent->get_name().c_str());
         port_map(scope, parent_ent, inst);         

         parent_arch->add_stmt(inst);
      }
      else {
         // Ignore this instantiation (already accounted for)
      }
   }
   
   return 0;
}

int draw_scope(ivl_scope_t scope, void *_parent)
{
   ivl_scope_t parent = static_cast<ivl_scope_t>(_parent);
   
   ivl_scope_type_t type = ivl_scope_type(scope);
   int rc = 0;
   switch (type) {
   case IVL_SCT_MODULE:
      rc = draw_module(scope, parent);
      break;
   default:
      error("No VHDL conversion for %s (at %s)",
            ivl_scope_tname(scope),
            ivl_scope_name(scope));
      break;
   }
   if (rc != 0)
      return rc;
   
   rc = ivl_scope_children(scope, draw_scope, scope);
   if (rc != 0)
      return rc;
   
   return 0;
}
