#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// 原始的有bug函数
void buggy_function()
{
	printf("Old buggy version running!\n");
}

// 修复后的新函数
void patched_function()
{
	printf("Patched version running! (Fixed)\n");
}

// 热补丁函数
void apply_hotpatch(void *old_func, void *new_func)
{
	// 1. 计算函数大小（简化处理）
	size_t func_size = 1024; // 实际中需要更精确的计算

	// 2. 修改内存页为可写
	void *page_start = (void *)((long)old_func & ~(getpagesize() - 1));
	if (mprotect(page_start, getpagesize(),
		     PROT_READ | PROT_WRITE | PROT_EXEC)) {
		perror("mprotect failed");
		return;
	}

	// 3. 保存原始指令（用于可能的回滚）
	static unsigned char backup[1024];
	memcpy(backup, old_func, func_size);

	// 4. 创建跳转指令（x86-64架构）
	unsigned char jump[14];
	// movabs $new_func, %rax
	jump[0] = 0x48;
	jump[1] = 0xB8;
	*(void **)(jump + 2) = new_func;
	// jmpq *%rax
	jump[10] = 0xFF;
	jump[11] = 0xE0;

	// 5. 应用补丁 - 原子替换
	memcpy(old_func, jump, sizeof(jump));

	printf("Hotpatch applied successfully!\n");
}

int main()
{
	printf("=== Before hotpatch ===\n");
	buggy_function();

	// 模拟热补丁应用
	printf("\nApplying hotpatch...\n");
	apply_hotpatch(buggy_function, patched_function);

	printf("\n=== After hotpatch ===\n");
	buggy_function(); // 现在调用的是修复后的版本

	return 0;
}

// # 编译程序（需要禁用栈保护）
// gcc -fno-stack-protector -g -z execstack -o hotpatch_demo hotpatch_demo.c
//
// # 运行程序
// ./hotpatch_demo
//
//=== Before hotpatch ===
// Old buggy version running!
//
// Applying hotpatch...
// Hotpatch applied successfully!
//
// === After hotpatch ===
// Patched version running! (Fixed)
