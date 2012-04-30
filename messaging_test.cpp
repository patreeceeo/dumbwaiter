#include <stdio.h>

#include <map>
#include <string>

#include "common.hpp"
#include "log.hpp"

   msg::TVerb
parseVerb(Tnc8 *verb)
   {
   if(strcmp(verb, "create") == 0)
      {
      return msg::VERB_CREATE;
      }
   else if(strcmp(verb, "delete") == 0)
      {
      return msg::VERB_DELETE;
      }
   else if(strcmp(verb, "get") == 0)
      {
      return msg::VERB_GET;
      }
   else if(strcmp(verb, "set") == 0)
      {
      return msg::VERB_SET;
      }
   return msg::VERB_ACK;
   }

int main(int argc, char *argv[])
   {
   std::map<msg::TResourceKey, std::map<std::string, std::string> > resources;
   msg::initialize();



   if(argc >= 5 && (argc - 3) % 2 == 0)
      {
      Tn8 *recipient = argv[1];
      Tn8 *verb = argv[2];

      MESSAGING_LOG_INFO("recipient: %s verb: %s (%u)", recipient, verb, parseVerb(verb));

      msg::TMsg out(parseVerb(verb));
      msg::TAgentKey myKey = msg::createAgent("/util");
      out.setSender(myKey);
      msg::TAgentKey agentKey = msg::createAgent(recipient);
      out.setRecipient(agentKey);

      ssize_t pairIndex = 3;
      for(;pairIndex < argc - 1; pairIndex+=2)
         {
         Tn8 *resource = argv[pairIndex];
         Tn8 *value = argv[pairIndex+1];
         msg::TResourceKey resourceKey = msg::createResource(resource);
         MESSAGING_LOG_INFO("resource: %s (%u), value: %s", msg::stringify::getResourceKey(resourceKey), resourceKey, value);
         out.append(resourceKey, strlen(value), value);
         }

      msg::send(&out);
      }
   else if(argc == 2)
      {
      Tn8 *me = argv[1];

      msg::TAgentKey myKey = msg::createAgent(me);

      msg::TMsg *in = msg::receive(myKey);

      MESSAGING_LOG_INFO("received message");

      size_t currentField = 0;
      Tn8 value[128];
      Tn8 repValue[128];
      TBoolean reading = TRUE;
      msg::TResourceKey ark;
      while(reading)
         {
         ssize_t fieldOffset = 0;
         ark = in->getResourceKey(currentField);
         msg::TResourceType type = msg::getResourceType(ark);
         if(ark == msg::NO_MORE_RESOURCES)
            {
            reading = FALSE;
            }
         else
            {
            fieldOffset = in->extract(value, currentField);
            value[fieldOffset - msg::FIELD_HEADER_SIZE] = '\0';
            switch(type)
               {
               case msg::OCTET_STR:
                  MESSAGING_LOG_INFO("Received an OCTET_STR!");
                  strncpy(repValue, value, sizeof(repValue));
                  break;
               case msg::INTEGER:
                  snprintf(repValue, sizeof(repValue), "%d", *value);
                  break;
               case msg::UNSIGNED:
                  snprintf(repValue, sizeof(repValue), "%u", *value);
                  break;
               default:
                  snprintf(repValue, sizeof(repValue), "(Cannot represent this type of value: %u)", type);
               }
            MESSAGING_LOG_INFO("verb: %s resource: %s (%u) value: %s", msg::stringify::getVerb(in->getVerb()), msg::stringify::getResourceKey(ark), ark, repValue);
            }

         if(-1 == fieldOffset)
            {
            reading = FALSE;
            }
         else
            {
            currentField += fieldOffset;
            }
         }
      }

   }
