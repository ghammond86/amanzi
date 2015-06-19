/*
  This is the Operator component of the Amanzi code.

  Copyright 2010-2013 held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
          Ethan Coon (ecoon@lanl.gov)
*/

#ifndef AMANZI_OPERATOR_DIFFUSION_HH_
#define AMANZI_OPERATOR_DIFFUSION_HH_

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "exceptions.hh"
#include "tensor.hh"
#include "Point.hh"
#include "CompositeVector.hh"
#include "DenseMatrix.hh"

#include "BCs.hh"
#include "Operator.hh"
#include "OperatorDefs.hh"


/*
  Pure interface for diffusion operators.
*/ 

namespace Amanzi {
namespace Operators {

class OperatorDiffusion {
 public:
  OperatorDiffusion(const Teuchos::RCP<Operator>& global_op) :
      global_op_(global_op) {};

  OperatorDiffusion(const Teuchos::RCP<const AmanziMesh::Mesh>& mesh) :
      mesh_(mesh) {};

  OperatorDiffusion(const Teuchos::RCP<AmanziMesh::Mesh>& mesh) :
      mesh_(mesh) {};
  
  // main virtual members
  // -- setup 
  virtual void Setup(const Teuchos::RCP<std::vector<WhetStone::Tensor> >& K) = 0;
  virtual void Setup(const Teuchos::RCP<const CompositeVector>& k,
                     const Teuchos::RCP<const CompositeVector>& dkdp) = 0;

  // -- creation of an operator
  virtual void UpdateMatrices(const Teuchos::Ptr<const CompositeVector>& flux,
          const Teuchos::Ptr<const CompositeVector>& u) = 0;
  virtual void UpdateMatricesNewtonCorrection(
          const Teuchos::Ptr<const CompositeVector>& flux,
          const Teuchos::Ptr<const CompositeVector>& u) {};

  //Apply nonlinear 1D solver to compute consistent solution value on a boundary face
  double DeriveBoundaryFaceValue(int f, double atm_pressure, const CompositeVector& u);

  // -- after solving the problem: postrocessing
  virtual void UpdateFlux(const CompositeVector& u, CompositeVector& flux) = 0;

  // -- matrix modification
  virtual void ApplyBCs(bool primary, bool eliminate) = 0;
  virtual void ModifyMatrices(const CompositeVector& u) = 0;
  virtual void ScaleMassMatrices(double s) = 0;

  virtual double ComputeTransmisibility(int face){return 0.;};
  virtual double ComputeGravityFlux(int face){return 0.;};

  // default implementation  
  virtual void Setup(const Teuchos::RCP<std::vector<WhetStone::Tensor> >& K,
                     const Teuchos::RCP<const CompositeVector>& k,
                     const Teuchos::RCP<const CompositeVector>& dkdp) {
    Setup(K);
    Setup(k, dkdp);
  }
  virtual void Setup(const Teuchos::RCP<std::vector<WhetStone::Tensor> >& K,
                     const Teuchos::RCP<const CompositeVector>& k,
                     const Teuchos::RCP<const CompositeVector>& dkdp,
                     double rho) {
    SetDensity(rho);
    Setup(K);
    Setup(k, dkdp);
  }
  virtual void Setup(const Teuchos::RCP<std::vector<WhetStone::Tensor> >& K,
                     const Teuchos::RCP<const CompositeVector>& k,
                     const Teuchos::RCP<const CompositeVector>& dkdp,
                     const Teuchos::RCP<const CompositeVector>& rho) {
    SetDensity(rho);
    Setup(K);
    Setup(k, dkdp);
  }

  // boundary conditions (BC) require information on test and
  // trial spaces. For a single PDE, these BCs could be the same.
  virtual void SetBCs(const Teuchos::RCP<BCs>& bc_trial,
                      const Teuchos::RCP<BCs>& bc_test) {
    SetTrialBCs(bc_trial);  
    SetTestBCs(bc_test);  
  }
  virtual void SetTrialBCs(const Teuchos::RCP<BCs>& bc) {
    if (bcs_trial_.size() == 0) {
      bcs_trial_.resize(1);
    }
    bcs_trial_[0] = bc;
    global_op_->SetTrialBCs(bc);
  }
  virtual void SetTestBCs(const Teuchos::RCP<BCs>& bc) {
    if (bcs_test_.size() == 0) {
      bcs_test_.resize(1);
    }
    bcs_test_[0] = bc;
    global_op_->SetTestBCs(bc);
  }

  virtual void AddBCs(const Teuchos::RCP<BCs>& bc_trial,
                      const Teuchos::RCP<BCs>& bc_test) {
    bcs_trial_.push_back(bc_trial);
    bcs_test_.push_back(bc_test);
  }
  virtual void AddTrialBCs(const Teuchos::RCP<BCs>& bc) {
    bcs_trial_.push_back(bc);
  }
  virtual void AddTestBCs(const Teuchos::RCP<BCs>& bc) {
    bcs_test_.push_back(bc);
  }

  // gravity terms -- may not be implemented
  virtual void SetGravity(const AmanziGeometry::Point& g) {
    Errors::Message msg("OperatorDiffusion: This diffusion implementation does not support gravity.");
    Exceptions::amanzi_throw(msg);
  }
  virtual void SetDensity(double rho) {
    Errors::Message msg("OperatorDiffusion: This diffusion implementation does not support gravity.");
    Exceptions::amanzi_throw(msg);
  }
  virtual void SetDensity(const Teuchos::RCP<const CompositeVector>& rho) {
    Errors::Message msg("OperatorDiffusion: This diffusion implementation does not support gravity.");
    Exceptions::amanzi_throw(msg);
  }
  
  // access
  Teuchos::RCP<const Operator> global_operator() const { return global_op_; }
  Teuchos::RCP<Operator> global_operator() { return global_op_; }
  int schema_prec_dofs() { return global_op_schema_; }

  Teuchos::RCP<const Op> local_matrices() const { return local_op_; }
  Teuchos::RCP<Op> local_matrices() { return local_op_; }
  int schema_dofs() { return local_op_schema_; }

  Teuchos::RCP<const Op> jacobian_matrices() const { return jac_op_; }
  Teuchos::RCP<Op> jacobian_matrices() { return jac_op_; }
  int schema_jacobian() { return jac_op_schema_; }

  int little_k() { return little_k_; }
  
 protected:
  Teuchos::RCP<std::vector<WhetStone::Tensor> > K_;

  // physics
  bool scalar_rho_;
  double rho_;
  Teuchos::RCP<const CompositeVector> rho_cv_;

  Teuchos::RCP<const CompositeVector> k_, dkdp_;

  int little_k_;

  // operator
  Teuchos::RCP<Operator> global_op_;
  Teuchos::RCP<Op> local_op_;
  Teuchos::RCP<Op> jac_op_;
  int global_op_schema_, local_op_schema_, jac_op_schema_;
  std::vector<Teuchos::RCP<BCs> > bcs_trial_, bcs_test_;
  OperatorType operator_type_;

  // mesh info
  Teuchos::RCP<const AmanziMesh::Mesh> mesh_;
  int ncells_owned, ncells_wghost;
  int nfaces_owned, nfaces_wghost;
  int nnodes_owned, nnodes_wghost;
};




}  // namespace Operators
}  // namespace Amanzi

#endif


