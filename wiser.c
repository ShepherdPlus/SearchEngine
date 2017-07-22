#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wiser.h"
#include "util.h"

/**
 * 将文档添加到数据库中，建立倒排索引
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] title 文档的标题，为NULL时会清空缓冲区
 * @param[in] body 文档的正文
 */

static void
add_document(wiser_env *env, const char *title, const char *body)
{
    if (title && body) {
        UTF32Char *body32;
        int body32_len, document_id;
        unsigned int title_size, body_size;
        
        title_size = strlen(title);
        body_size = strlen(body);

        /* 将文档存储到数据库中并获得对应的文档编号 */
        db_add_document(env, title, title_size, body, body_size);
        document_id = db_get_document_id(env, title, title_size);

        /* 转换文档正文的字符编码 */
        if (!utf8toutf32(body, body_size, &body32, &body32_len)) {
            /* 为文档创建倒排列表 */
            text_to_postings_lists(env, document_id, body32, body32_len,
                                   env->token_len, &env->ii_buffer);
            env->ii_buffer_count++;
            free(body32);
        }
        env->indexed_count++;
        print_error("count:%d title: %s", env->indexed_count, title);
    }

    /* 当缓冲区中存储的文档数量到达了指定的阀值时，更新存储器上的倒排索引 */
    if (env->ii_buffer && (env->ii_buffer_count > env->ii_buffer_update_threshold || !title)) {
        inverted_index_hash *p;

        print_time_diff();

        /* 更新所有词元对应的倒排项 */
        for (p = env->ii_buffer; p != NULL; p = p->hh.next) {
            update_postings(env, p);
        }
        free_inverted_index(env->ii_buffer);
        print_error("index flushed.");
        env->ii_buffer = NULL;
        env->ii_buffer_count = 0;

        print_time_diff();
    }

}
