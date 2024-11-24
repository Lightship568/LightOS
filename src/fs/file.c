#include <lib/string.h>
#include <lightos/fs.h>
#include <lightos/task.h>
#include <sys/assert.h>

// 全系统最大同时打开文件数
#define FILE_NR 128

file_t file_table[FILE_NR];

file_t* get_file(void) {
    for (size_t i = 3; i < FILE_NR; ++i) {
        file_t* file = &file_table[i];
        if (!file->count) {
            file->count++;
            return file;
        }
    }
    panic("File table out of FILE_NR %d\n", FILE_NR);
}

void put_file(file_t* file) {
    assert(file->count > 0);
    file->count--;
    if (!file->count) {
        iput(file->inode);
    }
}

void file_init(void) {
    memset(file_table, 0, sizeof(file_table));
}
