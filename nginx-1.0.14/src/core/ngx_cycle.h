
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_CYCLE_H_INCLUDED_
#define _NGX_CYCLE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


#ifndef NGX_CYCLE_POOL_SIZE
#define NGX_CYCLE_POOL_SIZE     16384
#endif


#define NGX_DEBUG_POINTS_STOP   1
#define NGX_DEBUG_POINTS_ABORT  2


typedef struct ngx_shm_zone_s  ngx_shm_zone_t;

typedef ngx_int_t (*ngx_shm_zone_init_pt) (ngx_shm_zone_t *zone, void *data);

struct ngx_shm_zone_s {
    void                     *data;
    ngx_shm_t                 shm;
    ngx_shm_zone_init_pt      init;
    void                     *tag;
};

/*
 * Nginx 世界的“根节点（root object）
 * 把配置、内存、连接、事件、日志、文件、共享内存全部组织起来，
 *  按 Nginx 启动顺序分析 ngx_cycle_t 的创建与继承关系：
 *
 * -------------------------------------------------------------------------
 *  1️⃣ 启动阶段（master 创建第一个 cycle）
 * -------------------------------------------------------------------------
 *
 *      命令：
 *          nginx -c /etc/nginx/nginx.conf
 *
 *      步骤：
 *          - master 进程启动
 *          - 调用 ngx_init_cycle(NULL)
 *          - 创建第一个 ngx_cycle_t（称作“初始化 cycle”）
 *              · 解析配置文件
 *              · 打开日志文件
 *              · 创建监听端口（socket）
 *              · 加载模块配置
 *
 *      然后：
 *          - master 把该 cycle 保存为全局变量 ngx_cycle
 *          - fork 出多个 worker 进程
 *          - 每个 worker 会复制（继承）这个 ngx_cycle_t 的内容
 *
 * -------------------------------------------------------------------------
 *  2️⃣ worker 进程阶段
 * -------------------------------------------------------------------------
 *
 *      - worker 进程启动后，会继承 master 的内存空间副本（fork 的特性）
 *
 *      因此：
 *          master:  ngx_cycle(master)
 *          worker1: ngx_cycle(worker1) ← fork 复制
 *          worker2: ngx_cycle(worker2) ← fork 复制
 *
 *      注意：
 *          - 每个 worker 都有自己的 ngx_cycle_t（独立副本）
 *          - 它们不会共享指针里的资源（例如内存池 pool 是独立的）
 *          - 只有部分资源（如 shared_memory）是跨进程共享的
 *
 * -------------------------------------------------------------------------
 *  3️⃣ reload（重新加载配置）
 * -------------------------------------------------------------------------
 *
 *      命令：
 *          nginx -s reload
 *
 *      流程：
 *          - master 不会立即终止 worker，而是执行平滑重载：
 *
 *              1. master 调用 ngx_init_cycle(old_cycle)
 *                  → 创建一个新的 ngx_cycle_t 对象（new cycle）
 *                  → 重新加载配置文件
 *                  → 打开新日志、创建新监听端口
 *
 *              2. 新的 cycle 完成后：
 *                     ngx_cycle → 指向 new_cycle
 *                     old_cycle → 保存旧的（方便资源过渡）
 *
 *              3. master 通知 worker 平滑重启（优雅退出）
 *                     - 新 worker 使用 new_cycle（新配置）
 *                     - 旧 worker 继续处理未完成请求（旧配置）
 *
 *              4. 最终：
 *                     - 旧 worker 处理完旧请求后退出
 *                     - old_cycle 被释放
 *                     - 新 worker 接管所有流量
 *
 * reload 时 master 确实会创建一个新的 ngx_cycle_t 结构体，
 * 而 新 worker 进程是基于这个新结构体（new cycle）fork 出来的。
 *
 */
struct ngx_cycle_s {

    /*
     * ---------------------------------------------------------------------
     * 各模块配置上下文
     * ---------------------------------------------------------------------
        cycle
        │
        └── conf_ctx ****
             │
             ├── [core_module.index]  →  ngx_core_conf_t *
             ├── [event_module.index] →  ngx_event_conf_t *
             ├── [http_module.index]  →  ngx_http_conf_ctx_t *
             │                           │
             │                           ├── main_conf[ctx_index] → 各子模块 main_conf
             │                           ├── srv_conf[ctx_index]  → 各子模块 srv_conf
             │                           └── loc_conf[ctx_index]  → 各子模块 loc_conf
             │
             └── [stream_module.index] → ngx_stream_conf_ctx_t *
     */
    void                  ****conf_ctx;

    /*
     * 内存池（整个 cycle 生命周期内有效）
     * 所有临时结构体、链表、数组等均从此池分配。
     */
    ngx_pool_t               *pool;

    /*
     * ---------------------------------------------------------------------
     * 日志系统
     * ---------------------------------------------------------------------
     * log      → 当前正在使用的日志对象指针
     * new_log  → 当 reload 时暂存新日志对象
     * ---------------------------------------------------------------------
     */
    ngx_log_t                *log;
    ngx_log_t                 new_log;

    /*
     * ---------------------------------------------------------------------
     * 文件与连接管理
     * ---------------------------------------------------------------------
     * files[]              : 文件描述符表 (fd -> connection)
     * free_connections     : 空闲连接链表
     * free_connection_n    : 当前空闲连接数量
     * reusable_connections_queue : 可复用连接队列（keepalive 等）
     * ---------------------------------------------------------------------
     */
    ngx_connection_t        **files;
    ngx_connection_t         *free_connections;
    ngx_uint_t                free_connection_n;

    ngx_queue_t               reusable_connections_queue;

    /*
     * ---------------------------------------------------------------------
     * 监听端口、路径、文件、共享内存等系统资源
     * ---------------------------------------------------------------------
     * listening   : 所有监听的 socket（端口、地址）
     * pathes      : 所有 Nginx 使用的目录路径
     * open_files  : 打开的文件列表（日志、pid、配置等）
     * shared_memory : 共享内存列表（缓存、状态、限速等）
     * ---------------------------------------------------------------------
     */
    ngx_array_t               listening;
    ngx_array_t               pathes;
    ngx_list_t                open_files;
    ngx_list_t                shared_memory;

    /*
     * ---------------------------------------------------------------------
     * 连接与事件数组
     * ---------------------------------------------------------------------
     * connection_n  : 当前连接总数上限
     * files_n       : 文件描述符数量上限
     *
     * connections[] : 所有连接对象数组
     * read_events[] : 对应连接的读事件数组
     * write_events[]: 对应连接的写事件数组
     * ---------------------------------------------------------------------
     */
    ngx_uint_t                connection_n;
    ngx_uint_t                files_n;

    ngx_connection_t         *connections;
    ngx_event_t              *read_events;
    ngx_event_t              *write_events;

    /*
     * ---------------------------------------------------------------------
     * old_cycle
     * ---------------------------------------------------------------------
     * 指向上一个 cycle（reload 时用）
     * 在新配置加载后仍需保留旧资源以保证平滑过渡。
     * 当旧 worker 全部退出后，old_cycle 被释放。
     * ---------------------------------------------------------------------
     */
    ngx_cycle_t              *old_cycle;

    /*
     * ---------------------------------------------------------------------
     * Nginx 运行参数信息
     * ---------------------------------------------------------------------
     * conf_file   : 当前配置文件路径
     * conf_param  : 命令行传入的参数（如 -g）
     * conf_prefix : 配置文件路径前缀（通常是 conf/）
     * prefix      : 安装前缀路径（例如 /usr/local/nginx/）
     * lock_file   : 进程锁文件路径（如 nginx.lock）
     * hostname    : 主机名
     * ---------------------------------------------------------------------
     */
    ngx_str_t                 conf_file;
    ngx_str_t                 conf_param;
    ngx_str_t                 conf_prefix;
    ngx_str_t                 prefix;
    ngx_str_t                 lock_file;
    ngx_str_t                 hostname;
};

/*
 * ngx_core_module 的配置结构体
 * 1. 创建时机： 由核心模块 ngx_core_module 的上下文函数负责创建
 * 2. 填充时机： ngx_conf_parse(&conf, &cycle->conf_file);  读取配置文件中每一行，若属于核心模块的指令，
 *    就会触发相应的 set 回调函数（例如 ngx_conf_set_flag_slot、ngx_conf_set_str_slot 等），
 *    从而把值写入 ngx_core_conf_t 中对应的字段。
 * 3. 访问方式：  ngx_core_conf_t *ccf;
 */
typedef struct {
    ngx_flag_t               daemon;              // 是否以守护进程方式运行（daemon on/off）
    ngx_flag_t               master;              // 是否启用 master/worker 模式（master_process on/off） master 单进程模式

    ngx_msec_t               timer_resolution;    // 定时器分辨率（timer_resolution 100ms）

    ngx_int_t                worker_processes;    // worker 进程数（worker_processes N）
    ngx_int_t                debug_points;        // 调试断点设置（debug_points stop/abort）

    ngx_int_t                rlimit_nofile;       // 每进程最大文件句柄数（worker_rlimit_nofile）
    ngx_int_t                rlimit_sigpending;   // 挂起信号上限（worker_rlimit_sigpending）
    off_t                    rlimit_core;         // core dump 文件大小限制（worker_rlimit_core）

    int                      priority;            // 进程优先级（worker_priority）

    ngx_uint_t               cpu_affinity_n;      // CPU 亲和掩码数量（worker_cpu_affinity）
    u_long                  *cpu_affinity;        // CPU 亲和掩码数组

    char                    *username;            // 运行用户名称（user nobody）
    ngx_uid_t                user;                // 用户 UID
    ngx_gid_t                group;               // 用户 GID

    ngx_str_t                working_directory;   // 工作目录（working_directory /path）
    ngx_str_t                lock_file;           // 锁文件路径（lock_file /path/to/lock）

    ngx_str_t                pid;                 // PID 文件路径（pid /path/to/nginx.pid）
    ngx_str_t                oldpid;              // 旧 PID 文件路径（reload 时使用）

    ngx_array_t              env;                 // 环境变量数组（env VAR=value）
    char                   **environment;         // C 语言形式的环境变量表

#if (NGX_THREADS)
    ngx_int_t                worker_threads;      // 每个 worker 的线程数（worker_threads N）
    size_t                   thread_stack_size;   // 每线程栈大小（thread_stack_size SIZE）
#endif

} ngx_core_conf_t;



typedef struct {
     ngx_pool_t              *pool;   /* pcre's malloc() pool */
} ngx_core_tls_t;


#define ngx_is_init_cycle(cycle)  (cycle->conf_ctx == NULL)


ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle);
ngx_int_t ngx_create_pidfile(ngx_str_t *name, ngx_log_t *log);
void ngx_delete_pidfile(ngx_cycle_t *cycle);
ngx_int_t ngx_signal_process(ngx_cycle_t *cycle, char *sig);
void ngx_reopen_files(ngx_cycle_t *cycle, ngx_uid_t user);
char **ngx_set_environment(ngx_cycle_t *cycle, ngx_uint_t *last);
ngx_pid_t ngx_exec_new_binary(ngx_cycle_t *cycle, char *const *argv);
u_long ngx_get_cpu_affinity(ngx_uint_t n);
ngx_shm_zone_t *ngx_shared_memory_add(ngx_conf_t *cf, ngx_str_t *name,
    size_t size, void *tag);


extern volatile ngx_cycle_t  *ngx_cycle;
extern ngx_array_t            ngx_old_cycles;
extern ngx_module_t           ngx_core_module;
extern ngx_uint_t             ngx_test_config;
extern ngx_uint_t             ngx_quiet_mode;
#if (NGX_THREADS)
extern ngx_tls_key_t          ngx_core_tls_key;
#endif


#endif /* _NGX_CYCLE_H_INCLUDED_ */
