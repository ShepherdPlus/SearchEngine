#include <stdio.h>

#include "util.h"
#include "database.h"

/**
 * 将字节序列还原成倒排列表
 * @param[in] posting_e 待还原的倒排列表（字节序列）
 * @param[in] postings_e_size 待还原的倒排列表（字节序列）的大小
 * @param[out] postings 还原后的倒排列表
 * @param[out] postings_len 还原后的倒排列表的元素数
 * @retval 0 成功
 */

static int
decode_postings_none(const char *posting_e, int postings_e_size,
                     postings_list **postings, int *postings_len)
{
    const int *p, *pend;

    *postings = NULL;
    postings_len = 0;
    for (p = (const int *)posting_e,
         pend = (const int *)posting_e + postings_e_size; p < pend;) {
        postings_list *pl;
        int document_id, positions_count;

        document_id = *(p++);
        positions_count = *(p++);
        if ((pl = malloc(sizeof(postings_list)))) {
            int i;
            pl->document_id = document_id;
            pl->positions_count = positions_count;
            utarray_new(pl->positions, &ut_int_icd);
            LL_APPEND(*postings, pl);
            (*postings_len)++;

            /* decode positions */
            for (i = 0; i < positions_count; i++) {
                utarray_push_back(pl->positions, p);
                p++;
            }

        } else {
                p +=  positions_count;
        }
    }
    return 0;
}

/**
 * 将倒排列表转换成字节序列
 * @param[in] postings 倒排列表
 * @param[in] postings_len 倒排列表中的元素数
 * @param[out] postings_e 转换后的倒排列表
 * @retval 0 成功
 */

static int
encode_postings_none(const postings_list *postings,
                     const int postings_len, buffer *postings_e)
{
    const postings_list *p;
    LL_FOREACH(postings, p) {
        int *pos = NULL;
        append_buffer(postings_e, (void *)&p->document_id, sizeof(int));
        append_buffer(postings_e, (void *)&p->positions_count, sizeof(int));
        while ((pos = (int *)utarray_next(p->positions, pos))) {
            append_buffer(postings_e, (void *)pos, sizeof(int));
        }
    }
}

/**
 * 从数据中的指定位置读取1个比特
 * @param[in, out] buf 数据的开头
 * @param[in] buf_end 数据的结尾
 * @param[in, out] bit 从变量buf的哪个位置读取1个比特
 * @return 读取出的比特值
 */
static inline int
read_bit(const char **buf, const char *buf_end, unsigned char *bit)
{
    int r;
    if (*buf >= buf_end) { return -1; }
    r = (**buf & *bit) ? 1 : 0;
    *bit >>= 1;
    if (!*bit) {
        *bit = 0x80;
        (*buf)++;
    }
    return r;
}

/**
 * 根据Golomb编码中的参数m，计算出编码和解码过程中所需的参数b和参数t
 * @param[in] m Golomb编码中的参数m
 * @param[out] b Golomb编码中的参数b。ceil(log2(m))
 * @param[out] t pow2(b) - m
 */
static void
calc_golomb_params(int m, int *b, int *t)
{
    int l;
    assert(m > 0);
    for (*b = 0, l = 1; m > l; (*b)++, l <<= 1) {}
    *t = l - m;
}

/**
 * 用Golomb编码对1个数值进行解码
 * @param[in] m Golomb编码中的参数m
 * @param[in] b Golomb编码中的参数b。ceil(log2(m))
 * @param[in] t pow2(b) - m
 * @param[in,out] buf 待解码的数据
 * @param[in] buf_end 待解码数据的结尾
 * @param[in,out] bit 待解码数据的起始比特
 * @return 解码后的数值
 */

static inline int
golomb_decoding(int m, int b, int t,
                const char **buf, const char *buf_end, unsigned char *bit)
{
    int n = 0;

    /* decode (n / m) with unary code */
    while (read_bit(buf, buf_end, bit) == 1) {
        n += m;
    }
    /* decode (n % m) */
    if (m > 1) {
        int i, r = 0;
        for (i = 0; i < b - 1; i++){
            int z = read_bit(buf, buf_end, bit);
            if (z == -1) {print_error("invalid golomb code"); break; }
            r = (r << 1) | z;
        }
        if (r >= t) {
            int z = read_bit(buf, buf_end, bit);
            if (z == -1) {
                print_error("invalid golomb code");
            } else {
                r = (r << 1) | z;
                r -= t;
            }
        }
        n += r;
    }
    return n;
}

/**
 * 对倒排列表进行转换和编码
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] postings 待转换或编码前的倒排列表
 * @param[in] postings_len 待转换或编码前的倒排列表中的元素数
 * @param[out] postings_e 转换或编码后的倒排列表
 * @retval 0 成功
 */

static int
encode_postings(const wiser_env *env,
                const postings_list *postings, const int postings_len,
                buffer *postings_e)
{
    switch (env->compress) {
        case compress_none:
            return encode_postings_none(postings, postings_len, postings_e);
        case compress_golomb:
            return encode_postings_golomb(db_get_document_count(env),
                                          postings, postings_len, postings_e);
        default:
            abort();
    }
}

int fetch_postings(const wiser_env *env, const int token_id,
                   postings_list **postings, int *postings_len);
void merge_inverted_index(inverted_index_hash *base,
                          inverted_index_hash *to_be_added);
/**
 * 获取将两个倒排列表合并后得到的倒排列表
 * @param pa 要合并的倒排列表
 * @param pb 要合并的倒排列表
 * @return 合并后的倒排列表
 */
static postings_list *
merge_postings(postings_list *pa, postings_list *pb)
{
    postings_list *ret = NULL, *p;
    while (pa || pb) {
        postings_list *e;
        if (!pb || pa->document_id <= pb->document_id) {
            e = pa;
            pa = pa->next;
        } else if (!pa || pb->document_id <= pa->document_id) {
            e = pb;
            pb = pb->next;
        } else {
            abort();
        }
        e->next = NULL;
        if (!ret) {
            ret = e;
        } else {
            p->next = e;
        }
        p = e;
    }

    return ret;
}
/**
 * 将内存上（小倒排索引）的倒排列表与存储器上的倒排列表合并后存储
 * @param[in] env 存储着应用程序运行环境的结构体
 * @param[in] p 含有倒排列表的倒排索引中的索引项
 */

void update_postings(const wiser_env *env, inverted_index_hash *p)
{
    int old_postings_len;
    postings_list *old_positings;

    if (!fetch_postings(env, p->token_id, &old_positings,
                        &old_postings_len)) {
        buffer *buf;
        if (old_postings_len) {
            p->postings_list = merge_postings(old_positings, p->postings_list);
            p->docs_count += old_postings_len;
        }
        if (buf == alloc_buffer()) {
            encode_postings()
        }
    }
}
void dump_postings_list(const postings_list *postings);
void free_postings_list(postings_list *pl);
void dump_inverted_index(wiser_env *env, inverted_index_hash *ii);
void free_inverted_index(inverted_index_hash *ii);
