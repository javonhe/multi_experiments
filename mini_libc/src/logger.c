/**
 * logger.c - 日志功能模块
 * 支持DEBUG、INFO、ERROR三个日志级别
 * 自动打印文件名、函数名和行号
 */

#include "mini_lib.h"



// 当前日志级别，默认为INFO
static int current_log_level = LOG_LEVEL_DEBUG;

// 日志级别对应的字符串
static const char *log_level_str[] = {
    "DEBUG",
    "INFO",
    "ERROR"
};

// 从完整路径中提取文件名
static const char *get_filename(const char *path)
{
    const char *filename = path;
    const char *p = path;
    
    while (*p)
    {
        if (*p == '/')
        {
            filename = p + 1;
        }
        p++;
    }
    
    return filename;
}

// 日志输出函数
void log_output(int level, const char *file, const char *func, int line, const char *fmt, ...)
{
    // 参数有效性检查
    if (!file || !func || !fmt)
    {
        write(2, "Invalid log parameters\n", 22);
        return;
    }

    // 检查日志级别范围
    if (level < LOG_LEVEL_DEBUG || level > LOG_LEVEL_ERROR)
    {
        write(2, "Invalid log level\n", 17);
        return;
    }

    // 如果当前日志级别高于要输出的级别，则不输出
    if (level < current_log_level)
    {
        return;
    }

    // 格式化日志
    char buf[1024];
    va_list args;
    
    // 先格式化日志头，限制长度避免缓冲区溢出
    int header_len = snprintf(buf, sizeof(buf), "[%s][%s:%d][%s] ", 
                           log_level_str[level],
                           get_filename(file),
                           line,
                           func);
                           
    // 检查是否发生截断
    if (header_len < 0 || header_len >= sizeof(buf))
    {
        write(2, "Log header too long\n", 19);
        return;
    }

    // 计算剩余缓冲区大小
    int remaining = sizeof(buf) - header_len;
    if (remaining <= 0)
    {
        write(2, "No space for log content\n", 24);
        return;
    }

    // 格式化日志内容
    va_start(args, fmt);
    int content_len = vsnprintf(buf + header_len, remaining, fmt, args);
    va_end(args);

    // 检查是否发生截断
    if (content_len < 0 || content_len >= remaining)
    {
        write(2, "Log content too long\n", 20);
        return;
    }

    // 输出到标准输出
    write(1, buf, header_len + content_len);
    write(1, "\n", 1);
}

// 设置日志级别
void set_log_level(int level)
{
    if (level >= LOG_LEVEL_DEBUG && level <= LOG_LEVEL_ERROR)
    {
        current_log_level = level;
    }
}

// 日志宏定义
#define LOG_DEBUG(fmt, ...) log_output(LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_output(LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_output(LOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
