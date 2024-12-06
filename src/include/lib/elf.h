#include <sys/types.h>

typedef u32 Elf32_Word;
typedef u32 Elf32_Addr;
typedef u32 Elf32_Off;
typedef u16 Elf32_Half;

// ELF 文件标记
typedef struct ELFIdent {
    u8 ei_magic[4];     // 内容为 0x7F, E, L, F
    u8 ei_class;        // 文件种类 1-32位，2-64 位
    u8 ei_data;         // 标记大小端 1-小端，2-大端
    u8 ei_version;      // 与 e_version 一样，必须为 1
    u8 ei_pad[16 - 7];  // 占满 16 个字节
} _packed ELFIdent;

// ELF 文件头
typedef struct Elf32_Ehdr {
    ELFIdent e_ident;   // ELF 文件标记，文件最开始的 16 个字节
    Elf32_Half e_type;  // 文件类型，见 Etype
    Elf32_Half e_machine;  // 处理器架构类型，标记运行的 CPU，见 EMachine
    Elf32_Word e_version;  // 文件版本，见 EVersion
    Elf32_Addr e_entry;    // 程序入口地址
    Elf32_Off e_phoff;  // program header offset 程序头表在文件中的偏移量
    Elf32_Off e_shoff;  // section header offset 节头表在文件中的偏移量
    Elf32_Word e_flags;      // 处理器特殊标记
    Elf32_Half e_ehsize;     // ELF header size ELF 文件头大小
    Elf32_Half e_phentsize;  // program header entry size 程序头表入口大小
    Elf32_Half e_phnum;      // program header number 程序头数量
    Elf32_Half e_shentsize;  // section header entry size 节头表入口大小
    Elf32_Half e_shnum;      // section header number 节头表数量
    Elf32_Half e_shstrndx;   // 节字符串表在节头表中的索引
} Elf32_Ehdr;

// ELF 文件类型
enum Etype {
    ET_NONE = 0,         // 无类型
    ET_REL = 1,          // 可重定位文件
    ET_EXEC = 2,         // 可执行文件
    ET_DYN = 3,          // 动态链接库
    ET_CORE = 4,         // core 文件，未说明，占位
    ET_LOPROC = 0xff00,  // 处理器相关低值
    ET_HIPROC = 0xffff,  // 处理器相关高值
};

// ELF 机器(CPU)类型
enum EMachine {
    EM_NONE = 0,   // No machine
    EM_M32 = 1,    // AT&T WE 32100
    EM_SPARC = 2,  // SPARC
    EM_386 = 3,    // Intel 80386
    EM_68K = 4,    // Motorola 68000
    EM_88K = 5,    // Motorola 88000
    EM_860 = 7,    // Intel 80860
    EM_MIPS = 8,   // MIPS RS3000
};

// ELF 文件版本
enum EVersion {
    EV_NONE = 0,     // 不可用版本
    EV_CURRENT = 1,  // 当前版本
};

// 程序头
typedef struct Elf32_Phdr {
    Elf32_Word p_type;    // 段类型，见 SegmentType
    Elf32_Off p_offset;   // 段在文件中的偏移量
    Elf32_Addr p_vaddr;   // 加载到内存中的虚拟地址
    Elf32_Addr p_paddr;   // 加载到内存中的物理地址
    Elf32_Word p_filesz;  // 文件中占用的字节数
    Elf32_Word p_memsz;   // 内存中占用的字节数
    Elf32_Word p_flags;   // 段标记，见 SegmentFlag
    Elf32_Word p_align;   // 段对齐约束
} Elf32_Phdr;

// 段类型
enum SegmentType {
    PT_NULL = 0,     // 未使用
    PT_LOAD = 1,     // 可加载程序段
    PT_DYNAMIC = 2,  // 动态加载信息
    PT_INTERP = 3,   // 动态加载器名称
    PT_NOTE = 4,     // 一些辅助信息
    PT_SHLIB = 5,    // 保留
    PT_PHDR = 6,     // 程序头表
    PT_LOPROC = 0x70000000,
    PT_HIPROC = 0x7fffffff,
};

enum SegmentFlag {
    PF_X = 0x1,  // 可执行
    PF_W = 0x2,  // 可写
    PF_R = 0x4,  // 可读
};

typedef struct Elf32_Shdr {
    Elf32_Word sh_name;   // 节名，在字符串表中的索引
    Elf32_Word sh_type;   // 节类型，见 SectionType
    Elf32_Word sh_flags;  // 节标记，见 SectionFlag
    Elf32_Addr sh_addr;   // 节地址
    Elf32_Off sh_offset;  // 节在文件中的偏移量
    Elf32_Word sh_size;   // 节大小
    Elf32_Word sh_link;   // 保存了头表索引链接，与节类型相关
    Elf32_Word sh_info;   // 额外信息，与节类型相关
    Elf32_Word sh_addralign;  // 地址对齐约束
    Elf32_Word sh_entsize;    // 子项入口大小
} Elf32_Shdr;

enum SectionType {
    SHT_NULL = 0,             // 不可用
    SHT_PROGBITS = 1,         // 程序信息
    SHT_SYMTAB = 2,           // 符号表
    SHT_STRTAB = 3,           // 字符串表
    SHT_RELA = 4,             // 有附加重定位
    SHT_HASH = 5,             // 符号哈希表
    SHT_DYNAMIC = 6,          // 动态链接信息
    SHT_NOTE = 7,             // 标记文件信息
    SHT_NOBITS = 8,           // 该节文件中无内容
    SHT_REL = 9,              // 无附加重定位
    SHT_SHLIB = 10,           // 保留，用于非特定的语义
    SHT_DYNSYM = 11,          // 符号表
    SHT_LOPROC = 0x70000000,  // 以下与处理器相关
    SHT_HIPROC = 0x7fffffff,
    SHT_LOUSER = 0x80000000,
    SHT_HIUSER = 0xffffffff,
};

enum SectionFlag {
    SHF_WRITE = 0x1,  // 执行时可写
    SHF_ALLOC = 0x2,  // 执行时占用内存，有些节执行时可以不在内存中
    SHF_EXECINSTR = 0x4,  // 包含可执行的机器指令，节里有代码
    SHF_MASKPROC = 0xf0000000,  // 保留，与处理器相关
};

typedef struct Elf32_Sym {
    Elf32_Word st_name;   // 符号名称，在字符串表中的索引
    Elf32_Addr st_value;  // 符号值，与具体符号相关
    Elf32_Word st_size;   // 符号的大小
    u8 st_info;   // 指定符号类型和约束属性，见 SymbolBinding
    u8 st_other;  // 为 0，无意义
    Elf32_Half st_shndx;  // 符号对应的节表索引
} Elf32_Sym;

// 通过 info 获取约束
#define ELF32_ST_BIND(i) ((i) >> 4)
// 通过 info 获取类型
#define ELF32_ST_TYPE(i) ((i) & 0xF)
// 通过 约束 b 和 类型 t 获取 info
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

// 符号约束
enum SymbolBinding {
    STB_LOCAL = 0,   // 外部不可见符号，优先级最高
    STB_GLOBAL = 1,  // 外部可见符号
    STB_WEAK = 2,  // 弱符号，外部可见，如果符号重定义，则优先级更低
    STB_LOPROC = 13,  // 处理器相关低位
    STB_HIPROC = 15,  // 处理器相关高位
};

// 符号类型
enum SymbolType {
    STT_NOTYPE = 0,   // 未定义
    STT_OBJECT = 1,   // 数据对象，比如 变量，数组等
    STT_FUNC = 2,     // 函数或可执行代码
    STT_SECTION = 3,  // 节，用于重定位
    STT_FILE = 4,     // 文件，节索引为 SHN_ABS，约束为 STB_LOCAL，
                      // 而且优先级高于其他 STB_LOCAL 符号
    STT_LOPROC = 13,  // 处理器相关
    STT_HIPROC = 15,  // 处理器相关
};
