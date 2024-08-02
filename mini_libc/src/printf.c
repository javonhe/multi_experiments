#include "mini_lib.h"

int printf(const char *format, ...)
{
    int ret = 0;

    va_list args;
    va_start(args, format);
    
    const char *s = NULL;
    unsigned char find_flag = 0;

    for (s = format; *s != '\0'; s++)
    {
        switch (*s)
        {
            case '%':
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
            case 'd':
                if (find_flag == 1)
                {
                    int tmp_int = va_arg(args, int);
                    char tmp_str[8] = { 0 };

                    itoa(tmp_int, tmp_str, 10);
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
            case 's':
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

    return ret;
}


