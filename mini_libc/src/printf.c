#include "mini_lib.h"

int printf(const char *format, ...)
{
    int ret = 0;

    va_list args;
    va_start(args, format);
    
    const char *s = NULL;
    unsigned char find_flag = 0; // 用来标识是否找到一个占位符%

    for (s = format; *s != '\0'; s++)
    {
        switch (*s)
        {
            case '%': // 遇到占位符
                if (find_flag == 0)
                {
                    find_flag = 1;
                }
                else
                {
                    write(1, s, 1);
                    find_flag = 0;
                    ret++;
                }
                break;
            case 'd': // 十进制
                if (find_flag == 1)
                {
                    int tmp_int = va_arg(args, int);
                    char tmp_str[16] = { 0, 0, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0, 0, 0, 0 };

                    itoa(tmp_int, tmp_str, 10, 1);
                    write(1, tmp_str, strlen(tmp_str));

                    find_flag = 0;
                    ret += strlen(tmp_str);
                }
                else
                {
                    write(1, s, 1);
                    ret++;
                }
                break;
            case 'l': // 长整形
                if (find_flag == 1)
                {
                    const char *p = s + 1;
                    if (*p == 'd') // 十进制
                    {
                        long tmp_int = va_arg(args, long);
                        char tmp_str[16] = { 0, 0, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0, 0, 0, 0 };
                        itoa(tmp_int, tmp_str, 10, 1);
                        write(1, tmp_str, strlen(tmp_str));
                        find_flag = 0;
                        ret += strlen(tmp_str);
                        s++;
                    }
                    else if (*p == 'x') // 十六进制
                    {
                        long tmp_int = va_arg(args, long);
                        char tmp_str[16] = { 0, 0, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0, 0, 0, 0 };
                        itoa(tmp_int, tmp_str, 16, 0);
                        write(1, tmp_str, strlen(tmp_str));
                        find_flag = 0;
                        ret += strlen(tmp_str);
                        s++;
                    }
                    else
                    {
                        find_flag = 0;
                        write(1, s, 1);
                        ret ++;
                    }
                }
                else
                {
                    write(1, s, 1);
                    ret++;
                }
                break;
            case 'x': // 十六进制
                if (find_flag == 1)
                {
                    int tmp_int = va_arg(args, int);
                    char tmp_str[16] = { 0, 0, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0, 0, 0, 0, 0 };

                    itoa(tmp_int, tmp_str, 16, 0);
                    write(1, tmp_str, strlen(tmp_str));

                    find_flag = 0;
                    ret += strlen(tmp_str);
                }
                else
                {
                    write(1, s, 1);
                    ret++;
                }
                break;
            case 's': // 字符串
                if (find_flag == 1)
                {
                    char *tmp_str = va_arg(args, char *);
                    write(1, tmp_str, strlen(tmp_str));
                    find_flag = 0;
                    ret += strlen(tmp_str);
                }
                else
                {
                    write(1, s, 1);
                    ret++;
                }
                break;
            default:
                if (find_flag == 1)
                {
                    find_flag = 0;
                }

                write(1, s, 1);
                ret++;
                break;
        }
    }

    va_end(args);

    return ret;
}


