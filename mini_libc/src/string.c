#include "mini_lib.h"


int strlen(const char *s)
{
    int len = 0;
    while (*s++) // 遇到字符串结束符时，跳出循环
    {
        len++;
    }

    return len;
}


char *itoa(long num, char *str, int radix, unsigned char sign_flag)
{
    int i = 0;
    int sign = 1;
    unsigned long num_u = (unsigned long) num;
    
    if (sign_flag == 1)
    {
        sign = num < 0 ? -1 : 1;
    }

     // 处理错误的基数
    if (radix < 2 || radix > 16)
    {
        return NULL;
    }

    // 处理0的情况
    if (num_u == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // 生成数字字符串
    while (num_u != 0)
    {
        unsigned int rem = num_u % radix;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num_u /= radix;
    }

    // 如果数字是负数，则添加负号
    if (sign == -1)
        str[i++] = '-';

    // 反转字符串
    for (int j = 0; j < i / 2; ++j)
    {
        char temp = str[j];
        str[j] = str[i - 1 - j];
        str[i - 1 - j] = temp;
    }

    str[i] = '\0'; // 添加字符串结束符
    return str;
}
