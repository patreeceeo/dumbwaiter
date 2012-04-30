/*
 * test_cases.cpp
 *
 *  Created on: Dec 8, 2010
 *      Author: patrick
 */


#include <cstdio>
#include <cstring>
#include <set>

#include "../../include/messaging.hpp"

#define TEST_BLOCKING
#ifndef TEST_BLOCKING
#define TEST_ACKSYNC
#endif

   TVoid
set_text(msg::TMsg &msg, Tnc8 *text)
   {
   strcpy(msg.body, text);
   msg.body_size = strlen(text)+1;
   }

   int
main(int argc, Tnc8 *argv[])
   {
   Tn8 recv_msg_path[32];
   size_t speech_count = 0;
   msg::TMsg speech[20];
   size_t speech_index;

   if(argc < 2)
      return 1;

#ifdef TEST_BLOCKING
   msg::TAgentKey msg_key_sir_robin = msg::createAgent("/sir_robin", 1, 8192);
   msg::TAgentKey msg_key_gaurdian = msg::createAgent("/gaurdian", 1, 8192);
   msg::TAgentKey msg_key_king_arthur = msg::createAgent("/king_arthur", 1, 8192);
   msg::TAgentKey msg_key_mykey = msg::NOT_AN_AGENT;
#else
   msg::TAgentKey msg_key_sir_robin = msg::createAgent("/sir_robin");
   msg::TAgentKey msg_key_gaurdian = msg::createAgent("/gaurdian");
   msg::TAgentKey msg_key_king_arthur = msg::createAgent("/king_arthur");
   msg::TAgentKey msg_key_mykey = msg::NOT_AN_AGENT;

   msg::set_attr(msg_key_sir_robin, O_NONBLOCK);
   msg::set_attr(msg_key_gaurdian, O_NONBLOCK);
   msg::set_attr(msg_key_king_arthur, O_NONBLOCK);
#endif


   sprintf(recv_msg_path, "/%s", argv[1]);

   for(speech_index = 0; speech_index < sizeof(speech)/sizeof(*speech); speech_index++)
      {
      speech[speech_index].recipient = msg::NOT_AN_AGENT;
      speech[speech_index].verb = msg::VERB_SET;
      }

   if(strcmp(argv[1], "sir_robin") == 0)
      {
      msg_key_mykey = msg_key_sir_robin;

      set_text(speech[1], "My name is Sir Robin.");
      speech[1].recipient = msg_key_gaurdian;

      set_text(speech[3], "I seek the Grail!");
      speech[3].recipient = msg_key_gaurdian;

      set_text(speech[5], "Blue...I mean green! Aaaaah!!!");
      speech[5].recipient = msg_key_gaurdian;
      speech_count = 6;
      }
   else if(strcmp(argv[1], "king_arthur") == 0)
      {
      msg_key_mykey = msg_key_king_arthur;

      set_text(speech[1], "Arthur, King of England");
      speech[1].recipient =  msg_key_gaurdian;

      set_text(speech[2], "and Leader of the Round Table!");
      speech[2].recipient =  msg_key_gaurdian;

      set_text(speech[4], "I seek the Holy Grail.");
      speech[4].recipient = msg_key_gaurdian;

      set_text(speech[6], "An African swallow or European swallow?");
      speech[6].recipient = msg_key_gaurdian;
      speech_count = 7;
      }
   else if(strcmp(argv[1], "gaurdian") == 0)
      {
      msg_key_mykey = msg_key_gaurdian;

      set_text(speech[0], "What is your name?");
      speech[0].recipient = msg_key_sir_robin;

      set_text(speech[2], "What is your quest?");
      speech[2].recipient = msg_key_sir_robin;

      set_text(speech[4], "What is your favorite color?");
      speech[4].recipient =  msg_key_sir_robin;

      set_text(speech[6], "What is your name?");
      speech[6].recipient = msg_key_king_arthur;

      set_text(speech[9], "What is your quest?");
      speech[9].recipient = msg_key_king_arthur;

      set_text(speech[11], "What is the average wind speed velocity of an unladen swallow???");
      speech[11].recipient = msg_key_king_arthur;

      set_text(speech[13], "Uhh?! I don't know! Aaaaaah!!!");
      speech[13].recipient = msg_key_king_arthur;
      speech_count = 14;
      }
   else if(strcmp(argv[1], "evil_bunny") == 0)
      {
      printf("evil bunny!!!\n");
      msg::flush(msg_key_sir_robin);
      msg::flush(msg_key_king_arthur);
      msg::flush(msg_key_gaurdian);
      return 0;
      }
   else if(strcmp(argv[1], "lancelot") == 0)
      {
      msg::TMsg msg_help;
      set_text(msg_help, "Help!!!");
      msg_help.sender = msg_key_sir_robin;
      msg_help.recipient = msg_key_gaurdian;
      msg_help.verb = msg::VERB_SET;
      msg::send(&msg_help);
      return 0;
      }
   else
      {
      printf("I am %s.\n", argv[1]);
      return 1;
      }


   enum
      {
      ST_NONE,
      ST_TALK,
      ST_LISTEN,
      ST_PROCESS,
      }state = ST_TALK;

   speech_index = 0;
   msg::TMsg *msg_recvd = NULL;
   TBoolean finished = FALSE;

   while(!finished)
      {
      switch(state)
         {
         case ST_TALK:
            printf("state talk\n");
            if(msg::NOT_AN_AGENT != speech[speech_index].recipient)
               {
               speech[speech_index].sender = msg_key_mykey;
               printf("%s: %s\n", argv[1], speech[speech_index].body);
               if(SUCCESS != msg::send(speech + speech_index))
                  printf("%s failed to send message!\n", argv[1]);
               }
#ifdef TEST_BLOCKING
            else
               {
               state = ST_LISTEN;
               }
#else
            state = ST_LISTEN;
#endif

            if(speech_index <= speech_count)
               speech_index++;

            break;
         case ST_LISTEN:
           printf("state listen\n");
           msg_recvd = msg::receive(msg_key_mykey);

#ifdef TEST_ACKSYNC
           if(msg_recvd)
              {
              failure_count = 0;
              if(msg_recvd->verb != msg::VERB_ACK)
                 {
                 state = ST_PROCESS;
                 }
              else
                 {
                 state = ST_TALK;
                 }
              }
#else
              state = ST_PROCESS;
#endif
           break;
         case ST_PROCESS:

            printf("%s: %s\n", msg::getPath(msg_recvd->sender), msg_recvd->body);
#ifdef TEST_ACKSYNC
            msg::TMsg msg_ack;
            msg_ack.verb = msg::VERB_ACK;
            msg_ack.recipient = msg_recvd->sender;
            msg_ack.sender = msg_key_mykey;
            set_text(msg_ack, "");
            msg::send(&msg_ack);
#endif
            state = ST_TALK;
            break;
         default:
            break;
         }

      }

   return 0;
   }
