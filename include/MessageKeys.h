#ifndef MessageKeys_h
#define MessageKeys_h

#include "messages.h"

/// This is where we define the list of available socket messages to be
/// sent/received by any actor.

MESSAGES_ENUM(\
  // generic messages
  INVALID_KEY,                                                         \
  
  // listener messages
  REMOVE_LISTENER,                                                     \
  
  // master messages
  MASTER_BROADCAST, MASTER_DISCONNECT,                                 \
  SET_LISTENER_ID, LISTENER_DELETED                                    \
);  

#endif
