/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and LICENSE.
 *
 * Copyright:     (c) 1997-2022 Lawrence Livermore National Security, LLC
 * Description:   Factory for creating outernode sum transaction objects
 *
 ************************************************************************/
#include "SAMRAI/algs/OuternodeSumTransactionFactory.h"

#include "SAMRAI/pdat/OuternodeData.h"
#include "SAMRAI/algs/OuternodeSumTransaction.h"


namespace SAMRAI {
namespace algs {

/*
 *************************************************************************
 *
 * Default constructor and destructor.
 *
 *************************************************************************
 */

OuternodeSumTransactionFactory::OuternodeSumTransactionFactory()
{
}

OuternodeSumTransactionFactory::~OuternodeSumTransactionFactory()
{
}

/*
 *************************************************************************
 *
 * Allocate outernode sum transaction object.
 *
 *************************************************************************
 */

std::shared_ptr<tbox::Transaction>
OuternodeSumTransactionFactory::allocate(
   const std::shared_ptr<hier::PatchLevel>& dst_level,
   const std::shared_ptr<hier::PatchLevel>& src_level,
   const std::shared_ptr<hier::BoxOverlap>& overlap,
   const hier::Box& dst_node,
   const hier::Box& src_node,
   const xfer::RefineClasses::Data** refine_data,
   int item_id,
   const hier::Box& box,
   bool use_time_interpolation) const
{
   NULL_USE(box);
   NULL_USE(use_time_interpolation);

   TBOX_ASSERT(dst_level);
   TBOX_ASSERT(src_level);
   TBOX_ASSERT(overlap);
   TBOX_ASSERT(dst_node.getLocalId() >= 0);
   TBOX_ASSERT(src_node.getLocalId() >= 0);
   TBOX_ASSERT(refine_data != 0);
   TBOX_ASSERT_OBJDIM_EQUALITY4(*dst_level, *src_level, dst_node, src_node);

   return std::make_shared<OuternodeSumTransaction>(dst_level,
                                                      src_level,
                                                      overlap,
                                                      dst_node,
                                                      src_node,
                                                      refine_data,
                                                      item_id);
}

std::shared_ptr<tbox::Transaction>
OuternodeSumTransactionFactory::allocate(
   const std::shared_ptr<hier::PatchLevel>& dst_level,
   const std::shared_ptr<hier::PatchLevel>& src_level,
   const std::shared_ptr<hier::BoxOverlap>& overlap,
   const hier::Box& dst_node,
   const hier::Box& src_node,
   const xfer::RefineClasses::Data** refine_data,
   int item_id) const
{
   TBOX_ASSERT(dst_level);
   TBOX_ASSERT(src_level);
   TBOX_ASSERT(overlap);
   TBOX_ASSERT(dst_node.getLocalId() >= 0);
   TBOX_ASSERT(src_node.getLocalId() >= 0);
   TBOX_ASSERT(refine_data != 0);
   TBOX_ASSERT_OBJDIM_EQUALITY4(*dst_level, *src_level, dst_node, src_node);

   return allocate(dst_level,
      src_level,
      overlap,
      dst_node,
      src_node,
      refine_data,
      item_id,
      hier::Box(dst_level->getDim()),
      false);
}

/*
 *************************************************************************
 *
 * Initialize (to 0.0) scratch storage for sum transactions.
 *
 *************************************************************************
 */

void OuternodeSumTransactionFactory::preprocessScratchSpace(
   const std::shared_ptr<hier::PatchLevel>& level,
   double fill_time,
   const hier::ComponentSelector& preprocess_vector) const
{
   NULL_USE(fill_time);
   TBOX_ASSERT(level);

   for (hier::PatchLevel::iterator ip(level->begin());
        ip != level->end(); ++ip) {
      const std::shared_ptr<hier::Patch>& patch = *ip;

      const int ncomponents = preprocess_vector.getSize();
      for (int n = 0; n < ncomponents; ++n) {
         if (preprocess_vector.isSet(n)) {
            std::shared_ptr<pdat::OuternodeData<double> > onode_data(
               SAMRAI_SHARED_PTR_CAST<pdat::OuternodeData<double>, hier::PatchData>(
                  patch->getPatchData(n)));
            TBOX_ASSERT(onode_data);
            onode_data->fillAll(0.0);
         }
      }

   }
}

}
}
