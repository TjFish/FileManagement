#include<iostream>
#include"fs.h"
/*��inode�����ṩ���½ӿڣ�
  �ڴ����ϴ���һ���µ�inode
  1. ��ȡ�Ѿ��ڴ����ϴ��ڵ�inode iget()
  2. �ڴ����ϴ���һ���µ�inode  new_inode()
  3. �ڴ�����ɾ�������һ��inode  free_inode()
  4. ���ڶ���̶��ԣ���Ҫ����iput �ý��̷�����inode��ʹ��Ȩ
������ע�� Ŀǰ����û�ж���̣��ʶ���i_count���ź���ʹ�ò����淶
*/



/*
  ���ڿ��ǵ��ڴ�ռ��ʹ��Ч�ʵ�������ʹ��inode_table�洢�Ѿ����뵽�ڴ��е�����inode
  inode_table ��СΪ32������ζ�Ÿ�ϵͳ���ڴ�����ౣ��32��inode�ڵ�
  inode.i_count ����inode������������i_count=0 ������û�н���ʹ��������Ȼ��ϵͳû�ж���̣�
 */
static struct m_inode* inode_tabel[NR_INODE];
static int head = 0;
/*д��inode*/
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
/*�����ȡinode_table �Ŀռ䣬�κζ�ȡ���ڴ��е�inode ����Ҫ����inode_tabel����ռ�*/
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
		printf("inode_table �ռ䲻��");
		//����״�����Ҳ�������Ӧ�ò����������Ͼ��ǵ��߳�
	}
	memset(inode, 0, sizeof(*inode));
	inode->i_count = 1;
	return inode;
}

/*��ʼ��inode_tabel*/
void init_inode_table() {
	for (int i = 0; i < NR_INODE; ++i)
	{
		inode_tabel[i] = new m_inode();
	}
}
/*��inode_table������Ϣд�ش���*/
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


/*���������inode�ڵ�*/
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

/*����i�ڵ�ţ�����inode�ڵ�*/
struct m_inode *iget(int dev,int nr) 
{
	struct m_inode * inode;
	/*���Ȳ鿴inode�Ƿ��Ѿ����ڴ���*/
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
	//�Ӵ����ж�ȡ
	read_inode(inode);
	inode->i_dirt = 0;
	inode->i_count = 1;
	inode->i_update = 1;
	return inode;
}

//ɾ�������ϵ�inode �ڵ�
void free_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;

	if (!inode)
		return;
	/*���Ҫ��յ��ļ��ڵ��������ڵ�ָ��������˵������bug*/
	if (inode->i_nlinks)
	{
		printf("������BUG trying to free inode with links");
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
	//����ֻ������ڴ������ݣ�������ʵ����մ����ϵ�����
	memset(inode, 0, sizeof(*inode));
}

/*����һ���µ�inode�ڵ㣬��inode�ڵ��Ӧ�����Ͽ��е�λ��*/
struct m_inode * new_inode(int dev)
{
	struct m_inode * inode;
	struct super_block * sb;
	struct buffer_head * bh;
	int i, j;

	/*�����ȡ�ڴ���inode�Ŀռ䣨inode_table)*/
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
	//�޸�i�ڵ�λͼ
	if (get_bit(j, bh->b_data))
	{
		//λͼ�Ѿ����ڣ�˵���������bug
		printf("������BUG new_inode: bit already set");
		return 0;
	}
	set_bit(j, bh->b_data);
	bh->b_dirt = 1;

	//��ʼ��inode
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

/*���ڶ���̣�iput���ڽ����ͷ�inode��ʹ��Ȩ����ʹi_count��һ
  ��i_count=0 ʱ,���ýڵ��й��޸ģ�������д�ش��̣���δ�޸ģ�����д��
  ����Ҫע����ǣ�i_count=0ʱ��������������ո�inode�ڵ���inode_table�е�����(����Ȼ�������ڴ��У�
  ��Ϊ���ܸýڵ�֮����ٴα��õ����������Լ��ٴ��̶�ȡ������Ч��
  ֻ�е�inode_table��Ҫ�õ��յ�inodeʱ���ŻὫ��inode���,����� get_empty_inode����*/
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
	/*���ָ���inode��������Ϊ 0��ɾ�����ļ������inode�����������������ͷ�inode�ڵ�*/
	if (!inode->i_nlinks) {
		truncate(inode);
		free_inode(inode);
		//printf("******�ļ�ɾ��******");
		return;
	}
	if (inode->i_dirt) {
		write_inode(inode);	
		inode->i_dirt = 0;
	}
	inode->i_count--;
	return;
}

/*�߼����ݿ����������ݿ��ַת���������߼����ݿ飬�����������ݿ�*/
int bmap(struct m_inode * inode, int block)
{
	struct buffer_head * bh;
	int i;
	
	if (block < 0)
		printf("��ͼ��ȡ�����ڵ����ݿ�");
	if (block >= 7 + 512 + 512 * 512)
		printf("��ͼ��ȡ������Χ�����ݿ�");
	/*ֱ�Ӳ�ѯ*/
	if (block < 7) {
		return inode->i_zone[block];
	}
	/*һ��Ŀ¼��ѯ*/
	block -= 7;
	if (block < 512) {
		if (!inode->i_zone[7])
			return 0;
		bh = bread(inode->i_zone[7]);
		i = ((unsigned short *)(bh->b_data))[block];
		brelse(bh);
		return i;
	}
	/*����Ŀ¼��ѯ*/
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

/*��ȡ���ݿ飬����һ��i�ڵ㣬�Լ����ݿ��ţ���ȡ�����ݿ�,
	ע�⣡���� ��������ݿ鲻���ڣ���ᴴ����*/
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
		//�����ж�һ��Ŀ¼�Ƿ񴴽�
		if (!inode->i_zone[7])
			if ((inode->i_zone[7] = new_block(inode->i_dev))) {
				inode->i_dirt = 1;
				inode->i_ctime = CurrentTime();
			}
		//����ʧ��
		if (!inode->i_zone[7])
			return 0;
		if (!(bh = bread(inode->i_zone[7])))
			return 0;
		i = ((unsigned short *)(bh->b_data))[block];
		//�жϾ���block�Ƿ񴴽���û�д����򴴽�
		if (!i)
			if ((i = new_block(inode->i_dev))) {
				((unsigned short *)(bh->b_data))[block] = i;
				bh->b_dirt = 1;
			}
		brelse(bh);
		return i;
	}
	block -= 512;
	//�����ж϶���Ŀ¼�Ƿ񴴽�
	if (!inode->i_zone[8])
		if ((inode->i_zone[8] = new_block(inode->i_dev))) {
			inode->i_dirt = 1;
			inode->i_ctime = CurrentTime();
		}
	//����ʧ��
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