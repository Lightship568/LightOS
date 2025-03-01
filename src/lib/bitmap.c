#include <lib/bitmap.h>
#include <lib/string.h>
#include <sys/assert.h>

// 构造位图
void bitmap_make(bitmap_t* map, char* bits, u32 length, u32 offset) {
    map->bits = bits;
    map->length = length; // 字节数
    map->offset = offset; // 起始偏移
}

// 位图初始化，全部置为 0
void bitmap_init(bitmap_t* map, char* bits, u32 length, u32 offset) {
    memset(bits, 0, length);
    bitmap_make(map, bits, length, offset);
}

// 测试某一位是否为 1
bool bitmap_test(bitmap_t* map, idx_t index) {
    assert(index >= map->offset);

    // 得到位图的索引
    idx_t idx = index - map->offset;

    // 位图数组中的字节
    u32 bytes = idx / 8;

    // 该字节中的那一位
    u8 bits = idx % 8;

    assert(bytes < map->length);

    // 返回那一位是否等于 1
    return (map->bits[bytes] & (1 << bits));
}

// 设置位图某位的值（传入index是页的索引，即>>12）
void bitmap_set(bitmap_t* map, idx_t index, bool value) {

    assert(index >= map->offset);

    // 得到位图的索引
    idx_t idx = index - map->offset;

    // 位图数组中的字节
    u32 bytes = idx / 8;

    assert(bytes < map->length);

    // 该字节中的那一位
    u8 bits = idx % 8;
    if (value) {
        // 置为 1
        map->bits[bytes] |= (1 << bits);
    } else {
        // 置为 0
        map->bits[bytes] &= ~(1 << bits);
    }
}

// 从位图中得到连续的 count 位
int bitmap_scan(bitmap_t* map, u32 count) {
    int start = EOF;                  // 标记目标开始的位置
    u32 bits_left = map->length * 8;  // 剩余的位数
    u32 index = 0;                    // 下一个位
    u32 counter = 0;                  // 计数器

    // 从头开始找
    while (bits_left-- > 0) {
        if (!bitmap_test(map, map->offset + index)) {
            // 如果下一个位没有占用，则计数器加一
            counter++;
        } else {
            // 否则计数器置为 0，继续寻找
            counter = 0;
        }

        // 下一位，位置加一
        index++;

        // 找到数量一致，则设置开始的位置，结束
        if (counter == count) {
            start = index - count;
            break;
        }
    }

    // 如果没找到，则返回 EOF(END OF FILE)
    if (start == EOF)
        return EOF;

    // 否则将找到的位，全部置为 1
    bits_left = count;
    index = start;
    while (bits_left--) {
        bitmap_set(map, map->offset + index, true);
        index++;
    }

    // 然后返回索引
    return start + map->offset;
}
