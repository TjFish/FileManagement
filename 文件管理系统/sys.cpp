#include<iostream>
#include<string>
#include"fs.h"
#include<cmath>
#include<Windows.h>
#include"printfc.h"
using namespace std;



static string GetFileSize(long size)
{
	float num = 1024.00; //byte
	if (size < num)
		return to_string(size) + "B";
	if (size < pow(num, 2))
		return to_string((size / num)) + "K"; //kb
	if (size < pow(num, 3))
		return to_string(size / pow(num, 2)) + "M"; //M
	if (size < pow(num, 4))
		return to_string(size / pow(num, 3)) + "G"; //G

	return to_string(size / pow(num, 4))+ "T"; //T
}
static string GetFileMode(long mode)
{
	float num = 1024.00; //byte
	if (S_ISDIR(mode))
		return "目录文件";
	if (S_ISREG(mode))
		return "普通文件";
	return"未知文件类型";
}

/*文件分两种，普通文件和目录文件
对于目录文件，用户使用时想要的是
使用一个目录指针，该目录指针可以得到每一条目录的名字
对于普通文件，用户使用时想要的是
使用一个文件指针，该文件指针可以得到文件指定的内容
可以得到文件名，文件大小等信息
*/
/*文件操作的系统调用，包括open，close，read，write，lseek*/
int sys_open(string filename, int flag, int mode) {
	struct m_inode * inode;
	struct file * f;
	int i, fd;

	for (fd = 0; fd < NR_OPEN; fd++)
	{
		if (!fileSystem->filp[fd])
			break;
	}

	if (fd >= NR_OPEN)
		return -EINVAL;
	f = fileSystem->filp[fd] = new file;
	if ((i = open_file(filename.c_str(),flag, mode, &inode)) < 0) {
		fileSystem->filp[fd] = NULL;
		delete f;
		return i;
	}
	f->f_mode = mode;
	f->f_flags = flag;
	f->f_count = 1;
	f->f_inode = inode;
	f->f_pos = 0;
	return (fd);
}
int sys_close(unsigned int fd)
{
	struct file * filp;

	if (fd >= NR_OPEN)
		return -EINVAL;
	if (!(filp = fileSystem->filp[fd]))
		return -EINVAL;
	if (--filp->f_count)
		return (0);
	iput(filp->f_inode);
	delete filp;
	fileSystem->filp[fd] = NULL;
	return (0);
}
int sys_read(unsigned int fd, char * buf, int count) {
	struct file * file;
	struct m_inode * inode;

	if (fd >= NR_OPEN || count < 0 || !(file = fileSystem->filp[fd]))
		return -EINVAL;
	if (!count)
		return 0;
	//verify_area(buf, count);
	inode = file->f_inode;
	/*目前允许读目录和普通文件*/
	if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
		if (count + file->f_pos > inode->i_size)
			count = inode->i_size - file->f_pos;
		if (count <= 0)
			return 0;
		return file_read(inode, file, buf, count);
	}
	printf("(Read)inode->i_mode=%06o\n\r", inode->i_mode);
	return -EINVAL;
}
int sys_write(unsigned int fd, char * buf, int count)
{
	struct file * file;
	struct m_inode * inode;

	if (fd >= NR_OPEN || count < 0 || !(file = fileSystem->filp[fd]))
		return -EINVAL;
	if (!count)
		return 0;
	inode = file->f_inode;
	/*只允许写普通文件*/
	if (S_ISREG(inode->i_mode))
		return file_write(inode, file, buf, count);
	printf("(Write)inode->i_mode=%06o\n\r", inode->i_mode);
	return -EINVAL;
}
/*移动文件指针*/
int sys_lseek(unsigned int fd, off_t offset, int origin)
{
	struct file * file;
	int tmp;
	if (fd >= NR_OPEN || !(file = fileSystem->filp[fd]) || !(file->f_inode))
		return -EBADF;
	switch (origin) {
	case 0:
		if (offset < 0) return -EINVAL;
		file->f_pos = offset;
		break;
	case 1:
		if (file->f_pos + offset < 0) return -EINVAL;
		file->f_pos += offset;
		break;
	case 2:
		if ((tmp = file->f_inode->i_size + offset) < 0)
			return -EINVAL;
		file->f_pos = tmp;
		break;
	default:
		return -EINVAL;
	}
	return file->f_pos;
}
int sys_get_work_dir(struct m_inode* inode, string & out)
{
	int inum = inode->i_num;
	if (inum == fileSystem->root->i_num)
	{
		out = "/";
		return 1;
	}
	out = "";
	char name[20];
	struct m_inode* fa;
	inode->i_count++;//之后会iput一次

	while (inode->i_num != fileSystem->root->i_num)
	{
		if (!get_name(inode, name, 20))
		{
			return NULL;
		}

		string s(&name[0], &name[strlen(name)]);
		s.insert(0, "/");
		out.insert(0, s);
		if (!(fa = get_father(inode)))
		{
			return NULL;
		};

		iput(inode);
		inode = fa;
	}
	return 1;
}

//ls命令 显示当前目录下所有文件
int cmd_ls() 
{
	int entries;
	int block, i;
	string out="";
	struct buffer_head * bh;
	struct dir_entry * de;
	struct m_inode ** dir = &fileSystem->current;
	entries = (*dir)->i_size / (sizeof(struct dir_entry));
	
	block = (*dir)->i_zone[0];
	if (block <= 0) {
		return -EPERM;
	}
	int count = 0;
	bh = bread(block);
	i = 0;
	de = (struct dir_entry *) bh->b_data;
	while (i < entries) {
		/*一个目录块读取完毕,则换下一个目录寻找*/
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
			else
				de = (struct dir_entry *) bh->b_data;
		}
		/*de->inode =0 代表该目录是空的*/
		if (strcmp(de->name,"..")&& strcmp(de->name, ".")&& de->inode!=0)
		{
			m_inode* inode = iget(0, de->inode);
			if (inode)
			{
				if (S_ISDIR(inode->i_mode))
					pdirc(de->name);
				else
					pfilec(de->name);
				printf("  ");
			}
			count++;
			iput(inode);
		}
		de++;
		i++;
	}
	brelse(bh);
	if (count <= 0){
		pinfoc("该目录为空");
	}
	cout << endl;
}

/*stat命令，显示文件详细信息*/
int cmd_stat(string path)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, *inode;
	struct buffer_head * bh, *dir_block;
	struct dir_entry * de;
	if (!(dir = dir_namei(path.c_str(), &namelen, &basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	bh = find_entry(&dir, basename, namelen, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}
	if (!(inode = iget(dir->i_dev, de->inode))) {
		iput(dir);
		brelse(bh);
		return -EPERM;
	}

	cout << "name: " <<string(basename)<< endl;
	cout <<"dev: "<<inode->i_dev << endl;
	cout << "mode: " << GetFileMode(inode->i_mode) << endl;
	cout << "nlinks: " << to_string(inode->i_nlinks) << endl;
	cout << "num: " << inode->i_num << endl;
	cout << "firstzone: " << inode->i_zone[0] << endl;
	cout << "size: " << GetFileSize(inode->i_size) << endl;
	//cout << "最后访问时间: " << longtoTime(inode->i_atime) << endl;
	cout << "最后修改时间: " << longtoTime(inode->i_mtime) << endl;
	//cout << "i节点自身最终被修改时间: " << longtoTime(inode->i_ctime) << endl;
	return 0;
}

/*cd命令，移动工作目录*/
int cmd_cd(string path)
{
	struct m_inode *dir=NULL;
	if (dir = get_inode(path.c_str()))
	{
		iput(fileSystem->current);
		fileSystem->current = dir;
		sys_get_work_dir(dir, fileSystem->name);
	}
	else
	{
		return -ENOENT;
	}
	return 0;
}

/*pwd命令，输出当前路径名*/
int cmd_pwd()
{
	struct m_inode *dir = NULL;
	string pwd = "";
	if (sys_get_work_dir(fileSystem->current, pwd))
	{
		ppathc(pwd);
		return 0;
	}
	return -EPERM;
}

/*cat命令，输出指定文件的所有内容*/
int cmd_cat(string path) {
	int fd, size,i;
	struct m_inode* inode;
	if (!(inode = get_inode(path.c_str())))
		return -ENOENT;
	if (inode->i_mode == S_IFDIR)
		return cmd_stat(path);

	fd=sys_open(path, O_RDONLY, S_IFREG);
	if (fd < 0) {
		iput(inode);
		return fd;
	}
	size = inode->i_size;
	char* buf = new char[size+2];
	if (i = sys_read(fd, buf, size) < 0)
	{
		sys_close(fd);
		iput(inode);
		return i;
	}
	if (size == 0)
	{
		pinfoc("文件为空");
	}
	else if (S_ISREG(inode->i_mode))
	{
		pinfoc("文件大小："+ GetFileSize(size));
		printf("%s\n", buf);
	}else//其他文件类型以二进制流输出
	{
		pinfoc("文件大小：" + GetFileSize(size));
		for(i=0;i<size;++i)
		{
			printf("%x", buf[i]);
		}
		printf("\n");
	}
	sys_close(fd);
	iput(inode);
	delete buf;
	return 0;
}

/*vi指令，可以在文件末尾增添内容*/
int cmd_vi(string path)
{
	int fd, size, i;
	string in="";
	fd=sys_open(path, O_APPEND, S_IFREG);


	if (fd < 0) {
		return fd;
	}
	printfc(FG_YELLOW, "请输入内容：");
	cin >> in;
	getchar();
	i = sys_write(fd, (char*)in.c_str(), in.length());
	if (i < 0) {
			sys_close(fd);
			return i;
		}
	psucc("\n添加成功");
	sys_close(fd);
	return 0;
}

/*mkdir命令，创建一个新的文件夹（目录）*/
int cmd_mkdir(const char * pathname, int mode)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, *inode;
	struct buffer_head * bh, *dir_block;
	struct dir_entry * de;
	//dir是找到的父目录
	if (!(dir = dir_namei(pathname, &namelen, &basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	//判断要增加的目录是否已经存在
	bh = find_entry(&dir, basename, namelen, &de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}

	//开始创建，首先创建子目录的inode节点
	inode = new_inode(dir->i_dev);

	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}
	inode->i_size = 32;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_atime = CurrentTime();

	//为子目录创建第一个数据块，它将包含两个子目录项 . 和 ..
	if (!(inode->i_zone[0] = new_block(inode->i_dev))) {
		iput(dir);
		inode->i_nlinks--;
		iput(inode);
		return -ENOSPC;
	}
	inode->i_dirt = 1;
	if (!(dir_block = bread(inode->i_zone[0]))) {
		iput(dir);
		free_block(inode->i_dev, inode->i_zone[0]);
		inode->i_nlinks--;
		iput(inode);
		return -1;
	}
	de = (struct dir_entry *) dir_block->b_data;
	/*加入. 和 .. 两个子目录*/
	de->inode = inode->i_num;
	strcpy(de->name, ".");
	de++;
	de->inode = dir->i_num;
	strcpy(de->name, "..");

	/*由于一个目录节点新建时，有父目录指向它，加上 . 目录项指向自己，故i_nlinks=2*/
	inode->i_nlinks = 2;
	dir_block->b_dirt = 1;
	brelse(dir_block);
	inode->i_mode = mode;
	inode->i_dirt = 1;
	//然后将子目录插入到父目录中
	bh = add_entry(dir, basename, namelen, &de);

	if (!bh) { //增加子目录失败，释放所有资源
		iput(dir);
		free_block(inode->i_dev, inode->i_zone[0]);
		inode->i_nlinks = 0;
		iput(inode);
		return -ENOSPC;
	}
	//cout << "add_entry successful" << endl;
	//终于创建成功，设定初始值
	de->inode = inode->i_num;
	bh->b_dirt = 1;
	dir->i_nlinks++;
	dir->i_dirt = 1;
	iput(dir);
	iput(inode);
	brelse(bh);
	//psucc("文件夹成功创建");
	return 0;
}

/*touch命令,创建一个普通的文件节点*/
int cmd_touch(const char * filename, int mode)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, *inode;
	struct buffer_head * bh;
	struct dir_entry * de;

	if (!(dir = dir_namei(filename, &namelen, &basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	bh = find_entry(&dir, basename, namelen, &de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}
	inode = new_inode(dir->i_dev);
	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}
	inode->i_mode = mode;
	inode->i_mtime = inode->i_atime = CurrentTime();
	inode->i_dirt = 1;

	bh = add_entry(dir, basename, namelen, &de);
	if (!bh) {
		iput(dir);
		inode->i_nlinks = 0;
		iput(inode);
		return -ENOSPC;
	}
	de->inode = inode->i_num;
	bh->b_dirt = 1;
	iput(dir);
	iput(inode);
	brelse(bh);
	psucc("文件创建成功");
	return 0;
}

/*rmdir命令，删除一个空的文件夹（目录）*/
int cmd_rmdir(const char * name)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, *inode;
	struct buffer_head * bh;
	struct dir_entry * de;

	if (!(dir = dir_namei(name, &namelen, &basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	bh = find_entry(&dir, basename, namelen, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}
	if (!(inode = iget(dir->i_dev, de->inode))) {
		iput(dir);
		brelse(bh);
		return -EPERM;
	}
	/*如果该文件正有其他进程正在使用*/
	if (inode->i_count > 1) {
		iput(dir);
		iput(inode);
		brelse(bh);
		return -EPERM;
	}

	if (inode == dir) {	/* we may not delete ".", but "../dir" is ok */
		iput(inode);
		iput(dir);
		brelse(bh);
		return -EPERM;
	}
	/*删除的不是一个目录，而是其他类型文件*/
	if (!S_ISDIR(inode->i_mode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -ENOTDIR;
	}
	if (!empty_dir(inode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -ENOTEMPTY;
	}
	if (inode->i_nlinks != 2)
		printf("empty directory has nlink!=2 (%d)\n", inode->i_nlinks);
	/*删除目录索引*/
	de->inode = 0;
	bh->b_dirt = 1;
	brelse(bh);
	/*删除目录*/
	inode->i_nlinks = 0;
	inode->i_dirt = 1;
	/*修改父目录信息，由于子目录的..项被删除，所以父目录的i_nliks--*/
	dir->i_nlinks--;
	dir->i_ctime = dir->i_mtime = CurrentTime();
	dir->i_dirt = 1;
	iput(dir);
	iput(inode);
	psucc("文件夹删除成功");
	return 0;
}

/*rm命令，删除操作，强制删除普通文件*/
int cmd_rm(const char * name)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, *inode;
	struct buffer_head * bh;
	struct dir_entry * de;

	if (!(dir = dir_namei(name, &namelen, &basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	bh = find_entry(&dir, basename, namelen, &de);
	if (!bh) {
		iput(dir);
		return -ENOENT;
	}
	if (!(inode = iget(dir->i_dev, de->inode))) {
		iput(dir);
		brelse(bh);
		return -ENOENT;
	}
	if (S_ISDIR(inode->i_mode)) {
		iput(inode);
		iput(dir);
		brelse(bh);
		return -EISDIR;
	}
	/*如果文件的引用数已经为0，说明出现程序bug，并修正文件i_nlinks 为1*/
	if (!inode->i_nlinks) {
		printf("！！！BUG Deleting nonexistent file (%04x:%d), %d\n",
			inode->i_dev, inode->i_num, inode->i_nlinks);
		inode->i_nlinks = 1;
	}
	de->inode = 0;
	bh->b_dirt = 1;
	brelse(bh);
	inode->i_nlinks--;
	inode->i_dirt = 1;
	inode->i_ctime = CurrentTime();
	iput(inode);
	iput(dir);
	psucc("文件删除成功");
	return 0;
}
/*sync命令，保持目前的所有修改信息*/
int cmd_sync()
{
	file* f;
	for (int fd = 0; fd < NR_OPEN; ++fd)
	{
		if (f = fileSystem->filp[fd])
		{
			iput(f->f_inode);
			delete f;
		}
	}
	realse_inode_table();
	realse_all_blocks();
	psucc("保存成功");
	return 0;
}
/*exit命令，退出文件系统，将所有信息写回磁盘*/
int cmd_exit()
{
	iput(fileSystem->current);
	iput(fileSystem->root);
	cmd_sync();
	printfc(FG_YELLOW, string("系统时间为: ") + longtoTime(CurrentTime()));
	return 0;
}

void myhint(int errorCode) {
	if (errorCode == 0){
		//cout << "操作成功" << endl;
	}
	else if(errorCode== -ENOENT){
		perrorc("路径不正确，找不到指定的路径");
	}
	else if (errorCode == -ENOTDIR) {
		perrorc( "路径指向的不是目录文件" );
	}
	else if (errorCode == -ENOTEMPTY) {
		perrorc("文件夹非空");
	}
	else if (errorCode == -EPERM) {
		perrorc("系统内部问题");
	}
	else if (errorCode == -EEXIST) {
		perrorc("文件已经存在") ;
	}
	else if (errorCode == -EACCES) {
		perrorc("权限不足"); 
	}
	else if (errorCode == -EINVAL) {
		perrorc("不正确的参数");
	}
	else if (errorCode == -ENOSPC) {
		perrorc("无法申请到资源，空间不足");
	}
	else if (errorCode == -EISDIR) {
		perrorc("路径指向为目录文件");
	}
	else {
		perrorc("未知错误");
	}
}