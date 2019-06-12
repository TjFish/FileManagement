#include <fstream>
#include <iostream>
#include<map>
#include"fs.h"
using namespace std;
/*
目前暂不设置内存中blocks 的大小
磁盘这部分为外界提供几个功能
1. 获取已经在磁盘上存在的数据块 bread()
2. 在磁盘上创建一个新的数据块  new_block()
3. 在磁盘上删除并清空一个数据块  free_block()
4. 对于多进程而言，需要定义brelse 让进程放弃对block的使用权
！！！注意 目前由于没有多进程，故对于b_count等信号量使用并不规范
*/
map<int,buffer_head*> blocks;

/*向内存中申请一块空间存放block*/
static buffer_head* getblk(int block)
{
	buffer_head* bh;
	bh = blocks[block];
	if (bh) //如果内存中已经存在该数据块，则释放该其内存
	{
		if (bh->b_uptodate)
		{
			cerr << "尝试申请一块已经在内存中存在，且最新的数据块" << endl;
			return NULL;
		}
		delete blocks[block]->b_data;
		delete blocks[block];
	}
	auto bb = new buffer_block();
	bh = new buffer_head();
	bh->b_data = bb;
	bh->b_blocknr = block;
	bh->b_count = 1;
	bh->b_dirt = 0;
	//刚刚申请的内存还未读入数据块
	bh->b_uptodate = 0;
	
	blocks[block] = bh;
	return bh;
}
/*获取内存中已经存在的block*/
static buffer_head* get_hash_table(int block)
{
	auto bk = blocks.find(block);
	if (bk != blocks.end())
	{
		if (bk->second->b_uptodate)
		{
			return bk->second;
		}
	}
	return NULL;
}
//磁盘块写入函数
static char* bwrite(int block, char *bh) {
	ofstream disk;
	/*血泪啊，c++想要修改文件部分的值，必须以读加写模式打开，单纯以写模式打开，会清空数据*/
	disk.open("hdc-0.11.img", ios::binary | ios::out|ios::in);
	disk.seekp((block + 1)*BLOCK_SIZE,ios::beg);
	//cout << block << "  Write to the file" << endl;
	disk.write(bh, BLOCK_SIZE);
	disk.close();
	return bh;
}
void realse_all_blocks()
{
	for (auto i : blocks)
	{
		auto bh = i.second;
		if (bh->b_dirt)
		{
			//printf("WARING %d b_count!=0 \n",i.first);
			printf("%d block write\n", i.first);
			bwrite(bh->b_blocknr, bh->b_data);
			delete bh->b_data;
			delete bh;
		}
	}
	blocks.clear();
}

/*
根据block编号获取已经在磁盘上存在的数据块 
*/
buffer_head* bread(int block) {
	buffer_head* bh;
	//从blocks查找看该block是否已经读入内存，存在则直接返回
	if (bh = get_hash_table(block))
	{
		bh->b_count++;
		return bh;
	}
	//向blocks申请内存中block
	bh = getblk(block);
	//从磁盘中读取
	ifstream disk;
	disk.open("hdc-0.11.img", ios::binary);
	disk.seekg((block+1)*BLOCK_SIZE);
	disk.read(bh->b_data, BLOCK_SIZE);
	//cout << block<<"  Reading from the file"<< endl;
	disk.close();
	bh->b_uptodate = 1;
	bh->b_dirt = 0;
	bh->b_count = 1;
	return bh;
}

/*清空指定数据区，其实并不会将磁盘上的数据清0，只是将对应数据块位图进行修改为0*/
void free_block(int dev, int block)
{
	struct super_block * sb;
	struct buffer_head * bh;
	if (!(sb = get_super(dev)))
		printf("trying to free block on nonexistent device");
	if (block < sb->s_firstdatazone || block >= sb->s_nzones)
		printf("trying to free block not in datazone");
	/*首先查看内存中是否已经存在要清空的数据区，有则将该数据区作废*/
	bh = get_hash_table(block);
	if (bh) {
		bh->b_count++;
		if (bh->b_count != 1) {
			printf("WARING trying to free block (%04x:%d), count=%d\n",
				dev, block, bh->b_count);
			return;
		}
		bh->b_dirt = 0;
		/*修改uptodata，表示内存中数据不是最新的*/
		bh->b_uptodate = 0;
		brelse(bh);
	}
	/*修改数据块位图*/
	block -= sb->s_firstdatazone - 1;
	if (!get_bit(block%BLOCK_BIT, sb->s_zmap[block / BLOCK_BIT]->b_data))
	{	//如果要修改的位图已经为0,说明出现程序bug
		printf("WARING block :%d already cleared\n", block + sb->s_firstdatazone - 1);
	}
	clear_bit(block%BLOCK_BIT, sb->s_zmap[block / BLOCK_BIT]->b_data);
	sb->s_zmap[block / 8192]->b_dirt = 1;
}

/*创建一个新的数据块，并写回磁盘的数据区*/
int new_block(int dev)
{
	struct buffer_head * bh;
	struct super_block * sb;
	int i, j;

	if (!(sb = get_super(dev)))
		printf("trying to get new block from nonexistant device");

	j = 8192;
	for (i = 0; i < 8; i++)
		if ((bh = sb->s_zmap[i]))
			if ((j = find_first_zero(bh->b_data)) < 8192)
				break;
	if (i >= 8 || !bh || j >= 8192)
		return 0;
	//修改逻辑块位图
	if (get_bit(j, bh->b_data))
	{
		printf("new_block: bit already set\n");
		return 0;
	}
	set_bit(j, bh->b_data);
	bh->b_dirt = 1;

	j += i * 8192 + sb->s_firstdatazone - 1;
	//超过了超级块的范围
	if (j >= sb->s_nzones)
		return 0;

	//之后可以考虑增加blocks的最大值，即需要申请空闲block
	/*
	if (!(bh = getblk(dev, j)))
		panic("new_block: cannot get block");
	if (bh->b_count != 1)
		panic("new block: count is != 1");
	clear_block(bh->b_data);
	*/
	//申请一块新的block空间
	bh = getblk(j);
	bh->b_count = 1;
	bh->b_uptodate = 1;
	bh->b_dirt = 1;
	brelse(bh);
	return j;
}

/*给予其他进程使用，每当一个进程通过bread获取数据块时，i_count++，
当b_count=0时，如果该数据块已经被修改，则写回磁盘，该块将加入空闲队列
b_count=0 仅代表目前该数据块没有进程使用*/
int brelse(buffer_head* bh) {
	if (!bh)return 1;
	if (bh->b_count > 1)
	{
		bh->b_count--;
		return 1;
	}
	if (bh->b_dirt)
	{
		//先不写回,syncy一次性写回
		//bwrite(bh->b_blocknr, bh->b_data);
		//bh->b_dirt = 0;
	}
	bh->b_count--;
	//暂且不考虑内存的释放
	return 1;
}

