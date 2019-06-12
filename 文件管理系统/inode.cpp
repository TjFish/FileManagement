#include<iostream>
#include"fs.h"
/*对inode操作提供以下接口，
  在磁盘上创建一个新的inode
  1. 获取已经在磁盘上存在的inode iget()
  2. 在磁盘上创建一个新的inode  new_inode()
  3. 在磁盘上删除并清空一个inode  free_inode()
  4. 对于多进程而言，需要定义iput 让进程放弃对inode的使用权
！！！注意 目前由于没有多进程，故对于i_count等信号量使用并不规范
*/



/*
  由于考虑到内存空间和使用效率的提升，使用inode_table存储已经读入到内存中的所有inode
  inode_table 大小为32，即意味着该系统在内存中最多保留32个inode节点
  inode.i_count 代表inode的引用数，当i_count=0 即代表没有进程使用它（虽然本系统没有多进程）
 */
static struct m_inode* inode_tabel[NR_INODE];
static int head = 0;
/*写回inode*/
static void write_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;
	int block;

	if (!inode->i_dirt) {
		return;
	}
	if (!(sb = get_super(inode->i_dev)))
		printf("trying to write inode without device");
	block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
		(inode->i_num - 1) / INODES_PER_BLOCK;
	if (!(bh = bread(block)))
		printf("unable to read i-node block");
	((struct d_inode *)bh->b_data)
		[(inode->i_num - 1) % INODES_PER_BLOCK] =
		*(struct d_inode *)inode;
	bh->b_dirt = 1;
	inode->i_dirt = 0;
	brelse(bh);
}
/*申请获取inode_table 的空间，任何读取到内存中的inode 都需要先向inode_tabel申请空间*/
static struct m_inode * get_empty_inode()
{
	struct m_inode * inode = NULL;
	for (int i = 0; i < NR_INODE; ++i)
	{
		inode = inode_tabel[head];
		head = (head + 1) % NR_INODE;
		if (inode->i_count == 0)
		{
			if (inode->i_dirt)
			{
				write_inode(inode);
			}
			break;
		}
	}

	if (inode == NULL)
	{
		printf("inode_table 空间不足");
		//特殊状况暂且不做处理，应该不会遇到，毕竟是单线程
	}
	memset(inode, 0, sizeof(*inode));
	inode->i_count = 1;
	return inode;
}

/*初始化inode_tabel*/
void init_inode_table() {
	for (int i = 0; i < NR_INODE; ++i)
	{
		inode_tabel[i] = new m_inode();
	}
}
/*将inode_table所有信息写回磁盘*/
void realse_inode_table() {
	m_inode* inode;
	for (int i = 0; i < NR_INODE; ++i)
	{
		inode = inode_tabel[i];
		if (inode->i_dirt)
		{
			int block = 2 +11+(inode->i_num - 1) / INODES_PER_BLOCK;
			//if (inode->i_count != 0)
				//printf("%d WARING %d i_count!=0 \n",inode->i_num,block);
			write_inode(inode);
		}
	}
}


/*读入磁盘上inode节点*/
static void read_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;
	int block;

	if (!(sb = get_super(inode->i_dev)))
		printf("trying to read inode without dev");
	block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
		(inode->i_num - 1) / INODES_PER_BLOCK;
	if (!(bh = bread(block)))
		printf("unable to read i-node block");

	*(struct d_inode *)inode =
		((struct d_inode *)bh->b_data)
		[(inode->i_num - 1) % INODES_PER_BLOCK];
	brelse(bh);
}

/*给出i节点号，返回inode节点*/
struct m_inode *iget(int dev,int nr) 
{
	struct m_inode * inode;
	/*首先查看inode是否已经在内存中*/
	for (int i = 0; i < NR_INODE; ++i)
	{
		inode = inode_tabel[i];
		if (inode->i_num == nr)
		{
			inode->i_count++;
			return inode;
		}
	}
	inode = get_empty_inode();
	inode->i_dev = dev;
	inode->i_num = nr;
	//从磁盘中读取
	read_inode(inode);
	inode->i_dirt = 0;
	inode->i_count = 1;
	inode->i_update = 1;
	return inode;
}

//删除磁盘上的inode 节点
void free_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;

	if (!inode)
		return;
	/*如果要清空的文件节点有其他节点指向它，则说明出现bug*/
	if (inode->i_nlinks)
	{
		printf("！！！BUG trying to free inode with links");
		return;
	}
	if (!(sb = get_super(inode->i_dev)))
		printf("trying to free block on nonexistent device");
	if (!(bh = sb->s_imap[inode->i_num >> 13]))
	{
		printf("nonexistent imap in superblock");
		return;
	}

	if (!get_bit(inode->i_num % BLOCK_BIT, bh->b_data))
		printf("!!!BUG free_inode: bit already cleared.\n\r");
	clear_bit(inode->i_num % BLOCK_BIT, bh->b_data);
	bh->b_dirt = 1;
	//这里只清空了内存中数据，并不会实际清空磁盘上的数据
	memset(inode, 0, sizeof(*inode));
}

/*创建一个新的inode节点，该inode节点对应磁盘上空闲的位置*/
struct m_inode * new_inode(int dev)
{
	struct m_inode * inode;
	struct super_block * sb;
	struct buffer_head * bh;
	int i, j;

	/*申请获取内存中inode的空间（inode_table)*/
	if (!(inode = get_empty_inode()))
		return NULL;

	if (!(sb = get_super(dev)))
		printf("new_inode with unknown device");
	j = 8192;
	for (i = 0; i < 8; i++)
		if ((bh = sb->s_imap[i]))
			if ((j = find_first_zero(bh->b_data)) < 8192)
				break;
	if (!bh || j >= 8192 || j + i * 8192 > sb->s_ninodes) {
		iput(inode);
		return NULL;
	}
	//修改i节点位图
	if (get_bit(j, bh->b_data))
	{
		//位图已经存在，说明代码出现bug
		printf("！！！BUG new_inode: bit already set");
		return 0;
	}
	set_bit(j, bh->b_data);
	bh->b_dirt = 1;

	//初始化inode
	inode->i_count = 1;
	inode->i_nlinks = 1;
	inode->i_dev = dev;
	//inode->i_uid = current->euid;
	//inode->i_gid = current->egid;
	inode->i_dirt = 1;
	inode->i_num = j + i * 8192;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CurrentTime();
	return inode;
}

/*对于多进程，iput用于进程释放inode的使用权，即使i_count减一
  当i_count=0 时,若该节点有过修改，则立即写回磁盘，如未修改，无需写回
  但需要注意的是，i_count=0时，并不会立即清空该inode节点在inode_table中的数据(即仍然保留在内存中）
  因为可能该节点之后会再次被用到，这样可以减少磁盘读取，提升效率
  只有当inode_table需要得到空的inode时，才会将该inode清空,具体见 get_empty_inode函数*/
void iput(struct m_inode * inode)
{

	if (!inode) return;
	if (!inode->i_count)
		printf("!!!BUG iput: trying to free free inode");
	if (inode->i_count>1)
	{
		inode->i_count--;
		return;
	}
	/*如果指向该inode的链接数为 0，删除该文件，清空inode的所有数据区，并释放inode节点*/
	if (!inode->i_nlinks) {
		truncate(inode);
		free_inode(inode);
		//printf("******文件删除******");
		return;
	}
	if (inode->i_dirt) {
		write_inode(inode);	
		inode->i_dirt = 0;
	}
	inode->i_count--;
	return;
}

/*逻辑数据块与物理数据块地址转换，给出逻辑数据块，返回物理数据块*/
int bmap(struct m_inode * inode, int block)
{
	struct buffer_head * bh;
	int i;
	
	if (block < 0)
		printf("试图读取不存在的数据块");
	if (block >= 7 + 512 + 512 * 512)
		printf("试图读取超过范围的数据块");
	/*直接查询*/
	if (block < 7) {
		return inode->i_zone[block];
	}
	/*一级目录查询*/
	block -= 7;
	if (block < 512) {
		if (!inode->i_zone[7])
			return 0;
		bh = bread(inode->i_zone[7]);
		i = ((unsigned short *)(bh->b_data))[block];
		brelse(bh);
		return i;
	}
	/*二级目录查询*/
	block -= 512;
	if (!inode->i_zone[8])
		return 0;
	bh = bread(inode->i_zone[8]);
	i = ((unsigned short *)bh->b_data)[block >> 9];
	brelse(bh);
	if (!i)
		return 0;
	bh = bread(i);
	i = ((unsigned short *)bh->b_data)[block & 511];
	brelse(bh);
	return i;
}

/*读取数据块，给出一个i节点，以及数据块编号，读取出数据块,
	注意！！！ 如果该数据块不存在，则会创建它*/
int create_block(struct m_inode * inode, int block)
{
	struct buffer_head * bh;
	int i;

	if (block < 0)
		printf("_bmap: block<0");
	if (block >= 7 + 512 + 512 * 512)
		printf("_bmap: block>big");
	if (block < 7) {
		if (!inode->i_zone[block])
			if ((inode->i_zone[block] = new_block(inode->i_dev))) {
				inode->i_ctime = CurrentTime();
				inode->i_dirt = 1;
			}
		return inode->i_zone[block];
	}
	block -= 7;
	if (block < 512) {
		//首先判断一级目录是否创建
		if (!inode->i_zone[7])
			if ((inode->i_zone[7] = new_block(inode->i_dev))) {
				inode->i_dirt = 1;
				inode->i_ctime = CurrentTime();
			}
		//创建失败
		if (!inode->i_zone[7])
			return 0;
		if (!(bh = bread(inode->i_zone[7])))
			return 0;
		i = ((unsigned short *)(bh->b_data))[block];
		//判断具体block是否创建，没有创建则创建
		if (!i)
			if ((i = new_block(inode->i_dev))) {
				((unsigned short *)(bh->b_data))[block] = i;
				bh->b_dirt = 1;
			}
		brelse(bh);
		return i;
	}
	block -= 512;
	//首先判断二级目录是否创建
	if (!inode->i_zone[8])
		if ((inode->i_zone[8] = new_block(inode->i_dev))) {
			inode->i_dirt = 1;
			inode->i_ctime = CurrentTime();
		}
	//创建失败
	if (!inode->i_zone[8])
		return 0;

	if (!(bh = bread(inode->i_zone[8])))
		return 0;
	i = ((unsigned short *)bh->b_data)[block >> 9];
	if (!i)
		if ((i = new_block(inode->i_dev))) {
			((unsigned short *)(bh->b_data))[block >> 9] = i;
			bh->b_dirt = 1;
		}
	brelse(bh);
	if (!i)
		return 0;
	if (!(bh = bread(i)))
		return 0;
	i = ((unsigned short *)bh->b_data)[block & 511];
	if (!i)
		if ((i = new_block(inode->i_dev))) {
			((unsigned short *)(bh->b_data))[block & 511] = i;
			bh->b_dirt = 1;
		}
	brelse(bh);
	return i;
}