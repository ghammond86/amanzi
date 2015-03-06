/*
  This is the mpc_pk component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Ethan Coon

  Interface for the derived MPCStrong class.  Is both a PK and a Model
  Evalulator, providing needed methods for BDF time integration of the
  coupled system.

  Completely automated and generic to any sub PKs, this uses a block diagonal
  preconditioner.

  See additional documentation in the base class src/pks/mpc_pk/MPC_PK.hh
*/

#ifndef AMANZI_STRONG_MPC_HH_
#define AMANZI_STRONG_MPC_HH_

#include <vector>

#include "Teuchos_ParameterList.hpp"

#include "BDF1_TI.hh"
#include "FnTimeIntegratorPK.hh"
#include "MPC_PK.hh"
#include "PK_Factory.hh"

namespace Amanzi {

template<class PK_Base>
class MPCStrong : public MPC_PK<PK_Base>, public FnTimeIntegratorPK {
 public:
  MPCStrong(Teuchos::ParameterList& pk_tree,
            const Teuchos::RCP<Teuchos::ParameterList>& global_list,
            const Teuchos::RCP<State>& S,
            const Teuchos::RCP<TreeVector>& soln);

  // MPCStrong is a PK
  virtual void Setup();
  virtual void Initialize();

  // -- dt is the minimum of the sub pks
  virtual double get_dt() { return dt_; }
  virtual void set_dt(double dt) { dt_ = dt; }

  // -- advance each sub pk dt.
  virtual bool AdvanceStep(double t_old, double t_new);

  // MPCStrong is an ImplicitFn
  // -- computes the non-linear functional g = g(t,u,udot)
  //    By default this just calls each sub pk Functional().
  virtual void Functional(double t_old, double t_new, Teuchos::RCP<TreeVector> u_old,
           Teuchos::RCP<TreeVector> u_new, Teuchos::RCP<TreeVector> g);

  // -- enorm for the coupled system
  virtual double ErrorNorm(Teuchos::RCP<const TreeVector> u,
                       Teuchos::RCP<const TreeVector> du);

  // MPCStrong's preconditioner is, by default, just the block-diagonal
  // operator formed by placing the sub PK's preconditioners on the diagonal.
  // -- Apply preconditioner to u and returns the result in Pu.
  virtual void ApplyPreconditioner(Teuchos::RCP<const TreeVector> u, Teuchos::RCP<TreeVector> Pu);

  // -- Update the preconditioner.
  virtual void UpdatePreconditioner(double t, Teuchos::RCP<const TreeVector> up, double h);

  // -- Experimental approach -- calling this indicates that the time
  //    integration scheme is changing the value of the solution in
  //    state.
  virtual void ChangedSolution();

  // -- Admissibility of the solution.
  virtual bool IsAdmissible(Teuchos::RCP<const TreeVector> u);

  // -- Modify the predictor.
  virtual bool ModifyPredictor(double h, Teuchos::RCP<const TreeVector> u0,
          Teuchos::RCP<TreeVector> u);

  // -- Modify the correction.
  virtual AmanziSolvers::FnBaseDefs::ModifyCorrectionResult
      ModifyCorrection(double h, Teuchos::RCP<const TreeVector> res,
                       Teuchos::RCP<const TreeVector> u,
                       Teuchos::RCP<TreeVector> du);

 protected:
  using MPC_PK<PK_Base>::sub_pks_;
  using MPC_PK<PK_Base>::S_;

  using MPC_PK<PK_Base>::global_list_;
  using MPC_PK<PK_Base>::pk_tree_;
  using MPC_PK<PK_Base>::my_list_;

  using MPC_PK<PK_Base>::solution_;

  // timestep control
  double dt_;
  Teuchos::RCP<Amanzi::BDF1_TI<TreeVector, TreeVectorSpace> > time_stepper_;

 private:
  // factory registration
  static RegisteredPKFactory<MPCStrong> reg_;
};


// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
template<class PK_Base>
MPCStrong<PK_Base>::MPCStrong(Teuchos::ParameterList& pk_tree,
                              const Teuchos::RCP<Teuchos::ParameterList>& global_list,
                              const Teuchos::RCP<State>& S,
                              const Teuchos::RCP<TreeVector>& soln) :
    MPC_PK<PK_Base>(pk_tree, global_list, S, soln) {
};


// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
template<class PK_Base>
void MPCStrong<PK_Base>::Setup() {
  // Tweak the sub-PK parameter lists. This allows the PK to
  // potentially not assemble things.
  Teuchos::RCP<Teuchos::ParameterList> pks_list = Teuchos::sublist(global_list_, "PKs");
  for (Teuchos::ParameterList::ConstIterator param = pk_tree_.begin();
       param != pk_tree_.end(); ++param) {
    std::string pname = param->first;
    if (pks_list->isSublist(pname)) {
      pks_list->sublist(pname).set("strongly coupled PK", true);
    }
  }

  // call each sub-PKs Setup()
  MPC_PK<PK_Base>::Setup();

  // Set the initial timestep as the min of the sub-pk sizes.
  dt_ = get_dt();
};


// -----------------------------------------------------------------------------
// Initialize each sub-PK and the time integrator.
// -----------------------------------------------------------------------------
template<class PK_Base>
void MPCStrong<PK_Base>::Initialize() {
  // Just calls both subclass's initialize.  NOTE - order is important
  // here -- MPC<PK_Base> grabs the primary variables from each sub-PK
  // and stuffs them into the solution, which must be done prior to
  // initializing the timestepper.

  // Initialize all sub PKs.
  MPC_PK<PK_Base>::Initialize();

  // set up the timestepping algorithm if this is not strongly coupled
  if (!my_list_->template get<bool>("strongly coupled PK", false)) {
    // -- instantiate time stepper
    Teuchos::ParameterList& ts_plist = my_list_->sublist("time integrator").sublist("BDF1");
    ts_plist.set("initial time", S_->time());
    time_stepper_ = Teuchos::rcp(new Amanzi::BDF1_TI<TreeVector,
        TreeVectorSpace>(*this, ts_plist, solution_));

    // -- initialize time derivative
    Teuchos::RCP<TreeVector> solution_dot = Teuchos::rcp(new TreeVector(*solution_));
    solution_dot->PutScalar(0.0);

    // -- set initial state
    time_stepper_->SetInitialState(S_->time(), solution_, solution_dot);
  }
}


// -----------------------------------------------------------------------------
// Make one time step 
// -----------------------------------------------------------------------------
template<class PK_Base>
bool MPCStrong<PK_Base>::AdvanceStep(double t_old, double t_new) {
  // take a bdf timestep
  double dt_solver;
  bool fail;
  if (true) { // this is here simply to create a context for timer,
              // which stops the clock when it is destroyed at the
              // closing brace.
    fail = time_stepper_->TimeStep(dt_, dt_solver, solution_);
  }

  if (!fail) {
    // commit the step as successful
    time_stepper_->CommitSolution(dt_, solution_);
    // commit_state(dt_, S_);

    // update the timestep size
    if (dt_solver < dt_ && dt_solver >= dt_) {
      // We took a smaller step than we recommended, and it worked fine (not
      // suprisingly).  Likely this was due to constraints from other PKs or
      // vis.  Do not reduce our recommendation.
    } else {
      dt_ = dt_solver;
    }
  } else {
    // take the decreased timestep size
    dt_ = dt_solver;
  }

  return fail;
}


// -----------------------------------------------------------------------------
// Compute the non-linear functional g = g(t,u,udot).
// -----------------------------------------------------------------------------
template<class PK_Base>
void MPCStrong<PK_Base>::Functional(
    double t_old, double t_new, Teuchos::RCP<TreeVector> u_old,
    Teuchos::RCP<TreeVector> u_new, Teuchos::RCP<TreeVector> g) {
  // loop over sub-PKs
  for (unsigned int i = 0; i != sub_pks_.size(); ++i) {
    // pull out the old solution sub-vector
    Teuchos::RCP<TreeVector> pk_u_old(Teuchos::null);
    if (u_old != Teuchos::null) {
      pk_u_old = u_old->SubVector(i);
      if (pk_u_old == Teuchos::null) {
        Errors::Message message("MPC: vector structure does not match PK structure");
        Exceptions::amanzi_throw(message);
      }
    }

    // pull out the new solution sub-vector
    Teuchos::RCP<TreeVector> pk_u_new = u_new->SubVector(i);
    if (pk_u_new == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    // pull out the residual sub-vector
    Teuchos::RCP<TreeVector> pk_g = g->SubVector(i);
    if (pk_g == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    // fill the nonlinear function with each sub-PKs contribution
    sub_pks_[i]->Functional(t_old, t_new, pk_u_old, pk_u_new, pk_g);
  }
}


// -----------------------------------------------------------------------------
// Applies preconditioner to u and returns the result in Pu.
// -----------------------------------------------------------------------------
template<class PK_Base>
void MPCStrong<PK_Base>::ApplyPreconditioner(
    Teuchos::RCP<const TreeVector> u, Teuchos::RCP<TreeVector> Pu) {
  // loop over sub-PKs
  for (unsigned int i = 0; i != sub_pks_.size(); ++i) {
    // pull out the u sub-vector
    Teuchos::RCP<const TreeVector> pk_u = u->SubVector(i);
    if (pk_u == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    // pull out the preconditioned u sub-vector
    Teuchos::RCP<TreeVector> pk_Pu = Pu->SubVector(i);
    if (pk_Pu == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    // Fill the preconditioned u as the block-diagonal product using each sub-PK.
    sub_pks_[i]->ApplyPreconditioner(pk_u, pk_Pu);
  }
}


// -----------------------------------------------------------------------------
// Compute a norm on u-du and returns the result.
// For a Strong MPC, the enorm is just the max of the sub PKs enorms.
// -----------------------------------------------------------------------------
template<class PK_Base>
double MPCStrong<PK_Base>::ErrorNorm(Teuchos::RCP<const TreeVector> u,
                                     Teuchos::RCP<const TreeVector> du) {
  double norm = 0.0;

  // loop over sub-PKs
  for (unsigned int i=0; i!=sub_pks_.size(); ++i) {
    // pull out the u sub-vector
    Teuchos::RCP<const TreeVector> pk_u = u->SubVector(i);
    if (pk_u == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    // pull out the du sub-vector
    Teuchos::RCP<const TreeVector> pk_du = du->SubVector(i);
    if (pk_du == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    // norm is the max of the sub-PK norms
    double tmp_norm = sub_pks_[i]->ErrorNorm(pk_u, pk_du);
    norm = std::max(norm, tmp_norm);
  }
  return norm;
}


// -----------------------------------------------------------------------------
// Update the preconditioner.
// -----------------------------------------------------------------------------
template<class PK_Base>
void MPCStrong<PK_Base>::UpdatePreconditioner(
    double t, Teuchos::RCP<const TreeVector> up, double h) {
  // loop over sub-PKs
  for (unsigned int i = 0; i != sub_pks_.size(); ++i) {
    // pull out the up sub-vector
    Teuchos::RCP<const TreeVector> pk_up = up->SubVector(i);
    if (pk_up == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    // update precons of each of the sub-PKs
    sub_pks_[i]->UpdatePreconditioner(t, pk_up, h);
  }
}


// -----------------------------------------------------------------------------
// Experimental approach -- calling this indicates that the time integration
// scheme is changing the value of the solution in state.
// -----------------------------------------------------------------------------
template<class PK_Base>
void MPCStrong<PK_Base>::ChangedSolution() {
  // loop over sub-PKs
  for (typename MPC_PK<PK_Base>::SubPKList::iterator pk = MPC_PK<PK_Base>::sub_pks_.begin();
      pk != MPC_PK<PK_Base>::sub_pks_.end(); ++pk) {
    (*pk)->ChangedSolution();
  }
}


// -----------------------------------------------------------------------------
// Check admissibility of each sub-pk
// -----------------------------------------------------------------------------
template<class PK_Base>
bool MPCStrong<PK_Base>::IsAdmissible(Teuchos::RCP<const TreeVector> u) {
  // First ensure each PK thinks we are admissible -- this will ensure
  // the residual can at least be evaluated.
  for (unsigned int i = 0; i != sub_pks_.size(); ++i) {
    // pull out the u sub-vector
    Teuchos::RCP<const TreeVector> pk_u = u->SubVector(i);
    if (pk_u == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    if (!sub_pks_[i]->IsAdmissible(pk_u)) {
      return false;
    }
  }
}


// -----------------------------------------------------------------------------
// Modify predictor from each sub pk.
// -----------------------------------------------------------------------------
template<class PK_Base>
bool MPCStrong<PK_Base>::ModifyPredictor(double h, Teuchos::RCP<const TreeVector> u0,
                                         Teuchos::RCP<TreeVector> u) {
  // loop over sub-PKs
  bool modified = false;
  for (unsigned int i = 0; i != sub_pks_.size(); ++i) {
    // pull out the u sub-vector
    Teuchos::RCP<const TreeVector> pk_u0 = u0->SubVector(i);
    Teuchos::RCP<TreeVector> pk_u = u->SubVector(i);
    if (pk_u == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    modified |= sub_pks_[i]->ModifyPredictor(h, pk_u0, pk_u);
  }
  return modified;
}


// -----------------------------------------------------------------------------
// Modify correction from each sub pk.
// -----------------------------------------------------------------------------
template<class PK_Base>
AmanziSolvers::FnBaseDefs::ModifyCorrectionResult
    MPCStrong<PK_Base>::ModifyCorrection(double h, Teuchos::RCP<const TreeVector> res,
                                         Teuchos::RCP<const TreeVector> u,
                                         Teuchos::RCP<TreeVector> du) {
  // loop over sub-PKs
  AmanziSolvers::FnBaseDefs::ModifyCorrectionResult 
      modified = AmanziSolvers::FnBaseDefs::CORRECTION_NOT_MODIFIED;
  for (unsigned int i = 0; i != sub_pks_.size(); ++i) {
    // pull out the u sub-vector
    Teuchos::RCP<const TreeVector> pk_u = u->SubVector(i);
    Teuchos::RCP<const TreeVector> pk_res = res->SubVector(i);
    Teuchos::RCP<TreeVector> pk_du = du->SubVector(i);

    if (pk_u == Teuchos::null || pk_du == Teuchos::null) {
      Errors::Message message("MPC: vector structure does not match PK structure");
      Exceptions::amanzi_throw(message);
    }

    modified = std::max(modified, sub_pks_[i]->ModifyCorrection(h, pk_res, pk_u, pk_du));
  }
  return modified;
}

}  // namespace Amanzi

#endif
