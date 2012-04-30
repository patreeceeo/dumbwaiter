/*
 * log.hpp
 *
 *  Created on: Oct 7, 2010
 *      Author: patrick
 */

#ifndef LOG_HPP_
#define LOG_HPP_

//#ifndef STAND_ALONE
//#include "../base/log_mgr.hpp"
//#include <cstdio>
//#include <cerrno>
//#include <cstring>
//#define MESSAGING_LOG_ERROR(msg, args...) printf("error in %s: " msg"\n", __FUNCTION__, ##args)
//#define MESSAGING_LOG_POSIX_ERROR  printf("%s: %s\n", __FUNCTION__, strerror(errno))
//#define MESSAGING_LOG_INFO(msg, args...)  {printf("info from %s: " msg"\n", __FUNCTION__, ##args); fflush(NULL);}
//#else
#include <cstdio>
#include <cerrno>
#include <cstring>
#define MESSAGING_LOG_ERROR(msg, args...) printf("%s ERROR: " msg"\n", __FUNCTION__, ##args)
#define MESSAGING_LOG_POSIX_ERROR  printf("%s POSIX ERROR: %s\n", __FUNCTION__, strerror(errno))
#define MESSAGING_LOG_INFO(msg, args...)  printf("%s INFO: " msg"\n", __FUNCTION__, ##args)
//#endif
#endif /* LOG_HPP_ */
