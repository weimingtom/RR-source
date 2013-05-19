#ifndef FPSGAME_MESSAGES_H
#define FPSGAME_MESSAGES_H

namespace messageSystem
{
    /**
     * Message by the server to register an id for a message name
     */
    struct RegisterId : MessageType
    {
        RegisterId();
        void receive(int receiver, int sender, ucharbuf &p);
    };

    #ifdef SERVER
    void send_RegisterId(const char *name, int id, int to = -1);
    #endif

    /**
     * Message or notification in specified message box,
     * By example: domination messages on user's hud
     */
    struct TextMessage : MessageType
    {
        TextMessage();
        void receive(int receiver, int sender, ucharbuf &p);
    };

    #ifdef SERVER
    void send_TextMessage(const char *name, int id, int to = -1);
    #endif
}

#endif