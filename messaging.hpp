/*
 * common.hpp
 *
 *  Created on: Oct 6, 2010
 *      Author: patrick
 */

#ifndef MESSAGING_HPP_
#define MESSAGING_HPP_

#include <list>
#include "include/rest.h"
#include "solib/data/data.hpp"
#include "include/aditypes.h"

#define VALID_BITMASK 42

namespace mqueue
   {
   #include <mqueue.h>
   }

namespace msg
   {
   using namespace mqueue;

   typedef size_t TAgentKey;
   typedef size_t TResourceKey;

   extern const TAgentKey NOT_AN_AGENT;
   extern const TResourceKey NOT_A_RESOURCE;

   extern const TResourceKey RESOURCE_AGENT_NAME;
   extern const TResourceKey RESOURCE_AGENT_KEY;
   extern const TResourceKey RESOURCE_RESOURCE_NAME;
   extern const TResourceKey RESOURCE_RESOURCE_KEY;
   extern const TResourceKey NO_MORE_RESOURCES;

   extern const size_t FIELD_HEADER_SIZE;

   /*!
    * for temporary backwards compat.
    */
   typedef enum
      {
      OCTET_STR    = RESOURCE_TYPE__OCTET_STR,
      BOOLEAN      = RESOURCE_TYPE__BOOLEAN,
      INTEGER      = RESOURCE_TYPE__INTEGER,
      UNSIGNED     = RESOURCE_TYPE__UNSIGNED,
      OBJECT_ID    = RESOURCE_TYPE__OBJECT_ID,
      COUNTER64    = RESOURCE_TYPE__COUNTER64,
      COUNTER      = RESOURCE_TYPE__COUNTER,
      UINTEGER     = RESOURCE_TYPE__UINTEGER,
      IPADDRESS    = RESOURCE_TYPE__IPADDRESS,
      TIMETICKS    = RESOURCE_TYPE__TIMETICKS,
      GAUGE        = RESOURCE_TYPE__GAUGE,
      OPAQUE       = RESOURCE_TYPE__OPAQUE,
      RESOURCE_KEY = RESOURCE_TYPE__RESOURCE_KEY,
      UNKNOWN_TYPE
      } TResourceType;


#define MESSAGE_BODY_MEM_SIZE 8*KB
      /*
       * Following TLV convention
       */
   class TMsg
      {
      private:
         union
            {
            struct
               {
               TRestVerb verb;
               TAgentKey sender;
               TAgentKey recipient;
               size_t bodySize;
               T8 valid;
               // no more data members after _body!
               Tn8 body[MESSAGE_BODY_MEM_SIZE];
               } standard;
//            struct
//               {
//               TGemMsgHdr  hdr;
//               Tn8         attachment[MESSAGE_BODY_MEM_SIZE];
//               } legacy;
               int dummy;
            } _bc;
            TVoid invalidate();
            TVoid dump (size_t arbitraryStart);
      public:
                     TMsg();
                     TMsg(TRestVerb);
         TRestVerb   getVerb();
         TVoid       setVerb(TRestVerb);
         TVoid       setSender(TAgentKey);
         TAgentKey   getSender();
         TVoid       setRecipient(TAgentKey);
         TAgentKey   getRecipient();
         Tnc8       *getBody();
         size_t      getBodySize();
         TVoid       append(TResourceKey resourceKey, size_t length, const TVoid * const value);
         TVoid       appendInteger(TResourceKey rkey, size_t l, Ts32 integer);
         TVoid       appendString(TResourceKey rkey, size_t l, Tnc8 *value);
         size_t      appendFrom(Data::TBase&, Tnc8 *tableName, Tnc8 **colNames);
         TVoid       appendBang();
         Tn8        *reserve(TResourceKey rkey, size_t fieldLength);
         TVoid       constrict(size_t oldFieldLength, size_t fieldLength);
         size_t      extract(TVoid *value, size_t fieldStart);
         Ts64        extractInteger (size_t fieldStart);
         size_t      extractString (Tn8 *str, size_t len, size_t fieldStart);
         size_t      extractInto(Data::TBase&, Tnc8 *tableName, TResourceKey indexKey);
         size_t      getFieldSize(size_t fieldStart);
         Tnc8       *getFieldPointer(size_t fieldStart);
         TResourceKey getResourceKey(size_t field_start);
         TBoolean    isBang(size_t field_start);
         TVoid      *getValue(size_t field_start);
         ssize_t     getNextFieldOffset(size_t field_start);
         TBoolean    isValid();
         TVoid       erase();
      };

   namespace stringify
      {
      Tnc8* getVerb(TRestVerb);
      Tnc8 *getResourceKey(TResourceKey);
      }

   TVoid initialize();

   TVoid initialize(Tnc8 *configpath, TBoolean doRep = FALSE);

   TAgentKey createAgent(Tnc8 *path);

   TAgentKey createAgent(Tnc8 *path, size_t max_msg_count, size_t max_msg_size, TBoolean blocking = TRUE);

   TAgentKey getAgentKey(Tnc8 *path);

   Ts32 destroyAgent(Tnc8* agentName);

   TResourceKey createResource(Tnc8 *name);

   TResourceKey getResourceKey(Tnc8 *name);

   Tnc8* getResourceName(TResourceKey key);

   //receive messages addressed to the argued agent
   TMsg *receive(TAgentKey);

   TMsg *blockingReceive(TAgentKey);

   Ts32 send(TMsg *);

   size_t getReceivedCount(TAgentKey);

   size_t getCachedCount(TAgentKey key);

   TVoid flush(TAgentKey);

   Tnc8 *getPath(TAgentKey);

   TVoid setAttributes(TAgentKey, Ts32 flags);

   TVoid unsetAttributes(TAgentKey, long flags);

   size_t getMaxBodySize(TAgentKey);

   TResourceType getResourceType(TResourceKey);
   }

#endif /* MESSAGING_HPP_ */
