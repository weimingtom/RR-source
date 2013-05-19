
#include "messages.h"
#include "messageSystem.h"


namespace messageSystem
{
    RegisterId::RegisterId() : MessageType("internal.reg")
    {
        setId(InternalType::REGISTER);
    };


    #ifdef CLIENT
        void RegisterId::receive(int receiver, int sender, ucharbuf& p)
        {
            char name[MAXTRANS];
            getstring(name, p);
            int id = getint(p);

            loopv(messageTypes)
            {
                if(0 == strcmp(messageTypes[i]->name, name))
                {
                    messageTypes[i]->setId(id);
                    break;
                }
            }

            LOG_DEBUG("Register message type %s %i", name, id);
        }
    #endif

    #ifdef SERVER
    void send_RegisterId(const char *name, int id, int to)
    {
        sendf(to, MessageManager::MAIN_CHANNEL, "risi", InternalType::REGISTER, name, id);
    }
    #endif

    TextMessage::TextMessage() : MessageType("tig.message")
    {
    };

    void TextMessage::receive(int receiver, int sender, ucharbuf &p)
    {
        char text[MAXTRANS];

        int box = getint(p);
        getstring(text, p);

#ifdef CLIENT
        conoutf(box, "%s", text);
#else
        LOG_WARNING("Receiving tig.message from client. (receiver=%i, sender=%i, box=%i, text=%s)", receiver, sender, box, text);
#endif
    }

#ifdef SERVER
    void send_TextMessage(int type, const char *message, int id, int to)
    {
        sendf(to, MessageManager::MAIN_CHANNEL, "riis", id, type, message);
    }
#endif
}
