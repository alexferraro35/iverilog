#ifndef __pcb_priv_H
#define __pcb_priv_H
/*
 * Copyright (c) 2011 Stephen Williams (steve@icarus.com)
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

# include  <string>
# include  <set>
# include  <list>
# include  <map>
# include  <ivl_target.h>

extern int scan_scope(ivl_scope_t scope);

/*
 * This nexus_list is a list of all the nets that the scan_scope
 * collects, wrapped up into a list. The show_netlist dumps that list
 * as a netlist.
 */
struct nexus_data {
      std::string name;
      std::set<std::string> pins;
};

extern std::list<struct nexus_data*> nexus_list;

/*
 * The element_list is a collection of all the elements that were
 * located by the scope scan. The key is the refdes for the element,
 * and the value is the element_data_t structure that describes that
 * element.
 */
struct element_data_t {
      std::string description;
      std::string value;
};

extern std::map <std::string, element_data_t*> element_list;

extern void show_netlist(const char*net_path);

extern void show_pcb(const char*pcb_path);

#endif
