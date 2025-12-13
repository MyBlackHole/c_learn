#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/generic-radix-tree.h>
#include <linux/slab.h>

// 定义要存储的数据结构
struct student {
	int id;
	char name[20];
	int score;
};

// 定义全局 GENRADIX
// static struct genradix students_radix;
DEFINE_GENRADIX(students_radix, struct student);

// 初始化 GENRADIX
static int __init genradix_demo_init(void)
{
	struct genradix_iter iter;
	struct student *stu;
	int i;

	printk(KERN_INFO "GENRADIX 演示模块加载\n");

	// // 初始化 GENRADIX
	// genradix_init(&students_radix);

	// 添加一些学生数据
	for (i = 0; i < 5; i++) {
		// 在索引 i 处分配空间
		stu = genradix_ptr_alloc(&students_radix, i, GFP_KERNEL);
		if (!stu) {
			printk(KERN_ERR "无法分配学生数据\n");
			return -ENOMEM;
		}

		// 填充数据
		stu->id = i;
		snprintf(stu->name, sizeof(stu->name), "学生%d", i);
		stu->score = 70 + i * 5;

		printk(KERN_INFO "添加: ID=%d, 姓名=%s, 分数=%d\n", stu->id,
		       stu->name, stu->score);
	}

	// 检索特定学生
	stu = genradix_ptr(&students_radix, 2);
	if (stu) {
		printk(KERN_INFO "查找索引2: %s, 分数=%d\n", stu->name,
		       stu->score);
	}

	// 尝试检索不存在的索引
	stu = genradix_ptr(&students_radix, 10);
	if (!stu) {
		printk(KERN_INFO "索引10不存在 (这是正常的)\n");
	}

	// 遍历所有学生
	genradix_for_each(&students_radix, iter, stu) {
		printk(KERN_INFO "遍历: ID=%d, 姓名=%s, 分数=%d\n", stu->id,
		       stu->name, stu->score);
	}
	return 0;
}

// 清理函数
static void __exit genradix_demo_exit(void)
{
	printk(KERN_INFO "GENRADIX 演示模块卸载\n");

	// 释放所有内存
	genradix_free(&students_radix);
}

module_init(genradix_demo_init);
module_exit(genradix_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Black");
MODULE_DESCRIPTION("简单的 GENRADIX 演示");


// [29413.744998] GENRADIX 演示模块加载
// [29413.745006] 添加: ID=0, 姓名=学生0, 分数=70
// [29413.745010] 添加: ID=1, 姓名=学生1, 分数=75
// [29413.745011] 添加: ID=2, 姓名=学生2, 分数=80
// [29413.745013] 添加: ID=3, 姓名=学生3, 分数=85
// [29413.745014] 添加: ID=4, 姓名=学生4, 分数=90
// [29413.745016] 查找索引2: 学生2, 分数=80
// [29413.745018] 遍历: ID=0, 姓名=学生0, 分数=70
// [29413.745019] 遍历: ID=1, 姓名=学生1, 分数=75
// [29413.745022] 遍历: ID=2, 姓名=学生2, 分数=80
// [29413.745023] 遍历: ID=3, 姓名=学生3, 分数=85
// [29413.745025] 遍历: ID=4, 姓名=学生4, 分数=90
// [29413.745026] 遍历: ID=0, 姓名=, 分数=0
// [29413.745028] 遍历: ID=0, 姓名=, 分数=0
// [29413.745030] 遍历: ID=0, 姓名=, 分数=0
// [29413.745031] 遍历: ID=0, 姓名=, 分数=0
// [29413.745032] 遍历: ID=0, 姓名=, 分数=0
// [29413.745034] 遍历: ID=0, 姓名=, 分数=0
// [29413.745035] 遍历: ID=0, 姓名=, 分数=0
// [29413.745036] 遍历: ID=0, 姓名=, 分数=0
// [29413.745038] 遍历: ID=0, 姓名=, 分数=0
// [29413.745039] 遍历: ID=0, 姓名=, 分数=0
// [29413.745041] 遍历: ID=0, 姓名=, 分数=0
// [29413.745042] 遍历: ID=0, 姓名=, 分数=0
// [29413.745043] 遍历: ID=0, 姓名=, 分数=0
