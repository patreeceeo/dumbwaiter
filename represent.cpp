#include <stdlib.h>

#include "represent.hpp"

#include "snmp_debug.hpp"

#include "log.hpp"

   size_t
RepresentAsInternalIpv4A
   (
   char *pout,
   size_t lout,
   const char *pin,
   size_t lin
   )
   {
   size_t iin = 0;
   size_t iout = 0;
   TBoolean includeNull = TRUE;
   iout += snprintf(pout, lout - iout, "%d", pin[iin]);
   SNMP_LOG_INFO("First byte = %d", pin[0]);
   for(iin = 1; iin < lin && iin < 4 && iout < lout; iin++)
      {
      size_t l = snprintf(pout + iout, lout - iout, ".%d", pin[iin]);
      if(l + iout < lout)
         iout += l;
      else
         {
         includeNull = FALSE;
         break;
         }
      }
   SNMP_LOG_INFO("Address = %s", pout);
   return iout + (includeNull? 1 : 0); // to include null
   }

   size_t
RepresentAsSnmpIpv4A
   (
   char *pout,
   size_t lout,
   const char *pin,
   size_t lin
   )
   {
   size_t iin;
   size_t iout = 0;
   for(iin = 0; iin < lin && iout < lout; iin++)
      {
      if(pin[iin] == '.')
         {
         iout++;
         }
      else if(iin == 0 || pin[iin - 1] == '.')
         {
         pout[iout] = (char)strtol(pin + iin, NULL, 10);
         }
      }
   return iout + 1;
   }

   long
RepresentInteger
   (
   long i,
   msg::TResourceType t,
   msg::TAgentKey a
   )
   {
   if(msg::getAgentKey("/snmp") == a)
      {
      switch(t)
         {
         case msg::BOOLEAN:
            //MESSAGING_LOG_INFO("Its a boolean!");
            if(i)
               return 1;
            else
               return 2;
         case msg::OBJECT_ID:
            //MESSAGING_LOG_INFO("Its an object ID! i = %ld", i);
            return i + 1;
         default:;
         }
      }
   else if(msg::getAgentKey("/mux_manager") == a)
      {
      switch(t)
         {
         case msg::BOOLEAN:
            if(i == 1)
               return TRUE;
            else
               return FALSE;
         case msg::OBJECT_ID:
            return i - 1;
         default:;
         }
      }
   return i;
   }

   size_t
RepresentValue
   (
   char *pout,
   size_t lout,
   const char *pin,
   size_t lin,
   msg::TResourceType t,
   msg::TAgentKey a
   )
   {
   size_t len;
   if(msg::getAgentKey("/snmp") == a)
      {
      if(t == msg::IPADDRESS)
         {
         len = RepresentAsSnmpIpv4A(pout, lout, pin, lin);
         return len + 1; // to include null
         }
      }
   else if(msg::getAgentKey("/mux_manager") == a)
      {
      if(t == msg::IPADDRESS)
         {
         len = RepresentAsInternalIpv4A(pout, lout, pin, lin);
         return len;
         }
      }

   len = lin > lout ? lout : lin;
   strncpy(pout, pin, len);
   return len;
   }





