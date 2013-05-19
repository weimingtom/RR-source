/**
 * Message system inspired by intensity
 */

#ifndef FPSGAME_MESSAGE_SYSTEM_H
#define FPSGAME_MESSAGE_SYSTEM_H

namespace messageSystem
{
    struct MessageType
    {
        int id;
        const char *name;

        MessageType(const char *name);
        ~MessageType();

        void setId(int id);
        int getId();

        void receive(int receiver, int sender, ucharbuf &p);
    };

    struct MessageManager
    {
        /**
         * The minimum of the dynamic messages,
         * everything above this will be removed when the message manager is reset.
         */
        static const int MIN_TYPE = NUMMSG + 50;

        /**
         * The default channel to send messages on.
         *
         * 0 - positions
         * 1 - genic packages
         * 2 - files
         */
        static const int MAIN_CHANNEL = 1;

        /**
         * Will automatically register a message type and resolve it's id.
         * @param newMessageType
         */
        static void registerMessageType(MessageType *newMessageType);

        /**
         * Registers internal message types
         */
        static void registerInternal();

        /**
         * Receive a package from the server/client
         * @param type
         * @param receiver
         * @param sender
         * @param p
         * @return is this a valid message
         */
        static bool receive(int type, int receiver, int sender, ucharbuf &p);

        /**
         * Synchronize the messages of the client with the server's
         */
        static void syncMessages(int cn);
    };

}

#include "messages.h"
#endif