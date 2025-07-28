#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/page-flags.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/atomic.h>
#include <linux/hashtable.h>

#define CACHE_SIZE_MB 64
#define CACHE_SECTOR_SIZE 512
#define CACHE_BLOCKS ((CACHE_SIZE_MB * 1024 * 1024) / SECTOR_SIZE)
#define CACHE_HASH_BITS 10 // 1024个哈希桶

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DeepSeek");
MODULE_DESCRIPTION("HDD Caching Module using blk-mq");

// 缓存块结构
struct cache_block {
	sector_t sector; // 起始扇区
	struct page *page; // 数据页
	struct hlist_node hash; // 哈希链表节点
	struct list_head lru; // LRU 链表节点
	bool dirty; // 脏数据标志
	atomic_t ref_count; // 引用计数
};

// 缓存设备结构
struct cache_device {
	struct file *bdev_file;   // 后端设备文件
	struct block_device *bdev; // 后端块设备
	struct gendisk *gd; // 生成的磁盘
	struct blk_mq_tag_set tag_set; // blk-mq 标签集

	// 缓存管理
	DECLARE_HASHTABLE(block_hash, CACHE_HASH_BITS); // 哈希表
	struct list_head lru_list; // LRU 链表
	spinlock_t lock; // 缓存锁
	atomic_t block_count; // 当前缓存块数

	// 回写线程
	struct task_struct *thread; // 回写线程
	bool stop_thread; // 线程停止标志

	// 统计信息
	atomic_t cache_hits; // 缓存命中统计
	atomic_t cache_misses; // 缓存未命中统计
};

static struct cache_device *cache_dev;

// 查找缓存块
static struct cache_block *cache_lookup(sector_t sector)
{
	struct cache_block *blk;
	unsigned int hash_key = hash_32(sector, CACHE_HASH_BITS);

	hlist_for_each_entry(blk, &cache_dev->block_hash[hash_key], hash) {
		if (blk->sector == sector) {
			// 移动到最后（LRU）
			spin_lock(&cache_dev->lock);
			list_move_tail(&blk->lru, &cache_dev->lru_list);
			spin_unlock(&cache_dev->lock);

			atomic_inc(&blk->ref_count);
			return blk;
		}
	}
	return NULL;
}

// 分配新缓存块
static struct cache_block *alloc_cache_block(sector_t sector)
{
	struct cache_block *blk;

	// 检查缓存是否已满
	if (atomic_read(&cache_dev->block_count) >= CACHE_BLOCKS) {
		// 尝试从LRU前端移除（最近最少使用）
		spin_lock(&cache_dev->lock);
		if (!list_empty(&cache_dev->lru_list)) {
			blk = list_first_entry(&cache_dev->lru_list,
					       struct cache_block, lru);

			// 如果是脏块，需要先写回（简化处理：跳过）
			if (!blk->dirty) {
				// 从哈希表移除
				unsigned int hash_key =
					hash_32(blk->sector, CACHE_HASH_BITS);
				hlist_del_init(&blk->hash);
				list_del_init(&blk->lru);

				// 重用现有块
				blk->sector = sector;
				blk->dirty = false;
				atomic_set(&blk->ref_count, 1);

				// 添加到新位置
				hlist_add_head(
					&blk->hash,
					&cache_dev->block_hash[hash_key]);
				list_add_tail(&blk->lru, &cache_dev->lru_list);
				spin_unlock(&cache_dev->lock);
				return blk;
			}
		}
		spin_unlock(&cache_dev->lock);
	}

	// 分配新块
	blk = kzalloc(sizeof(struct cache_block), GFP_KERNEL);
	if (!blk)
		return NULL;

	blk->page = alloc_page(GFP_KERNEL);
	if (!blk->page) {
		kfree(blk);
		return NULL;
	}

	blk->sector = sector;
	blk->dirty = false;
	atomic_set(&blk->ref_count, 1);
	INIT_HLIST_NODE(&blk->hash);
	INIT_LIST_HEAD(&blk->lru);

	// 添加到哈希表和LRU列表
	unsigned int hash_key = hash_32(sector, CACHE_HASH_BITS);
	spin_lock(&cache_dev->lock);
	hlist_add_head(&blk->hash, &cache_dev->block_hash[hash_key]);
	list_add_tail(&blk->lru, &cache_dev->lru_list);
	atomic_inc(&cache_dev->block_count);
	spin_unlock(&cache_dev->lock);

	return blk;
}

// 释放缓存块
static void free_cache_block(struct cache_block *blk)
{
	if (atomic_dec_and_test(&blk->ref_count)) {
		// 从数据结构中移除
		spin_lock(&cache_dev->lock);
		hlist_del_init(&blk->hash);
		list_del_init(&blk->lru);
		atomic_dec(&cache_dev->block_count);
		spin_unlock(&cache_dev->lock);

		// 释放资源
		if (blk->page)
			__free_page(blk->page);
		kfree(blk);
	}
}

// blk-mq 队列请求处理函数
static blk_status_t cache_queue_rq(struct blk_mq_hw_ctx *hctx,
				   const struct blk_mq_queue_data *bd)
{
	struct request *rq = bd->rq;
	struct bio *bio = rq->bio;
	struct cache_block *blk = NULL;
	blk_status_t ret = BLK_STS_OK;

	// 处理每个bio
	while (bio) {
		sector_t sector = bio->bi_iter.bi_sector;
		int dir = bio_data_dir(bio);

		// 处理读请求
		if (dir == READ) {
			blk = cache_lookup(sector);
			if (blk) {
				// 缓存命中
				void *src = kmap_atomic(blk->page);
				void *dst = kmap_atomic(bio_page(bio));

				memcpy(dst, src, SECTOR_SIZE);

				kunmap_atomic(dst);
				kunmap_atomic(src);

				atomic_inc(&cache_dev->cache_hits);
				bio_endio(bio);
				free_cache_block(blk);
				goto next_bio;
			}
			atomic_inc(&cache_dev->cache_misses);
		}

		// 处理写请求
		if (dir == WRITE) {
			blk = cache_lookup(sector);
			if (!blk) {
				blk = alloc_cache_block(sector);
				if (!blk)
					goto passthrough;
			}

			// 数据复制到缓存
			void *dst = kmap_atomic(blk->page);
			void *src = kmap_atomic(bio_page(bio));

			memcpy(dst, src, SECTOR_SIZE);

			kunmap_atomic(src);
			kunmap_atomic(dst);

			blk->dirty = true;
			bio_endio(bio);
			free_cache_block(blk);
			goto next_bio;
		}

passthrough:
		// 未处理请求直接传递给后端设备
		bio->bi_bdev = cache_dev->bdev;
		ret = BLK_STS_IOERR; // 简化处理，实际应正确处理

next_bio:
		bio = bio->bi_next;
	}

	// 标记请求完成
	blk_mq_end_request(rq, ret);
	return ret;
}

// blk-mq 操作结构体
static const struct blk_mq_ops cache_mq_ops = {
	.queue_rq = cache_queue_rq,
};

// 回写线程函数
static int writeback_thread(void *data)
{
	struct cache_block *blk, *tmp;
	struct list_head write_list;

	INIT_LIST_HEAD(&write_list);

	while (!kthread_should_stop() && !cache_dev->stop_thread) {
		msleep(5000); // 每5秒回写一次

		// 收集脏块
		spin_lock(&cache_dev->lock);
		list_for_each_entry_safe(blk, tmp, &cache_dev->lru_list, lru) {
			if (blk->dirty) {
				atomic_inc(&blk->ref_count);
				list_move_tail(&blk->lru, &write_list);
			}
		}
		spin_unlock(&cache_dev->lock);

		// 处理收集的脏块
		while (!list_empty(&write_list)) {
			blk = list_first_entry(&write_list, struct cache_block,
					       lru);
			list_del_init(&blk->lru);

			// 创建写请求
			struct bio *bio = bio_alloc(GFP_NOIO, 1);
			bio->bi_bdev = cache_dev->bdev;
			bio->bi_iter.bi_sector = blk->sector;
			bio->bi_opf = REQ_OP_WRITE;

			// 添加数据页
			if (bio_add_page(bio, blk->page, SECTOR_SIZE, 0) ==
			    SECTOR_SIZE) {
				// 提交请求并等待完成
				submit_bio_wait(bio);
			}
			bio_put(bio);

			// 清除脏标志
			spin_lock(&cache_dev->lock);
			blk->dirty = false;
			// 放回LRU列表
			list_add_tail(&blk->lru, &cache_dev->lru_list);
			spin_unlock(&cache_dev->lock);

			free_cache_block(blk);
		}
	}

	return 0;
}

// 块设备操作
static const struct block_device_operations cache_fops = {
	.owner = THIS_MODULE,
};

// 初始化缓存设备
static int init_cache_device(void)
{
	int ret = 0;

	cache_dev = kzalloc(sizeof(struct cache_device), GFP_KERNEL);
	if (!cache_dev)
		return -ENOMEM;

	// 初始化缓存结构
	INIT_LIST_HEAD(&cache_dev->lru_list);
	spin_lock_init(&cache_dev->lock);
	hash_init(cache_dev->block_hash);
	atomic_set(&cache_dev->block_count, 0);
	atomic_set(&cache_dev->cache_hits, 0);
	atomic_set(&cache_dev->cache_misses, 0);

	// 打开后端设备 (这里使用sda为例)
	cache_dev->bdev = blkdev_get_by_path(
		"/dev/sda", FMODE_READ | FMODE_WRITE, THIS_MODULE);
	if (IS_ERR(cache_dev->bdev)) {
		ret = PTR_ERR(cache_dev->bdev);
		goto error;
	}

	// 设置blk-mq标签集
	memset(&cache_dev->tag_set, 0, sizeof(cache_dev->tag_set));
	cache_dev->tag_set.ops = &cache_mq_ops;
	cache_dev->tag_set.nr_hw_queues = 1;
	cache_dev->tag_set.queue_depth = 128;
	cache_dev->tag_set.numa_node = NUMA_NO_NODE;
	cache_dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	cache_dev->tag_set.cmd_size = 0;
	cache_dev->tag_set.driver_data = cache_dev;

	ret = blk_mq_alloc_tag_set(&cache_dev->tag_set);
	if (ret) {
		printk(KERN_ERR "Failed to allocate tag set\n");
		goto error_bdev;
	}

	// 创建gendisk
	cache_dev->gd = blk_mq_alloc_disk(&cache_dev->tag_set, cache_dev);
	if (IS_ERR(cache_dev->gd)) {
		ret = PTR_ERR(cache_dev->gd);
		goto error_tagset;
	}

	// 设置磁盘属性
	snprintf(cache_dev->gd->disk_name, DISK_NAME_LEN, "cache_disk");
	cache_dev->gd->major = 0; // 动态分配主设备号
	cache_dev->gd->first_minor = 0;
	cache_dev->gd->minors = 1;
	cache_dev->gd->fops = &cache_fops;
	cache_dev->gd->private_data = cache_dev;

	// 设置容量（与后端设备相同）
	set_capacity(cache_dev->gd, get_capacity(cache_dev->bdev->bd_disk));
	blk_queue_logical_block_size(cache_dev->gd->queue, SECTOR_SIZE);
	blk_queue_physical_block_size(cache_dev->gd->queue, SECTOR_SIZE);

	// 添加磁盘
	ret = add_disk(cache_dev->gd);
	if (ret) {
		printk(KERN_ERR "Failed to add disk\n");
		goto error_disk;
	}

	// 启动回写线程
	cache_dev->stop_thread = false;
	cache_dev->thread =
		kthread_run(writeback_thread, NULL, "cache_writeback");
	if (IS_ERR(cache_dev->thread)) {
		ret = PTR_ERR(cache_dev->thread);
		goto error_unregister;
	}

	printk(KERN_INFO "HDD Cache module loaded. Cache size: %d MB\n",
	       CACHE_SIZE_MB);
	return 0;

error_unregister:
	del_gendisk(cache_dev->gd);
error_disk:
	put_disk(cache_dev->gd);
error_tagset:
	blk_mq_free_tag_set(&cache_dev->tag_set);
error_bdev:
	blkdev_put(cache_dev->bdev, FMODE_READ | FMODE_WRITE);
error:
	kfree(cache_dev);
	return ret;
}

// 清理缓存设备
static void cleanup_cache_device(void)
{
	struct cache_block *blk, *tmp;
	struct hlist_node *n;
	int bkt;

	if (cache_dev) {
		// 停止回写线程
		cache_dev->stop_thread = true;
		if (cache_dev->thread)
			kthread_stop(cache_dev->thread);

		// 移除磁盘
		del_gendisk(cache_dev->gd);
		put_disk(cache_dev->gd);

		// 释放标签集
		blk_mq_free_tag_set(&cache_dev->tag_set);

		// 释放所有缓存块
		spin_lock(&cache_dev->lock);
		hash_for_each_safe(cache_dev->block_hash, bkt, n, blk, hash) {
			hlist_del(&blk->hash);
			list_del(&blk->lru);
			if (blk->dirty) {
				struct bio *bio = bio_alloc(GFP_NOIO, 1);
				bio->bi_bdev = cache_dev->bdev;
				bio->bi_iter.bi_sector = blk->sector;
				bio->bi_opf = REQ_OP_WRITE;

				if (bio_add_page(bio, blk->page,
						 CACHE_SECTOR_SIZE,
						 0) == CACHE_SECTOR_SIZE) {
					spin_unlock(&cache_dev->lock);
					submit_bio_wait(bio);
					spin_lock(&cache_dev->lock);
				}
				bio_put(bio);
			}
			__free_page(blk->page);
			kfree(blk);
		}
		spin_unlock(&cache_dev->lock);

		// 打印统计信息
		int hits = atomic_read(&cache_dev->cache_hits);
		int misses = atomic_read(&cache_dev->cache_misses);
		printk(KERN_INFO
		       "Cache stats: Hits=%d, Misses=%d, Hit rate=%.2f%%\n",
		       hits, misses, (hits * 100.0) / (hits + misses + 1));

		// 清理资源
		blkdev_put(cache_dev->bdev, FMODE_READ | FMODE_WRITE);
		kfree(cache_dev);
	}

	printk(KERN_INFO "HDD Cache module unloaded\n");
}

static int __init cache_init(void)
{
	return init_cache_device();
}

static void __exit cache_exit(void)
{
	cleanup_cache_device();
}

module_init(cache_init);
module_exit(cache_exit);
