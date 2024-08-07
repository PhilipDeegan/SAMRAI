/*************************************************************************
 *
 * This file is part of the SAMRAI distribution.  For full copyright
 * information, see COPYRIGHT and LICENSE.
 *
 * Copyright:     (c) 1997-2024 Lawrence Livermore National Security, LLC
 * Description:   Periodic shift identifier in periodic domain.
 *
 ************************************************************************/
#include "SAMRAI/hier/PeriodicId.h"

#include <iostream>

namespace SAMRAI {
namespace hier {

const PeriodicId PeriodicId::s_invalid_id(-1);
const PeriodicId PeriodicId::s_zero_id(0);


/*
 ******************************************************************************
 ******************************************************************************
 */
std::ostream&
operator << (
   std::ostream& co,
   const PeriodicId& r)
{
   co << r.d_value;
   return co;
}

}
}
