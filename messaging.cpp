/*!
 * @namespace msg
 * Abstraction layer for message passing. The public interface attempts to be
 * agnostic to the OS mechanism thats actually being used. This layer also
 * introduces a REST-inspired messaging architecture. Can be used for both
 * inter-process and inter-thread communication.
 *
 * This abstraction layer is also written to allow the underlying library
 * (POSIX message queues) to scale or be replaced without requiring
 * significant re-writting while still completely abstracting this under-
 * lying library from the application.
 *
 * Rationale:
 *
 * Q: Why use a new message structure?
 * A: The new message structure breaks information down into its logically
 * separate pieces. The action being requested is separate from the object
 * the action is being performed on, the identity of the agents (entities,
 * processes etc) that are communicating are yet two more distinct pieces.
 * Breaking information down this way allows for simpler message handling
 * code by broadening the class of messages that can be recognized by
 * looking at one piece. For example, if you want to ignore all "delete"
 * messages:
 *    if verb(message) = delete
 *       do nothing
 *    else ...
 * The new message structure also facilitates forward-compatibility by making
 * it possible for a legacy receiver to understand parts of the message and
 * take some appropriate action. For example, a legacy receiver can at least
 * extract the sender's ID from a message an send a reply indicating a lack
 * of comprehension.
 *
 * Q: Why use key-length-value tuples in the message body?
 * A: This is mainly for forward-compatibility. If the receiver doesn't
 * understand one of the keys, it can skip directly to the next key using
 * the associated length field.
 *
 * Q: Why use universally unique identifiers that are created at run-time?
 * A: Both message queues (agents) and their resources are identified by
 * a unique integer value called an agent key and a resource key respectively.
 * One advantage of the identifiers being created at run-time is it frees the
 * programmer of the burden of maintaining a long enumerations without
 * sacrificing source-code literacy. The identifiers are computed by this
 * module when it initializes by reading a configuration file containing the
 * list of recognized names. Another advantage is that when a new agent or
 * resource is added, all of the relevant applications do not need to be
 * recompiled. Since enumerations are compiled into a binary, every time one is
 * updated in the source each binary that uses it needs to be recompiled. Since
 * these identifiers are not enumerations but are computed at run-time, they
 * do not have this disadvantage. The most obvious advantage is that it ensures
 * that everyone is speaking the same language, that when an encoder is
 * referring to a video stream in a message, everyone who receives that message
 * knows that he is referring to a video stream.
 *
 * Note: If you want to change the 'non-settable' attributes of an existing
 * message queue you must either restart the system or mount the mqueue fs
 * and delete the queue, i.e.
 *
mkdir /dev/mqueue
mount -t mqueue none /dev/mqueue
rm /dev/mqueue/q_name
 *
 *
 * Todo write example use-cases or flow chart
 */


#include <string>
#include <map>
#include <cmath>

#include "solib/base/file.hpp"
#include "solib/hash/hash.hpp"

#include "messaging.hpp"
#include "represent.hpp"
#include "log.hpp"

#define CONFIG_FILE "./names.conf"

#define _sender _bc.standard.sender
#define _verb _bc.standard.verb
#define _recipient _bc.standard.recipient
#define _bodySize _bc.standard.bodySize
#define _valid _bc.standard.valid
#define _body _bc.standard.body
#define _hdr _bc.legacy.header
#define _attachment _bc.legacy.attachment

static TBoolean initialized = FALSE;

#define L_FIELD_MAX 256

namespace msg
   {
   const TAgentKey NOT_AN_AGENT = hash::getMaxKey();
   const TResourceKey NOT_A_RESOURCE = hash::getMaxKey();

   const TResourceKey RESOURCE_AGENT_NAME =     hash::getMaxKey()+1;
   const TResourceKey RESOURCE_AGENT_KEY =      hash::getMaxKey()+2;
   const TResourceKey RESOURCE_RESOURCE_NAME =  hash::getMaxKey()+3;
   const TResourceKey RESOURCE_RESOURCE_KEY =   hash::getMaxKey()+4;
   const TResourceKey RESOURCE_BANG =           hash::getMaxKey()+5;
   const TResourceKey NO_MORE_RESOURCES =       hash::getMaxKey()+100;


   const size_t FIELD_HEADER_SIZE = 8;

   std::map<TAgentKey, std::list<TMsg> > received_cache;
   std::map<TAgentKey, mq_attr> attribute_cache;
   std::map<TAgentKey, Ts32> special_flags;
   std::map<TAgentKey, std::string> agent_names;
   std::map<TAgentKey, mqd_t> descriptor_cache;
   std::map<string, TResourceKey> resource_key_cache;
   std::map<string, TAgentKey> agent_key_cache;

   std::map<TResourceKey, std::string> resource_names;
   std::map<TResourceKey, TResourceType> resource_types;

   TBoolean g_doRep = FALSE;

#ifdef SOLIPSISM
   int solip_imqd = 0;
#endif

      TResourceType
   parseResourceType
      (
      Tnc8 *string
      )
      {
      if(string != NULL)
         {
         if(strcasecmp(string, "OCTET_STR") == 0)
            return OCTET_STR;
         if(strcasecmp(string, "BOOLEAN") == 0)
            return BOOLEAN;
         if(strcasecmp(string, "INTEGER") == 0)
            return INTEGER;
         if(strcasecmp(string, "UNSIGNED") == 0)
            return UNSIGNED;
         if(strcasecmp(string, "OBJECT_ID") == 0)
            return OBJECT_ID;
         if(strcasecmp(string, "COUNTER64") == 0)
            return COUNTER64;
         if(strcasecmp(string, "COUNTER") == 0)
            return COUNTER;
         if(strcasecmp(string, "UINTEGER") == 0)
            return UINTEGER;
         if(strcasecmp(string, "IPADDRESS") == 0)
            return IPADDRESS;
         if(strcasecmp(string, "TIMETICKS") == 0)
            return TIMETICKS;
         if(strcasecmp(string, "GAUGE") == 0)
            return GAUGE;
         if(strcasecmp(string, "OPAQUE") == 0)
            return OPAQUE;
         if(strcasecmp(string, "RESOURCE_KEY") == 0)
            return RESOURCE_KEY;
         }
      MESSAGING_LOG_ERROR("Unrecognized: %s", string);
      return OPAQUE;
      }

   namespace stringify
      {
         Tnc8*
      getVerb
         (
         TRestVerb v
         )
         {
         switch(v)
            {
            case REST_GET:
               return "get";
            case REST_SET:
               return "set";
            case REST_CREATE:
               return "create";
            case REST_DELETE:
               return "delete";
            case REST_ACK:
               return "acknowledge";
            default:;
            }
         return "???";
         }

      /*
       * Deprecated. Use msg::getResourceName() instead.
       */
         Tnc8*
      getResourceKey
         (
         TResourceKey key
         )
         {
         return resource_names[key].c_str();
         }
      }

   TMsg::TMsg()
      {
      _sender = NOT_AN_AGENT;
      _recipient = NOT_AN_AGENT;
      _bodySize = 0;
      _valid = VALID_BITMASK;
      }

   TMsg::TMsg
      (
      TRestVerb verb
      )
      {
      _verb = verb;
      _sender = NOT_AN_AGENT;
      _recipient = NOT_AN_AGENT;
      _bodySize = 0;
      _valid = VALID_BITMASK;
      }

      TRestVerb
   TMsg::getVerb()
      {
      return _verb;
      }

      TVoid
   TMsg::setVerb
      (
      TRestVerb v
      )
      {
      _verb = v;
      }

      TVoid
   TMsg::setSender
      (
      TAgentKey sender
      )
      {
      _sender = sender;
      }

      TAgentKey
   TMsg::getSender()
      {
      return _sender;
      }

      TVoid
   TMsg::setRecipient
      (
      TAgentKey recipient
      )
      {
//      MESSAGING_LOG_INFO("%s => %u", agent_names[recipient].c_str(), recipient);
      _recipient = recipient;
      }

      TAgentKey
   TMsg::getRecipient()
      {
      return _recipient;
      }

      Tnc8*
   TMsg::getBody()
      {
      return _body;
      }

      size_t
   TMsg::getBodySize()
      {
      return _bodySize;
      }

      TVoid
   TMsg::append
      (
      TResourceKey resourceKey,
      size_t length,
      const TVoid * const value
      )
      {
      if(length > L_FIELD_MAX)
         {
         MESSAGING_LOG_ERROR("Field length (%u bytes) exceeds maximum limit of %d bytes!", length, L_FIELD_MAX);
         return;
         }

      size_t originalBodySize = _bodySize;

      // append key
      memcpy(_body+_bodySize, &resourceKey, sizeof(resourceKey));
      _bodySize+=sizeof(resourceKey);

      // append length

      memcpy(_body+_bodySize, &length, sizeof(length));
      _bodySize+=sizeof(length);

      // append value
      memcpy(_body+_bodySize, value, length);
      _bodySize += length;
      if(_bodySize > getMaxBodySize(_recipient))
          {
          MESSAGING_LOG_ERROR("Message size (%u bytes) exceeds maximum limit of %ld bytes! Truncating and invalidating.", _bodySize, msg::attribute_cache[_recipient].mq_msgsize);
          _bodySize = originalBodySize;
          invalidate();
          }
      }

      TVoid
   TMsg::appendInteger
      (
      TResourceKey rkey,
      size_t length,
      Ts32 integer
      )
      {
      TResourceType rtype = INTEGER;

      if(g_doRep)
         rtype = getResourceType(rkey);

      integer = RepresentInteger(integer, rtype, getRecipient());
      append(rkey, length, &integer);
      }

      TVoid
   TMsg::appendString
      (
      TResourceKey rkey,
      size_t length,
      Tnc8 *value
      )
      {
      TResourceType rtype = OPAQUE;
      Tn8 newValue[L_FIELD_MAX];
      size_t lengthOriginal = length;
      Tnc8 *valueOriginal = value;

      if(g_doRep)
         {
         rtype = getResourceType(rkey);
         length = RepresentValue(newValue, L_FIELD_MAX, valueOriginal, lengthOriginal, rtype, getRecipient());
         value = newValue;
         }

      append(rkey, length, value);
      }

      size_t
   TMsg::appendFrom
      (
      Data::TBase &db,
      Tnc8 *tableName,
      Tnc8 **colNames // Null-delimited
      )
      {
      size_t dbIndex;
      for(
         dbIndex = db.begin(tableName);
         dbIndex < db.end(tableName);
         dbIndex = db.next(tableName, dbIndex)
         )
         {
         size_t iCol = 0;
         while(colNames && colNames[iCol])
            {
            setVerb((TRestVerb)Data::GetInteger(db, tableName, "verb", dbIndex));
            if(Data::HasString(db, tableName, colNames[iCol], dbIndex))
               {
               Tnc8 *s = Data::GetString(db, tableName, colNames[iCol], dbIndex);
               appendString(msg::getResourceKey(colNames[iCol]), strlen(s), s);
               }
            else if(Data::HasInteger(db, tableName, colNames[iCol], dbIndex))
               {
               Ts32 integer = (Ts32)Data::GetInteger(db, tableName, colNames[iCol], dbIndex);
               //MESSAGING_LOG_INFO("%s:%s(%u).%u = %ld", tableName, colNames[iCol], msg::getResourceKey(colNames[iCol]), dbIndex, integer);
               appendInteger(msg::getResourceKey(colNames[iCol]), 32 / 8, integer);
               }
            iCol++;
            }
         }
      return getBodySize();
      }

      TVoid
   TMsg::appendBang()
      {
      append(RESOURCE_BANG, 0, NULL);
      }

      Tn8*
   TMsg::reserve
      (
      TResourceKey rkey,
      size_t fieldLength
      )
      {
      size_t newBodySize = _bodySize + sizeof(rkey) + sizeof(fieldLength) + fieldLength;
      if(_bodySize + sizeof(rkey) + sizeof(fieldLength) + fieldLength >=  getMaxBodySize(_recipient))
         {
         MESSAGING_LOG_ERROR("New payload size (%u bytes) exceeds maximum limit of %ld bytes! Truncating.", newBodySize, msg::attribute_cache[_recipient].mq_msgsize);
         return NULL;
         }

      memcpy(_body + _bodySize, &rkey, sizeof(rkey));
      _bodySize += sizeof(rkey);

      memcpy(_body + _bodySize, &fieldLength, sizeof(fieldLength));
      _bodySize += sizeof(fieldLength);

      size_t reservationStart = _bodySize;
      _bodySize += fieldLength;
      return _body + reservationStart;
      }

      TVoid
   TMsg::constrict
      (
      size_t oldFieldLength,
      size_t fieldLength
      )
      {
      size_t fieldStart = _bodySize - oldFieldLength;
      size_t fieldIndex = sizeof(TResourceKey);
      size_t sizeOfValue;
      memcpy(&sizeOfValue, _body + fieldStart + fieldIndex, sizeof(sizeOfValue));
      if(sizeOfValue != oldFieldLength)
         {
         MESSAGING_LOG_ERROR("looking for field of length %u bytes, found a field of length %u bytes.", oldFieldLength, sizeOfValue);
         return;
         }

      memcpy(_body + fieldStart + fieldIndex, &fieldLength, sizeof(fieldLength));
      }

   /*!
    * Opposite of append
    * @param field_start The byte offset of the start of the field you wish to extract.
    * @return the offset of the start of the next field from field_start.
    */
      size_t
   TMsg::extract
      (
      TVoid *value,
      size_t fieldStart
      )
      {
      size_t keySize = sizeof(TResourceKey);
      size_t fieldIndex = keySize;
      size_t sizeOfValue = 0;
      memcpy(&sizeOfValue, _body + fieldStart + fieldIndex, sizeof(sizeOfValue));
      fieldIndex += sizeof(sizeOfValue);

      memcpy(value, _body + fieldStart + fieldIndex, sizeOfValue);
      fieldIndex += sizeOfValue;
      return keySize + sizeof(sizeOfValue) + sizeOfValue;
      }

      Ts64
   TMsg::extractInteger
      (
      size_t fieldStart
      )
      {
      Ts32 integer = 0;
      TResourceType rtype = INTEGER;
      extract(&integer, fieldStart);

      if(g_doRep)
         rtype = getResourceType(getResourceKey(fieldStart));

      integer = RepresentInteger(integer, rtype, getRecipient());
      return integer;
      }

      size_t
   TMsg::extractString
      (
      Tn8 *str,
      size_t len,
      size_t fieldStart
      )
      {
      TResourceType rtype = OPAQUE;
      if(g_doRep)
         rtype = getResourceType(getResourceKey(fieldStart));

      return RepresentValue(
         str,
         len,
         getFieldPointer(fieldStart),
         getFieldSize(fieldStart),
         rtype,
         getRecipient());
      }


   /*!
    * This shouldn't be in messaging, this is resource specific.
    */
#include <ctype.h>
      TVoid
   ComputeTableName
      (
      Tn8 *tableName,
      Tnc8 *resourceName
      )
      {
      //MESSAGING_LOG_INFO("strlen(resourceName) = %u", strlen(resourceName));
      if(strlen(resourceName) > 0)
         for(size_t i = strlen(resourceName) - 1; i >= 0; i--)
            if(isupper(resourceName[i]))
               {
               strncpy(tableName, resourceName, i);
               tableName[i] = '\0';
               strcat(tableName, "Table");
               break;
               }
      }

      static TResourceKey
   ComputeIndexKey
      (
      Tnc8 *tableName
      )
      {
      Tn8 indexName[64];
      strcpy(indexName, tableName);
      strcat(indexName, "Index");
      return getResourceKey(indexName);
      }

      TVoid
   TMsg::dump
      (
      size_t fieldStart
      )
      {
      const size_t maxChunk = 64;
      Tn8 buffer[64 + maxChunk * 4 + (maxChunk / 8) + 1];
      Tn8 *pBuf = buffer;
      size_t fieldIndex = 0;

      TResourceKey rkey = 0;
      memcpy(&rkey, _body + fieldStart, sizeof(TResourceKey));
      pBuf += sprintf(pBuf, "rkey: \"%s\" (%u)\n", msg::getResourceName(rkey), rkey);
      fieldIndex += sizeof(TResourceKey);

      size_t sizeOfValue = 0;
      memcpy(&sizeOfValue, _body + fieldStart + fieldIndex, sizeof(sizeOfValue));
      pBuf += sprintf(pBuf, "vsize: %u\n", sizeOfValue);
      fieldIndex += sizeof(sizeOfValue);

      size_t iBody;
      size_t iChunk;
      for(iBody = fieldIndex, iChunk = 0; iBody < getBodySize() && iChunk < maxChunk && iChunk < getFieldSize(iBody); iBody++, iChunk++)
         {
         Tn8 d = _body[iBody];
         pBuf += sprintf(pBuf, "%3d ", d);
         if(iChunk && iChunk % 8 == 0)
            pBuf += sprintf(pBuf, "\n");
         }
      if(iChunk < maxChunk && iChunk < getFieldSize(iBody))
         sprintf(pBuf, "end");
      MESSAGING_LOG_INFO("\n%s", buffer);
      }

      size_t
   TMsg::extractInto
      (
      Data::TBase& db,
      Tnc8 *tableName,
      TResourceKey indexKey
      )
      {
      size_t iBody = 0;
      TResourceKey rkey = getResourceKey(iBody);
      size_t currentIdx = 1;
      Data::SetInteger(db, "indexeRKs", tableName, 0, indexKey);

      for(; iBody < getBodySize(); iBody = getNextFieldOffset(iBody))
         {
         for(;
             !isBang(iBody) && iBody < getBodySize();
             iBody = getNextFieldOffset(iBody) // jump to next field
            )
            {
            MESSAGING_LOG_INFO("iBody = %u", iBody);
            rkey = getResourceKey(iBody);
            //MESSAGING_LOG_INFO("1 iBody = %u, fieldSize = %u", iBody, getFieldSize(iBody));
            switch(getResourceType(rkey))
               {
               case UNKNOWN_TYPE:
                  {
                  MESSAGING_LOG_ERROR("Found a resource of unknown type. Dumping message contents.");
                  dump(iBody);
                  }
                  break;
               case OCTET_STR:
               case OPAQUE:
               case IPADDRESS:
                  {
                  Tn8 buffer[L_FIELD_MAX+1];
                  //MESSAGING_LOG_INFO("2 iBody = %u, fieldSize = %u", iBody, getFieldSize(iBody));
                  size_t len = extractString(buffer, L_FIELD_MAX, iBody);
                  buffer[len] = '\0';
                  MESSAGING_LOG_INFO("%s %s %u = \"%s\", length = %u", tableName, getResourceName(rkey), currentIdx, buffer, len);
                  Data::SetString(db, tableName, getResourceName(rkey), currentIdx, buffer, len);
                  dump(iBody);
                  }
                  break;
               case OBJECT_ID:
                  {
                  if(rkey == indexKey)
                     {
                     currentIdx = extractInteger(iBody);
                     MESSAGING_LOG_INFO("%s %s = %u", tableName, getResourceName(rkey), currentIdx);
                     Data::SetInteger(db, tableName, getResourceName(rkey), currentIdx, currentIdx);
                     break;
                     }
                  //MESSAGING_LOG_INFO("OBJECT_ID %s:%s = %u", tableName, getResourceName(rkey), currentIdx);
                  }
               default:
                  {
                  Ts32 integer = extractInteger(iBody);
                  //MESSAGING_LOG_INFO("3 iBody = %u, fieldSize = %u", iBody, getFieldSize(iBody));
                  MESSAGING_LOG_INFO("%s %s %u = %ld", tableName, getResourceName(rkey), currentIdx, integer);
                  Data::SetInteger(db, tableName, getResourceName(rkey), currentIdx, integer);
                  dump(iBody);
                  }
               }
            }
         //MESSAGING_LOG_INFO("%s verb %u = %s", tableName, currentIdx, RestVerbToString(getVerb()));

         Data::SetInteger(db, tableName, "verb", currentIdx, getVerb());
         }
      return iBody;
      }


      size_t
   TMsg::getFieldSize
      (
      size_t fieldStart
      )
      {
      size_t keySize = sizeof(TResourceKey);
      size_t fieldIndex = keySize;
      size_t sizeOfField;
      memcpy(&sizeOfField, _body + fieldStart + fieldIndex, sizeof(sizeOfField));
      return sizeOfField;
      }

      Tnc8*
   TMsg::getFieldPointer
      (
      size_t fieldStart
      )
      {
      size_t keySize = sizeof(TResourceKey);
      size_t fieldIndex = keySize + sizeof(size_t);
      if(fieldStart + fieldIndex < MESSAGE_BODY_MEM_SIZE)
         return _body + fieldStart + fieldIndex;
      else
         return NULL;
      }


   /*!
    * Get the offset from the beginning of the message to the start of the next field.
    * @param fieldStart The byte address of the start of any field relative to the start of the message body.
    *    The first field is always at fieldStart = 0
    * @return The aforementioned offset or _bodySize if there are no fields after fieldStart.
    */
      ssize_t
   TMsg::getNextFieldOffset
      (
      size_t fieldStart
      )
      {
      size_t offset = 0;
      size_t valueSize = 0;
      if(fieldStart + offset >= _bodySize)
         return _bodySize;
      offset += sizeof(TResourceKey);
      if(fieldStart + offset >= _bodySize)
         return _bodySize;
      memcpy(&valueSize, _body+fieldStart+offset, sizeof(size_t));
      offset+=sizeof(size_t);
      offset += valueSize;

      return fieldStart + offset;
      }


   /*!
    * Get the key that corresponds to the MIB table column from whence this field's data was sourced.
    * @param msg The message containing the field
    * @param field_start The byte offset of the start of the field from the beginning of msg's body.
    * @return The key
    */
      TResourceKey
   TMsg::getResourceKey(size_t fieldStart)
      {
      TResourceKey key = NOT_A_RESOURCE;
      if(fieldStart + sizeof(TResourceKey) > _bodySize)
         return NO_MORE_RESOURCES;
      memcpy(&key, _body+fieldStart, sizeof(key));
      if(key > NO_MORE_RESOURCES)
         key = NOT_A_RESOURCE;
      return key;
      }

      TBoolean
   TMsg::isBang(size_t fieldStart)
      {
      return getResourceKey(fieldStart) == RESOURCE_BANG;
      }

      TBoolean
   TMsg::isValid()
      {
      return _valid == VALID_BITMASK;
      }

      TVoid
   TMsg::invalidate()
      {
      _valid = !VALID_BITMASK;
      }

      TVoid
   TMsg::erase()
      {
      _bodySize = 0;
      }

   /*!
    * Create a one-to-one mapping of keys to names.
    * The current approach is to use hashing.
    */
      static TAgentKey
   computeAgentKey
      (
      Tnc8 *name
      )
      {
      TAgentKey agentKey = (TAgentKey)hash::compute(name, ' ', 'z');
      if(agent_names.count(agentKey) == 0)
         {
         //MESSAGING_LOG_ERROR("The name \"%s\" does not name an agent. Forget to call msg::initialize?", name);
         agentKey = NOT_AN_AGENT;
         }
      else while(agent_names.count(agentKey) > 0 &&
            agent_names[agentKey].compare(name) != 0) // chain if there's a collision
         {
         (size_t)agentKey++;
         }

      //MESSAGING_LOG_INFO("%s => %u", name, agentKey);

      return agentKey;
      }

   /*!
    * Create a one-to-one mapping of keys to names.
    * The current approach is to use hashing.
    */
      static TResourceKey
   computeResourceKey
      (
      Tnc8 *name
      )
      {
      //The key we will 'compute'
      TResourceKey resourceKey;
      if(strcmp(name, "resourceName") == 0)
         {
         resourceKey = hash::getMaxKey()+1;
         }
      else if(strcmp(name, "noMoreResources") == 0)
         {
         resourceKey = hash::getMaxKey()+2;
         }
      else
         {
         resourceKey = (TAgentKey)hash::compute(name, ' ', 'z');
         if(resource_names.count(resourceKey) == 0)
            {
            //MESSAGING_LOG_ERROR("The name \"%s\" does not name a resource. Forget to call msg::initialize?", name);
            resourceKey = NOT_A_RESOURCE;
            }
         else while(resource_names.count(resourceKey) > 0 &&
               resource_names[resourceKey].compare(name) != 0) // chain if there's a collision
            {
            (size_t)resourceKey++;
            }
         }

      //MESSAGING_LOG_INFO("%s => %u", name, resourceKey);
      return resourceKey;
      }

      static Ts32
   getSettableBits
      (
      Ts32 mq_flags
      )
      {
      return mq_flags && O_NONBLOCK;
      }

      static TVoid
   getMaxAttributes
      (
      mq_attr *attr
      )
      {
      Tn8 buffer[16];
      /* fail-safe maximums */
      attr->mq_maxmsg = 5;
      attr->mq_msgsize = 4096;
      /* user-specified maximums */
      if(SUCCESS != file::getContents("/proc/sys/fs/mqueue/msg_max", buffer, sizeof(buffer)))
         {
         MESSAGING_LOG_ERROR("Failure to read from /proc/sys/fs/mqueue/msg_max");
         }
      else if(EOF == sscanf(buffer, "%lu", &(attr->mq_maxmsg)))
         {
         MESSAGING_LOG_ERROR("Failure to parse msg_max");
         }
      if(SUCCESS != file::getContents("/proc/sys/fs/mqueue/msgsize_max", buffer, sizeof(buffer)))
         {
         MESSAGING_LOG_ERROR("Failure to read from /proc/sys/fs/mqueue/msgsize_max");
         }
      else if(EOF == sscanf(buffer, "%lu", &(attr->mq_msgsize)))
         {
         MESSAGING_LOG_ERROR("Failure to parse msgsize_max");
         }
      }

      TVoid
   initialize()
      {
      initialize(NULL);
      }

      TVoid
   initialize
      (
      Tnc8 *configfile,
      TBoolean doRep
      )
      {
      if(!initialized)
         {
         initialized = TRUE;
         Tn8 line[128];

         g_doRep = doRep;

         if(configfile == NULL)
            {
            configfile = CONFIG_FILE;
            }

         FILE *fp = fopen(configfile, "rb");

         if(fp)
            {
            while(fgets(line, sizeof(line), fp))
               {
               Tnc8 *delim = " \n";

               //MESSAGING_LOG_INFO("line: %s", line);

               /* get name of agent or resource */
               Tn8 *tok = strtok(line, delim);
               if('/' == line[0])
                  {
                  TAgentKey agentKey = (TAgentKey)hash::compute(tok, ' ', 'z');
                  while(agent_names.count(agentKey) > 0 &&
                        agent_names[agentKey].compare(tok) != 0) // chain if there's a collision
                     {
                     (size_t)agentKey++;
                     }
                  //MESSAGING_LOG_INFO("create agent %s", tok);
                  agent_names[agentKey] = tok;
                  }
               else
                  {
                  TResourceKey resourceKey = (TResourceKey)hash::compute(tok, ' ', 'z');
                  while(resource_names.count(resourceKey) > 0 &&
                        resource_names[resourceKey].compare(tok) != 0) // chain if there's a collision
                     {
                     (size_t)resourceKey++;
                     }
                  if(FALSE && strstr(tok, "ipPort"))
                     MESSAGING_LOG_INFO("Created resource \"%s\" <==> %u", tok, resourceKey);
                  resource_names[resourceKey] = tok;
                  /* get ASN type of resource */
                  tok = strtok(NULL, delim);

                  resource_types[resourceKey] = parseResourceType(tok);
                  }


               }
            }
         else
            {
            MESSAGING_LOG_ERROR("Cannot open \"%s\"", configfile);
            }
         }
      }

      TAgentKey
   createAgent
      (
      Tnc8 *path,
      mq_attr *attr
      )
      {
      mqd_t mqd = -1;

      MESSAGING_LOG_INFO("Allocating message queue at '%s'", path);
      MESSAGING_LOG_INFO("Maximum number of messages for '%s': %ld", path, attr->mq_maxmsg);
      MESSAGING_LOG_INFO("Maximum size of messages for '%s': %ld", path, attr->mq_msgsize);

      attr->mq_flags |= O_EXCL | O_RDWR;
#ifndef SOLIPSISM
      mqd = mq_open(path, attr->mq_flags | O_CREAT, S_IRUSR | S_IWUSR, attr);
#else
      mqd = solip_imqd++;
#endif
      if (-1 != mqd)
         {
         MESSAGING_LOG_INFO("Created new message queue");
         goto success;
         }
      else if(EEXIST != errno)
         {
         MESSAGING_LOG_POSIX_ERROR;
         return -1;
         }

#ifndef SOLIPSISM //Note: This code is unreachable with solipsism
      mqd = mq_open(path, attr->mq_flags, S_IRUSR | S_IWUSR, attr);
#endif
      if(-1 != mqd)
         MESSAGING_LOG_INFO("Used existing message queue");
      else
         {
         MESSAGING_LOG_ERROR("Something dreadful happened!");
         MESSAGING_LOG_POSIX_ERROR;
         return -1;
         }

      success:
      TAgentKey key = computeAgentKey(path);
      //MESSAGING_LOG_INFO("key = %u", key);
      msg::attribute_cache[key] = *attr;
      msg::special_flags[key] = 0;
      msg::agent_names[key] = path;
      msg::descriptor_cache[key] = mqd;
      msg::agent_key_cache[path] = key;
      //MESSAGING_LOG_INFO("Success");
      return key;
      }

      TAgentKey
   createAgent
      (
      Tnc8 *path,
      size_t max_msg_count,
      size_t max_msg_size, /* in bytes */
      TBoolean blocking
      )
      {
      mq_attr attr;
      mq_attr max_attr;
      attr.mq_maxmsg = max_msg_count;
      attr.mq_msgsize = max_msg_size;
      attr.mq_flags = 0;
      if(!blocking)
         {
         attr.mq_flags |= O_NONBLOCK;
         }

      if(attr.mq_msgsize > MESSAGE_BODY_MEM_SIZE)
         {
         MESSAGING_LOG_ERROR("Argued body size (%ld) is larger than static memory allocation (%u).", attr.mq_msgsize, MESSAGE_BODY_MEM_SIZE);
         return -1;
         }

      getMaxAttributes(&max_attr);
      if(attr.mq_maxmsg <= max_attr.mq_maxmsg && attr.mq_msgsize <= max_attr.mq_msgsize)
         return createAgent(path, &attr);
      else
         {
         MESSAGING_LOG_ERROR("Argued value(s) larger than soft-maximum");
         return -1;
         }
      }

   /*!
    * Create a new key and associated facility for sending/receiving messages.
    *
    * @return The key
    * @retval -1 Error
    */
      TAgentKey
   createAgent(Tnc8 *path)
      {
      mq_attr attr;

      getMaxAttributes(&attr);
      attr.mq_flags = 0;
      return createAgent(path, &attr);
      }

      TAgentKey
   getAgentKey
      (
      Tnc8 *path
      )
      {
      return agent_key_cache[path];
      }

      Ts32
   destroyAgent(Tnc8 *agentName)
      {
      TAgentKey key = computeAgentKey(agentName);
      if(msg::descriptor_cache.count(key));
         if(mq_close(msg::descriptor_cache[key]) != 0)
            return FAILURE;
      if(mq_unlink(agentName) == 0)
         {
         msg::attribute_cache.erase(msg::attribute_cache.find(key));
         msg::special_flags.erase(msg::special_flags.find(key));
         msg::received_cache.erase(msg::received_cache.find(key));
         msg::agent_names.erase(msg::agent_names.find(key));
         msg::descriptor_cache.erase(msg::descriptor_cache.find(key));
         return SUCCESS;
         }
      return FAILURE;
      }

      TResourceKey
   createResource(Tnc8 *name)
      {
      TResourceKey key = computeResourceKey(name);
      resource_key_cache[name] = key;
      return key;
      }

      TResourceKey
   getResourceKey(Tnc8 *name)
      {
      return msg::resource_key_cache[name];
      }

      Tnc8*
   getResourceName
      (
      TResourceKey key
      )
      {
      return resource_names[key].c_str();
      }

      static TMsg *
   receive
      (
      TAgentKey key,
      timespec* pTimeout
      )
      {
         {
         unsigned int priority;
         TMsg message;
         //MESSAGING_LOG_INFO("Attempting to receive message for %s", msg::agent_names[key].c_str());
#ifndef SOLIPSISM
         Ts32 mq_result;
         if(pTimeout)
            {
            mq_result = mq_timedreceive (
                  msg::descriptor_cache[key],
                  (char*)&message,
                  msg::attribute_cache[key].mq_msgsize,
                  &priority,
                  pTimeout);
            if(-1 == mq_result)
               {
               if(errno != EAGAIN && errno != EINTR && errno != ETIMEDOUT)
                  {
                  MESSAGING_LOG_POSIX_ERROR;
                  }
               return NULL;
               }
            }
         else
            {
            mq_result = mq_receive (
                  msg::descriptor_cache[key],
                  (char*)&message,
                  msg::attribute_cache[key].mq_msgsize,
                  &priority);
            }
//         MESSAGING_LOG_INFO("Receiving message for '%s'", agent_names[key].c_str());
         msg::received_cache[key].push_back(message);
#endif
         MESSAGING_LOG_INFO("Received message: '%s' ===> '%s'", agent_names[message.getSender()].c_str(), agent_names[key].c_str());
         return &msg::received_cache[key].back();
         }
      MESSAGING_LOG_ERROR("Invalid key");
      return NULL;
      }

      TMsg *
   receive
      (
      TAgentKey key
      )
      {
      struct timespec timeout;
      timeout.tv_nsec = 50;
      timeout.tv_sec = 0;
      return receive(key, &timeout);
      }

      TMsg *
   blockingReceive
      (
      TAgentKey key
      )
      {
      return receive(key, NULL);
      }


      Ts32
   send(TMsg *message)
      {
      if(msg::descriptor_cache.count(message->getSender()))
         {
         if(msg::descriptor_cache.count(message->getRecipient()))
            {
            MESSAGING_LOG_INFO("sending message: '%s' ===> '%s'", agent_names[message->getSender()].c_str(), agent_names[message->getRecipient()].c_str());
#ifndef SOLIPSISM
//            MESSAGING_LOG_INFO("Sending %s message from '%s' to '%s'", verb_to_string(msg->verb), agent_names[msg->sender].c_str(), agent_names[msg->recipient].c_str());
            if(-1 == mq_send(msg::descriptor_cache[message->getRecipient()], (const char*)message, message->getBodySize() + sizeof(TMsg) - MESSAGE_BODY_MEM_SIZE, 1))
               {
               MESSAGING_LOG_POSIX_ERROR;
               return FAILURE;
               }
#else
            // Put the message directly into the receiver's list since the
            // sender and receiver are one in the mind of a solipsist.
            msg::received_cache[message->getRecipient()].push_back(message);
#endif
            return SUCCESS;
            }
         else
            {
            MESSAGING_LOG_ERROR("Invalid recipient");
            return FAILURE;
            }
         }
      else
         {
         MESSAGING_LOG_INFO("Invalid sender");
         return FAILURE;
         }
      }

      size_t
   getReceivedCount(TAgentKey key)
      {
#ifndef SOLIPSISM
      if(msg::descriptor_cache.count(key))
         {
         mq_getattr(msg::descriptor_cache[key], &msg::attribute_cache[key]);
         return msg::attribute_cache[key].mq_curmsgs;
         }
      else
         {
         return 0;
         }
#else
      return 0;
#endif
      }

      size_t
   getLocalQueueSize(TAgentKey key)
      {
      if(msg::received_cache.count(key))
         {
         return msg::received_cache[key].size();
         }
      else
         {
         return 0;
         }
      }

   /*!
    * Remove all messages for this key.
    */
      TVoid
   flush(TAgentKey key)
      {
      if(msg::attribute_cache.count(key))
         {
         TMsg message;
         size_t count = 0;
         ssize_t nBytes;
#ifndef SOLIPSISM
         do {
            nBytes = mq_receive(msg::descriptor_cache[key], (char *)&message, msg::attribute_cache[key].mq_msgsize, NULL); // will not block
            count++;
            }
         while (nBytes > 0);
#endif
         count += msg::received_cache[key].size();
         msg::received_cache[key].clear();
         MESSAGING_LOG_INFO("Flushed %u messages", count);
         }
      else
         {
         MESSAGING_LOG_ERROR("Invalid key");
         }
      }

      Tnc8*
   getPath(TAgentKey key)
      {
      return msg::agent_names[key].c_str();
      }

      TVoid
   setAttributes(TAgentKey key, Ts32 flags, Ts32 special_flags)
      {
      // Avoid a context switch by checking whether any flags are actually
      // changing
      if(getSettableBits(msg::attribute_cache[key].mq_flags) && !flags)
         {
         msg::attribute_cache[key].mq_flags |= flags;
#ifndef SOLIPSISM
         mq_setattr(msg::descriptor_cache[key], &msg::attribute_cache[key], NULL);
#endif
         }
      msg::special_flags[key] |= special_flags;
      }

      TVoid
   unsetAttributes(TAgentKey key, Ts32 flags, Ts32 special_flags)
      {
      if(getSettableBits(msg::attribute_cache[key].mq_flags) && flags)
         {
         msg::attribute_cache[key].mq_flags &= !flags;
#ifndef SOLIPSISM
         mq_setattr(msg::descriptor_cache[key], &msg::attribute_cache[key], NULL);
#endif
         }
      msg::special_flags[key] &= !special_flags;
      }

      size_t
   getMaxBodySize(TAgentKey key)
      {
      return msg::attribute_cache[key].mq_msgsize;
      }

      TResourceType
   getResourceType(TResourceKey rk)
      {
      if(resource_types.count(rk) > 0)
         return resource_types[rk];
      else
         return UNKNOWN_TYPE;
      }
   }


