/*
 *  linux/fs/truncate.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include"fs.h"

static void free_ind(int dev, int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)
		return;
	if ((bh = bread(block))) {
		p = (unsigned short *)bh->b_data;
		for (i = 0; i < 512; i++, p++)
			if (*p)
				free_block(dev, *p);
		brelse(bh);
	}
	free_block(dev, block);
}

static void free_dind(int dev, int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)
		return;
	if ((bh = bread(block))) {
		p = (unsigned short *)bh->b_data;
		for (i = 0; i < 512; i++, p++)
			if (*p)
				free_ind(dev, *p);
		brelse(bh);
	}
	free_block(dev, block);
}
/*文件截断函数，清空文件的数据块（实际上是删除指向数据块的索引），同时将数据块从磁盘删除*/
void truncate(struct m_inode * inode)
{
	int i;
	/*只清空普通文件和目录文件*/
	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return;
	for (i = 0; i < 7; i++)
		if (inode->i_zone[i]) {
			free_block(inode->i_dev, inode->i_zone[i]);
			inode->i_zone[i] = 0;
		}
	free_ind(inode->i_dev, inode->i_zone[7]);
	free_dind(inode->i_dev, inode->i_zone[8]);
	inode->i_zone[7] = inode->i_zone[8] = 0;
	inode->i_size = 0;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_ctime = CurrentTime();
}

