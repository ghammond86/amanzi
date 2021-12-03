/*
  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Markus Berndt
           Ethan Coon (ecoon@lanl.gov)
*/
//! Visualizes a lifted domain set on the parent mesh.

#include "Epetra_Vector.h"
#include "VisualizationDomainSet.hh"

namespace Amanzi {

void
VisualizationDomainSet::WriteVector(const Epetra_MultiVector& vec,
        const std::vector<std::string>& names ) const
{
  Key varname = Keys::getVarName(names[0]);
  if (!lifted_vectors_.count(varname)) {
    // create a lifted vector if we don't currently have one
    auto lifted_vec = Teuchos::rcp(new Epetra_MultiVector(mesh()->cell_map(false),
            vec.NumVectors()));

    // also create a lifted set of names
    std::vector<std::string> lifted_names;
    for (const auto& name : names) {
      KeyTriple split_name;
      Keys::splitDomainSet(name, split_name);
      lifted_names.emplace_back(Keys::getKey(std::get<0>(split_name), std::get<2>(split_name)));
    }
    lifted_vectors_[varname] = std::make_pair(lifted_vec, lifted_names);
  }

  // copy from the domain-set vector into the lifted vector
  Epetra_MultiVector& lifted_vec = *lifted_vectors_[varname].first;
  auto subdomain = subdomains_.at(Keys::getDomain(names[0]));
  for (int j=0; j!=vec.NumVectors(); ++j) {
    for (int c=0; c!=vec.MyLength(); ++c) {
      auto parent_id = subdomain->entity_get_parent(AmanziMesh::Entity_kind::CELL, c);
      lifted_vec[j][parent_id] = vec[j][c];
    }
  }
}


void
VisualizationDomainSet::WriteVector(const Epetra_Vector& vec, const std::string& name ) const
{
  Key varname = Keys::getVarName(name);
  if (!lifted_vectors_.count(varname)) {
    // create a lifted vector if we don't currently have one
    auto lifted_vec = Teuchos::rcp(new Epetra_MultiVector(mesh()->cell_map(false), 1));

    std::vector<std::string> lifted_names;

    KeyTriple split_name;
    Keys::splitDomainSet(name, split_name);
    lifted_names.emplace_back(Keys::getKey(std::get<0>(split_name), std::get<2>(split_name)));

    lifted_vectors_[varname] = std::make_pair(lifted_vec, lifted_names);
  }

  // copy from the domain-set vector into the lifted vector
  Epetra_MultiVector& lifted_vec = *lifted_vectors_[varname].first;
  auto subdomain = subdomains_.at(Keys::getDomain(name));
  for (int c=0; c!=vec.MyLength(); ++c) {
    lifted_vec[0][subdomain->entity_get_parent(AmanziMesh::Entity_kind::CELL, c)] = vec[c];
  }
}

void
VisualizationDomainSet::FinalizeTimestep() const
{
  // FIXME -- Have to confirm that these are collective.  Some evals may only
  // be on some submeshes, meaning that the lifted vector may be on a subset of
  // processes.  Therefore each lifted vector much confirm it is collective
  // before trying to write. See #636
  //
  // For now we just error...  Note that even this error could be fooled if the
  // set of lifted vector keys are different, but the same number, on each
  // process.  In that case, vis would just totally be messed up, or maybe
  // would error later when # of DoF mismatches were found.
  int l_nlifted = lifted_vectors_.size();
  int g_nlifted = -1;
  mesh()->get_comm()->MaxAll(&l_nlifted, &g_nlifted, 1);
  if (l_nlifted != g_nlifted) {
    Errors::Message msg("VisualizationDomainSet: the number of lifted vectors on each process differs.  See Amanzi #636");
    Exceptions::amanzi_throw(msg);
  }

  // write the lifted vectors
  for (const auto& vecs : lifted_vectors_) {
    if (vecs.second.first->NumVectors() == 1) {
      Visualization::WriteVector(*(*vecs.second.first)(0), vecs.second.second[0]);
    } else {
      Visualization::WriteVector(*vecs.second.first, vecs.second.second);
    }
  }

  // clear the lifted vector cache, no need to keep this around as it may be big.
  lifted_vectors_.clear();

  // finalize the files
  Visualization::FinalizeTimestep();
}

} // namespace
