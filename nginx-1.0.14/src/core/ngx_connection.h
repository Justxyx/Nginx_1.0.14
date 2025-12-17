
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CONNECTION_H_INCLUDED_
#define _NGX_CONNECTION_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct ngx_listening_s  ngx_listening_t;

/*
 * ngx_listening_t —— 监听 socket 对象结构体
 *
 * 描述：
 *   每个 ngx_listening_t 对应一个监听端口（socket），
 *   保存了 socket 配置、状态、事件处理函数等信息。
 *
 * 主要用途：
 *   1. 保存监听的地址、端口、backlog 等信息
 *   2. 保存 socket 对应的 connection 对象
 *   3. 保存事件模块中注册事件的指针（previous）
 *   4. 为 worker 进程提供独立的监听状态
 */
struct ngx_listening_s {
    ngx_socket_t        fd;                 /* 套接字文件描述符 */

    struct sockaddr    *sockaddr;           /* 监听地址 */
    socklen_t           socklen;            /* sockaddr 大小 */
    size_t              addr_text_max_len;  /* 地址文本长度上限 */
    ngx_str_t           addr_text;          /* 地址文本 */

    int                 type;               /* socket 类型（TCP/UDP） */
    int                 backlog;            /* listen backlog */
    int                 rcvbuf;             /* 接收缓冲区大小 */
    int                 sndbuf;             /* 发送缓冲区大小 */

    ngx_connection_handler_pt handler;      /* 接收到连接后的回调函数 */
    void               *servers;           /* 对应模块的服务数组，如 ngx_http_in_addr_t */

    ngx_log_t           log;                /* 日志结构 */
    ngx_log_t          *logp;               /* 指向日志 */

    size_t              pool_size;          /* 每个连接的内存池大小 */
    size_t              post_accept_buffer_size; /* Windows 特性: AcceptEx 预读缓冲区 */
    ngx_msec_t          post_accept_timeout;      /* Windows 特性: 延迟 accept 超时 */

    ngx_listening_t    *previous;           /*
                                                master 进程已经有监听 socket，然后执行平滑重启（reload）或热升级时才有意义：
                                                指向之前注册的事件对象
                                                master 中会保存原始注册事件
                                                worker fork 后需要清空为 NULL
                                                否则 worker 会误操作不存在的事件
                                             */
    ngx_connection_t   *connection;         /* 当前 socket 对应的 ngx_connection_t 对象 */

    unsigned            open:1;             /* 是否已打开 */
    unsigned            remain:1;           /* 保留标志 */
    unsigned            ignore:1;           /* 是否忽略 */
    unsigned            bound:1;            /* 是否已 bind */
    unsigned            inherited:1;        /* 是否继承自 previous process */
    unsigned            nonblocking_accept:1;
    unsigned            listen:1;
    unsigned            nonblocking:1;
    unsigned            shared:1;           /* 是否多线程或多进程共享 */
    unsigned            addr_ntop:1;

#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
    unsigned            ipv6only:2;         /* IPv6 仅模式 */
#endif

#if (NGX_HAVE_DEFERRED_ACCEPT)
    unsigned            deferred_accept:1;
    unsigned            delete_deferred:1;
    unsigned            add_deferred:1;
#ifdef SO_ACCEPTFILTER
    char               *accept_filter;      /* AcceptFilter 名称 */
#endif
#endif
#if (NGX_HAVE_SETFIB)
    int                 setfib;             /* 仅 FreeBSD */
#endif
};


typedef enum {
     NGX_ERROR_ALERT = 0,
     NGX_ERROR_ERR,
     NGX_ERROR_INFO,
     NGX_ERROR_IGNORE_ECONNRESET,
     NGX_ERROR_IGNORE_EINVAL
} ngx_connection_log_error_e;


typedef enum {
     NGX_TCP_NODELAY_UNSET = 0,
     NGX_TCP_NODELAY_SET,
     NGX_TCP_NODELAY_DISABLED
} ngx_connection_tcp_nodelay_e;


typedef enum {
     NGX_TCP_NOPUSH_UNSET = 0,
     NGX_TCP_NOPUSH_SET,
     NGX_TCP_NOPUSH_DISABLED
} ngx_connection_tcp_nopush_e;


#define NGX_LOWLEVEL_BUFFERED  0x0f
#define NGX_SSL_BUFFERED       0x01


struct ngx_connection_s {
    void               *data;
    ngx_event_t        *read;
    ngx_event_t        *write;

    ngx_socket_t        fd;

    ngx_recv_pt         recv;
    ngx_send_pt         send;
    ngx_recv_chain_pt   recv_chain;
    ngx_send_chain_pt   send_chain;

    ngx_listening_t    *listening;

    off_t               sent;

    ngx_log_t          *log;

    ngx_pool_t         *pool;

    struct sockaddr    *sockaddr;
    socklen_t           socklen;
    ngx_str_t           addr_text;

#if (NGX_SSL)
    ngx_ssl_connection_t  *ssl;
#endif

    struct sockaddr    *local_sockaddr;

    ngx_buf_t          *buffer;

    ngx_queue_t         queue;

    ngx_atomic_uint_t   number;

    ngx_uint_t          requests;

    unsigned            buffered:8;

    unsigned            log_error:3;     /* ngx_connection_log_error_e */

    unsigned            single_connection:1;
    unsigned            unexpected_eof:1;
    unsigned            timedout:1;
    unsigned            error:1;
    unsigned            destroyed:1;

    unsigned            idle:1;
    unsigned            reusable:1;
    unsigned            close:1;

    unsigned            sendfile:1;
    unsigned            sndlowat:1;
    unsigned            tcp_nodelay:2;   /* ngx_connection_tcp_nodelay_e */
    unsigned            tcp_nopush:2;    /* ngx_connection_tcp_nopush_e */

#if (NGX_HAVE_IOCP)
    unsigned            accept_context_updated:1;
#endif

#if (NGX_HAVE_AIO_SENDFILE)
    unsigned            aio_sendfile:1;
    ngx_buf_t          *busy_sendfile;
#endif

#if (NGX_THREADS)
    ngx_atomic_t        lock;
#endif
};


ngx_listening_t *ngx_create_listening(ngx_conf_t *cf, void *sockaddr,
    socklen_t socklen);
ngx_int_t ngx_set_inherited_sockets(ngx_cycle_t *cycle);
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle);
void ngx_configure_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_listening_sockets(ngx_cycle_t *cycle);
void ngx_close_connection(ngx_connection_t *c);
ngx_int_t ngx_connection_local_sockaddr(ngx_connection_t *c, ngx_str_t *s,
    ngx_uint_t port);
ngx_int_t ngx_connection_error(ngx_connection_t *c, ngx_err_t err, char *text);

ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log);
void ngx_free_connection(ngx_connection_t *c);

void ngx_reusable_connection(ngx_connection_t *c, ngx_uint_t reusable);

#endif /* _NGX_CONNECTION_H_INCLUDED_ */
