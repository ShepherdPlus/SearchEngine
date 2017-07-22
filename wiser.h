#ifndef __WISER_H__
#define __WISER_H__

#include "utlist.h"
#include "utarray.h"
#include "uthash.h"
#include <sqlite3.h>

/* bi-gram */
#define N_GRAM 2

/* 倒排列表（以文档编号和位置信息为元素的链表结构） */
typedef struct _postings_list {
    int document_id;                /* 文档编号 */
    UT_array *positions;            /* 位置信息的数组 */
    int positions_count;            /* 位置信息的条数 */
    struct _postings_list *next;    /* 指向下一个倒排列表的指针*/
} postings_list;

/* 倒排索引（以词元编号为键，以倒排列表为值的关联数组） */
typedef struct {
    int token_id;                   /* 词元的编号 */
    postings_list *postings_list;   /* 指向包含该词元的倒排列表的指针 */
    int docs_count;                 /* 出现过该词元的文档数 */
    int positions_count;            /* 所有文档中该词元的出现的次数之和 */
    UT_hash_handle hh;              /* 用于管理倒排列表的关联数组 */
} inverted_index_hash, inverted_index_value;

typedef enum {
    compress_none,   /* 不压缩 */
    compress_golomb,    /* 使用Golomb码进行压缩 */
} compress_method;

/* 应用程序的全局配置 */
typedef struct _wiser_env {
    const char *db_path;            /* 数据库的路径*/

    int token_len;                  /* 词元的长度。N-gram中N的取值 */
    compress_method compress;       /* 压缩倒排列表等数据的方法 */
    int enable_phrase_search;       /* 是否进行短语检索 */

    inverted_index_hash *ii_buffer; /* 用于更新倒排索引的缓冲区（Buffer） */
    int ii_buffer_count;            /* 用于更新倒排索引的缓冲区中的文档数 */
    int ii_buffer_update_threshold; /* 缓冲区中文档数的阈值 */
    int indexed_count;              /* 建立了索引的文档数 */

    /* 与sqlite3相关的配置 */
    sqlite3 *db; /* sqlite3的实例 */
    /* sqlite3的准备语句 */
    sqlite3_stmt *get_document_id_st;
    sqlite3_stmt *get_document_title_st;
    sqlite3_stmt *insert_document_st;
    sqlite3_stmt *update_document_st;
    sqlite3_stmt *get_token_id_st;
    sqlite3_stmt *get_token_st;
    sqlite3_stmt *store_token_st;
    sqlite3_stmt *get_postings_st;
    sqlite3_stmt *update_postings_st;
    sqlite3_stmt *get_settings_st;
    sqlite3_stmt *replace_settings_st;
    sqlite3_stmt *get_document_count_st;
    sqlite3_stmt *begin_st;
    sqlite3_stmt *commit_st;
    sqlite3_stmt *rollback_st;
} wiser_env;

#endif