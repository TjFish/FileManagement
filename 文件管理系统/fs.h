#pragma once

#include<iostream>
#include<string>

#define NAME_LEN 14
#define BLOCK_SIZE 1024
#define BLOCK_BIT (BLOCK_SIZE*8)
#define ROOT_INO 1
#define ROOT_DEV 0
/*一个block中inode个数---32*/
#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode)))
#define SUPER_MAGIC 0x137F
#define NR_OPEN 20
#define NR_INODE 32
#define NR_SUPER 8

/*文件读写权限*/
#define O_RDONLY 1
#define O_WRONLY 2
#define O_RDWR 3
#define O_APPEND 4 //append模式即可读又可写，写只能在文件末尾添加

/*一块中目录项的个数---64*/
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry)))
#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8

/*定义文件类型*/
#define S_IFMT  00170000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)


#define reverse_bit(x,y)  x^=(1<<y)

//磁盘块
typedef char buffer_block[BLOCK_SIZE];

struct buffer_head {
	char * b_data;			/* pointer to data block (1024 bytes) */
	unsigned long b_blocknr;	/* block number */
	unsigned short b_dev;		/* device (0 = free) */
	unsigned char b_uptodate;
	unsigned char b_dirt;		/* 0-clean,1-dirty */
	unsigned char b_count;		/* users using this block */
	unsigned char b_lock;		/* 0 - ok, 1 -locked */
	struct task_struct * b_wait;
	struct buffer_head * b_prev;
	struct buffer_head * b_next;
	struct buffer_head * b_prev_free;
	struct buffer_head * b_next_free;
};
//超级块
struct d_super_block {
	unsigned short s_ninodes; /*i节点数*/
	unsigned short s_nzones;  /*数据块数*/
	unsigned short s_imap_blocks; /*i节点位图的数据块个数*/
	unsigned short s_zmap_blocks; /*逻辑块位图的数据块个数*/
	unsigned short s_firstdatazone;/*第一个数据块的编号*/
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;/*文件类型*/
};
//内存中超级块
struct super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
	/* These are only in memory */
	struct buffer_head * s_imap[8];/*i节点位图数组*/
	struct buffer_head * s_zmap[8];/*逻辑节点位图数组*/
	unsigned short s_dev;
	struct m_inode * s_isup;
	struct m_inode * s_imount;
	unsigned long s_time;
	struct task_struct * s_wait;
	unsigned char s_lock;
	unsigned char s_rd_only;
	unsigned char s_dirt;
};
//目录项
struct dir_entry {
	unsigned short inode;
	char name[NAME_LEN];
};
//磁盘中inode节点
struct d_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_time;
	unsigned char i_gid;
	/*有多少个文件目录项指向该节点*/
	unsigned char i_nlinks;
	unsigned short i_zone[9];
};
//内存中inode节点
struct m_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
	/* these are in memory also */
	struct task_struct * i_wait;
	unsigned long i_atime;
	unsigned long i_ctime;
	unsigned short i_dev;
	unsigned short i_num;
	unsigned short i_count;
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_mount;
	unsigned char i_seek;
	unsigned char i_update;
};

struct file {
	unsigned short f_mode;
	unsigned short f_flags;
	unsigned short f_count;
	struct m_inode * f_inode;
	off_t f_pos;
};
struct FileManageMent
{
	struct file* filp[NR_OPEN];
	m_inode* root;
	m_inode* current;
	std::string name;
};
extern FileManageMent* fileSystem;

extern buffer_head* bread(int block);
extern int brelse(buffer_head* bh);
extern struct super_block * get_super(int dev);
extern struct m_inode *iget(int dev, int nr);
extern void mount_root();
extern int bmap(struct m_inode * inode, int block);
struct m_inode * get_inode(const char * pathname);
//struct m_inode * get_dir(const char * pathname);
void free_block(int dev, int block);
void free_inode(struct m_inode * inode);
struct m_inode * new_inode(int dev);
int new_block(int dev);
void truncate(struct m_inode * inode);
void iput(struct m_inode * inode);
struct m_inode * dir_namei(const char * pathname,int * namelen, const char ** name);
struct buffer_head * find_entry(struct m_inode ** dir,const char * name, int namelen, struct dir_entry ** res_dir);
struct buffer_head * add_entry(struct m_inode * dir,
	const char * name, int namelen, struct dir_entry ** res_dir);
int create_block(struct m_inode * inode, int block);
struct m_inode * get_empty_inode();
int empty_dir(struct m_inode * inode);
int get_name(struct m_inode * inode, char *buf,int size);
struct m_inode *get_father(struct m_inode * inode);
void init_inode_table();
void realse_inode_table();
void realse_all_blocks();
/*位图操作函数*/
int find_first_zero(char* data);
int get_bit(int k, char* data);
int clear_bit(int k, char* data);
int set_bit(int k, char* data);

unsigned long CurrentTime();
char* longtoTime(long l);
int open_file(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode);
int file_read(struct m_inode * inode, struct file * filp, char * buf, int count);
int file_write(struct m_inode * inode, struct file * filp, char * buf, int count);