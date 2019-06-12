#include"fs.h"
#include<iostream>
using namespace std;

static super_block* sb[NR_SUPER];
struct super_block * get_super(int dev)
{
	return sb[0];
}

/*读入超级块信息*/
static struct super_block * read_super(int dev)
{
	auto s = new super_block;
	buffer_head* bh;
	int i, block;

	s->s_dev = dev;
	s->s_isup = NULL;
	s->s_imount = NULL;
	s->s_time = 0;
	s->s_rd_only = 0;
	s->s_dirt = 0;
	bh = bread(1);
	*((struct d_super_block *) s) =
		*((struct d_super_block *) bh->b_data);

	brelse(bh);

	if (s->s_magic != SUPER_MAGIC) {
		s->s_dev = 0;
		//free_super(s);
		return NULL;
	}
	for (i = 0; i < I_MAP_SLOTS; i++)
		s->s_imap[i] = NULL;
	for (i = 0; i < Z_MAP_SLOTS; i++)
		s->s_zmap[i] = NULL;
	block = 2;
	for (i = 0; i < s->s_imap_blocks; i++)
	{
		s->s_imap[i] = bread(block);
		block++;
	}
	for (i = 0; i < s->s_zmap_blocks; i++)
	{
		s->s_zmap[i] = bread(block);
		block++;
	}
	s->s_imap[0]->b_data[0] |= 1;
	s->s_zmap[0]->b_data[0] |= 1;
	sb[0] = s;
	return s;
}

void mount_root(void)
{
	int i, free;
	struct super_block * p;
	struct m_inode * mi;
	if (!(p = read_super(ROOT_DEV)))
	{
		printf("无法读入超级块");
		return;
	}
	if (!(mi = iget(ROOT_DEV,ROOT_INO)))
	{
		printf("无法读入根目录");
		return;
	}
	p->s_isup = p->s_imount = mi;
	fileSystem->root = fileSystem->current=mi;
	mi->i_count += 2;
	fileSystem->name = "/";
	free = 0;
	i = p->s_nzones;
	//统计位图信息，给出磁盘上空闲的i节点和逻辑块
	while (--i >= 0)
		if(!get_bit(i% BLOCK_BIT,p->s_zmap[i/ BLOCK_BIT]->b_data))
			free++;
	printf("%d/%d free blocks\n\r", free, p->s_nzones);
	free = 0;
	i = p->s_ninodes + 1;
	while (--i >= 0)
		if (!get_bit(i% BLOCK_BIT,p->s_imap[i/ BLOCK_BIT]->b_data))
			free++;
	printf("%d/%d free inodes\n\r", free, p->s_ninodes);
	printf("system load!\n");
}

