/*
 * represent.hpp
 *
 *  Created on: Aug 12, 2011
 *      Author: patrick
 */

#ifndef REPRESENT_HPP_
#define REPRESENT_HPP_

#include "solib/messaging/messaging.hpp"

long  RepresentInteger       (long i, msg::TResourceType t, msg::TAgentKey);
size_t RepresentValue    (char *pout, size_t lout, const char *pin, size_t lin, msg::TResourceType t, msg::TAgentKey);
size_t RepresentAsInternalIpv4A (char *pout, size_t lout, const char *pin, size_t lin);
#endif /* REPRESENT_HPP_ */
