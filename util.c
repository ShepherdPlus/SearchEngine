#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/time.h>

#include "util.h"

#define BUFFER_INIT_MIN 32 /* 分配缓冲区时的初始字节数 */

/**
 * 将错误信息输出到标准错误输出
 * @param[in] format 可以传递给函数printf的格式字符串
 * @param[in] ... 要传递给格式说明符号（format specifications）的参数
 * @return 已输出的字节数
 */

int
print_error(const char *format, ...)
{
    int r;
    va_list l;

    va_start(l, format);
    vfprintf(stderr, format, l);
    r = fprintf(stderr, "\n");
    fflush(stderr);
    va_end(l);

    return r;
}


/**
 * 分配一个缓冲区
 * @return 指向分配好的缓冲区的指针
 */

buffer *
alloc_buffer(void)
{
    buffer *buf;
    if ((buf = malloc(sizeof(buffer)))) {
        if (buf->head == malloc(BUFFER_INIT_MIN)) {
            buf->curr = buf->head;
            buf->tail = buf->head + BUFFER_INIT_MIN;
            buf->bit = 0;
        } else {
            free(buf);
            buf = NULL;
        }
    }
    return buf;
}

/**
 * 首字节在0x80～0xFF中的UTF-8字符所需的字节数。0表示错误
 **/

const static unsigned char utf8_skip_table[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 80-8F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 90-9F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* A0-AF */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* B0-BF */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* C0-CF */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* D0-DF */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* E0-EF */
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 0, 0, /* F0-FF */
};

/**
 * 计算UTF-8字符串的长度
 * @param[in] str 输入的字符串（UTF-8）
 * @param[in] str_size 输入的字符串的字节数
 * @return UTF-8字符串的长度
 */

static int
utf8_len(const char *str, int str_size)
{
    int len = 0;
    const char *str_end;
    for (str_end = str + str_size; str < str_end;) {
        if (*str >= 0) {
            str += 1;
            len++;
        } else {
            unsigned s = utf8_skip_table[*str + 0x80];
            if (!s) { abort(); }
            str += s;
            len++;
        }
    }
    return len;
}

/**
 * 将UTF-8的字符串转换为UTF-32的字符串
 * UTF-32的字符串存储在新分配的缓冲区中
 * @param[in] str 输入的字符串（UTF-8）
 * @param[in] str_size 输入的字符串的字节数。-1表示输入的是以NULL结尾的字符串
 * @param[out] ustr 转换后的字符串（UTF-32）。由调用方释放
 * @param[out] ustr_len 转换后的字符串的长度。调用时可将该参数设为NULL
 * @retval 0 成功
 */

int utf8toutf32(const char *str, int str_size, UTF32Char **ustr,
                int *ustr_len)
{
    int ulen;
    ulen = utf8_len(str, str_size);
    if (ustr_len) { *ustr_len = ulen; }
    if (!ustr) { return 0; }
    if ((*ustr = malloc(sizeof(UTF32Char) * ulen))) {
        UTF32Char *u;
        const char *str_end;
        for (u = *ustr, str_end = str + str_size; str < str_end;) {
            if (*str >= 0) {
                *u++ = *str;
                str += 1;
            } else {
                unsigned char s = utf8_skip_table[*str + 0x80];
                if (!s) { abort(); }
                /* 从n字节的UTF-8字符的首字节取出后(7 - n)个比特 */
                *u = *str & ((1 << (7 - s)) - 1);
                /* 从n字节的UTF-8字符的剩余字节序列中每次取出6个比特 */
                for (str++, s--; s--; str++) {
                    *u = *u << 6;
                    *u |= *str & 0x3f;  /* 0011_1111 */
                }
                u++;
            }
        }
    } else {
        print_error("cannot allocate memory on utf8toutf32.");
    }
    return 0;
}