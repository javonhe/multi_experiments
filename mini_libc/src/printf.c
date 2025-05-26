/**
 * printf.c - 支持多线程和UTF-8的printf实现
 */

#include "mini_lib.h"

/**
 * 检查是否是UTF-8字符的后续字节
 */
static int is_utf8_continuation(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

/**
 * 获取UTF-8字符的字节数
 */
static int get_utf8_char_length(unsigned char c)
{
    if ((c & 0x80) == 0) return 1;        // ASCII
    if ((c & 0xE0) == 0xC0) return 2;     // 2字节UTF-8
    if ((c & 0xF0) == 0xE0) return 3;     // 3字节UTF-8
    if ((c & 0xF8) == 0xF0) return 4;     // 4字节UTF-8
    return 1;  // 无效UTF-8字符当作1字节处理
}

/**
 * 格式化输出到缓冲区
 */
int vsprintf(char *buf, const char *format, va_list args)
{
    char *str = buf;
    char num_buf[32];
    const char *s = format;
    
    while (*s)
    {
        if (*s != '%')
        {
            // 处理UTF-8字符
            int char_len = get_utf8_char_length(*s);
            for (int i = 0; i < char_len && *s; i++)
            {
                *str++ = *s++;
            }
            continue;
        }
        
        s++;  // 跳过%
        
        // 处理格式说明符
        switch (*s)
        {
            case 'd':  // 十进制整数
            {
                int value = va_arg(args, int);
                int len = strlen(itoa(value, num_buf, 10, 1));
                memcpy(str, num_buf, len);
                str += len;
                break;
            }
            
            case 'x':  // 十六进制整数
            {
                unsigned int value = va_arg(args, unsigned int);
                int len = strlen(itoa(value, num_buf, 16, 0));
                memcpy(str, num_buf, len);
                str += len;
                break;
            }
            
            case 'l':  // 长整型
                if (*(s + 1) == 'd')
                {
                    long value = va_arg(args, long);
                    int len = strlen(itoa(value, num_buf, 10, 1));
                    memcpy(str, num_buf, len);
                    str += len;
                    s++;
                }
                break;
                
            case 's':  // 字符串
            {
                char *p = va_arg(args, char*);
                while (*p)
                {
                    // 处理字符串中的UTF-8字符
                    int char_len = get_utf8_char_length(*p);
                    for (int i = 0; i < char_len && *p; i++)
                    {
                        *str++ = *p++;
                    }
                }
                break;
            }
            
            default:
                *str++ = *s;
                break;
        }
        
        s++;
    }
    
    *str = '\0';
    return str - buf;
}

/**
 * 格式化输出到标准输出
 */
int printf(const char *format, ...)
{
    char buf[1024];  // 输出缓冲区
    va_list args;
    int ret;
        
    // 格式化字符串
    va_start(args, format);
    ret = vsprintf(buf, format, args);
    va_end(args);
    
    // 写入标准输出
    if (ret > 0)
    {
        write(1, buf, ret);
    }
        
    return ret;
}

/**
 * 格式化输出到缓冲区
 */
int sprintf(char *buf, const char *format, ...)
{
    va_list args;
    int ret;
    
    va_start(args, format);
    ret = vsprintf(buf, format, args);
    va_end(args);
    
    return ret;
}

/**
 * 带长度限制的格式化输出到缓冲区
 */
int vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
    if (!buf || !format || size == 0)
    {
        return -1;
    }

    // 使用临时缓冲区
    char temp[1024];
    int ret = vsprintf(temp, format, args);
    
    // 如果格式化失败或结果太长，返回错误
    if (ret < 0 || ret >= size)
    {
        return -1;
    }
    
    // 复制到目标缓冲区
    memcpy(buf, temp, ret);
    buf[ret] = '\0';
    
    return ret;
}

/**
 * 带长度限制的格式化输出到缓冲区
 */
int snprintf(char *buf, size_t size, const char *format, ...)
{
    va_list args;
    int ret;
    
    va_start(args, format);
    ret = vsnprintf(buf, size, format, args);
    va_end(args);
    
    return ret;
}


