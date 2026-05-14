#ifndef LOG_MEMORY_H
#define LOG_MEMORY_H

#define LOG_FRAMES 6

void log_memory_init(void);

void log_cache_write(const char* msg);

void log_flush_all(void);

#endif