/* lua-socket.c
 * author: xjdrew
 * date: 2014-07-10
 */

/*
This module provides an interface to Berkeley socket IPC.

Limitations:

- Only AF_INET, AF_INET6 address families are supported
- Only SOCK_STREAM, SOCK_DGRAM socket type are supported
- Only IPPROTO_TCP, IPPROTO_UDP protocal type are supported
- Don't support dns lookup, must be numerical network address

Module interface:
- socket.socket(family, type[, proto]) --> new socket object
- socket.AF_INET, socket.SOCK_STREAM, etc.: constants from <socket.h>
- an Internet socket address is a pair (hostname, port)
  where hostname can be anything recognized by gethostbyname()
  (including the dd.dd.dd.dd notation) and port is in host byte order
- where a hostname is returned, the dd.dd.dd.dd notation is used
- a UNIX domain socket address is a string specifying the pathname
*/

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include "lua.h"
#include "lauxlib.h"

#define SOCKET_METATABLE "socket_metatable"

#if !defined(NI_MAXHOST)
#define NI_MAXHOST 1025
#endif

#if !defined(NI_MAXSERV)
#define NI_MAXSERV 32
#endif

typedef struct _sock_t {
    int fd;
    int family;
    int type;
    int protocol;
} socket_t;

/* 
 *   internal function 
 */
inline static
socket_t* _getsock(lua_State *L, int index) {
    socket_t* sock = (socket_t*)luaL_checkudata(L, index, SOCKET_METATABLE);
    return sock;
}

static int
_getsockaddrarg(socket_t *sock, const char *host, const char *port, struct addrinfo **res) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = sock->family;
    hints.ai_socktype = sock->type;
    hints.ai_protocol = sock->protocol;
    hints.ai_flags = AI_NUMERICHOST;

    return getaddrinfo(host, port, &hints, res);
}

static int
_getsockaddrlen(socket_t *sock, socklen_t *len) {
    switch(sock->family) {
        case AF_INET:
            *len = sizeof(struct sockaddr_in);
            return 1;
        case AF_INET6:
            *len = sizeof(struct sockaddr_in6);
            return 1;
    }
    return 0;
}

static int
_makeaddr(lua_State *L, struct sockaddr *addr, int addrlen) {

}

/*
 *    end
 */

/*
 *   args: int domain, int type, int protocal
 */
static int
_socket(lua_State *L) {
    int family    = luaL_checkint(L, 1);
    int type      = luaL_checkint(L, 2);
    int protocol  = 0;
    if(lua_gettop(L) > 2) {
        protocol = luaL_checkint(L, 3);
    }

    int fd = socket(family, type, protocol);
    if(fd < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    socket_t *sock = (socket_t*) lua_newuserdata(L, sizeof(socket_t));
    luaL_getmetatable(L, SOCKET_METATABLE);
    lua_setmetatable(L, -2);

    sock->fd = fd;
    sock->family = family;
    sock->type = type;
    sock->protocol = protocol;
    return 1;
}

/* socket object methods */
static int
_sock_setblocking(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int block = lua_toboolean(L, 2);
    int flag = fcntl(sock->fd, F_GETFL, 0);
    if (block) {
        flag &= (~O_NONBLOCK);
    } else {
        flag |= O_NONBLOCK;
    }
    fcntl(sock->fd, F_SETFL, flag);
    return 0;
}

static int
_sock_connect(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    const char *host = luaL_checkstring(L, 2);
    luaL_checkint(L, 3);
    const char *port = lua_tostring(L, 3);

    struct addrinfo *res = 0;
    int err = _getsockaddrarg(sock, host, port, &res);
    if(err != 0) {
        lua_pushinteger(L, err);
        return 1;
    }

    err = connect(sock->fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if(err != 0) {
        lua_pushinteger(L, errno);
    } else {
        lua_pushinteger(L, err);
    }
    return 1;
}

static int
_sock_recv(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    size_t len = luaL_checkunsigned(L, 2);
    char* buf = lua_newuserdata(L, len);
    ssize_t nread = recv(sock->fd, buf, len, 0);
    if(nread < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushlstring(L, buf, nread);
    return 1;
}

static int
_sock_send(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    size_t len;
    const char* buf = luaL_checklstring(L, 2, &len);
    int flags = MSG_NOSIGNAL;

    int n = send(sock->fd, buf, len, flags);
    if(n < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushinteger(L, n);
    return 1;
}

static int
_sock_recvfrom(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    return 0;
}

static int
_sock_sendto(lua_State *L) {
    return 0;
}

static int
_sock_bind(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    const char *host = luaL_checkstring(L, 2);
    luaL_checkint(L, 3);
    const char *port = lua_tostring(L, 3);

    struct addrinfo *res = 0;
    int err = _getsockaddrarg(sock, host, port, &res);
    if(err != 0) {
        lua_pushinteger(L, err);
        return 1;
    }

    err = bind(sock->fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if(err != 0) {
        lua_pushinteger(L, errno);
    } else {
        lua_pushinteger(L, err);
    }
    return 1;
}

static int
_sock_listen(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int backlog = luaL_optnumber(L, 2, 5);
    int err = listen(sock->fd, backlog);
    if(err != 0) {
        lua_pushinteger(L, errno);
    } else {
        lua_pushinteger(L, err);
    }
    return 1;
}

static int
_sock_accept(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int fd = accept(sock->fd, NULL, NULL);
    if(fd < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    socket_t *nsock = (socket_t*) lua_newuserdata(L, sizeof(socket_t));
    luaL_getmetatable(L, SOCKET_METATABLE);
    lua_setmetatable(L, -2);

    nsock->fd = fd;
    nsock->family = sock->family;
    nsock->type = sock->type;
    nsock->protocol = sock->protocol;
    return 1;
}

static int
_sock_fileno(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    lua_pushinteger(L, sock->fd);
    return 1;
}

static int
_sock_getpeername(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    socklen_t len;
    if(!_getsockaddrlen(sock, &len)) {
        return luaL_error(L, "getpeername: bad family(%d)", sock->family);
    }

    struct sockaddr *addr = (struct addr*)lua_newuserdata(L, len);
    int err = getpeername(sock->fd, addr, len);
    if(err < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushlstring(L, addr, len);
    return 1;
}

static int
_sock_getsockname(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    socklen_t len;
    if(!_getsockaddrlen(sock, &len)) {
        return luaL_error(L, "getsockname: bad family(%d)", sock->family);
    }

    struct sockaddr *addr = (struct addr*)lua_newuserdata(L, len);
    int err = getsockname(sock->fd, addr, len);
    if(err < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushlstring(L, (char*)addr, len);
    return 1;
}

static int
_sock_getsockopt(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int level = luaL_checkint(L, 2);
    const char* optname = luaL_checkstring(L, 3);
    int buflen = luaL_optnumber(L, 4, 0);

    if(buflen < 0 || buflen > 1024) {
        return luaL_error(L, "getsockopt buflen out of range");
    }

    int err;
    if(buflen == 0) {
        int flag = 0;
        socklen_t flagsize = sizeof(flag);
        err = getsockopt(sock->fd, level, optname, (void*)&flag, &flagsize);
        if(err < 0) {
            goto failed;
        }
        lua_pushinteger(L, flag);
    } 
    else {
        void *optval = lua_newuserdata(L, buflen);
        err = getsockopt(sock->fd, level, optname, optval, buflen);
        if(err < 0) {
            goto failed;
        }
        lua_pushlstring(L, optval, buflen);
    }
    return 1;
failed:
    lua_pushnil(L);
    lua_pushinteger(L, errno);
    return 2;
}


static int
_sock_setsockopt(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int level = luaL_checkint(L, 2);
    const char* optname = luaL_checkstring(L, 3);
    luaL_checkany(L, 4);

    const char* buf;
    int buflen;

    int type = lua_type(L, 4);
    if(type == LUA_TSTRING) {
        buf = luaL_checklstring(L, 4, &buflen);
    } else if(type == LUA_TNUMBER) {
        int flag = luaL_checkint(L, 4);
        buf = (const char*)&flag;
        buflen = sizeof(flag);
    } else {
        return luaL_error(L, "setsockopt: unsupported arg 4 type");
    }

    int err = setsockopt(sock->fd, level, optname, buf, buflen);
    if(err < 0) {
        lua_pushboolean(L, 0);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int
_sock_close(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int fd = sock->fd;
    if(fd != -1) {
        sock->fd = -1;
        close(fd);
    }
    return 0;
}

static int
_sock_tostring(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    lua_pushfstring(L, "socket: %p", sock);
    return 1;
}

/* end */
static void
_add_int_constant(lua_State *L, const char* name, int value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}

int
luaopen_sock_c(lua_State *L) {
    luaL_checkversion(L);
        
    // +construct socket metatable
    luaL_Reg socket_mt[] = {
        {"__gc", _sock_close},
        {"__tostring",  _sock_tostring},
        {NULL, NULL}
    };

    luaL_Reg socket_methods[] = {
        {"setblocking", _sock_setblocking},
        {"connect", _sock_connect},

        {"recv", _sock_recv},
        {"send", _sock_send},

        {"recvfrom", _sock_recvfrom},
        {"sendto", _sock_sendto},

        {"bind", _sock_bind},
        {"listen", _sock_listen},
        {"accept", _sock_accept},

        {"fileno", _sock_fileno},
        {"getpeername", _sock_getpeername},
        {"getsockname", _sock_getsockname},

        {"getsockopt", _sock_getsockopt},
        {"setsockopt", _sock_setsockopt},

        {"close", _sock_close},
        {NULL, NULL}
    };

    luaL_newmetatable(L, SOCKET_METATABLE);
    luaL_setfuncs(L, socket_mt, 0);

    luaL_newlib(L, socket_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
    // +end

    luaL_Reg l[] = {
        {"socket", _socket},
        {NULL, NULL}
    };

    luaL_newlib(L, l);
    // address family
    _add_int_constant(L, "AF_INET", AF_INET);
    _add_int_constant(L, "AF_INET6", AF_INET6);

    // socket type
    _add_int_constant(L, "SOCK_STREAM", SOCK_STREAM);
    _add_int_constant(L, "SOCK_DGRAM", SOCK_DGRAM);

    // protocal type
    _add_int_constant(L, "IPPROTO_TCP", IPPROTO_TCP);
    _add_int_constant(L, "IPPROTO_UDP", IPPROTO_UDP);

    return 1;
}
