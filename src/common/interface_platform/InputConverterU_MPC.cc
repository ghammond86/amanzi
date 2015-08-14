/*
  This is the input component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <algorithm>
#include <sstream>
#include <string>

//TPLs
#include <xercesc/dom/DOM.hpp>

// Amanzi's
#include "errors.hh"
#include "exceptions.hh"
#include "dbc.hh"

#include "InputConverterU.hh"
#include "InputConverterU_Defs.hh"

namespace Amanzi {
namespace AmanziInput {

XERCES_CPP_NAMESPACE_USE

/* ******************************************************************
* Create MPC list, version 2, dubbed cycle driver.
****************************************************************** */
Teuchos::ParameterList InputConverterU::TranslateCycleDriver_()
{
  Teuchos::ParameterList out_list;

  Errors::Message msg;
  if (vo_->getVerbLevel() >= Teuchos::VERB_HIGH)
    *vo_->os() << "Translating cycle driver" << std::endl;

  MemoryManager mm;
  DOMNodeList *node_list, *children;
  DOMNode* node;
  DOMElement* element;

  // parse defaults of execution_controls 
  bool flag;
  node_list = doc_->getElementsByTagName(mm.transcode("execution_controls"));
  node = GetUniqueElementByTagsString_(node_list->item(0), "execution_control_defaults", flag);

  double t0, t1, dt0, t0_steady, t1_steady, dt0_steady;
  char *method, *tagname;
  bool flag_steady(false); 
  std::string method_d, dt0_d, filename;

  method_d = GetAttributeValueS_(static_cast<DOMElement*>(node), "method", false, "");
  dt0_d = GetAttributeValueS_(static_cast<DOMElement*>(node), "init_dt", false, "0.0");

  // parse execution_control
  std::map<double, std::string> tp_method, tp_mode;
  std::map<double, double> tp_dt0, tp_t1;
  std::map<double, int> tp_max_cycles;

  children = node_list->item(0)->getChildNodes();
  for (int i = 0; i < children->getLength(); ++i) {
    DOMNode* inode = children->item(i);
    if (inode->getNodeType() != DOMNode::ELEMENT_NODE) continue;
    element = static_cast<DOMElement*>(inode);

    tagname = mm.transcode(inode->getNodeName());
    if (strcmp(tagname, "execution_control") == 0) {
      t0 = TimeStringToValue_(GetAttributeValueS_(static_cast<DOMElement*>(inode), "start"));
      t1 = TimeStringToValue_(GetAttributeValueS_(static_cast<DOMElement*>(inode), "end"));
      dt0 = TimeStringToValue_(GetAttributeValueS_(element, "init_dt", false, dt0_d));
      std::string mode = GetAttributeValueS_(static_cast<DOMElement*>(inode), "mode");

      if (mode == "steady") {
        t0_steady = t0;
        t1_steady = t1;
        dt0_steady = dt0;
        flag_steady = true;
      } else {
        if (tp_mode.find(t0) != tp_mode.end()) {
          msg << "Transient \"execution_controls\" cannot have the same start time.\n";
          Exceptions::amanzi_throw(msg);
        }  

        tp_mode[t0] = mode;
        tp_t1[t0] = t1;
        tp_dt0[t0] = dt0;
        tp_method[t0] = GetAttributeValueS_(element, "method", false, method_d);
        tp_max_cycles[t0] = GetAttributeValueD_(element, "max_cycles", false, 10000000);

        filename = GetAttributeValueS_(element, "restart", false, "");
      }
    }
  }

  // old version 
  // -- parse available PKs
  int transient_model(0);
  std::map<std::string, bool> pk_state;

  node_list = doc_->getElementsByTagName(mm.transcode("process_kernels"));
  node = node_list->item(0);
  children = node->getChildNodes();

  for (int i = 0; i < children->getLength(); ++i) {
    DOMNode* inode = children->item(i);
    if (inode->getNodeType() != DOMNode::ELEMENT_NODE) continue;

    tagname = mm.transcode(inode->getNodeName());
    if (strcmp(tagname, "comments") == 0) continue;

    std::string state = GetAttributeValueS_(static_cast<DOMElement*>(inode), "state");
    pk_state[tagname] = (strcmp(state.c_str(), "on") == 0);

    if (strcmp(tagname, "flow") == 0) {
      flow_model_ = GetAttributeValueS_(
          static_cast<DOMElement*>(inode), "model", "richards, saturated, constant");
      pk_model_["flow"] = (flow_model_ == "richards") ? "richards" : "darcy";
      if (flow_model_ != "constant") transient_model += 4 * pk_state[tagname];
    } else if (strcmp(tagname, "chemistry") == 0) {
      std::string model = GetAttributeValueS_(static_cast<DOMElement*>(inode), "engine");
      pk_model_["chemistry"] = model;
      transient_model += pk_state[tagname];
    } else if (strcmp(tagname, "transport") == 0) {
      transient_model += 2 * pk_state[tagname];
    }
  }

  // -- create steady-state TP
  int tp_id(0);
  Teuchos::ParameterList pk_tree_list;

  if (flag_steady && pk_state["flow"]) {
    if (flow_model_ == "constant") {
      if (t1_steady != t0_steady) {
        msg << "Constant flow must have end time = start time.\n";
        Exceptions::amanzi_throw(msg);
      }
      node = GetUniqueElementByTagsString_(
          "unstructured_controls, unstr_steady-state_controls, unstr_initialization", flag);
      if (!flag) {
        msg << "Constant flow must have an initialization list, unless state=off.\n";
        Exceptions::amanzi_throw(msg);
      }
    }

    Teuchos::ParameterList& tmp_list = out_list.sublist("time periods").sublist("TP 0");
    tmp_list.sublist("PK Tree").sublist("Flow Steady").set<std::string>("PK type", pk_model_["flow"]);
    tmp_list.set<double>("start period time", t0_steady);
    tmp_list.set<double>("end period time", t1_steady);
    tmp_list.set<double>("initial time step", dt0_steady);

    tp_id++;
  }

  // -- create PK tree for transient TP
  std::map<double, std::string>::iterator it = tp_mode.begin();
  while (it != tp_mode.end()) {
    switch (transient_model) {
    case 1:
      pk_tree_list.sublist("Chemistry").set<std::string>("PK type", "chemistry");
      break;
    case 2:
      pk_tree_list.sublist("Transport").set<std::string>("PK type", "transport");
      break;
    case 3:
      {
        Teuchos::ParameterList& tmp_list = pk_tree_list.sublist("Reactive Transport");
        tmp_list.set<std::string>("PK type", "reactive transport");
        tmp_list.sublist("Transport").set<std::string>("PK type", "transport");
        tmp_list.sublist("Chemistry").set<std::string>("PK type", "chemistry");  
        break;
      }
    case 4:
      pk_tree_list.sublist("Flow").set<std::string>("PK type", pk_model_["flow"]);    
      break;
    case 5:
      {
        Teuchos::ParameterList& tmp_list = pk_tree_list.sublist("Flow and Chemistry");
        tmp_list.set<std::string>("PK type", "flow reactive transport");
        tmp_list.sublist("Chemistry").set<std::string>("PK type", "chemistry");
        tmp_list.sublist("Flow").set<std::string>("PK type", pk_model_["flow"]); 
        break;
      }
    case 6:
      {
        Teuchos::ParameterList& tmp_list = pk_tree_list.sublist("Flow and Transport");
        tmp_list.set<std::string>("PK type", "flow reactive transport");
        tmp_list.sublist("Transport").set<std::string>("PK type", "transport");
        tmp_list.sublist("Flow").set<std::string>("PK type", pk_model_["flow"]);
        break;
      }
    case 7:
      {
        Teuchos::ParameterList& tmp_list = pk_tree_list.sublist("Flow and Reactive Transport");
        tmp_list.set<std::string>("PK type", "flow reactive transport");
        tmp_list.sublist("Reactive Transport").set<std::string>("PK type", "reactive transport");
        tmp_list.sublist("Reactive Transport").sublist("Transport").set<std::string>("PK type", "transport");
        tmp_list.sublist("Reactive Transport").sublist("Chemistry").set<std::string>("PK type", "chemistry");
        tmp_list.sublist("Flow").set<std::string>("PK type", pk_model_["flow"]);
        break;
      }
    default:
      Exceptions::amanzi_throw(Errors::Message("This model does not supported by new MPC driver."));
    }

    std::ostringstream ss;
    ss << "TP " << tp_id;

    Teuchos::ParameterList& tmp_list = out_list.sublist("time periods").sublist(ss.str());
    tmp_list.sublist("PK Tree") = pk_tree_list;
    tmp_list.set<double>("start period time", it->first);
    tmp_list.set<double>("end period time", tp_t1[it->first]);
    tmp_list.set<int>("maximum cycle number", tp_max_cycles[it->first]);
    tmp_list.set<double>("initial time step", tp_dt0[it->first]);

    tp_id++;
    it++;
  }

  if (transient_model & 2 || transient_model & 1) {
    out_list.set<Teuchos::Array<std::string> >("component names", comp_names_all_);
  }

  out_list.sublist("Time Period Control") = TranslateTimePeriodControls_();
  if (filename.size() > 0) {
    out_list.sublist("Restart").set<std::string>("File Name", filename);
  }
  out_list.sublist("VerboseObject") = verb_list_.sublist("VerboseObject");

  return out_list;
}


/* ******************************************************************
* Empty
****************************************************************** */
Teuchos::ParameterList InputConverterU::TranslateTimePeriodControls_()
{
  Teuchos::ParameterList out_list;

  if (vo_->getVerbLevel() >= Teuchos::VERB_HIGH) {
    Teuchos::OSTab tab = vo_->getOSTab();
    *vo_->os() << "Translating time period controls" << std::endl;
  }

  // get the default time steps
  MemoryManager mm;
  DOMNodeList *node_list, *children;
  DOMNode* node;

  bool flag;
  node = GetUniqueElementByTagsString_("execution_controls, execution_control_defaults", flag);

  double dt_init_d, dt_max_d;
  dt_init_d = GetAttributeValueD_(static_cast<DOMElement*>(node), "init_dt", false, RESTART_TIMESTEP);
  dt_max_d = GetAttributeValueD_(static_cast<DOMElement*>(node), "max_dt", false, MAXIMUM_TIMESTEP);

  // add start times of all boundary conditions and all sources to the list
  Teuchos::Array<double> time_init, dt_init, dt_max;
  std::map<double, double> time_map, dt_max_map;

  std::vector<std::string> bc_names;
  bc_names.push_back("hydrostatic");
  bc_names.push_back("uniform_pressure");
  bc_names.push_back("inward_mass_flux");
  bc_names.push_back("aqueous_conc");

  node_list = doc_->getElementsByTagName(mm.transcode("boundary_conditions"));
  if (node_list->getLength() == 0) return out_list;
  node = node_list->item(0);

  for (int n = 0; n < bc_names.size(); ++n) {
    children = static_cast<DOMElement*>(node)->getElementsByTagName(mm.transcode(bc_names[n].c_str()));
    for (int i = 0; i < children->getLength(); ++i) {
      DOMNode* inode = children->item(i);
      if (inode->getNodeType() != DOMNode::ELEMENT_NODE) continue;

      double t = GetAttributeValueD_(static_cast<DOMElement*>(inode), "start");
      time_map[t] = dt_init_d;
      dt_max_map[t] = -1.0;
    }
  }

  // add these last so that the default initial time steps get overwritten
  children = doc_->getElementsByTagName(mm.transcode("execution_control"));
  for (int i = 0; i < children->getLength(); ++i) {
    DOMNode* inode = children->item(i);
    if (inode->getNodeType() != DOMNode::ELEMENT_NODE) continue;

    double t = GetAttributeValueD_(static_cast<DOMElement*>(inode), "start");
    double dt = GetAttributeValueD_(static_cast<DOMElement*>(inode), "init_dt", false, dt_init_d);
    double dt_max = GetAttributeValueD_(static_cast<DOMElement*>(inode), "max_dt", false, dt_max_d);
    time_map[t] = dt;
    dt_max_map[t] = dt_max;
  }

  // save times in the XML
  time_init.clear();
  dt_init.clear();
  dt_max.clear();

  for (std::map<double, double>::const_iterator map_it = time_map.begin(), max_it = dt_max_map.begin();
       map_it != time_map.end(); ++map_it, ++max_it) {
    time_init.push_back(map_it->first);
    dt_init.push_back(map_it->second);
    if (max_it->second < 0.0) {
      if (max_it == dt_max_map.begin()) {
	dt_max.push_back(dt_max_d);
      } else {
        int sz = dt_max.size();
        if (sz > 0) dt_max.push_back(dt_max[sz-1]);
      }
    } else {
      dt_max.push_back(max_it->second);
    }
  }

  out_list.set<Teuchos::Array<double> >("Start Times", time_init);
  out_list.set<Teuchos::Array<double> >("Initial Time Step", dt_init);
  out_list.set<Teuchos::Array<double> >("Maximum Time Step", dt_max);

  return out_list;
}


/* ******************************************************************
* Translate PKs list
****************************************************************** */
Teuchos::ParameterList InputConverterU::TranslatePKs_(const Teuchos::ParameterList& cd_list)
{
  Teuchos::ParameterList out_list;

  if (vo_->getVerbLevel() >= Teuchos::VERB_HIGH) {
    *vo_->os() << "Translating process kernels" << std::endl;
  }

  // create PKs list
  Teuchos::ParameterList tp_list = cd_list.sublist("time periods");

  for (Teuchos::ParameterList::ConstIterator it = tp_list.begin(); it !=tp_list.end(); ++it) {
    if ((it->second).isList()) {
      Teuchos::ParameterList& pk_tree = tp_list.sublist(it->first).sublist("PK Tree");
      RegisterPKsList_(pk_tree, out_list);
    }
  }

  // parse PKs list
  for (Teuchos::ParameterList::ConstIterator it = out_list.begin(); it != out_list.end(); ++it) {
    if ((it->second).isList()) {
      if (it->first == "Flow") {
        out_list.sublist(it->first) = TranslateFlow_(FLOW_TRANSIENT_REGIME);
      }
      else if (it->first == "Flow Steady") {
        out_list.sublist(it->first) = TranslateFlow_(FLOW_STEADY_REGIME);
      }
      else if (it->first == "Transport") {
        out_list.sublist(it->first) = TranslateTransport_();
      }
      else if (it->first == "Chemistry") {
        out_list.sublist(it->first) = TranslateChemistry_();
      }
      else if (it->first == "Reactive Transport") {
        Teuchos::Array<std::string> pk_names;
        pk_names.push_back("Chemistry");
        pk_names.push_back("Transport");
        out_list.sublist(it->first).set<Teuchos::Array<std::string> >("PKs order", pk_names);
      }
      else if (it->first == "Flow and Reactive Transport") {
        Teuchos::Array<std::string> pk_names;
        pk_names.push_back("Flow");
        pk_names.push_back("Reactive Transport");
        out_list.sublist(it->first).set<Teuchos::Array<std::string> >("PKs order", pk_names);
        out_list.sublist(it->first).set<int>("master PK index", 0);
      }
      else if (it->first == "Flow and Transport") {
        Teuchos::Array<std::string> pk_names;
        pk_names.push_back("Flow");
        pk_names.push_back("Transport");
        out_list.sublist(it->first).set<Teuchos::Array<std::string> >("PKs order", pk_names);
        out_list.sublist(it->first).set<int>("master PK index", 0);
      }
    }
  }

  return out_list;
}


/* ******************************************************************
* Empty
****************************************************************** */
void InputConverterU::RegisterPKsList_(
    Teuchos::ParameterList& pk_tree, Teuchos::ParameterList& pks_list)
{
  for (Teuchos::ParameterList::ConstIterator it = pk_tree.begin(); it !=pk_tree.end();++it) {
    if ((it->second).isList()) {
      pks_list.sublist(it->first);
      RegisterPKsList_(pk_tree.sublist(it->first), pks_list);
    }   
  }
}

}  // namespace AmanziInput
}  // namespace Amanzi
