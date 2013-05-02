
#ifndef ENGINE_LUA_H
#define ENGINE_LUA_H

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luajit.h"
}

#include "cube.h"

namespace lua {
    extern int bind_returncount;

    class nilType {
    };
    extern nilType nil;

    template<typename T>
    class wrapType {
    };

    template<typename... Args>
    class countArgs;

    template<>
    class countArgs<> {
    public:
        static const unsigned size = 0;
    };

    template<typename T1>
    class countArgs<T1> {
    public:
        static const unsigned size = 1;
    };

    template<typename T1, typename T2>
    class countArgs<T1, T2> {
    public:
        static const unsigned size = 2;
    };

    template<int Size>
    struct ArgSize {
    };

    template<typename T>
    struct ReturnTag {
    };

    template<typename T, typename FT>
    struct WrapperReturnTag {
    };

    template< typename T>
    struct removePointer {
        typedef T type;
    };

    template< typename T>
    struct removePointer<T&> {
        typedef T type;
    };

    template< typename T>
    struct removePointer<T*> {
        typedef T type;
    };

    template< typename T>
    struct removePointer<T * const> {
        typedef T type;
    };

    template< typename T>
    struct removePointer<T * volatile> {
        typedef T type;
    };

    template< typename T>
    struct removePointer<T * const volatile> {
        typedef T type;
    };

    template<typename T>
    T doReturn(T var) {
        return var;
    }

    template<typename R>
    struct functionTrait;

    template<typename R>
    struct functionTrait<R(*)(void) > {
        static const unsigned size = 0;
        typedef R resultType;
    };

    template<typename R, typename T1>
    struct functionTrait<R(*)(T1)> {
        static const unsigned size = 1;
        typedef R resultType;
        typedef T1 arg1Type;
    };

    template<typename R, typename T1, typename T2>
    struct functionTrait<R(*)(T1, T2)> {
        static const unsigned size = 2;
        typedef R resultType;
        typedef T1 arg1Type;
        typedef T2 arg2Type;
    };

    template<typename R, typename T1, typename T2, typename T3>
    struct functionTrait<R(*)(T1, T2, T3)> {
        static const unsigned size = 3;
        typedef R resultType;
        typedef T1 arg1Type;
        typedef T2 arg2Type;
        typedef T3 arg3Type;
    };

    template<typename R, typename T1, typename T2, typename T3, typename T4>
    struct functionTrait<R(*)(T1, T2, T3, T4)> {
        static const unsigned size = 4;
        typedef R resultType;
        typedef T1 arg1Type;
        typedef T2 arg2Type;
        typedef T3 arg3Type;
        typedef T4 arg4Type;
    };

    template<typename R, typename T1, typename T2, typename T3, typename T4, typename T5>
    struct functionTrait<R(*)(T1, T2, T3, T4, T5)> {
        static const unsigned size = 5;
        typedef R resultType;
        typedef T1 arg1Type;
        typedef T2 arg2Type;
        typedef T3 arg3Type;
        typedef T4 arg4Type;
        typedef T5 arg5Type;
    };

    template<typename R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
    struct functionTrait<R(*)(T1, T2, T3, T4, T5, T6)> {
        static const unsigned size = 6;
        typedef R resultType;
        typedef T1 arg1Type;
        typedef T2 arg2Type;
        typedef T3 arg3Type;
        typedef T4 arg4Type;
        typedef T5 arg5Type;
        typedef T6 arg6Type;
    };

    template<typename R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
    struct functionTrait<R(*)(T1, T2, T3, T4, T5, T6, T7)> {
        static const unsigned size = 7;
        typedef R resultType;
        typedef T1 arg1Type;
        typedef T2 arg2Type;
        typedef T3 arg3Type;
        typedef T4 arg4Type;
        typedef T5 arg5Type;
        typedef T6 arg6Type;
        typedef T7 arg7Type;
    };

    template<typename R, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
    struct functionTrait<R(*)(T1, T2, T3, T4, T5, T6, T7, T8)> {
        static const unsigned size = 8;
        typedef R resultType;
        typedef T1 arg1Type;
        typedef T2 arg2Type;
        typedef T3 arg3Type;
        typedef T4 arg4Type;
        typedef T5 arg5Type;
        typedef T6 arg6Type;
        typedef T7 arg7Type;
        typedef T8 arg8Type;
    };

    class Environment {
    private:
        lua_State *L;
        bool isInitialized;
        /**
         * Pushes a variable to the lua stack, internally used to decide the type
         */

        void pushVar(nilType);
        void pushVar(bool value);
        void pushVar(int value);
        void pushVar(unsigned int value);
        void pushVar(lua_Number value);
        void pushVar(lua_CFunction value);
        void pushVar(const char * value);
        void pushVar(string value);
        //void pushVar(const char * value, size_t length);



        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<0>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function()
                    );

            return 1;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<0>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function();

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<1>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ())
                    )
                    );

            return 1;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<1>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ())
                    );

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<2>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ())
                    )
                    );

            return 1;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<2>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ())
                    );

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<3>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ())
                    )
                    );

            return 1;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<3>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ())
                    );

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<4>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ())
                    )
                    );

            return 1;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<4>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ())
                    );

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<5>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ()),
                    env.to(5, wrapType<typename FunctionTraits::arg5Type > ())
                    );

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<5>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ()),
                    env.to(5, wrapType<typename FunctionTraits::arg5Type > ())
                    )
                    );

            return 1;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<6>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ()),
                    env.to(5, wrapType<typename FunctionTraits::arg5Type > ()),
                    env.to(6, wrapType<typename FunctionTraits::arg6Type > ())
                    );

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<6>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ()),
                    env.to(5, wrapType<typename FunctionTraits::arg5Type > ()),
                    env.to(6, wrapType<typename FunctionTraits::arg6Type > ())
                    )
                    );

            return 1;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<7>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ()),
                    env.to(5, wrapType<typename FunctionTraits::arg5Type > ()),
                    env.to(6, wrapType<typename FunctionTraits::arg6Type > ()),
                    env.to(7, wrapType<typename FunctionTraits::arg7Type > ())
                    );

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<7>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ()),
                    env.to(5, wrapType<typename FunctionTraits::arg5Type > ()),
                    env.to(6, wrapType<typename FunctionTraits::arg6Type > ()),
                    env.to(7, wrapType<typename FunctionTraits::arg7Type > ())
                    )
                    );

            return 1;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<void, FunctionTraits>, ArgSize<8>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ()),
                    env.to(5, wrapType<typename FunctionTraits::arg5Type > ()),
                    env.to(6, wrapType<typename FunctionTraits::arg6Type > ()),
                    env.to(7, wrapType<typename FunctionTraits::arg7Type > ()),
                    env.to(8, wrapType<typename FunctionTraits::arg8Type > ())
                    );

            return 0;
        }

        template<typename FunctionPointerType, typename FunctionTraits>
        static int wrapFunction(Environment &env, WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits>, ArgSize<8>) {
            FunctionPointerType function = env.toUserdata<FunctionPointerType> (
                    env.upvalueIndex(1)
                    );

            env.push(
                    function(
                    env.to(1, wrapType<typename FunctionTraits::arg1Type > ()),
                    env.to(2, wrapType<typename FunctionTraits::arg2Type > ()),
                    env.to(3, wrapType<typename FunctionTraits::arg3Type > ()),
                    env.to(4, wrapType<typename FunctionTraits::arg4Type > ()),
                    env.to(5, wrapType<typename FunctionTraits::arg5Type > ()),
                    env.to(6, wrapType<typename FunctionTraits::arg6Type > ()),
                    env.to(7, wrapType<typename FunctionTraits::arg7Type > ()),
                    env.to(8, wrapType<typename FunctionTraits::arg8Type > ())
                    )
                    );

            return 1;
        }
        template<typename FunctionPointerType>
        static int wrapFunction(lua_State *L) {
            typedef typename removePointer<FunctionPointerType>::type FunctionType;
            typedef functionTrait<FunctionPointerType> FunctionTraits;

            Environment env(L);


            if (!((unsigned int) env.top() >= FunctionTraits::size))
                    env.error("Not enough arguments!");

                return wrapFunction<FunctionPointerType, FunctionTraits > (
                        env,
                        WrapperReturnTag<typename FunctionTraits::resultType, FunctionTraits > (),
                        ArgSize<FunctionTraits::size>()
                        );

        }


    public:
        void error(const char *message);
        void argError(int index, const char *message);

        /**
         * Sets the function that handles lua panic errors.
         * @todo make available for any C++ function
         * @param function the panic handler
         * @return the old function
         */
        //lua_CFunction setPanicHandler(lua_CFunction function);

        /**
         * Selects a field by name
         * @param index Index of the table
         * @param field Name of the field
         */
        void select(int index, const char *field);

        /**
         * Select a field by name, and select subfields
         * @param index
         * @param field
         * @param ...
         */
        void select(int index, const char *field, const char *...);

        /**
         * Selects a field by name in the global table
         * @param field name of the field
         */
        void select(const char *field);

        /**
         * Select a field by name, and select subfields
         * @param field
         * @param ...
         */
        void select(const char *field, const char*...);

        /**
         * pushes a lua_CFunction to the stack
         * @param function
         */
        void pushFunction(lua_CFunction function) {
            pushVar(function);
        }

        /**
         * Pushes a function containter to the stack.
         * when called it decides wich arguments should be converted to what types.
         * @param function
         */
        template<typename functionType>
        void pushFunction(functionType function) {
            lua_pushlightuserdata(L, reinterpret_cast<void *> (function));
            lua_pushcclosure(L, wrapFunction<functionType>, 1);
        }

        template<typename T1, typename... T2>
        void push(T1(value)(T2...)) {
            pushFunction(value);
        }

        /**
         * Pushes a variable to the stack.
         * @param value
         */
        template<typename T>
        int push(T value) {
            pushVar(value);
            return 1;
        }

        /**
         * pushes a list of variables to the stack
         * @param value
         * @param args
         */

        template<typename T, typename... Args>
        int push(T value, Args... args) {
            push(value);
            return 1 + push(args...);
        }

        int push() {
            return 0;
        }

        void pushValue(int index);

        template<typename T>
        void pushUserData(T &value) {
            lua_pushlightuserdata(L, &value);
        }

        template<class ClassName>
        typename removePointer<ClassName>::type * toUserdata(int index) {
            if (!isUserdata(index))
                argError(index, "Is not userdata");

            return reinterpret_cast<typename removePointer<ClassName>::type *> (
                    lua_touserdata(L, index)
                    );
        }

        void pushClosure(lua_CFunction funct, int count);

        template<typename T1>
        bool registerFunction(const char *name, T1 function) {
            getGlobal("_E");
            if (!isTable(-1)) {
                newTable(1);
                lua_setglobal(L, "_E");

                pop(1);
                getGlobal("_E");
            }

            pushRow(name, function);
            pop(1);

            return true;
        }


        /**
         * Converts the value on the stack at index index to the selected type
         * @param index
         * @param an instance of wrapType<typename> is used to decide what type is being requested.
         * @return mixed the value from the stack. depending on the wrapType<value>
         */
        int to(int index, wrapType<int>);

        inline int *to(int index, wrapType<int*>) {
            static int value = to(index, wrapType<int>());
            return &value;
        }

        unsigned int to(int index, wrapType<unsigned int>);

        inline unsigned int *to(int index, wrapType<unsigned int*>) {
            static unsigned int value = to(index, wrapType<unsigned int>());
            return &value;
        }

        float to(int index, wrapType<float>);

        inline float *to(int index, wrapType<float *>) {
            static float value = to(index, wrapType<float>());
            return &value;
        }

        long to(int index, wrapType<long>);
        unsigned long to(int index, wrapType<unsigned long>);
        lua_Number to(int index, wrapType<lua_Number>);
        bool to(int index, wrapType<bool>);
        const char * to(int index, wrapType<const char *>);
        char *to(int index, wrapType<string>);
        char *to(int index, wrapType<char *>);

        void newTable(int narr, int nrec);
        void newTable(int narr);
        void newTable();
        void setTable(int id);

        template<typename T1, typename T2>
        void pushRow(int table, T1 name, T2 value) {
            table -= 2;
            push(name);
            push(value);
            setTable(table);
        }

        template<typename T1, typename T2>
        void pushRow(T1 name, T2 value) {
            pushRow(-1, name, value);
        }

        void pop(int id);

        void getGlobal(const char *name);

        bool isBoolean(int index);
        bool isCFunction(int index);
        bool isFunction(int index);
        bool isUserdata(int index);
        bool isNil(int index);
        bool isNone(int index);
        bool isNoneOrNil(int index);
        bool isNumber(int index);
        bool isString(int index);
        bool isTable(int index);
        bool isThread(int index);

        int top();

        void dumpIndex(int index);
        void dump(int top);
        void dump();

        template<typename... Args>
        bool call(int rcount, Args... args) {
            int function = top();

            pushTrace();
            int errorFunction = top();

            pushValue(function);
            push(args...);
            int error = lua_pcall(L, countArgs < Args...>::size, rcount, errorFunction);
            remove(errorFunction);
            remove(function);

            if (error) {

                const char *msg = to(-1, wrapType<const char *>());
                pop(1);
                conoutf(CON_ERROR, "An error occured when calling a lua function: %s", msg);
                return false;
            }

            return true;
        }

        template<typename... Args>
        bool call(const char *name, int rcount, Args... args) {
            getGlobal(name);
            return call(rcount, args...);
        }

        int upvalueIndex(int index);

        bool init(const char *file = "data/scripts/init.lua");
        void pushTrace();
        void remove(int index);
        bool eval(const char *string);

        lua_State *getState();

        /**
         * Initialize an Environment around an existing lua state
         * @param State lua state
         */
        Environment(lua_State *State);

        /**
         * Initialize a new environment and create a new state.
         */
        Environment();

        vector<void (*)(void) > regFunctions;

        bool registerFunction(void(*fun)(void));
    };

    Environment &getEnvironment();
    void init();
}

#endif