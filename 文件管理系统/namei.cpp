#include<iostream>
#include"fs.h"
#include <ctime>

/*在给出的目录内根据文件名查询具体的某一项，返回查询到的inode 的数据块 */
struct buffer_head * find_entry(struct m_inode ** dir,
	const char * name, int namelen, struct dir_entry ** res_dir)
{
	int entries;
	int block, i;
	struct buffer_head * bh;
	struct dir_entry * de;
	struct super_block * sb;

	entries = (*dir)->i_size / (sizeof(struct dir_entry));
	*res_dir = NULL;
	if (!namelen) 
		return NULL;
	/* check for '..', as we might have to do some "magic" for it */
	if (namelen == 2 && name[0] == '.' && name[1] == '.') {
		/* '..' in a pseudo-root results in a faked '.' (just change namelen) */
		if ((*dir) == fileSystem->root)
			namelen = 1;
		else if ((*dir)->i_num == ROOT_INO) {
			/* '..' over a mount-point results in 'dir' being exchanged for the mounted
			   directory-inode. NOTE! We set mounted, so that we can iput the new dir */
			sb = get_super((*dir)->i_dev);
			if (sb->s_imount) {
				iput(*dir);
				(*dir) = sb->s_imount;
				(*dir)->i_count++;
			}
		}
	}
	block = (*dir)->i_zone[0];
	if (block <= 0)return NULL;
	bh = bread(block);
	i = 0;
	de = (struct dir_entry *) bh->b_data;
	while (i < entries) {
		/*一个目录块读取完毕，未找到目标，则换下一个目录寻找*/
		if ((char *)de >= BLOCK_SIZE + bh->b_data) {
			brelse(bh);
			block = bmap(*dir, i / DIR_ENTRIES_PER_BLOCK);
			bh = bread(block);
			if (bh == NULL || block <= 0)
			{
				/*如果下一个目录项读取失败，则跳过该目录*/
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = (struct dir_entry *) bh->b_data;
		}
		
		if (de->inode!=0 && !strcmp(name, de->name)) {
			*res_dir = de;
			return bh;
		}
		de++;
		i++;
	}
	brelse(bh);
	return NULL;
}

/*在给出的dir中增加一个目录项，需给出名字和长度
返回包含了子目录的数据块
res_dir 返回创建的子目录项，注意，add_entry只设定了新创建目录的名字，未设置i节点编号*/
struct buffer_head * add_entry(struct m_inode * dir,
	const char * name, int namelen, struct dir_entry ** res_dir)
{
	int block, i;
	struct buffer_head * bh;
	struct dir_entry * de;

	*res_dir = NULL;
#ifdef NO_TRUNCATE
	if (namelen > NAME_LEN)
		return NULL;
#else
	if (namelen > NAME_LEN)
		namelen = NAME_LEN;
#endif
	if (!namelen)
		return NULL;
	if (!(block = dir->i_zone[0]))
		return NULL;
	if (!(bh = bread(block)))
		return NULL;
	i = 0;
	de = (struct dir_entry *) bh->b_data;
	while (1) {

		/*遍历完了一个数据块*/
		if ((char *)de >= BLOCK_SIZE + bh->b_data)
		{
			brelse(bh);
			bh = NULL;
			block = create_block(dir, i / DIR_ENTRIES_PER_BLOCK);
			if (!block)
				return NULL;
			if (!(bh = bread(block))) {
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = (struct dir_entry *) bh->b_data;
		}

		/*遍历了所有子目录，未找到空闲项，则在最后增加一项*/
		if (i * sizeof(struct dir_entry) >= dir->i_size) {
			de->inode = 0;
			dir->i_size = (i + 1) * sizeof(struct dir_entry);
			dir->i_dirt = 1;
			dir->i_ctime = CurrentTime();
		}
		/*找到一个空闲的目录项*/
		if (!de->inode) {
			dir->i_mtime = CurrentTime();
			strncpy(de->name, name, NAME_LEN);
			de->name[NAME_LEN-1] = 0;
			for (i = namelen+1; i < NAME_LEN; i++)
				de->name[i] = 0;
			bh->b_dirt = 1;
			*res_dir = de;
			return bh;
		}
		de++;
		i++;
	}
	brelse(bh);
	return NULL;
}

/*给出一个路径，返回路径指向的inode节点
目录文件和普通文件文件节点结构相同且都存储在数据区，只是存储的数据不同罢了
对于路径 /user/linux 返回指向linux的inode节点
		/user/linux/ 同样返回指向linux的inode节点
		/user/linux/a.out 返回指向a.out的inode节点
*/
struct m_inode * get_inode(const char * pathname)
{
	char c;
	const char * thisname;
	struct m_inode * inode;
	struct buffer_head * bh;
	int namelen, inr, idev;
	struct dir_entry * de;
	if ((c = pathname[0]) == '/') {
		inode = fileSystem->root;
		pathname++;
	}
	else if (c)
		inode = fileSystem->current;
	else
		return NULL;	/* empty name is bad */
	inode->i_count++;
	while (1) {
		thisname = pathname;
		namelen = 0;
		while (pathname[namelen] != '/' && pathname[namelen] != '\0') {
			namelen++;
		}
		pathname += namelen;
		if (namelen <= 0)
			return inode;
		if (!(bh = find_entry(&inode, thisname, namelen, &de))) {
			iput(inode);
			return NULL;
		}
		inr = de->inode;
		idev = inode->i_dev;
		brelse(bh);
		iput(inode);
		if (!(inode = iget(idev, inr)))
			return NULL;
	}
}
/*
给出一个路径，返回路径指向的上面一层目录节点
----适用于 最后一层是一个普通文件的路径
对于路径 /user/linux 返回指向user的inode节点
		/user/linux/ 返回指向linux的inode节点（如果linux不是目录文件，则返回NULL）
		/user/linux/a.out 返回指向linux的inode节点
*/

struct m_inode * dir_namei(const char * pathname,int * namelen, const char ** name)
{
	char c;
	const char * basename;
	struct m_inode * dir;
	const char* _pathname = pathname;
	basename = pathname;
	while (true)
	{	
		c = pathname[0];
		pathname++;
		if (c == '\0') break;
		if (c == '/')
			basename = pathname;
	}
	*namelen = pathname - basename - 1;
	*name = basename;
	char* pathdir='\0';
	int len = strlen(_pathname) - *namelen;
	/*如果父目录字符长度为0，即返回当前工作目录，否则进行查询*/
	if (len == 0) {
		fileSystem->current->i_count++;
		return  fileSystem->current;
	}
	strncat(pathdir, _pathname, len);
	if (!(dir = get_inode(pathdir)))
		return NULL;
	return dir;
}

/*给出一个目录节点，判断该目录是否为空*/
int empty_dir(struct m_inode * inode)
{
	int nr, block;
	int len;
	struct buffer_head * bh;
	struct dir_entry * de;

	len = inode->i_size / sizeof(struct dir_entry);
	if (len < 2 || !inode->i_zone[0] ||
		!(bh = bread(inode->i_zone[0]))) {
		printf("warning - bad directory on dev %04x\n", inode->i_dev);
		return 0;
	}
	de = (struct dir_entry *) bh->b_data;
	if (de[0].inode != inode->i_num || !de[1].inode ||
		strcmp(".", de[0].name) || strcmp("..", de[1].name)) {
		printf("warning - bad directory on dev %04x\n", inode->i_dev);
		return 0;
	}
	nr = 2;
	de += 2;
	while (nr < len) {
		if ((void *)de >= (void *)(bh->b_data + BLOCK_SIZE)) {
			brelse(bh);
			block = bmap(inode, nr / DIR_ENTRIES_PER_BLOCK);
			if (!block) {
				nr += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			if (!(bh = bread(block)))
				return 0;
			de = (struct dir_entry *) bh->b_data;
		}
		/*找到一个子节点即返回*/
		if (de->inode) {
			brelse(bh);
			return 0;
		}
		de++;
		nr++;
	}
	brelse(bh);
	return 1;
}

/*给出inode 返回该inode的文件名*/
int get_name(struct m_inode * inode, char *buf,int size)
{
	int entries;
	int block, i;
	struct buffer_head * bh;
	struct dir_entry * de;
	if (inode->i_num == fileSystem->root->i_num)
	{
		buf[0]='/';
		return 1;
	}
	/*获取父节点*/
	struct m_inode * dir = get_father(inode);

	/*从父节点查询*/
	entries = dir->i_size / (sizeof(struct dir_entry));
	block = dir->i_zone[0];
	if (block <= 0)return NULL;
	bh = bread(block);
	i = 0;
	de = (struct dir_entry *) bh->b_data;
	while (i < entries) {
		/*一个目录块读取完毕，未找到目标，则换下一个目录寻找*/
		if ((char *)de >= BLOCK_SIZE + bh->b_data) {
			brelse(bh);
			block = bmap(dir, i / DIR_ENTRIES_PER_BLOCK);
			bh = bread(block);
			if (bh == NULL || block <= 0)
			{
				/*如果下一个目录项读取失败，则跳过该目录*/
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			de = (struct dir_entry *) bh->b_data;
		}

		if (de->inode==inode->i_num) {
			strncpy(buf, de->name, size);
			iput(dir);
			brelse(bh);
			return 1;
		}
		de++;
		i++;
	}
	brelse(bh);
	return NULL;
}
/*给出一个inode ，返回该inode的父节点*/
struct m_inode *get_father(struct m_inode * inode)
{
	int block = inode->i_zone[0];
	if (block <= 0)return NULL;
	buffer_head* bh = bread(block);
	struct dir_entry * de = (struct dir_entry *) bh->b_data;
	de++;
	struct m_inode * dir = iget(inode->i_dev, de->inode);
	brelse(bh);
	if (dir == NULL)
	{
		return NULL;
	}
	return dir;
}

unsigned long CurrentTime()
{
	time_t now = time(0);
	return now;
}
char* longtoTime(long l) {
	time_t time = (time_t)l;
	return ctime(&time);
}