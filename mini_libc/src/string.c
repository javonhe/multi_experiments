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

/**
 * 内存拷贝函数
 * @param dest: 目标内存地址
 * @param src: 源内存地址
 * @param n: 要拷贝的字节数
 * @return: 目标内存地址
 */
void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    // 检查源地址和目标地址是否重叠
    if ((d <= s) || (d >= (s + n)))
    {
        // 从前向后拷贝
        while (n--)
        {
            *d++ = *s++;
        }
    }
    else
    {
        // 从后向前拷贝
        d = d + n - 1;
        s = s + n - 1;
        while (n--)
        {
            *d-- = *s--;
        }
    }
    
    return dest;
}

/**
 * 字符串比较函数
 * 
 * 比较两个字符串，按照字典序比较
 * 
 * @param s1: 第一个字符串
 * @param s2: 第二个字符串
 * @return: 
 *   - 如果s1 < s2，返回负值
 *   - 如果s1 = s2，返回0
 *   - 如果s1 > s2，返回正值
 */
int strcmp(const char *s1, const char *s2)
{
    // 同时遍历两个字符串，直到遇到不同字符或字符串结束
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    
    // 返回两个字符的ASCII差值
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
