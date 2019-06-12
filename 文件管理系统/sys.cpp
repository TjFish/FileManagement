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
		return "Ŀ¼�ļ�";
	if (S_ISREG(mode))
		return "��ͨ�ļ�";
	return"δ֪�ļ�����";
}

/*�ļ������֣���ͨ�ļ���Ŀ¼�ļ�
����Ŀ¼�ļ����û�ʹ��ʱ��Ҫ����
ʹ��һ��Ŀ¼ָ�룬��Ŀ¼ָ����Եõ�ÿһ��Ŀ¼������
������ͨ�ļ����û�ʹ��ʱ��Ҫ����
ʹ��һ���ļ�ָ�룬���ļ�ָ����Եõ��ļ�ָ��������
���Եõ��ļ������ļ���С����Ϣ
*/
/*�ļ�������ϵͳ���ã�����open��close��read��write��lseek*/
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
	/*Ŀǰ�����Ŀ¼����ͨ�ļ�*/
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
	/*ֻ����д��ͨ�ļ�*/
	if (S_ISREG(inode->i_mode))
		return file_write(inode, file, buf, count);
	printf("(Write)inode->i_mode=%06o\n\r", inode->i_mode);
	return -EINVAL;
}
/*�ƶ��ļ�ָ��*/
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
	inode->i_count++;//֮���iputһ��

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

//ls���� ��ʾ��ǰĿ¼�������ļ�
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
		/*һ��Ŀ¼���ȡ���,����һ��Ŀ¼Ѱ��*/
		if ((char *)de >= BLOCK_SIZE + bh->b_data) {
			brelse(bh);
			block = bmap(*dir, i / DIR_ENTRIES_PER_BLOCK);
			bh = bread(block);
			if (bh == NULL || block <= 0)
			{
				/*�����һ��Ŀ¼���ȡʧ�ܣ���������Ŀ¼*/
				i += DIR_ENTRIES_PER_BLOCK;
				continue;
			}
			else
				de = (struct dir_entry *) bh->b_data;
		}
		/*de->inode =0 �����Ŀ¼�ǿյ�*/
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
		pinfoc("��Ŀ¼Ϊ��");
	}
	cout << endl;
}

/*stat�����ʾ�ļ���ϸ��Ϣ*/
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
	//cout << "������ʱ��: " << longtoTime(inode->i_atime) << endl;
	cout << "����޸�ʱ��: " << longtoTime(inode->i_mtime) << endl;
	//cout << "i�ڵ��������ձ��޸�ʱ��: " << longtoTime(inode->i_ctime) << endl;
	return 0;
}

/*cd����ƶ�����Ŀ¼*/
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

/*pwd��������ǰ·����*/
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

/*cat������ָ���ļ�����������*/
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
		pinfoc("�ļ�Ϊ��");
	}
	else if (S_ISREG(inode->i_mode))
	{
		pinfoc("�ļ���С��"+ GetFileSize(size));
		printf("%s\n", buf);
	}else//�����ļ������Զ����������
	{
		pinfoc("�ļ���С��" + GetFileSize(size));
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

/*viָ��������ļ�ĩβ��������*/
int cmd_vi(string path)
{
	int fd, size, i;
	string in="";
	fd=sys_open(path, O_APPEND, S_IFREG);


	if (fd < 0) {
		return fd;
	}
	printfc(FG_YELLOW, "���������ݣ�");
	cin >> in;
	getchar();
	i = sys_write(fd, (char*)in.c_str(), in.length());
	if (i < 0) {
			sys_close(fd);
			return i;
		}
	psucc("\n��ӳɹ�");
	sys_close(fd);
	return 0;
}

/*mkdir�������һ���µ��ļ��У�Ŀ¼��*/
int cmd_mkdir(const char * pathname, int mode)
{
	const char * basename;
	int namelen;
	struct m_inode * dir, *inode;
	struct buffer_head * bh, *dir_block;
	struct dir_entry * de;
	//dir���ҵ��ĸ�Ŀ¼
	if (!(dir = dir_namei(pathname, &namelen, &basename)))
		return -ENOENT;
	if (!namelen) {
		iput(dir);
		return -ENOENT;
	}
	//�ж�Ҫ���ӵ�Ŀ¼�Ƿ��Ѿ�����
	bh = find_entry(&dir, basename, namelen, &de);
	if (bh) {
		brelse(bh);
		iput(dir);
		return -EEXIST;
	}

	//��ʼ���������ȴ�����Ŀ¼��inode�ڵ�
	inode = new_inode(dir->i_dev);

	if (!inode) {
		iput(dir);
		return -ENOSPC;
	}
	inode->i_size = 32;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_atime = CurrentTime();

	//Ϊ��Ŀ¼������һ�����ݿ飬��������������Ŀ¼�� . �� ..
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
	/*����. �� .. ������Ŀ¼*/
	de->inode = inode->i_num;
	strcpy(de->name, ".");
	de++;
	de->inode = dir->i_num;
	strcpy(de->name, "..");

	/*����һ��Ŀ¼�ڵ��½�ʱ���и�Ŀ¼ָ���������� . Ŀ¼��ָ���Լ�����i_nlinks=2*/
	inode->i_nlinks = 2;
	dir_block->b_dirt = 1;
	brelse(dir_block);
	inode->i_mode = mode;
	inode->i_dirt = 1;
	//Ȼ����Ŀ¼���뵽��Ŀ¼��
	bh = add_entry(dir, basename, namelen, &de);

	if (!bh) { //������Ŀ¼ʧ�ܣ��ͷ�������Դ
		iput(dir);
		free_block(inode->i_dev, inode->i_zone[0]);
		inode->i_nlinks = 0;
		iput(inode);
		return -ENOSPC;
	}
	//cout << "add_entry successful" << endl;
	//���ڴ����ɹ����趨��ʼֵ
	de->inode = inode->i_num;
	bh->b_dirt = 1;
	dir->i_nlinks++;
	dir->i_dirt = 1;
	iput(dir);
	iput(inode);
	brelse(bh);
	//psucc("�ļ��гɹ�����");
	return 0;
}

/*touch����,����һ����ͨ���ļ��ڵ�*/
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
	psucc("�ļ������ɹ�");
	return 0;
}

/*rmdir���ɾ��һ���յ��ļ��У�Ŀ¼��*/
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
	/*������ļ�����������������ʹ��*/
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
	/*ɾ���Ĳ���һ��Ŀ¼���������������ļ�*/
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
	/*ɾ��Ŀ¼����*/
	de->inode = 0;
	bh->b_dirt = 1;
	brelse(bh);
	/*ɾ��Ŀ¼*/
	inode->i_nlinks = 0;
	inode->i_dirt = 1;
	/*�޸ĸ�Ŀ¼��Ϣ��������Ŀ¼��..�ɾ�������Ը�Ŀ¼��i_nliks--*/
	dir->i_nlinks--;
	dir->i_ctime = dir->i_mtime = CurrentTime();
	dir->i_dirt = 1;
	iput(dir);
	iput(inode);
	psucc("�ļ���ɾ���ɹ�");
	return 0;
}

/*rm���ɾ��������ǿ��ɾ����ͨ�ļ�*/
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
	/*����ļ����������Ѿ�Ϊ0��˵�����ֳ���bug���������ļ�i_nlinks Ϊ1*/
	if (!inode->i_nlinks) {
		printf("������BUG Deleting nonexistent file (%04x:%d), %d\n",
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
	psucc("�ļ�ɾ���ɹ�");
	return 0;
}
/*sync�������Ŀǰ�������޸���Ϣ*/
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
	psucc("����ɹ�");
	return 0;
}
/*exit����˳��ļ�ϵͳ����������Ϣд�ش���*/
int cmd_exit()
{
	iput(fileSystem->current);
	iput(fileSystem->root);
	cmd_sync();
	printfc(FG_YELLOW, string("ϵͳʱ��Ϊ: ") + longtoTime(CurrentTime()));
	return 0;
}

void myhint(int errorCode) {
	if (errorCode == 0){
		//cout << "�����ɹ�" << endl;
	}
	else if(errorCode== -ENOENT){
		perrorc("·������ȷ���Ҳ���ָ����·��");
	}
	else if (errorCode == -ENOTDIR) {
		perrorc( "·��ָ��Ĳ���Ŀ¼�ļ�" );
	}
	else if (errorCode == -ENOTEMPTY) {
		perrorc("�ļ��зǿ�");
	}
	else if (errorCode == -EPERM) {
		perrorc("ϵͳ�ڲ�����");
	}
	else if (errorCode == -EEXIST) {
		perrorc("�ļ��Ѿ�����") ;
	}
	else if (errorCode == -EACCES) {
		perrorc("Ȩ�޲���"); 
	}
	else if (errorCode == -EINVAL) {
		perrorc("����ȷ�Ĳ���");
	}
	else if (errorCode == -ENOSPC) {
		perrorc("�޷����뵽��Դ���ռ䲻��");
	}
	else if (errorCode == -EISDIR) {
		perrorc("·��ָ��ΪĿ¼�ļ�");
	}
	else {
		perrorc("δ֪����");
	}
}