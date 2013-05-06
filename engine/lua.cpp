
#include "lua.h"
#include "cube.h"

void* operator new(size_t size, lua_State* L) {
    void* ptr = lua_newuserdata(L, size);
    return ptr;
}
namespace lua {
    int bind_returncount = 0;
#ifdef EXCEPTIONS_ENABLED

    static int wrap_exceptions(lua_State *L, lua_CFunction f) {
        try {
            return f(L); // Call wrapped function and return result.
        } catch (const char *s) { // Catch and convert exceptions.
            lua_pushstring(L, s);
        } catch (std::exception& e) {
            lua_pushstring(L, e.what());
        } catch (...) {
            lua_pushliteral(L, "caught (...)");
        }
        return lua_error(L); // Rethrow as a Lua error.
    }
#endif

    static int traceback(lua_State *L) {
        const char *msg = lua_tostring(L, 1);

        if (msg) luaL_traceback(L, L, msg, 1);
        else if (!lua_isnoneornil(L, 1) && !luaL_callmeta(L, 1, "__tostring"))
            lua_pushliteral(L, "(no error message)");
        return 1;
    }

    void Environment::error(const char* message) {
        lua_pushstring(L, message);
        lua_error(L);
    }

    void Environment::argError(int index, const char* message) {
        luaL_argerror(L, index, message);
    }

    void Environment::pushVar(nilType) {
        lua_pushnil(L);
    }

    void Environment::pushVar(bool value) {
        lua_pushboolean(L, static_cast<int> (value));
    }

    void Environment::pushVar(int value) {
        lua_pushinteger(L, value);
    }

    void Environment::pushVar(unsigned int value) {
        lua_pushinteger(L, value);
    }

    void Environment::pushVar(lua_Number value) {
        lua_pushnumber(L, value);
    }

    void Environment::pushVar(lua_CFunction value) {
        lua_pushcfunction(L, value);
    }

    void Environment::pushVar(const char * value) {
        lua_pushstring(L, value);
    }

    void Environment::pushVar(string value) {
        lua_pushlstring(L, value, MAXSTRLEN);
    }

    /*	lua_CFunction Environment::setPanicHandler(lua_CFunction function)
        {
            return lua_atpanic(L, function);
        }*/

    void Environment::select(int index, const char *field) {
        lua_getfield(L, index, field);
    }

    void Environment::select(int index, const char *field, const char *a...) {
        select(index, field);
        select(-1, a);
    }

    void Environment::select(const char *field) {
        lua_getglobal(L, field);
    }

    void Environment::select(const char *field, const char *a...) {
        select(field);
        select(-1, a);
    }

    lua_State *Environment::getState() {
        return L;
    }

    void Environment::pushTrace() {
        lua_pushcfunction(L, traceback); // push traceback function
    }

    void Environment::remove(int index) {
        lua_remove(L, index);
    }

    bool Environment::run(const char *file) {
#ifdef EXCEPTIONS_ENABLED
        lua_pushlightuserdata(L, (void *) wrap_exceptions);
        luaJIT_setmode(L, -1, LUAJIT_MODE_WRAPCFUNC | LUAJIT_MODE_ON);
        pop(1);
#endif

        pushTrace();
        int error = luaL_loadfile(L, findfile(file, "r")) || lua_pcall(L, 0, 0, 1);
        remove(1);
        if (error) {

            const char *errmsg = to(-1, wrapType<const char *>());

            conoutf(CON_ERROR, "Could not execute file (%s):\n%s", file, errmsg);
            return false;
        }

        return true;
    }

    bool Environment::init() {
        getGlobal("_E");

        if (!isTable(-1)) {
            newTable(regFunctions->length());
            lua_setglobal(L, "_E");
        }
        pop(1);

        loopv(*regFunctions) {
            (*regFunctions)[i]();

        }

        delete regFunctions;
        
        isInitialized = true;

        return true;
    }

    void Environment::pushValue(int index) {
        lua_pushvalue(L, index);
    }

    void Environment::pushClosure(lua_CFunction funct, int count) {
        lua_pushcclosure(L, funct, count);
    }

    int Environment::to(int index, wrapType<int>) {
        return luaL_checkint(L, index);
    }

    unsigned int Environment::to(int index, wrapType<unsigned int>) {
        return static_cast<unsigned int> (luaL_checknumber(L, index));
    }

    float Environment::to(int index, wrapType<float>) {
        return static_cast<float> (luaL_checknumber(L, index));
    }

    long Environment::to(int index, wrapType<long>) {
        return static_cast<long> (luaL_checklong(L, index));
    }

    unsigned long Environment::to(int index, wrapType<unsigned long>) {
        return static_cast<unsigned long> (luaL_checknumber(L, index));
    }

    lua_Number Environment::to(int index, wrapType<lua_Number>) {
        return luaL_checknumber(L, index);
    }

    bool Environment::to(int index, wrapType<bool>) {
        luaL_checkany(L, index);

        if (lua_type(L, index) == LUA_TNUMBER && lua_tointeger(L, index) == 0)
            return false;

        return static_cast<bool> (lua_toboolean(L, index));
    }

    const char * Environment::to(int index, wrapType<const char *>) {
        return luaL_checkstring(L, index);
    }

    char *Environment::to(int index, wrapType<string>) {
        const char *in = luaL_checkstring(L, index);

        return newstring(in);
    }

    char *Environment::to(int index, wrapType<char *>) {
        return to(index, wrapType<string>());
    }

    void Environment::newTable(int narr, int nrec) {
        lua_createtable(L, narr, nrec);
    }

    void Environment::newTable(int narr) {
        lua_createtable(L, narr, narr);
    }

    void Environment::newTable() {
        newTable(0, 0);
    }

    void Environment::setTable(int id) {
        lua_settable(L, id);
    }

    void Environment::pop(int id) {
        lua_pop(L, id);
    }

    void Environment::getGlobal(const char *name) {
        lua_getglobal(L, name);
    }

    void Environment::setGlobal(const char* name) {
        lua_setglobal(L, name);
    }

    bool Environment::isBoolean(int index) {
        return lua_isboolean(L, index);
    }

    bool Environment::isCFunction(int index) {
        return lua_iscfunction(L, index);
    }

    bool Environment::isFunction(int index) {
        return lua_isfunction(L, index);
    }

    bool Environment::isUserdata(int index) {
        return lua_islightuserdata(L, index) || lua_isuserdata(L, index);
    }

    bool Environment::isNil(int index) {
        return lua_isnil(L, index);
    }

    bool Environment::isNone(int index) {
        return lua_isnone(L, index);
    }

    bool Environment::isNoneOrNil(int index) {
        return lua_isnoneornil(L, index);
    }

    bool Environment::isNumber(int index) {
        return lua_isnumber(L, index);
    }

    bool Environment::isString(int index) {
        return lua_isstring(L, index);
    }

    bool Environment::isTable(int index) {
        return lua_istable(L, index);
    }

    bool Environment::isThread(int index) {
        return lua_isthread(L, index);
    }

    int Environment::top() {
        return lua_gettop(L);
    }

    void Environment::dumpIndex(int i) {
        int t = lua_type(L, i);
        switch (t) {
            case LUA_TSTRING: /* strings */
                printf("\tstring: '%s'\n", lua_tostring(L, i));
                break;
            case LUA_TBOOLEAN: /* booleans */
                printf("\tboolean %s\n", lua_toboolean(L, i) ? "true" : "false");
                break;
            case LUA_TNUMBER: /* numbers */
                printf("\tnumber: %g\n", lua_tonumber(L, i));
                break;
            default: /* other values */
                printf("\t%s\n", lua_typename(L, t));
                break;
        }
    }

    void Environment::dump(int top) {
        int i;

        printf("total in stack %d\n", top);

        for (i = 1; i <= top; i++) { /* repeat for each level */
            dumpIndex(i);
        }
        printf("\n"); /* end the listing */
    }

    void Environment::dump() {
        dump(lua_gettop(L));
    }

    int Environment::upvalueIndex(int index) {
        return lua_upvalueindex(index);
    }

    Environment::Environment(lua_State *State) : L(State) {
    };

    Environment::Environment() {
        L = luaL_newstate();
        luaL_openlibs(L);
    }

    bool Environment::eval(const char *string) {
        pushTrace();
        int error = luaL_loadstring(L, string) || lua_pcall(L, 0, 0, 1);
        remove(1);
        if (error) {

            const char *errmsg = to(-1, wrapType<const char *>());

            conoutf(CON_ERROR, "Could not execute string: %s", errmsg);
            return false;
        }
        return false;
    }

    bool Environment::registerDelayedFunction(const char *name, void (*fun)(void)) {

        if (isInitialized) {

            fun();
                    return true;
        }
        else
        {
             if (!regFunctions) {
                regFunctions = new vector<void (*) (void)>;
            }
            regFunctions->add(fun);
        }

        return false;
    }

    static Environment env;

    Environment &getEnvironment() {
        return env;
    };

    void init() {
        getEnvironment().init();
    }


};

void lua_eval(const char *text) {
    lua::getEnvironment().eval(text);
}

COMMAND(lua_eval, "s");

LUAENVCOMMAND(cubescript,{
    tagval v;
    executeret(luaL_checkstring(L, 1), v);
    switch (v.type) {
        case VAL_INT:
            lua_pushinteger(L, v.getint());
        case VAL_FLOAT:
            lua_pushnumber(L, v.getfloat());
        case VAL_STR:
            lua_pushstring(L, v.getstr());
        default:
            lua_pushnil(L);
    }
    return 1;
})