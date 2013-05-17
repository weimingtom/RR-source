#include "cube.h"
#include "game.h"

extern "C"
{
    #include "uv.h"
    #include "utils.h"
    #include "luv_handle.h"
}

#include "server.h"

namespace server
{

    namespace lua
    {
        clientReference::~clientReference()
            {
                if(ci && ci->ref)
                {
                    //We are gone
                    ci->ref = NULL;
                }
            }

        #define LUA_SERVER_CLIENT_MT "tig_server_client"
        #define LUA_GETUSER(L) \
            ::server::lua::clientReference *cref = (::server::lua::clientReference *) \
                luaL_checkudata(L, 1, LUA_SERVER_CLIENT_MT); \
            if(!cref->ci) luaL_argerror(L, 1, "Reference is not a valid user anymore.");

        LUALIB_API int lua_client_getName(lua_State *L)
        {
            LUA_GETUSER(L);

            lua_pushstring(L, cref->ci->name);
            return 1;
        }

        LUALIB_API int lua_client_getDisplayName(lua_State *L)
        {
            LUA_GETUSER(L);

            lua_pushstring(L, colorname(cref->ci));
            return 1;
        }

        #undef LUA_GETUSER

        LUALIB_API int lua_getClient(lua_State *L)
        {
            int cn = luaL_checknumber(L, 1);
            clientinfo *ci = getinfo(cn);

            if(!ci)
            {
                luaL_argerror(L, 1, "Invalid client number.");
                return 0;
            }

            ::server::lua::clientReference *ref;
            if(!ci->ref)
            {
                ref = (::server::lua::clientReference *)lua_newuserdata(L, sizeof(::server::lua::clientReference));
                ref->ci = ci;
                ref->L = L;

                luaL_getmetatable(L, LUA_SERVER_CLIENT_MT);
                lua_setmetatable(L, -2);

                lua_newtable(L);
                lua_setfenv(L, -2);

                ref->r = luaL_ref(L, LUA_REGISTRYINDEX);
            }

            lua_rawgeti(L, LUA_REGISTRYINDEX, ref->r);
            ::lua::Environment(L).dump();
            
            return 1;
        }

        struct instanceReference
        {
            lua_State *L;
            int r;
        };

        struct Instance
        {

        };

        instanceReference *instance;

        void onConnect(instanceReference * ref)
        {
            lua_State *L = ref->L;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref->r);

            luv_emit_event(L, "connect", 0);
        }

        LUALIB_API int lua_onConnect(lua_State *L)
        {
            Instance *i = (Instance *)luaL_checkudata(L, 1, "tig_server_instance");

            luv_register_event(L, 1, "connect", 2);
            return 0;
        }

        LUALIB_API int lua_createInstance(lua_State *L)
        {
            Instance *instance = (Instance *)lua_newuserdata(
                                          L, sizeof(Instance));

            luaL_getmetatable(L, "tig_server_instance");
            lua_setmetatable(L, -2);

            lua_newtable(L);
            lua_setfenv(L, -2);

            instanceReference * ref = new instanceReference;
            ref->L = L;
            lua_pushvalue(L, -1);
            ref->r = luaL_ref(L, LUA_REGISTRYINDEX);

            server::lua::instance = ref;

            return 1;
        }

        static const luaL_reg client_methods[] =
        {
            {"getName", lua_client_getName},
            {NULL, NULL}
        };

        static const luaL_reg instance_methods[] =
        {
          { "onConnect", lua_onConnect },
          { NULL, NULL}
        };

        static const luaL_reg exports[] =
        {
          { "createInstance", lua_createInstance },
          { "getClient", lua_getClient },
          { NULL, NULL }
        };

        template<typename T>
        int GCMethod(lua_State* L)
        {
            reinterpret_cast<T*>(lua_touserdata(L, 1))->~T();
            return 0;
        }

        LUALIB_API int luaopen_server(lua_State *L)
        {
          luaL_newmetatable(L, "tig_server_instance");
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
            luaL_register(L, NULL, instance_methods);
          lua_pop(L, 1);

          luaL_newmetatable(L, LUA_SERVER_CLIENT_MT);
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
            luaL_register(L, NULL, client_methods);
            lua_pushstring(L, "__gc");
            lua_pushcfunction(L, GCMethod<instanceReference>);
            lua_settable(L, -3);
          lua_pop(L, 1);


          lua_newtable(L);
            luaL_register(L, NULL, exports);
          return 1;
        }

        #undef LUA_SERVER_CLIENT_MT
    }
    void bind_core_functions(lua_State * L, int T)
    {
    }
}

