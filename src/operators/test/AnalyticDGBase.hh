/*
  Copyright 2010-201x held jointly by participating institutions.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:
      Konstantin Lipnikov (lipnikov@lanl.gov)
*/

//! <MISSING_ONELINE_DOCSTRING>

#ifndef AMANZI_OPERATOR_ANALYTIC_DG_BASE_HH_
#define AMANZI_OPERATOR_ANALYTIC_DG_BASE_HH_

#include "Teuchos_RCP.hpp"

#include "DG_Modal.hh"
#include "Mesh.hh"
#include "Point.hh"
#include "Polynomial.hh"
#include "VectorPolynomial.hh"
#include "WhetStoneFunction.hh"
#include "Tensor.hh"

class AnalyticDGBase {
 public:
  AnalyticDGBase(Teuchos::RCP<const Amanzi::AmanziMesh::Mesh> mesh, int order,
                 bool advection)
    : mesh_(mesh),
      order_(order),
      advection_(advection),
      d_(mesh_->space_dimension()){};
  ~AnalyticDGBase(){};

  // analytic data in conventional Taylor basis
  // -- diffusion tensor
  virtual Amanzi::WhetStone::Tensor
  Tensor(const Amanzi::AmanziGeometry::Point& p, double t) = 0;

  // -- solution
  virtual void SolutionTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                              Amanzi::WhetStone::Polynomial& coefs) = 0;

  // -- velocity
  virtual void VelocityTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                              Amanzi::WhetStone::VectorPolynomial& v) = 0;

  // -- accumulation
  virtual void
  AccumulationTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                     Amanzi::WhetStone::Polynomial& a) = 0;

  // -- reaction
  virtual void ReactionTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                              Amanzi::WhetStone::Polynomial& r) = 0;

  // -- source term
  virtual void SourceTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                            Amanzi::WhetStone::Polynomial& src) = 0;

  // exact pointwise values
  // -- solution
  double SolutionExact(const Amanzi::AmanziGeometry::Point& p, double t)
  {
    Amanzi::WhetStone::Polynomial coefs;
    SolutionTaylor(p, t, coefs);
    return coefs(0, 0);
  }

  // -- velocity
  virtual Amanzi::AmanziGeometry::Point
  VelocityExact(const Amanzi::AmanziGeometry::Point& p, double t)
  {
    Amanzi::WhetStone::VectorPolynomial v;
    VelocityTaylor(p, t, v);

    Amanzi::AmanziGeometry::Point tmp(d_);
    for (int i = 0; i < d_; ++i) { tmp[i] = v[i](0, 0); }
    return tmp;
  }

  // exact solution inside a region defined by external function inside
  // -- typicall usage is for setting initial guess
  void InitialGuess(const Amanzi::WhetStone::DG_Modal& dg,
                    Epetra_MultiVector& p, double t,
                    bool inside(const Amanzi::AmanziGeometry::Point&) = NULL);

  // error calculations
  void
  ComputeCellError(const Amanzi::WhetStone::DG_Modal& dg, Epetra_MultiVector& p,
                   double t, double& pnorm, double& l2_err, double& inf_err,
                   double& l2_mean, double& inf_mean, double& l2_int);

  void ComputeCellErrorRemap(const Amanzi::WhetStone::DG_Modal& dg,
                             Epetra_MultiVector& p, double t, int p_location,
                             Teuchos::RCP<Amanzi::AmanziMesh::Mesh> mesh1,
                             double& pnorm, double& l2_err, double& inf_err,
                             double& l20_err, double& inf0_err);

  // communications
  void GlobalOp(std::string op, double* val, int n);

 protected:
  Teuchos::RCP<const Amanzi::AmanziMesh::Mesh> mesh_;
  int order_, d_;
  bool advection_;
};


/* ******************************************************************
 * Initial guess in specified region. Default is all domain.
 ****************************************************************** */
inline void
AnalyticDGBase::InitialGuess(const Amanzi::WhetStone::DG_Modal& dg,
                             Epetra_MultiVector& p, double t,
                             bool inside(const Amanzi::AmanziGeometry::Point&))
{
  int ncells = mesh_->num_entities(Amanzi::AmanziMesh::CELL,
                                   Amanzi::AmanziMesh::Parallel_type::ALL);

  for (int c = 0; c < ncells; ++c) {
    const Amanzi::AmanziGeometry::Point& xc = mesh_->cell_centroid(c);
    if (inside != NULL)
      if (!inside(xc)) continue;

    Amanzi::WhetStone::Polynomial coefs;
    const Amanzi::WhetStone::Basis& basis = dg.cell_basis(c);

    SolutionTaylor(xc, t, coefs);
    Amanzi::WhetStone::DenseVector data = coefs.coefs();
    basis.ChangeBasisNaturalToMy(data);

    for (int n = 0; n < data.NumRows(); ++n) p[n][c] = data(n);
  }
}


/* ******************************************************************
 * Error for cell-based fields
 ****************************************************************** */
inline void
AnalyticDGBase::ComputeCellError(const Amanzi::WhetStone::DG_Modal& dg,
                                 Epetra_MultiVector& p, double t, double& pnorm,
                                 double& l2_err, double& inf_err,
                                 double& l2_mean, double& inf_mean,
                                 double& l2_int)
{
  pnorm = 0.0;
  l2_err = l2_mean = l2_int = 0.0;
  inf_err = inf_mean = 0.0;

  Amanzi::WhetStone::NumericalIntegration numi(mesh_);

  int ncells = mesh_->num_entities(Amanzi::AmanziMesh::CELL,
                                   Amanzi::AmanziMesh::Parallel_type::OWNED);
  for (int c = 0; c < ncells; c++) {
    const Amanzi::AmanziGeometry::Point& xc = mesh_->cell_centroid(c);
    double volume = mesh_->cell_volume(c, false);

    int nk = p.NumVectors();
    Amanzi::WhetStone::DenseVector data(nk);
    for (int i = 0; i < nk; ++i) data(i) = p[i][c];

    Amanzi::WhetStone::Polynomial sol, poly(d_, order_, data);
    poly.set_origin(xc);

    // convert analytic solution from natural to my basis
    SolutionTaylor(xc, t, sol);
    data = sol.coefs();

    const Amanzi::WhetStone::Basis& basis = dg.cell_basis(c);
    basis.ChangeBasisNaturalToMy(data);
    for (int i = 0; i < nk; ++i) sol(i) = data(i);

    Amanzi::WhetStone::Polynomial poly_err(poly);
    poly_err -= sol;
    double err = poly_err.NormInf();

    l2_err += err * err * volume;
    inf_err = std::max(inf_err, fabs(err));

    err = poly_err(0);
    l2_mean += err * err * volume;
    inf_mean = std::max(inf_mean, fabs(err));
    // std::cout << c << " Exact: " << sol << "dG:" << poly << "ERRs: " <<
    // l2_err << " " << inf_err << "\n\n";

    pnorm += std::pow(sol(0, 0), 2.0) * volume;

    // integrated error
    data = poly_err.coefs();
    basis.ChangeBasisMyToNatural(data);
    for (int i = 0; i < nk; ++i) poly_err(i) = data(i);

    std::vector<const Amanzi::WhetStone::WhetStoneFunction*> funcs(2);
    funcs[0] = &poly_err;
    funcs[1] = &poly_err;
    l2_int += numi.IntegrateFunctionsTriangulatedCell(c, funcs, 2 * order_);
  }
#ifdef HAVE_MPI
  GlobalOp("sum", &pnorm, 1);
  GlobalOp("sum", &l2_err, 1);
  GlobalOp("sum", &l2_mean, 1);
  GlobalOp("sum", &l2_int, 1);
  GlobalOp("max", &inf_err, 1);
  GlobalOp("max", &inf_mean, 1);
#endif
  pnorm = sqrt(pnorm);
  l2_err = sqrt(l2_err);
  l2_mean = sqrt(l2_mean);
  l2_int = sqrt(l2_int);
}


/* ******************************************************************
 * Error for cell-based fields in original coordinates (p_location=0)
 * or Lagrangian coordinates (p_location=1)
 ****************************************************************** */
inline void
AnalyticDGBase::ComputeCellErrorRemap(
  const Amanzi::WhetStone::DG_Modal& dg, Epetra_MultiVector& p, double t,
  int p_location, Teuchos::RCP<Amanzi::AmanziMesh::Mesh> mesh1, double& pnorm,
  double& l2_err, double& inf_err, double& l20_err, double& inf0_err)
{
  auto& mesh0 = mesh_;

  pnorm = 0.0;
  l2_err = inf_err = 0.0;
  l20_err = inf0_err = 0.0;

  int ncells = mesh_->num_entities(Amanzi::AmanziMesh::CELL,
                                   Amanzi::AmanziMesh::Parallel_type::OWNED);
  for (int c = 0; c < ncells; ++c) {
    const Amanzi::AmanziGeometry::Point& xc = mesh0->cell_centroid(c);
    const Amanzi::AmanziGeometry::Point& yc = mesh1->cell_centroid(c);
    double volume = mesh1->cell_volume(c, false);

    int nk = p.NumVectors();
    Amanzi::WhetStone::DenseVector data(nk);
    Amanzi::WhetStone::Polynomial poly;
    for (int i = 0; i < nk; ++i) data(i) = p[i][c];

    double err;
    if (p_location == 0) {
      const Amanzi::WhetStone::Basis& basis = dg.cell_basis(c);
      poly = basis.CalculatePolynomial(mesh0, c, order_, data);
      err = poly.Value(xc) - SolutionExact(yc, t);
    } else {
      poly = Amanzi::WhetStone::Polynomial(d_, order_, data);
      poly.set_origin(yc);
      err = poly.Value(yc) - SolutionExact(yc, t);
    }

    inf0_err = std::max(inf0_err, fabs(err));
    l20_err += err * err * volume;

    Amanzi::AmanziGeometry::Point v0(d_), v1(d_);
    Amanzi::AmanziMesh::Entity_ID_List nodes;

    mesh0->cell_get_nodes(c, &nodes);
    int nnodes = nodes.size();
    for (int i = 0; i < nnodes; ++i) {
      mesh0->node_get_coordinates(nodes[i], &v0);
      mesh1->node_get_coordinates(nodes[i], &v1);
      if (p_location == 1) v0 = v1;

      double tmp = poly.Value(v0) - SolutionExact(v1, t);
      inf_err = std::max(inf_err, fabs(tmp));
      l2_err += tmp * tmp * volume / nnodes;
    }
  }

#ifdef HAVE_MPI
  GlobalOp("sum", &pnorm, 1);
  GlobalOp("sum", &l2_err, 1);
  GlobalOp("sum", &l20_err, 1);
  GlobalOp("max", &inf_err, 1);
  GlobalOp("max", &inf0_err, 1);
#endif
  pnorm = sqrt(pnorm);
  l2_err = sqrt(l2_err);
  l20_err = sqrt(l20_err);
}


/* ******************************************************************
 * Collective communications.
 ****************************************************************** */
inline void
AnalyticDGBase::GlobalOp(std::string op, double* val, int n)
{
  double* val_tmp = new double[n];
  memcpy(val_tmp,val,n*sizeof(double)); 
  //for (int i = 0; i < n; ++i) val_tmp[i] = val[i];

  if (op == "sum")
    Teuchos::reduceAll(*mesh_->get_comm(),Teuchos::REDUCE_SUM, 
      *val_tmp, Teuchos::outArg(*val));
  else if (op == "max")
    Teuchos::reduceAll(*mesh_->get_comm(),Teuchos::REDUCE_MAX, 
      *val_tmp, Teuchos::outArg(*val));

  delete[] val_tmp;
}

#endif
