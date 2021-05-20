/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and LICENSE.
 *
 * Copyright:     (c) 1997-2021 Lawrence Livermore National Security, LLC
 * Description:   Conservative linear refine operator for side-centered
 *                double data on a Cartesian mesh.
 *
 ************************************************************************/
#include "SAMRAI/geom/CartesianSideDoubleConservativeLinearRefine.h"
#include "SAMRAI/geom/CartesianPatchGeometry.h"
#include "SAMRAI/hier/Index.h"
#include "SAMRAI/pdat/SideData.h"
#include "SAMRAI/pdat/SideVariable.h"
#include "SAMRAI/pdat/CellData.h"
#include "SAMRAI/tbox/Utilities.h"

#include <cfloat>
#include <cmath>

#define SAMRAI_GEOM_MIN(a, b) (((b) < (a)) ? (b) : (a))

/*
 *************************************************************************
 *
 * External declarations for FORTRAN  routines.
 *
 *************************************************************************
 */

extern "C" {

#ifdef __INTEL_COMPILER
#pragma warning(disable : 1419)
#endif

// in cartrefine1d.f:
void SAMRAI_F77_FUNC(cartclinrefsidedoub1d, CARTCLINREFSIDEDOUB1D)(const int &,
                                                                   const int &,
                                                                   const int &, const int &,
                                                                   const int &, const int &,
                                                                   const int &, const int &,
                                                                   const int *, const double *, const double *,
                                                                   const double *, double *,
                                                                   double *, double *);
// in cartrefine2d.f:
void SAMRAI_F77_FUNC(cartclinrefsidedoub2d0, CARTCLINREFSIDEDOUB2D0)(const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &, const int &,
                                                                     const int &, const int &, const int &, const int &,
                                                                     const int &, const int &, const int &, const int &,
                                                                     const int *, const double *, const double *,
                                                                     const double *, double *,
                                                                     double *, double *, double *, double *);
void SAMRAI_F77_FUNC(cartclinrefsidedoub2d1, CARTCLINREFSIDEDOUB2D1)(const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &, const int &,
                                                                     const int &, const int &, const int &, const int &,
                                                                     const int &, const int &, const int &, const int &,
                                                                     const int *, const double *, const double *,
                                                                     const double *, double *,
                                                                     double *, double *, double *, double *);
// in cartrefine3d.f:
void SAMRAI_F77_FUNC(cartclinrefsidedoub3d0, CARTCLINREFSIDEDOUB3D0)(const int &,
                                                                     const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int *, const double *, const double *,
                                                                     const double *, double *,
                                                                     double *, double *, double *,
                                                                     double *, double *, double *);
void SAMRAI_F77_FUNC(cartclinrefsidedoub3d1, CARTCLINREFSIDEDOUB3D1)(const int &,
                                                                     const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int *, const double *, const double *,
                                                                     const double *, double *,
                                                                     double *, double *, double *,
                                                                     double *, double *, double *);
void SAMRAI_F77_FUNC(cartclinrefsidedoub3d2, CARTCLINREFSIDEDOUB3D2)(const int &,
                                                                     const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int &, const int &, const int &,
                                                                     const int *, const double *, const double *,
                                                                     const double *, double *,
                                                                     double *, double *, double *,
                                                                     double *, double *, double *);
}

namespace SAMRAI
{
namespace geom
{

CartesianSideDoubleConservativeLinearRefine::
    CartesianSideDoubleConservativeLinearRefine() : hier::RefineOperator("CONSERVATIVE_LINEAR_REFINE")
{
}

CartesianSideDoubleConservativeLinearRefine::~CartesianSideDoubleConservativeLinearRefine()
{
}

int CartesianSideDoubleConservativeLinearRefine::getOperatorPriority() const
{
   return 0;
}

hier::IntVector
CartesianSideDoubleConservativeLinearRefine::getStencilWidth(const tbox::Dimension &dim) const
{
   return hier::IntVector::getOne(dim);
}

void CartesianSideDoubleConservativeLinearRefine::refine(
    hier::Patch &fine,
    const hier::Patch &coarse,
    const int dst_component,
    const int src_component,
    const hier::BoxOverlap &fine_overlap,
    const hier::IntVector &ratio) const
{
   const tbox::Dimension &dim(fine.getDim());
   TBOX_ASSERT_DIM_OBJDIM_EQUALITY2(dim, coarse, ratio);

   std::shared_ptr<pdat::SideData<double> > cdata(
       SAMRAI_SHARED_PTR_CAST<pdat::SideData<double>, hier::PatchData>(
           coarse.getPatchData(src_component)));
   std::shared_ptr<pdat::SideData<double> > fdata(
       SAMRAI_SHARED_PTR_CAST<pdat::SideData<double>, hier::PatchData>(
           fine.getPatchData(dst_component)));

   const pdat::SideOverlap *t_overlap =
       CPP_CAST<const pdat::SideOverlap *>(&fine_overlap);

   TBOX_ASSERT(t_overlap != 0);

   TBOX_ASSERT(cdata);
   TBOX_ASSERT(fdata);
   TBOX_ASSERT(cdata->getDepth() == fdata->getDepth());

   const hier::IntVector &directions(fdata->getDirectionVector());

   TBOX_ASSERT(directions ==
               hier::IntVector::min(directions, cdata->getDirectionVector()));

   const hier::Box cgbox(cdata->getGhostBox());

   const hier::Index &cilo = cgbox.lower();
   const hier::Index &cihi = cgbox.upper();
   const hier::Index &filo = fdata->getGhostBox().lower();
   const hier::Index &fihi = fdata->getGhostBox().upper();

   const std::shared_ptr<CartesianPatchGeometry> cgeom(
       SAMRAI_SHARED_PTR_CAST<CartesianPatchGeometry, hier::PatchGeometry>(
           coarse.getPatchGeometry()));
   const std::shared_ptr<CartesianPatchGeometry> fgeom(
       SAMRAI_SHARED_PTR_CAST<CartesianPatchGeometry, hier::PatchGeometry>(
           fine.getPatchGeometry()));

   TBOX_ASSERT(cgeom);
   TBOX_ASSERT(fgeom);


   for (tbox::Dimension::dir_t axis = 0; axis < dim.getValue(); ++axis) {
      const hier::BoxContainer &boxes = t_overlap->getDestinationBoxContainer(axis);

      for (hier::BoxContainer::const_iterator b = boxes.begin();
           b != boxes.end(); ++b) {

         hier::Box fine_box(*b);
         TBOX_ASSERT_DIM_OBJDIM_EQUALITY1(dim, fine_box);

         const auto fine_side_box = pdat::SideGeometry::toSideBox(fine_box, axis);

         fine_box.setUpper(axis, fine_box.upper(axis) - 1);

         const hier::Box coarse_box = hier::Box::coarsen(fine_box, ratio);
         const hier::Index &ifirstc = coarse_box.lower();
         const hier::Index &ilastc = coarse_box.upper();
         const hier::Index &ifirstf = fine_box.lower();
         const hier::Index &ilastf = fine_box.upper();

         const hier::IntVector tmp_ghosts(dim, 0);

         std::vector<double> diff0_f(cgbox.numberCells(0) + 2);
         tbox::AllocatorDatabase *alloc_db = tbox::AllocatorDatabase::getDatabase();
         pdat::SideData<double> slope0_f(cgbox, 1, tmp_ghosts,
                                         directions, alloc_db->getTagAllocator());

#define HOIST_INTERMEDIATES 1
//#undef HOIST_INTERMEDIATES
#if defined(HAVE_RAJA)

#if defined(HOIST_INTERMEDIATES)

         SAMRAI::hier::Box fine_box_plus = fine_box;
         SAMRAI::hier::Box diff_box = coarse_box;
         SAMRAI::hier::Box slope_box = coarse_box;
         if (dim == tbox::Dimension(2)) {
            if ((axis == 0 && directions(0)) || (axis == 1 && directions(1))) {
               // Iteration space is slightly different between the directions
               if (axis == 0 && directions(0)) {
                  fine_box_plus.growUpper(0, 1);
                  diff_box.grow(0, 1);
                  diff_box.growUpper(1, 1);
                  slope_box.growUpper(0, 1);
               } else {
                  fine_box_plus.growUpper(1, 1);
                  diff_box.grow(1, 1);
                  diff_box.growUpper(0, 1);
                  slope_box.growUpper(1, 1);
               }
            }
         } else if (dim == tbox::Dimension(3)) {
            if ((axis == 0 && directions(0)) || (axis == 1 && directions(1)) || (axis == 2 && directions(2))) {
               if (axis == 0 && directions(0)) {
                  fine_box_plus.growUpper(0, 1);
                  diff_box.grow(0, 1);
                  diff_box.growUpper(1, 1);
                  diff_box.growUpper(2, 1);
                  slope_box.growUpper(0, 1);
               } else if (axis == 1 && directions(1)) {
                  fine_box_plus.growUpper(1, 1);
                  diff_box.grow(1, 1);
                  diff_box.growUpper(0, 1);
                  diff_box.growUpper(2, 1);
                  slope_box.growUpper(1, 1);
               } else if (axis == 2 && directions(2)) {
                  fine_box_plus.growUpper(2, 1);
                  diff_box.grow(2, 1);
                  diff_box.growUpper(0, 1);
                  diff_box.growUpper(1, 1);
                  slope_box.growUpper(2, 1);
               }
            }  // test for axis and directions
         }     // end if DIM 3
         pdat::ArrayData<double> diff(diff_box, dim.getValue(), alloc_db->getDevicePool());
         pdat::ArrayData<double> slope(slope_box, dim.getValue(), alloc_db->getDevicePool());
#endif  // Hoisting
#endif  // HAVE_RAJA

         for (int d = 0; d < fdata->getDepth(); ++d) {
            if ((dim == tbox::Dimension(1))) {
               if (directions(axis)) {
                  SAMRAI_F77_FUNC(cartclinrefsidedoub1d, CARTCLINREFSIDEDOUB1D)
                  (
                      ifirstc(0), ilastc(0),
                      ifirstf(0), ilastf(0),
                      cilo(0), cihi(0),
                      filo(0), fihi(0),
                      &ratio[0],
                      cgeom->getDx(),
                      fgeom->getDx(),
                      cdata->getPointer(0, d),
                      fdata->getPointer(0, d),
                      &diff0_f[0], slope0_f.getPointer(0));
               }
            } else if ((dim == tbox::Dimension(2))) {
#if defined(HAVE_RAJA)
               if ((axis == 0 && directions(0)) || (axis == 1 && directions(1))) {
#if !defined(HOIST_INTERMEDIATES)
                  SAMRAI::hier::Box fine_box_plus = fine_box;
                  SAMRAI::hier::Box diff_box = coarse_box;
                  SAMRAI::hier::Box slope_box = coarse_box;
                  // Iteration space is slightly different between the directions
                  if (axis == 0 && directions(0)) {
                     fine_box_plus.growUpper(0, 1);
                     diff_box.grow(0, 1);
                     diff_box.growUpper(1, 1);
                     slope_box.growUpper(0, 1);
                  } else {
                     fine_box_plus.growUpper(1, 1);
                     diff_box.grow(1, 1);
                     diff_box.growUpper(0, 1);
                     slope_box.growUpper(1, 1);
                  }

                  pdat::ArrayData<double> diff(diff_box, dim.getValue(), alloc_db->getDevicePool());
                  pdat::ArrayData<double> slope(slope_box, dim.getValue(), alloc_db->getDevicePool());
#endif
                  auto fine_array = fdata->getView<2>(axis, d);
                  auto coarse_array = cdata->getConstView<2>(axis, d);

                  auto diff0 = diff.getView<2>(0);
                  auto diff1 = diff.getView<2>(1);

                  auto slope0 = slope.getView<2>(0);
                  auto slope1 = slope.getView<2>(1);

                  const double *fdx = fgeom->getDx();
                  const double *cdx = cgeom->getDx();
                  const double fdx0 = fdx[0];
                  const double fdx1 = fdx[1];
                  const double cdx0 = cdx[0];
                  const double cdx1 = cdx[1];

                  const int r0 = ratio[0];
                  const int r1 = ratio[1];
                  if (axis == 0 && directions(0)) {
                     hier::parallel_for_all(diff_box, [=] SAMRAI_HOST_DEVICE(int j /*fast*/, int k /*slow */) {
                        diff0(j, k) = coarse_array(j + 1, k) - coarse_array(j, k);
                        diff1(j, k) = coarse_array(j, k) - coarse_array(j, k - 1);
                     });
                  } else {
                     hier::parallel_for_all(diff_box, [=] SAMRAI_HOST_DEVICE(int j /*fast*/, int k /*slow */) {
                        diff0(j, k) = coarse_array(j, k) - coarse_array(j - 1, k);
                        diff1(j, k) = coarse_array(j, k + 1) - coarse_array(j, k);
                     });
                  }


                  if (axis == 0 && directions(0)) {
                     hier::parallel_for_all(slope_box, [=] SAMRAI_HOST_DEVICE(int j, int k) {
                        const double coef2j = 0.5 * (diff0(j - 1, k) + diff0(j, k));
                        const double boundj = 2.0 * SAMRAI_GEOM_MIN(fabs(diff0(j - 1, k)), fabs(diff0(j, k)));

                        if (diff0(j, k) * diff0(j - 1, k) > 0.0 && cdx0 != 0) {
                           slope0(j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2j), boundj), coef2j) / cdx0;
                        } else {
                           slope0(j, k) = 0.0;
                        }

                        const double coef2k = 0.5 * (diff1(j, k + 1) + diff1(j, k));
                        const double boundk = 2.0 * SAMRAI_GEOM_MIN(fabs(diff1(j, k + 1)), fabs(diff1(j, k)));

                        if (diff1(j, k) * diff1(j, k + 1) > 0.0 && cdx1 != 0) {
                           slope1(j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2k), boundk), coef2k) / cdx1;
                        } else {
                           slope1(j, k) = 0.0;
                        }
                     });
                  } else {
                     hier::parallel_for_all(slope_box, [=] SAMRAI_HOST_DEVICE(int j, int k) {
                        const double coef2j = 0.5 * (diff0(j + 1, k) + diff0(j, k));
                        const double boundj = 2.0 * SAMRAI_GEOM_MIN(fabs(diff0(j + 1, k)), fabs(diff0(j, k)));

                        if (diff0(j, k) * diff0(j - 1, k) > 0.0 && cdx0 != 0) {
                           slope0(j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2j), boundj), coef2j) / cdx0;
                        } else {
                           slope0(j, k) = 0.0;
                        }

                        const double coef2k = 0.5 * (diff1(j, k - 1) + diff1(j, k));
                        const double boundk = 2.0 * SAMRAI_GEOM_MIN(fabs(diff1(j, k - 1)), fabs(diff1(j, k)));

                        if (diff1(j, k) * diff1(j, k - 1) > 0.0 && cdx1 != 0) {
                           slope1(j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2k), boundk), coef2k) / cdx1;
                        } else {
                           slope1(j, k) = 0.0;
                        }
                     });
                  }

                  // we can't use directions i.e IntVector on the device so make it more generic
                  const bool directions0 = directions(0);
                  const bool directions1 = directions(1);
                  hier::parallel_for_all(fine_box_plus, [=] SAMRAI_HOST_DEVICE(int j, int k) {
                     const int ic1 = (k < 0) ? (k + 1) / r1 - 1 : k / r1;
                     const int ic0 = (j < 0) ? (j + 1) / r0 - 1 : j / r0;

                     const int ir0 = j - ic0 * r0;
                     const int ir1 = k - ic1 * r1;
                     double deltax0, deltax1;

                     if (axis == 0 && directions0) {
                        deltax1 = (static_cast<double>(ir1) + 0.5) * fdx1 - cdx1 * 0.5;
                        deltax0 = static_cast<double>(ir0) * fdx0;
                     }

                     if (axis == 1 && directions1) {
                        deltax0 = (static_cast<double>(ir0) + 0.5) * fdx0 - cdx0 * 0.5;
                        deltax1 = static_cast<double>(ir1) * fdx1;
                     }

                     fine_array(j, k) = coarse_array(ic0, ic1) + slope0(ic0, ic1) * deltax0 + slope1(ic0, ic1) * deltax1;
                  });
               }
#else   // Fortran Dimension 2
               std::vector<double> diff1_f(cgbox.numberCells(1) + 2);
               pdat::SideData<double> slope1_f(cgbox, 1, tmp_ghosts,
                                               directions, alloc_db->getTagAllocator());

               if (axis == 0 && directions(0)) {
                  SAMRAI_F77_FUNC(cartclinrefsidedoub2d0, CARTCLINREFSIDEDOUB2D0)
                  (
                      ifirstc(0), ifirstc(1), ilastc(0), ilastc(1),
                      ifirstf(0), ifirstf(1), ilastf(0), ilastf(1),
                      cilo(0), cilo(1), cihi(0), cihi(1),
                      filo(0), filo(1), fihi(0), fihi(1),
                      &ratio[0],
                      cgeom->getDx(),
                      fgeom->getDx(),
                      cdata->getPointer(0, d),
                      fdata->getPointer(0, d),
                      &diff0_f[0], slope0_f.getPointer(0),
                      &diff1_f[0], slope1_f.getPointer(0));
               }
               if (axis == 1 && directions(1)) {
                  SAMRAI_F77_FUNC(cartclinrefsidedoub2d1, CARTCLINREFSIDEDOUB2D1)
                  (
                      ifirstc(0), ifirstc(1), ilastc(0), ilastc(1),
                      ifirstf(0), ifirstf(1), ilastf(0), ilastf(1),
                      cilo(0), cilo(1), cihi(0), cihi(1),
                      filo(0), filo(1), fihi(0), fihi(1),
                      &ratio[0],
                      cgeom->getDx(),
                      fgeom->getDx(),
                      cdata->getPointer(1, d),
                      fdata->getPointer(1, d),
                      &diff1_f[0], slope1_f.getPointer(1),
                      &diff0_f[0], slope0_f.getPointer(1));
               }
#endif  // test for RAJA
            } else if ((dim == tbox::Dimension(3))) {
#if defined(HAVE_RAJA)

               if ((axis == 0 && directions(0)) || (axis == 1 && directions(1)) || (axis == 2 && directions(2))) {
#if !defined(HOIST_INTERMEDIATES)
                  SAMRAI::hier::Box fine_box_plus = fine_box;
                  SAMRAI::hier::Box diff_box = coarse_box;
                  SAMRAI::hier::Box slope_box = coarse_box;
                  if (axis == 0 && directions(0)) {
                     fine_box_plus.growUpper(0, 1);
                     diff_box.grow(0, 1);
                     diff_box.growUpper(1, 1);
                     diff_box.growUpper(2, 1);
                     slope_box.growUpper(0, 1);
                  } else if (axis == 1 && directions(1)) {
                     fine_box_plus.growUpper(1, 1);
                     diff_box.grow(1, 1);
                     diff_box.growUpper(0, 1);
                     diff_box.growUpper(2, 1);
                     slope_box.growUpper(1, 1);
                  } else if (axis == 2 && directions(2)) {
                     fine_box_plus.growUpper(2, 1);
                     diff_box.grow(2, 1);
                     diff_box.growUpper(0, 1);
                     diff_box.growUpper(1, 1);
                     slope_box.growUpper(2, 1);
                  }

                  pdat::ArrayData<double> diff(diff_box, dim.getValue(), alloc_db->getDevicePool());
                  pdat::ArrayData<double> slope(slope_box, dim.getValue(), alloc_db->getDevicePool());

#endif
                  auto fine_array = fdata->getView<3>(axis, d);
                  auto coarse_array = cdata->getConstView<3>(axis, d);

                  auto diff0 = diff.getView<3>(0);
                  auto diff1 = diff.getView<3>(1);
                  auto diff2 = diff.getView<3>(2);

                  auto slope0 = slope.getView<3>(0);
                  auto slope1 = slope.getView<3>(1);
                  auto slope2 = slope.getView<3>(2);

                  const double *fdx = fgeom->getDx();
                  const double *cdx = cgeom->getDx();
                  const double fdx0 = fdx[0];
                  const double fdx1 = fdx[1];
                  const double fdx2 = fdx[2];
                  const double cdx0 = cdx[0];
                  const double cdx1 = cdx[1];
                  const double cdx2 = cdx[2];

                  const int r0 = ratio[0];
                  const int r1 = ratio[1];
                  const int r2 = ratio[2];

                  if (axis == 0 && directions(0)) {
                     hier::parallel_for_all(diff_box, [=] SAMRAI_HOST_DEVICE(int i /*fast*/, int j, int k /*slow */) {
                        diff0(i, j, k) = coarse_array(i + 1, j, k) - coarse_array(i, j, k);
                        diff1(i, j, k) = coarse_array(i, j, k) - coarse_array(i, j - 1, k);
                        diff2(i, j, k) = coarse_array(i, j, k) - coarse_array(i, j, k - 1);
                     });
                  } else if (axis == 1 && directions(1)) {
                     hier::parallel_for_all(diff_box, [=] SAMRAI_HOST_DEVICE(int i /*fast*/, int j, int k /*slow */) {
                        diff0(i, j, k) = coarse_array(i, j, k) - coarse_array(i - 1, j, k);
                        diff1(i, j, k) = coarse_array(i, j + 1, k) - coarse_array(i, j, k);
                        diff2(i, j, k) = coarse_array(i, j, k) - coarse_array(i, j, k - 1);
                     });
                  } else if (axis == 2 && directions(2)) {
                     hier::parallel_for_all(diff_box, [=] SAMRAI_HOST_DEVICE(int i /*fast*/, int j, int k /*slow */) {
                        diff0(i, j, k) = coarse_array(i, j, k) - coarse_array(i - 1, j, k);
                        diff1(i, j, k) = coarse_array(i, j, k) - coarse_array(i, j - 1, k);
                        diff2(i, j, k) = coarse_array(i, j, k + 1) - coarse_array(i, j, k);
                     });
                  }


                  if (axis == 0 && directions(0)) {
                     hier::parallel_for_all(slope_box, [=] SAMRAI_HOST_DEVICE(int i, int j, int k) {
                        const double coef2i = 0.5 * (diff0(i - 1, j, k) + diff0(i, j, k));
                        const double boundi = 2.0 * SAMRAI_GEOM_MIN(fabs(diff0(i - 1, j, k)), fabs(diff0(i, j, k)));

                        if (diff0(i, j, k) * diff0(i - 1, j, k) > 0.0 && cdx0 != 0) {
                           slope0(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2i), boundi), coef2i) / cdx0;
                        } else {
                           slope0(i, j, k) = 0.0;
                        }

                        const double coef2j = 0.5 * (diff1(i, j + 1, k) + diff1(i, j, k));
                        const double boundj = 2.0 * SAMRAI_GEOM_MIN(fabs(diff1(i, j + 1, k)), fabs(diff1(i, j, k)));

                        if (diff1(i, j, k) * diff1(i, j + 1, k) > 0.0 && cdx1 != 0) {
                           slope1(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2j), boundj), coef2j) / cdx1;
                        } else {
                           slope1(i, j, k) = 0.0;
                        }

                        const double coef2k = 0.5 * (diff2(i, j, k + 1) + diff2(i, j, k));
                        const double boundk = 2.0 * SAMRAI_GEOM_MIN(fabs(diff2(i, j, k + 1)), fabs(diff2(i, j, k)));

                        if (diff2(i, j, k) * diff2(i, j, k + 1) > 0.0 && cdx2 != 0) {
                           slope2(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2k), boundk), coef2k) / cdx2;
                        } else {
                           slope2(i, j, k) = 0.0;
                        }
                     });
                  } else if (axis == 1 && directions(1)) {
                     hier::parallel_for_all(slope_box, [=] SAMRAI_HOST_DEVICE(int i, int j, int k) {
                        const double coef2i = 0.5 * (diff0(i + 1, j, k) + diff0(i, j, k));
                        const double boundi = 2.0 * SAMRAI_GEOM_MIN(fabs(diff0(i + 1, j, k)), fabs(diff0(i, j, k)));

                        if (diff0(i, j, k) * diff0(i + 1, j, k) > 0.0 && cdx0 != 0) {
                           slope0(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2i), boundi), coef2i) / cdx0;
                        } else {
                           slope0(i, j, k) = 0.0;
                        }

                        const double coef2j = 0.5 * (diff1(i, j - 1, k) + diff1(i, j, k));
                        const double boundj = 2.0 * SAMRAI_GEOM_MIN(fabs(diff1(i, j - 1, k)), fabs(diff1(i, j, k)));

                        if (diff1(i, j, k) * diff1(i, j - 1, k) > 0.0 && cdx1 != 0) {
                           slope1(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2j), boundj), coef2j) / cdx1;
                        } else {
                           slope1(i, j, k) = 0.0;
                        }

                        const double coef2k = 0.5 * (diff2(i, j, k + 1) + diff2(i, j, k));
                        const double boundk = 2.0 * SAMRAI_GEOM_MIN(fabs(diff2(i, j, k + 1)), fabs(diff2(i, j, k)));

                        if (diff2(i, j, k) * diff2(i, j, k + 1) > 0.0 && cdx2 != 0) {
                           slope2(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2k), boundk), coef2k) / cdx2;
                        } else {
                           slope2(i, j, k) = 0.0;
                        }
                     });
                  } else if (axis == 2 && directions(2)) {
                     hier::parallel_for_all(slope_box, [=] SAMRAI_HOST_DEVICE(int i, int j, int k) {
                        const double coef2i = 0.5 * (diff0(i + 1, j, k) + diff0(i, j, k));
                        const double boundi = 2.0 * SAMRAI_GEOM_MIN(fabs(diff0(i + 1, j, k)), fabs(diff0(i, j, k)));

                        if (diff0(i, j, k) * diff0(i + 1, j, k) > 0.0 && cdx0 != 0) {
                           slope0(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2i), boundi), coef2i) / cdx0;
                        } else {
                           slope0(i, j, k) = 0.0;
                        }

                        const double coef2j = 0.5 * (diff1(i, j + 1, k) + diff1(i, j, k));
                        const double boundj = 2.0 * SAMRAI_GEOM_MIN(fabs(diff1(i, j + 1, k)), fabs(diff1(i, j, k)));

                        if (diff1(i, j, k) * diff1(i, j + 1, k) > 0.0 && cdx1 != 0) {
                           slope1(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2j), boundj), coef2j) / cdx1;
                        } else {
                           slope1(i, j, k) = 0.0;
                        }

                        const double coef2k = 0.5 * (diff2(i, j, k - 1) + diff2(i, j, k));
                        const double boundk = 2.0 * SAMRAI_GEOM_MIN(fabs(diff2(i, j, k - 1)), fabs(diff2(i, j, k)));

                        if (diff2(i, j, k) * diff2(i, j, k - 1) > 0.0 && cdx2 != 0) {
                           slope2(i, j, k) = copysign(SAMRAI_GEOM_MIN(fabs(coef2k), boundk), coef2k) / cdx2;
                        } else {
                           slope2(i, j, k) = 0.0;
                        }
                     });
                  }

                  const bool directions0 = directions(0);
                  const bool directions1 = directions(1);
                  const bool directions2 = directions(2);
                  hier::parallel_for_all(fine_box_plus, [=] SAMRAI_HOST_DEVICE(int i, int j, int k) {
                     const int ic0 = (i < 0) ? (i + 1) / r0 - 1 : i / r0;
                     const int ic1 = (j < 0) ? (j + 1) / r1 - 1 : j / r1;
                     const int ic2 = (k < 0) ? (k + 1) / r2 - 1 : k / r2;

                     const int ir0 = i - ic0 * r0;
                     const int ir1 = j - ic1 * r1;
                     const int ir2 = k - ic2 * r2;
                     double deltax0, deltax1, deltax2;

                     if (axis == 0 && directions0) {
                        deltax0 = static_cast<double>(ir0) * fdx0;
                        deltax1 = (static_cast<double>(ir1) + 0.5) * fdx1 - cdx1 * 0.5;
                        deltax2 = (static_cast<double>(ir2) + 0.5) * fdx2 - cdx2 * 0.5;
                     } else if (axis == 1 && directions1) {
                        deltax0 = (static_cast<double>(ir0) + 0.5) * fdx0 - cdx0 * 0.5;
                        deltax1 = static_cast<double>(ir1) * fdx1;
                        deltax2 = (static_cast<double>(ir2) + 0.5) * fdx2 - cdx2 * 0.5;
                     } else if (axis == 2 && directions2) {
                        deltax0 = (static_cast<double>(ir0) + 0.5) * fdx0 - cdx0 * 0.5;
                        deltax1 = (static_cast<double>(ir1) + 0.5) * fdx1 - cdx1 * 0.5;
                        deltax2 = static_cast<double>(ir2) * fdx2;
                     }

                     fine_array(i, j, k) = coarse_array(ic0, ic1, ic2) + slope0(ic0, ic1, ic2) * deltax0 + slope1(ic0, ic1, ic2) * deltax1 + slope2(ic0, ic1, ic2) * deltax2;
                  });
               }
#else  // Fortran Dimension 3
               std::vector<double> diff1_f(cgbox.numberCells(1) + 2);
               std::vector<double> diff2_f(cgbox.numberCells(2) + 2);

               pdat::SideData<double> slope1_f(cgbox, 1, tmp_ghosts,
                                               directions, alloc_db->getTagAllocator());
               pdat::SideData<double> slope2_f(cgbox, 1, tmp_ghosts,
                                               directions, alloc_db->getTagAllocator());

               if (axis == 0 && directions(0)) {
                  SAMRAI_F77_FUNC(cartclinrefsidedoub3d0, CARTCLINREFSIDEDOUB3D0)
                  (
                      ifirstc(0), ifirstc(1), ifirstc(2),
                      ilastc(0), ilastc(1), ilastc(2),
                      ifirstf(0), ifirstf(1), ifirstf(2),
                      ilastf(0), ilastf(1), ilastf(2),
                      cilo(0), cilo(1), cilo(2),
                      cihi(0), cihi(1), cihi(2),
                      filo(0), filo(1), filo(2),
                      fihi(0), fihi(1), fihi(2),
                      &ratio[0],
                      cgeom->getDx(),
                      fgeom->getDx(),
                      cdata->getPointer(0, d),
                      fdata->getPointer(0, d),
                      &diff0_f[0], slope0_f.getPointer(0),
                      &diff1_f[0], slope1_f.getPointer(0),
                      &diff2_f[0], slope2_f.getPointer(0));
               }
               if (axis == 1 && directions(1)) {
                  SAMRAI_F77_FUNC(cartclinrefsidedoub3d1, CARTCLINREFSIDEDOUB3D1)
                  (
                      ifirstc(0), ifirstc(1), ifirstc(2),
                      ilastc(0), ilastc(1), ilastc(2),
                      ifirstf(0), ifirstf(1), ifirstf(2),
                      ilastf(0), ilastf(1), ilastf(2),
                      cilo(0), cilo(1), cilo(2),
                      cihi(0), cihi(1), cihi(2),
                      filo(0), filo(1), filo(2),
                      fihi(0), fihi(1), fihi(2),
                      &ratio[0],
                      cgeom->getDx(),
                      fgeom->getDx(),
                      cdata->getPointer(1, d),
                      fdata->getPointer(1, d),
                      &diff1_f[0], slope1_f.getPointer(1),
                      &diff2_f[0], slope2_f.getPointer(1),
                      &diff0_f[0], slope0_f.getPointer(1));
               }
               if (axis == 2 && directions(2)) {
                  SAMRAI_F77_FUNC(cartclinrefsidedoub3d2, CARTCLINREFSIDEDOUB3D2)
                  (
                      ifirstc(0), ifirstc(1), ifirstc(2),
                      ilastc(0), ilastc(1), ilastc(2),
                      ifirstf(0), ifirstf(1), ifirstf(2),
                      ilastf(0), ilastf(1), ilastf(2),
                      cilo(0), cilo(1), cilo(2),
                      cihi(0), cihi(1), cihi(2),
                      filo(0), filo(1), filo(2),
                      fihi(0), fihi(1), fihi(2),
                      &ratio[0],
                      cgeom->getDx(),
                      fgeom->getDx(),
                      cdata->getPointer(2, d),
                      fdata->getPointer(2, d),
                      &diff2_f[0], slope2_f.getPointer(2),
                      &diff0_f[0], slope0_f.getPointer(2),
                      &diff1_f[0], slope1_f.getPointer(2));
               }
#endif
            } else {
               TBOX_ERROR(
                   "CartesianSideDoubleConservativeLinearRefine error...\n"
                   << "dim > 3 not supported." << std::endl);
            }
         }  // depth
      }     // boxes
   }        // axis
}  // procedure

}  // namespace geom
}  // namespace SAMRAI
