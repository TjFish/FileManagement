#include <fstream>
#include <iostream>
#include<map>
#include"fs.h"
using namespace std;
/*
Ŀǰ�ݲ������ڴ���blocks �Ĵ�С
�����ⲿ��Ϊ����ṩ��������
1. ��ȡ�Ѿ��ڴ����ϴ��ڵ����ݿ� bread()
2. �ڴ����ϴ���һ���µ����ݿ�  new_block()
3. �ڴ�����ɾ�������һ�����ݿ�  free_block()
4. ���ڶ���̶��ԣ���Ҫ����brelse �ý��̷�����block��ʹ��Ȩ
������ע�� Ŀǰ����û�ж���̣��ʶ���b_count���ź���ʹ�ò����淶
*/
map<int,buffer_head*> blocks;

/*���ڴ�������һ��ռ���block*/
static buffer_head* getblk(int block)
{
	buffer_head* bh;
	bh = blocks[block];
	if (bh) //����ڴ����Ѿ����ڸ����ݿ飬���ͷŸ����ڴ�
	{
		if (bh->b_uptodate)
		{
			cerr << "��������һ���Ѿ����ڴ��д��ڣ������µ����ݿ�" << endl;
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
	//�ո�������ڴ滹δ�������ݿ�
	bh->b_uptodate = 0;
	
	blocks[block] = bh;
	return bh;
}
/*��ȡ�ڴ����Ѿ����ڵ�block*/
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
//���̿�д�뺯��
static char* bwrite(int block, char *bh) {
	ofstream disk;
	/*Ѫ�ᰡ��c++��Ҫ�޸��ļ����ֵ�ֵ�������Զ���дģʽ�򿪣�������дģʽ�򿪣����������*/
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
����block��Ż�ȡ�Ѿ��ڴ����ϴ��ڵ����ݿ� 
*/
buffer_head* bread(int block) {
	buffer_head* bh;
	//��blocks���ҿ���block�Ƿ��Ѿ������ڴ棬������ֱ�ӷ���
	if (bh = get_hash_table(block))
	{
		bh->b_count++;
		return bh;
	}
	//��blocks�����ڴ���block
	bh = getblk(block);
	//�Ӵ����ж�ȡ
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

/*���ָ������������ʵ�����Ὣ�����ϵ�������0��ֻ�ǽ���Ӧ���ݿ�λͼ�����޸�Ϊ0*/
void free_block(int dev, int block)
{
	struct super_block * sb;
	struct buffer_head * bh;
	if (!(sb = get_super(dev)))
		printf("trying to free block on nonexistent device");
	if (block < sb->s_firstdatazone || block >= sb->s_nzones)
		printf("trying to free block not in datazone");
	/*���Ȳ鿴�ڴ����Ƿ��Ѿ�����Ҫ��յ������������򽫸�����������*/
	bh = get_hash_table(block);
	bh->b_count++;
	if (bh) {
		if (bh->b_count != 1) {
			printf("WARING trying to free block (%04x:%d), count=%d\n",
				dev, block, bh->b_count);
			return;
		}
		bh->b_dirt = 0;
		/*�޸�uptodata����ʾ�ڴ������ݲ������µ�*/
		bh->b_uptodate = 0;
		brelse(bh);
	}
	/*�޸����ݿ�λͼ*/
	block -= sb->s_firstdatazone - 1;
	if (!get_bit(block%BLOCK_BIT, sb->s_zmap[block / BLOCK_BIT]->b_data))
	{	//���Ҫ�޸ĵ�λͼ�Ѿ�Ϊ0,˵�����ֳ���bug
		printf("WARING block :%d already cleared\n", block + sb->s_firstdatazone - 1);
	}
	clear_bit(block%BLOCK_BIT, sb->s_zmap[block / BLOCK_BIT]->b_data);
	sb->s_zmap[block / 8192]->b_dirt = 1;
}

/*����һ���µ����ݿ飬��д�ش��̵�������*/
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
	//�޸��߼���λͼ
	if (get_bit(j, bh->b_data))
	{
		printf("new_block: bit already set\n");
		return 0;
	}
	set_bit(j, bh->b_data);
	bh->b_dirt = 1;

	j += i * 8192 + sb->s_firstdatazone - 1;
	//�����˳�����ķ�Χ
	if (j >= sb->s_nzones)
		return 0;

	//֮����Կ�������blocks�����ֵ������Ҫ�������block
	/*
	if (!(bh = getblk(dev, j)))
		panic("new_block: cannot get block");
	if (bh->b_count != 1)
		panic("new block: count is != 1");
	clear_block(bh->b_data);
	*/
	//����һ���µ�block�ռ�
	bh = getblk(j);
	bh->b_count = 1;
	bh->b_uptodate = 1;
	bh->b_dirt = 1;
	brelse(bh);
	return j;
}

/*������������ʹ�ã�ÿ��һ������ͨ��bread��ȡ���ݿ�ʱ��i_count++��
��b_count=0ʱ����������ݿ��Ѿ����޸ģ���д�ش��̣��ÿ齫������ж���
b_count=0 ������Ŀǰ�����ݿ�û�н���ʹ��*/
int brelse(buffer_head* bh) {
	if (!bh)return 1;
	if (bh->b_count > 1)
	{
		bh->b_count--;
		return 1;
	}
	if (bh->b_dirt)
	{
		bwrite(bh->b_blocknr, bh->b_data);
		bh->b_dirt = 0;
	}
	bh->b_count--;
	//���Ҳ������ڴ���ͷ�
	return 1;
}

