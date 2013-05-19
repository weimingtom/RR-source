#include "game.h"
#include "messageSystem.h"

#ifdef SERVER
#include "../server/server.h"
#endif

namespace messageSystem
{
    static vector<MessageType *> messageTypes;

    MessageType::MessageType(const char *name_) : name(name_)
    {
    }

    MessageType::~MessageType()
    {
        DELETEA(name);
    }

    void MessageType::setId(int id_)
    {
        id = id_;
    }

    int MessageType::getId()
    {
        return id;
    }

    void MessageType::receive(int receiver, int sender, ucharbuf &p)
    {
        LOG_FATAL("No receive handler for message %s (id=%i, receiver=%i, sender=%i)", name, id, receiver, sender);
        ASSERT(0);
    }

    struct InternalType
    {
        enum
        {
            REGISTER = NUMMSG,
        };
    };

    void MessageManager::registerMessageType(MessageType *newMessageType)
    {
        //Registers have controll over the message ids
#ifdef SERVER
        //Don't register internal types
        if(newMessageType->getId() > MessageManager::MIN_TYPE)
        {
            int maxId = MessageManager::MIN_TYPE;

            loopv(messageTypes)
            {
                maxId = max(messageTypes[i]->getId(), maxId);
            }

            maxId++;

            newMessageType->setId(maxId);
            send_RegisterId(newMessageType->name, newMessageType->getId());
        }
#endif
        messageTypes.add(newMessageType);
    }

    void MessageManager::registerInternal()
    {
        LOG_DEBUG("Registered internal messages.");
        messageTypes.add(new RegisterId);
    }

    bool MessageManager::receive(int type, int receiver, int sender, ucharbuf &p)
    {
        loopv(messageTypes)
        {
            if (messageTypes[i]->getId() == type)
            {
                LOG_TRACE("Receive custom message (type=%s(%i),receiver=%i,sender=%i)", messageTypes[i]->name, messageTypes[i]->getId(), receiver, sender);
                messageTypes[i]->receive(receiver, sender, p);
                return true;
            }
        }

        return false;
    }

    void MessageManager::syncMessages(int cn)
    {
        LOG_DEBUG("Sync messages with client %i", cn);
#ifdef SERVER
        //Todo: send reset
        loopv(messageTypes)
        {
            if(messageTypes[i]->getId() > MessageManager::MIN_TYPE)
            {
                send_RegisterId(messageTypes[i]->name, messageTypes[i]->getId(), cn);
            }
        }
#else
        ASSERT(0)
#endif

    }

}

#include "messages.cpp"