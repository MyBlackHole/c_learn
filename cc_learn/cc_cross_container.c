/**
 * cc_cross_container.c — CC (Convenient Containers) 同一对象跨多容器完整案例
 *
 * 演示用 CC 存指针，让同一组堆对象同时被三个容器交叉索引：
 *   map(int, file_info*)   — 按 id O(1) 哈希查找
 *   oset(file_info*)       — 按 mtime 红黑树有序，支持范围查询
 *   list(file_info*)       — LRU 顺序双向链表
 *
 * 编译:
 *   gcc -std=c11 -o cc_demo cc_cross_container.c
 * 或 (C++ 也能编译):
 *   g++ -std=c++11 -o cc_demo cc_cross_container.c
 *
 * 运行:
 *   ./cc_demo
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/* CC 首次包含：加载基本类型和容器声明 */
#include "cc.h"

/* ===================================================================
 * 1. 数据对象
 * =================================================================== */
typedef struct file_info {
    int    id;
    char  *path;
    time_t mtime;   /* 最后修改时间（用作有序排序键） */
    size_t size;
} file_info;

static file_info *file_new(int id, const char *path, time_t mtime, size_t size)
{
    file_info *f = (file_info *)malloc(sizeof(*f));
    if (!f) return NULL;
    f->id    = id;
    f->path  = strdup(path);
    f->mtime = mtime;
    f->size  = size;
    return f;
}

static void file_free(file_info *f)
{
    if (f) { free(f->path); free(f); }
}

static void file_print(const file_info *f)
{
    char buf[64];
    strftime(buf, sizeof(buf), "%m-%d %H:%M", localtime(&f->mtime));
    printf("  [id=%2d] %-30s mtime=%s size=%zu\n",
           f->id, f->path, buf, f->size);
}

/* ===================================================================
 * 2. CC 自定义函数
 *
 * oset(file_info*) 需要比较函数来决定顺序（按 mtime）。
 * map(int, file_info*) 的 key 是 int（CC 内置支持），无需自定义。
 * =================================================================== */
typedef file_info *file_info_ptr;

/* oset 排序规则：按 mtime 升序（早期在前） */
#define CC_CMPR file_info_ptr, {                                          \
    if (val_1->mtime < val_2->mtime) return -1;                           \
    if (val_1->mtime > val_2->mtime) return 1;                            \
    return 0;                                                              \
}

/* CC 第二次包含：注册 file_info_ptr 的自定义比较器。
 * （如果还要用无序 set/map，需额外定义 CC_HASH） */
#include "cc.h"

/* ===================================================================
 * 3. 辅助：创建临时 probe 对象用于 oset 范围查询
 *    first(&oset, &probe) 接受一个"探针"值，
 *    返回第一个不比它小的元素迭代器。
 * =================================================================== */
static file_info_ptr oset_probe(time_t mtime)
{
    /* 返回一个栈上对象的指针，仅在当前表达式内有效。
     * CC 的 first() 内部立刻比较，不会保存此指针。 */
    static file_info p;
    p.mtime = mtime;
    return &p;
}

/* ===================================================================
 * 4. main
 * =================================================================== */
int main(void)
{
    /* ---- 4a. 创建三个容器 ---- */
    map(int, file_info_ptr)  by_id;     init(&by_id);
    oset(file_info_ptr)      by_time;   init(&by_time);
    list(file_info_ptr)      lru;       init(&lru);

    /* ---- 4b. 准备数据 ---- */
    file_info *all[6];

    /* 故意打乱 mtime 顺序，验证 oset 自动排序 */
    all[0] = file_new(5, "/etc/nginx.conf",      time(NULL) - 86400*7,  1024);
    all[1] = file_new(2, "/var/log/syslog",      time(NULL) - 86400*1, 51200);
    all[2] = file_new(1, "/etc/passwd",           time(NULL) - 86400*5,  2048);
    all[3] = file_new(4, "/tmp/cache.bin",        time(NULL) - 86400*2, 1024000);
    all[4] = file_new(6, "/var/log/kern.log",     time(NULL) - 3600*6,  65536);
    all[5] = file_new(3, "/home/user/doc.txt",    time(NULL) - 86400*10, 4096);

    printf("=== 插入 %zu 个文件到三个容器 ===\n\n", sizeof(all)/sizeof(all[0]));

    for (int i = 0; i < 6; i++) {
        assert(insert(&by_id,  all[i]->id,   all[i]));   /* 哈希表 */
        assert(insert(&by_time, all[i]));                  /* 红黑树 */
        assert(push(&lru,      all[i]));                   /* LRU 链表尾 */
    }

    /* ================================================================
     * 演示 1：by_id 哈希 O(1) 查找
     * ================================================================ */
    printf("▶ [演示 1] 从 by_id 哈希表查找 id=3:\n");
    {
        file_info_ptr *found = get(&by_id, 3);
        if (found) file_print(*found);
    }
    printf("\n");

    /* ================================================================
     * 演示 2：by_time 有序范围查询
     * ================================================================ */
    printf("▶ [演示 2] 按 mtime 范围查询 [7天前 .. 2天前]:\n");
    {
        time_t now   = time(NULL);
        time_t start = now - 86400*7;
        time_t end   = now - 86400*2;

        file_info_ptr *it  = first(&by_time, oset_probe(start));
        file_info_ptr *end_it = first(&by_time, oset_probe(end));

        for (; it != end_it; it = next(&by_time, it))
            file_print(*it);
    }
    printf("\n");

    /* ================================================================
     * 演示 3：全量有序遍历（验证 oset 自动按 mtime 排序）
     * ================================================================ */
    printf("▶ [演示 3] 按 mtime 全量有序遍历:\n");
    for_each(&by_time, el)
        file_print(*el);
    printf("\n");

    /* ================================================================
     * 演示 4：跨容器操作 — 从哈希表查到后，在 oset 中确认存在
     * ================================================================ */
    printf("▶ [演示 4] 跨容器：by_id 查到 id=2，确认也在 by_time 中:\n");
    {
        file_info_ptr *found = get(&by_id, 2);
        assert(found && *found);

        file_info_ptr *in_oset = get(&by_time, *found);
        printf("   by_id 查到: "); file_print(*found);
        printf("   by_time 中 %s (mtime=%ld)\n",
               in_oset ? "✅ 存在" : "❌ 不存在",
               (long)(*found)->mtime);
    }
    printf("\n");

    /* ================================================================
     * 演示 5：LRU 命中 — 只需在 list 中 O(n) 移到头部
     * ================================================================ */
    printf("▶ [演示 5] LRU 命中 id=5，移到链表头部\n\n");

    /* 先打印 LRU 当前顺序 */
    printf("   移动前 LRU 顺序:\n");
    for_each(&lru, el) printf("   "), file_print(*el);

    /* LRU 命中 id=5：遍历找到 → 擦除 → 插到头部 */
    {
        /* 从哈希表查到对象 */
        file_info_ptr *hit = get(&by_id, 5);
        assert(hit);

        /* 在链表中找到它然后移到头部 */
        int moved = 0;
        for (file_info_ptr *p = first(&lru); p != end(&lru); p = next(&lru, p)) {
            if (*p == *hit) {
                erase(&lru, p);
                /* push_front &lru — CC list 没有 push_front，
                 * 但可以用 insert 插在 first 前面 */
                /* CC 的 insert(&list, before, value) 在 before 前插入 */
                insert(&lru, first(&lru), *hit);
                moved = 1;
                break;
            }
        }
        printf("   id=5 %s 移动到头部\n\n", moved ? "已" : "未");
    }

    printf("   移动后 LRU 顺序（id=5 在头部）:\n");
    for_each(&lru, el) printf("   "), file_print(*el);
    printf("\n");

    /* ================================================================
     * 演示 6：删除对象 — 从所有容器移除再 free
     * ================================================================ */
    printf("▶ [演示 6] 删除 id=4:\n");
    {
        file_info_ptr *found = get(&by_id, 4);
        if (found) {
            file_info *victim = *found;
            erase(&by_id,  victim->id);
            erase(&by_time, victim);
            /* list: 遍历删除 */
            for (file_info_ptr *p = first(&lru); p != end(&lru); p = next(&lru, p)) {
                if (*p == victim) { erase(&lru, p); break; }
            }
            file_free(victim);
            printf("   id=4 已从三个容器移除并释放\n\n");
        }
    }

    /* 验证删除后的状态 */
    printf("▶ 删除后验证:\n");
    printf("   by_id 查 id=4: %s\n\n", get(&by_id, 4) ? "存在 ❌" : "不存在 ✅");

    printf("▶ 剩余文件按 mtime 有序:\n");
    for_each(&by_time, el) file_print(*el);
    printf("\n");

    /* ================================================================
     * 清理
     * ================================================================ */
    for_each(&by_id, _k, _v)
        file_free(*_v);
    cleanup(&by_id);
    cleanup(&by_time);
    cleanup(&lru);

    printf("=== 演示完成 ===\n");
    return 0;
}
