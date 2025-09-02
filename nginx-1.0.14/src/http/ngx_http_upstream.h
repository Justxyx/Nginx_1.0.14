
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HTTP_UPSTREAM_H_INCLUDED_
#define _NGX_HTTP_UPSTREAM_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_event_connect.h>
#include <ngx_event_pipe.h>
#include <ngx_http.h>


#define NGX_HTTP_UPSTREAM_FT_ERROR           0x00000002
#define NGX_HTTP_UPSTREAM_FT_TIMEOUT         0x00000004
#define NGX_HTTP_UPSTREAM_FT_INVALID_HEADER  0x00000008
#define NGX_HTTP_UPSTREAM_FT_HTTP_500        0x00000010
#define NGX_HTTP_UPSTREAM_FT_HTTP_502        0x00000020
#define NGX_HTTP_UPSTREAM_FT_HTTP_503        0x00000040
#define NGX_HTTP_UPSTREAM_FT_HTTP_504        0x00000080
#define NGX_HTTP_UPSTREAM_FT_HTTP_404        0x00000100
#define NGX_HTTP_UPSTREAM_FT_UPDATING        0x00000200
#define NGX_HTTP_UPSTREAM_FT_BUSY_LOCK       0x00000400
#define NGX_HTTP_UPSTREAM_FT_MAX_WAITING     0x00000800
#define NGX_HTTP_UPSTREAM_FT_NOLIVE          0x40000000
#define NGX_HTTP_UPSTREAM_FT_OFF             0x80000000

#define NGX_HTTP_UPSTREAM_FT_STATUS          (NGX_HTTP_UPSTREAM_FT_HTTP_500  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_502  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_503  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_504  \
                                             |NGX_HTTP_UPSTREAM_FT_HTTP_404)

#define NGX_HTTP_UPSTREAM_INVALID_HEADER     40


#define NGX_HTTP_UPSTREAM_IGN_XA_REDIRECT    0x00000002
#define NGX_HTTP_UPSTREAM_IGN_XA_EXPIRES     0x00000004
#define NGX_HTTP_UPSTREAM_IGN_EXPIRES        0x00000008
#define NGX_HTTP_UPSTREAM_IGN_CACHE_CONTROL  0x00000010
#define NGX_HTTP_UPSTREAM_IGN_SET_COOKIE     0x00000020
#define NGX_HTTP_UPSTREAM_IGN_XA_LIMIT_RATE  0x00000040
#define NGX_HTTP_UPSTREAM_IGN_XA_BUFFERING   0x00000080
#define NGX_HTTP_UPSTREAM_IGN_XA_CHARSET     0x00000100


typedef struct {
    ngx_msec_t                       bl_time;
    ngx_uint_t                       bl_state;

    ngx_uint_t                       status;
    time_t                           response_sec;
    ngx_uint_t                       response_msec;
    off_t                            response_length;

    ngx_str_t                       *peer;
} ngx_http_upstream_state_t;


typedef struct {
    ngx_hash_t                       headers_in_hash;
    ngx_array_t                      upstreams;
                                             /* ngx_http_upstream_srv_conf_t */
} ngx_http_upstream_main_conf_t;

typedef struct ngx_http_upstream_srv_conf_s  ngx_http_upstream_srv_conf_t;

typedef ngx_int_t (*ngx_http_upstream_init_pt)(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us);
typedef ngx_int_t (*ngx_http_upstream_init_peer_pt)(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);


typedef struct {
    ngx_http_upstream_init_pt        init_upstream;
    ngx_http_upstream_init_peer_pt   init;
    void                            *data;
} ngx_http_upstream_peer_t;


typedef struct {
    ngx_addr_t                      *addrs;
    ngx_uint_t                       naddrs;
    ngx_uint_t                       weight;
    ngx_uint_t                       max_fails;
    time_t                           fail_timeout;

    unsigned                         down:1;
    unsigned                         backup:1;
} ngx_http_upstream_server_t;


#define NGX_HTTP_UPSTREAM_CREATE        0x0001
#define NGX_HTTP_UPSTREAM_WEIGHT        0x0002
#define NGX_HTTP_UPSTREAM_MAX_FAILS     0x0004
#define NGX_HTTP_UPSTREAM_FAIL_TIMEOUT  0x0008
#define NGX_HTTP_UPSTREAM_DOWN          0x0010
#define NGX_HTTP_UPSTREAM_BACKUP        0x0020


struct ngx_http_upstream_srv_conf_s {
    ngx_http_upstream_peer_t         peer;
    void                           **srv_conf;

    ngx_array_t                     *servers;  /* ngx_http_upstream_server_t */

    ngx_uint_t                       flags;
    ngx_str_t                        host;
    u_char                          *file_name;
    ngx_uint_t                       line;
    in_port_t                        port;
    in_port_t                        default_port;
};


typedef struct {
    // 指向上游（upstream）server 配置（如 proxy_pass 的目标）
    ngx_http_upstream_srv_conf_t    *upstream;

    // 与上游服务器建立连接的超时时间（毫秒）
    ngx_msec_t                       connect_timeout;
    // 向上游服务器发送请求的超时时间（毫秒）
    ngx_msec_t                       send_timeout;
    // 从上游服务器接收响应的超时时间（毫秒）
    ngx_msec_t                       read_timeout;
    // 通用的超时时间字段（某些特殊场景使用）
    ngx_msec_t                       timeout;

    // TCP 发送缓冲区低水位标记（通常很少使用）
    size_t                           send_lowat;
    // 单个缓冲区大小（用于存放上游响应）
    size_t                           buffer_size;

    // 缓冲区相关配置
    size_t                           busy_buffers_size;       // 已经写入但还未发送的缓冲区上限
    size_t                           max_temp_file_size;      // 响应过大时写入临时文件的最大值
    size_t                           temp_file_write_size;    // 每次写临时文件的大小

    // *_conf 是从配置文件解析出来的原始值
    size_t                           busy_buffers_size_conf;
    size_t                           max_temp_file_size_conf;
    size_t                           temp_file_write_size_conf;

    // 上游响应的缓冲区链配置（个数、大小等）
    ngx_bufs_t                       bufs;

    // 控制哪些响应头会被忽略（例如 "Date", "Server"）
    ngx_uint_t                       ignore_headers;
    // 出错时是否尝试下一个上游（如 502/504 时 failover）
    ngx_uint_t                       next_upstream;
    // 写文件时的权限（如 proxy_store）
    ngx_uint_t                       store_access;
    // 是否开启缓冲（proxy_buffering on/off）
    ngx_flag_t                       buffering;
    // 是否转发客户端请求头给上游
    ngx_flag_t                       pass_request_headers;
    // 是否转发客户端请求体给上游
    ngx_flag_t                       pass_request_body;

    // 是否忽略客户端主动断开连接（常用于长连接/后台任务）
    ngx_flag_t                       ignore_client_abort;
    // 是否拦截错误（如 404/500）交给 Nginx 处理，而不是透传给客户端
    ngx_flag_t                       intercept_errors;
    // 是否使用循环写临时文件（减少磁盘碎片）
    ngx_flag_t                       cyclic_temp_file;

    // 临时文件存放路径（proxy_temp_path 指令）
    ngx_path_t                      *temp_path;

    // 隐藏/传递响应头相关配置
    ngx_hash_t                       hide_headers_hash;   // 已隐藏的 header 哈希表
    ngx_array_t                     *hide_headers;        // 要隐藏的 headers 列表
    ngx_array_t                     *pass_headers;        // 要强制传递的 headers 列表

    // 绑定本地地址（如 proxy_bind）
    ngx_addr_t                      *local;

#if (NGX_HTTP_CACHE)
    // 缓存相关配置
    ngx_shm_zone_t                  *cache;               // 共享内存缓存区

    ngx_uint_t                       cache_min_uses;      // 缓存最少使用次数
    ngx_uint_t                       cache_use_stale;     // 是否使用过期缓存
    ngx_uint_t                       cache_methods;       // 哪些 HTTP 方法可缓存

    ngx_array_t                     *cache_valid;         // 缓存有效时间规则
    ngx_array_t                     *cache_bypass;        // 跳过缓存条件
    ngx_array_t                     *no_cache;            // 不缓存条件
#endif

    // proxy_store 文件路径相关（动态拼接路径时用）
    ngx_array_t                     *store_lengths;
    ngx_array_t                     *store_values;

    // store 标志位（-1 禁用，0 默认，1 启用）
    signed                           store:2;
    // 是否拦截 404 错误（交给 Nginx 自己处理）
    unsigned                         intercept_404:1;
    // 是否动态改变缓冲策略
    unsigned                         change_buffering:1;

#if (NGX_HTTP_SSL)
    // SSL 配置
    ngx_ssl_t                       *ssl;                 // SSL 上下文
    ngx_flag_t                       ssl_session_reuse;   // 是否重用 SSL 会话
#endif

    // 模块名（便于日志输出区分，比如 "proxy", "fastcgi"）
    ngx_str_t                        module;
} ngx_http_upstream_conf_t;


typedef struct {
    ngx_str_t                        name;
    ngx_http_header_handler_pt       handler;
    ngx_uint_t                       offset;
    ngx_http_header_handler_pt       copy_handler;
    ngx_uint_t                       conf;
    ngx_uint_t                       redirect;  /* unsigned   redirect:1; */
} ngx_http_upstream_header_t;


typedef struct {
    ngx_list_t                       headers;

    ngx_uint_t                       status_n;
    ngx_str_t                        status_line;  // /* 状态行字符串，例如 "HTTP/1.1 302 Found"。

    ngx_table_elt_t                 *status;
    ngx_table_elt_t                 *date;
    ngx_table_elt_t                 *server;
    ngx_table_elt_t                 *connection;

    ngx_table_elt_t                 *expires;
    ngx_table_elt_t                 *etag;
    ngx_table_elt_t                 *x_accel_expires;
    ngx_table_elt_t                 *x_accel_redirect;
    ngx_table_elt_t                 *x_accel_limit_rate;

    ngx_table_elt_t                 *content_type;
    ngx_table_elt_t                 *content_length;

    ngx_table_elt_t                 *last_modified;
    ngx_table_elt_t                 *location;
    ngx_table_elt_t                 *accept_ranges;
    ngx_table_elt_t                 *www_authenticate;

#if (NGX_HTTP_GZIP)
    ngx_table_elt_t                 *content_encoding;
#endif

    off_t                            content_length_n;

    ngx_array_t                      cache_control;
} ngx_http_upstream_headers_in_t;


typedef struct {
    ngx_str_t                        host;
    in_port_t                        port;
    ngx_uint_t                       no_port; /* unsigned no_port:1 */

    ngx_uint_t                       naddrs;
    in_addr_t                       *addrs;

    struct sockaddr                 *sockaddr;
    socklen_t                        socklen;

    ngx_resolver_ctx_t              *ctx;
} ngx_http_upstream_resolved_t;


typedef void (*ngx_http_upstream_handler_pt)(ngx_http_request_t *r,
    ngx_http_upstream_t *u);


struct ngx_http_upstream_s {
    /* 上游事件处理回调 */
    ngx_http_upstream_handler_pt read_event_handler;   /* 当上游可读时触发 */
    ngx_http_upstream_handler_pt write_event_handler;  /* 当上游可写时触发 */

    ngx_peer_connection_t peer;   /* 上游服务器连接 */

    ngx_event_pipe_t *pipe;       /* pipe 模式：直接把上游数据拷贝到下游（大文件/缓冲转发时用） */

    ngx_chain_t *request_bufs;    /* 待发送给上游的请求内容链表（HTTP 请求行、头、body） */

    ngx_output_chain_ctx_t output; /* 输出过滤链，用于处理响应 body */
    ngx_chain_writer_ctx_t writer; /* 发送响应数据到下游的 writer 上下文 */

    ngx_http_upstream_conf_t *conf;  /* upstream 配置参数（缓冲区大小、超时、隐藏头部等） */

    ngx_http_upstream_headers_in_t headers_in; /* 已解析的上游响应头（会映射到 r->headers_out） */

    ngx_http_upstream_resolved_t *resolved; /* 动态解析得到的上游服务器地址 */

    ngx_buf_t buffer;     /* 接收上游响应时的缓冲区 */
    size_t length;        /* 需要接收的数据长度 */

    /* 输出缓冲链 */
    ngx_chain_t *out_bufs;  /* 已准备好待发送的 chain */
    ngx_chain_t *busy_bufs; /* 已发送但还在发送中的 chain */
    ngx_chain_t *free_bufs; /* 可复用的空闲缓冲区 */

    /* body filter 回调 */
    ngx_int_t (*input_filter_init)(void *data);      /* 初始化 input filter */
    ngx_int_t (*input_filter)(void *data, ssize_t bytes); /* 收到上游响应 body 时调用 */
    void *input_filter_ctx;                          /* input_filter 的上下文 */

#if (NGX_HTTP_CACHE)
    ngx_int_t (*create_key)(ngx_http_request_t *r);  /* 缓存 key 的构造函数 */
#endif

    /* 生命周期相关回调 */
    ngx_int_t (*create_request)(ngx_http_request_t *r); /* 构造发往上游的请求（HTTP 协议头等） */
    ngx_int_t (*reinit_request)(ngx_http_request_t *r); /* 重新初始化请求（重试上游时用） */
    ngx_int_t (*process_header)(ngx_http_request_t *r); /* 解析上游响应头 */
    void (*abort_request)(ngx_http_request_t *r);       /* 请求中止时调用 */
    void (*finalize_request)(ngx_http_request_t *r,
                             ngx_int_t rc);             /* upstream 生命周期结束时调用 */

    ngx_int_t (*rewrite_redirect)(ngx_http_request_t *r,
                                  ngx_table_elt_t *h, size_t prefix); /* 处理上游返回的重定向 */

    ngx_msec_t timeout; /* 与上游交互的超时时间 */

    ngx_http_upstream_state_t *state; /* 当前 upstream 的状态（记录时间、字节数等统计信息） */

    ngx_str_t method;  /* 上游请求的方法（GET/POST/…） */
    ngx_str_t schema;  /* 协议（http/https） */
    ngx_str_t uri;     /* 请求的 URI */

    ngx_http_cleanup_pt *cleanup; /* 清理函数链表，请求销毁时调用 */

    /* 标志位 */
    unsigned store:1;        /* 是否存储响应到文件 */
    unsigned cacheable:1;    /* 响应是否可缓存 */
    unsigned accel:1;        /* 是否使用加速模式 */
    unsigned ssl:1;          /* 是否使用 SSL 与上游通信 */
#if (NGX_HTTP_CACHE)
    unsigned cache_status:3; /* 缓存状态 */
#endif

    unsigned buffering:1;    /* 是否启用缓冲模式（多 buffer 存储响应） */

    unsigned request_sent:1; /* 是否已发送请求到上游 */
    unsigned header_sent:1;  /* 是否已发送 header 到客户端 */
};


typedef struct {
    ngx_uint_t                      status;
    ngx_uint_t                      mask;
} ngx_http_upstream_next_t;


ngx_int_t ngx_http_upstream_header_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

ngx_int_t ngx_http_upstream_create(ngx_http_request_t *r);
void ngx_http_upstream_init(ngx_http_request_t *r);
ngx_http_upstream_srv_conf_t *ngx_http_upstream_add(ngx_conf_t *cf,
    ngx_url_t *u, ngx_uint_t flags);
char *ngx_http_upstream_bind_set_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
ngx_int_t ngx_http_upstream_hide_headers_hash(ngx_conf_t *cf,
    ngx_http_upstream_conf_t *conf, ngx_http_upstream_conf_t *prev,
    ngx_str_t *default_hide_headers, ngx_hash_init_t *hash);


#define ngx_http_conf_upstream_srv_conf(uscf, module)                         \
    uscf->srv_conf[module.ctx_index]


extern ngx_module_t        ngx_http_upstream_module;
extern ngx_conf_bitmask_t  ngx_http_upstream_cache_method_mask[];
extern ngx_conf_bitmask_t  ngx_http_upstream_ignore_headers_masks[];


#endif /* _NGX_HTTP_UPSTREAM_H_INCLUDED_ */
