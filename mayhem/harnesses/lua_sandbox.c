/* Linked into every Element fuzz harness with -Wl,--wrap=luaopen_{base,os,io,package}: every Lua
 * state Element opens (ScriptNode, DSPScript::validate's own sol::state, session-loaded script nodes)
 * loads its standard libraries through these, so fuzz-supplied Lua can't touch the filesystem,
 * spawn processes, exit the process, load native code or change process-global state (locale).
 * Pure computation (string, table, math, utf8, coroutine, debug, os.clock/time/date) is unchanged. */
#include "lua.h"
#include "lauxlib.h"

int __real_luaopen_base (lua_State* L);
int __real_luaopen_os (lua_State* L);
int __real_luaopen_io (lua_State* L);
int __real_luaopen_package (lua_State* L);

static void clear_fields (lua_State* L, const char* const* names)
{
    for (; *names != NULL; ++names)
    {
        lua_pushnil (L);
        lua_setfield (L, -2, *names);
    }
}

int __wrap_luaopen_base (lua_State* L)
{
    static const char* const unsafe[] = { "dofile", "loadfile", NULL };
    const int n = __real_luaopen_base (L);
    clear_fields (L, unsafe);
    return n;
}

int __wrap_luaopen_os (lua_State* L)
{
    static const char* const unsafe[] = { "execute", "exit", "remove", "rename", "tmpname",
                                          "setlocale", "getenv", NULL };
    const int n = __real_luaopen_os (L);
    clear_fields (L, unsafe);
    return n;
}

/* io keeps only what works on already-open handles (DSPScript::save/restore run their state
 * through io.tmpfile + io.input/io.output). Gone: io.open/popen/lines and io.input/io.output with a
 * file NAME (open or truncate any path). Default input is an empty anonymous tmpfile instead of
 * stdin, so io.read() cannot block; io.stdout/stderr are non-closable std handles in Lua. */
static int guarded_redirect (lua_State* L)
{
    if (lua_type (L, 1) == LUA_TSTRING)
        return luaL_error (L, "fuzz sandbox: io.input/io.output by file name is disabled");
    const int nargs = lua_gettop (L);
    lua_pushvalue (L, lua_upvalueindex (1));
    lua_insert (L, 1);
    lua_call (L, nargs, LUA_MULTRET);
    return lua_gettop (L);
}

static void guard_field (lua_State* L, const char* name)
{
    lua_getfield (L, -1, name);
    lua_pushcclosure (L, guarded_redirect, 1);
    lua_setfield (L, -2, name);
}

int __wrap_luaopen_io (lua_State* L)
{
    static const char* const unsafe[] = { "open", "popen", "lines", "stdin", NULL };
    const int n = __real_luaopen_io (L);
    lua_getfield (L, -1, "tmpfile");
    lua_call (L, 0, 1);
    lua_setfield (L, LUA_REGISTRYINDEX, "_IO_input" /* liolib.c IO_INPUT */);
    clear_fields (L, unsafe);
    guard_field (L, "input");
    guard_field (L, "output");
    return n;
}

int __wrap_luaopen_package (lua_State* L)
{
    static const char* const unsafe[] = { "loadlib", NULL };
    const int n = __real_luaopen_package (L);
    clear_fields (L, unsafe);
    lua_pushliteral (L, "");
    lua_setfield (L, -2, "path");
    lua_pushliteral (L, "");
    lua_setfield (L, -2, "cpath");
    return n;
}
