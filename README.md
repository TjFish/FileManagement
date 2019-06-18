# FileManagement
# 文件管理系统实现

操作系统 课程作业 3

#### Author：欧阳桥梁 1753112

## 项目背景

1. 在内存中开辟一个空间作为文件存储器（虚拟磁盘），在其上实现一个简单的文件管理系统
2. 退出这个文件系统时，应将文件系统的内容保存在磁盘上，下次将其恢复到内存中
3. 文件目录采用多级目录结构

## 项目概述

本项目是基于MINIX1.0 的文件管理系统。主要借鉴了linux0.11的源码，参考赵炯编编著的《linux内核完全注释》。去掉了linux0.11中的多进程，内存管理等模块，目标是能够真正的操作磁盘文件。
基于c++的控制台，实现了linux的基本文件操作命令，包括ls，cd，mkdir，rm，vi等等命令，同时提供基本文件接口，包括sys_open,sys_close,sys_read,sys_write,sys_lseek等系统调用。
**/文件管理系统/hdc0.11.img** 是一个MINIX文件格式的磁盘文件，可以装载在ubuntu下，下面其在ubuntu装载的显示根目录结构。本项目所有的演示操作都是对hdc0.11.img进行的。
（hdc0.11.img存储了linux0.11的所有源码以及相关开发工具）

![image](./image/ubuntu.png)


## 开发/运行环境

- 开发环境：Windows10 Pro 1803
- 开发语言：C++/C
- 开发工具：Microsoft Visual Studio Community 2017

## MINIX1.0文件系统

linux0.11采用的是MINIX 1.0文件系统。MINIX 1.0 文件系统与标准 UNIX 的文件系统基本相同。它由6个部分组成。对于一个360K的盘，其各部分的分布如下图所示。
( 图片来自《linux内核完全注释》)

![image](./image/总体结构.png)

图中，整个磁盘被划分成以1KB为单位的磁盘块，因此上图中共有360个磁盘块，每个方格表示一个磁盘块。在MIMX1.0文件系统中，其磁盘块大小与逻辑块大小正好是一样的，也是1KB字节。
因此360KB盘片也含有360个逻辑块。

### 1. 引导块

引导块是计算机加电启动时可由ROMBIOS自动读入的执行代码和数据。但并非所有盘都用于作为引导设备，所以对于不用于引导的盘片，这一盘块中可以不含代码。
但任何盘片必须含有引导块空间，以保持MIMX文件系统格式的统一。即文件系统只是在块设备上空出一个存放引导块的空间。如果你把内核映像文件放在文件系统中，
那么你就可以在文件系统所在设备的第1个块（即引导块空间）存放实际的引导程序，并由它来取得和加载文件系统中的内核映像文件（hdc-0.11.img的引导块存放的就是linux0.11的内核代码）。
由于本项目为文件系统，故并不关心引导块。

### 2. 超级块

用于描述文件系统的整体信息，并说明各部分的大小。在系统初始化时读入磁盘的超级块信息，获取磁盘的各项基本信息
包括：

1. 总/空闲inode数量
2. 总/空闲逻辑块数量
3. 第一个逻辑块位置
具体参见下图

![image](./image/超级块结构.png)


### 3. i节点位图

记录inode表的使用情况，每一位记录一个inode是否被使用，0表示未使用，1表示已经使用。


### 4. 逻辑块位图表

与i节点位图类似，用来记录块组中所有的逻辑块的使用情况。
从超级块的结构中我们还可以看出，逻辑块位图最多使用8块缓冲块（s_zmap[8]),而每块缓冲块大小是1024字节，每比特表示一个盘块的占用状态，因此一个缓冲块可代表8192个盘块。
8个缓冲块总共可表示65536个盘块，因此MIMX文件系统1.0所能支持的最大块设备容量（长度）是64MB。


### 5. inode表

i节点表存放着文件系统中文件或目录名的索引节点，每个文件或目录名都有一个i节点。 每个i节点结构中存放着对应文件的相关信息，如文件长度、访问修改时间以及文件数据块在盘上的位置等。

1. **inode**

   整个inode结构共使用32个字节，如下图所示。

   ![image](./image/inode节点结构.png)

   i_mode 属性表示文件的类型，该项目中只考虑REG（普通文件）和DIR（目录文件）两种类型文件。
   i_nlinks 属性表示有多少目录指向改inode节点，当nlinks为0时，该节点就应该被删除。
   Minix1.0 文件存储采用二级目录，i_zone[9]数组存放文件数据，指向文件的数据所在的盘块号。
   下面是具体使用i_zone数据的示意图

   ![image](./image/目录结构.png)

   文件中的数据是放在磁盘块的数据区中的，而一个文件名则通过对应的i节点与这些数据磁盘块相联系，这些盘块的号码就存放在i节点的逻辑块数组i_zone中。
   其中，i_zone[]数组用于存放i节点对应 文件的盘块号。i_zone[0]到i_zone[6]用于存放文件开始的7个磁盘块号，称为直接块。
   若文件长度小于 等于7K字节，则根据其i节点可以很快就找到它所使用的盘块。若文件大一些时，就需要用到一次间接 块了（i_zone[7]),这个盘块中存放着附加的盘块号。
   对于MINIX文件系统它可以存放512个盘块号， 因此可以寻址512个盘块。若文件还要大，则需要使用二次间接盘块（i_zone[8])。
   二次间接块的一级盘 块的作用类似与一次间接盘块，因此使用二次间接盘块可以寻址512*512个盘块。

### 6. 逻辑块表

记录块组中所有的逻辑块，数量由超级块给出

1. **buffer_head**
    buffer_head表示磁盘的数据块，b_blocknr记录数据块的盘块号，b_data指向连续1024字节的数据块。
    b_dirt 为脏标志，表示该磁盘块是否被修改，b_count则表示有多少进程使用该block(虽然本项目为单进程，不过仍然实现了该功能)
    linux0.11 实现时采用了高速缓冲区，本项目进行了适当的简化，故buffer_head 后半部分的数据并未使用。
    具体结构如下

    ```c
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
    ```


## 文件类型、属性、目录项

### 1. 文件类型和属性

linux0.11 中实现的文件类型较多而且比较复杂，本项目做了简化，目前仅支持REG普通文件和DIR目录文件。
文件属性指向的是文件的使用方式，包括只读，只写，可读可写，追加，截断等属性。

1. **file**
    下面是file类的具体结构

    ```c
    struct file {
	unsigned short f_mode;
	unsigned short f_flags;
	unsigned short f_count;
	struct m_inode * f_inode;
	off_t f_pos;
    };
    ```

    其中f_mode表示文件类型，f_flags 表示文件属性，f_count表示有文件的打开次数
    f_pos 表示文件指针，指向文件现在的读写位置。用户(程序员）可以通过sys_lseak系统调用来修改文件指针的位置。


### 2. 目录项

对于目录文件，其数据块中存储的是一个个目录项，每个目录项存储了子文件的名字和i节点号。
每个目录项只包括一个长度为14字节的文件名字符串和2字节的i节点号，因此一个逻辑磁盘块可以存放1024/16=64个目录项。
有关文件的其它信息则被保存在该i节点号指定的i节点结构中，每个i节点号的i节点都位于磁盘上的固定位置处。

    
1. **dir_entry**

    dir_entry为文件名目录项，具体结构如下

    ```c
    struct dir_entry {
	    unsigned short inode;  //i节点号
	    char name[NAME_LEN];  //文件名，14字节
    };
    ```

    在打开一个文件时，文件系统会根据给定的文件名找到其i节点号，从而通过其对应i节点信息找到文件所在的磁盘块位置，如下图所示。
    例如对于要查找文件名/usr/bin的i节点号，文件系统首先会从具有固定i节点号（1）的根目录开始操作，即从i节点号1的数据块中查找到名称为usr的目录项，从而得到文件/usr的i节点号。
    根据该i节点号文件系统可以顺利地取得目录/usr，并在其中可以查找到文件名bin的目录项。这样也就知道了/usr/bin的i节点号，从而可以从磁盘上得到该i节点号的i节点结构信息。
    下图是目录查询的示意图

    ![image](./image/目录查询.png)


### 3. 文件管理

由于本系统目前是单进程，故使用结构体FileManageMent保存文件系统的基本信息
用于记录用户当前所处的目录的inode索引,根目录索引，打开的文件等信息。
1. **FileManageMent**
    下面是FileManageMent的具体结构

    ```c
    struct FileManageMent
    {
	    struct file* filp[NR_OPEN];
	    m_inode* root;
	    m_inode* current;
	    std::string name;
    };
    extern FileManageMent* fileSystem;
    ```

    其中root为根目录i节点号，这在系统初始化读取超级块是设定，current 为当前路径的i节点号，随着用户调用cd命令而改变，初始值为根目录。
    name 为当前路径名。filp为系统已经打开的文件列表，NR_OPEN为20，即系统最多同时打开20个文件




## GUI设计及使用说明

整个文件管理系统包含ls，cd, stat, cat, mkdir, touch, vi, rmdir, rm, sync, exit 十个命令。
用户可以输入命令和参数实现文件管理，在操作后需要使用sync 命令进行保存，文件系统才会将所有修改保存到磁盘上。
对于不同文件和信息，使用不同颜色输出（颜色文字代码文件为printc.h 来自github）

##GUI说明

   - 主窗口

   ![image](./image/GUI/主界面.png)

   - ls 命令

   ![image](./image/GUI/ls命令.png)

   其中，绿色标识的文件是目录文件，白色标识的是普通文件。

   - cd 命令

   ![image](./image/GUI/cd命令.png)
   
   可以看到最左边的当前路径发生了改变，ls显示的目录为usr目录下的文件

   - stat 命令

   ![image](./image/GUI/stat命令.png)

   首先是输出出了include文件夹的信息，然后输出了const.h的信息，可以注意到const.h 的最后修改时间为1991年，正是当年linus编写linux的时间

   - cat 命令

   ![image](./image/GUI/cat命令.png)

   cat命令输出了const.h 的内容，可以看到const.h 主要定义了一些基本常量，如果对目录文件使用cat，则会输出目录的基本信息（相当于stat）

   - mkdir 命令

   ![image](./image/GUI/mkdir命令.png)

   让使用cd .. 我们返回根目录，试一试mkdir创建新的文件夹，然后stat 输出新创建的文件夹信息。

   - touch 命令

   ![image](./image/GUI/touch命令.png)

   使用touch命令创建新的文件，然后stat 输出新创建的文件信息。

   - vi 命令

   ![image](./image/GUI/vi命令.png)

   向新创建的文件写一点东西，然后cat命令输出刚刚写入的数据。

   - rmdir 命令

   ![image](./image/GUI/rmdir命令.png)

   删除文件夹oy，然而由于oy文件夹非空，所以系统给出错误信息，不能删除。

   - rm 命令

   ![image](./image/GUI/rm命令.png)

   先删除test.txt，再删除文件夹oy，成功删除。

   - sync 命令

   ![image](./image/GUI/sync命令.png)

   使用sync命令保存所有修改。

   - exit 命令

   ![image](./image/GUI/exit命令.png)

   使用exit命令退出系统。

## 具体实现

### 1. 系统架构和文件组织

根据我的理解，文件管理系统的作用是：**向下管理磁盘，向上提供文件视图服务**。基于此，我设计了如下架构：

![image](./image/文件管理系统架构.png)

其中，**用户界面**为用户看到的命令行窗口，是用户直接看到的图形化界面；sys.c为**命令和系统调用层**，是文件管理系统向上对用户提供的功能，它们在用户界面中表现为一个个的命令；**上层基础复用函数**是在底层结构的基础上，建立的文件系统的上层函数，包含目录查询，文件读写等抽象功能； **MINIX 1.0底层结构**是对MINIX 1.0 文件系统的实现，包含文件管理系统向下对磁盘文件进行管理的函数；disk.c为**磁盘读取层**，包含对磁盘文件的读写操作。


### 2. 用户界面

这部分逻辑代码较为简单。首先初始化读入磁盘，根据超级块信息打印出基本磁盘信息，然后代码将进入while循环，等待用户输入命令。若用户的磁盘损坏，或者文件格式不正确，将会给出错误信息。如果磁盘映像读取成功，文件管理系统便会根据映像中的内容恢复磁盘结构和文件内容。然后代码将进入while循环，等待用户输入命令，若用户输入命令有效则调用对应的命令函数，否则给出提示信息。其中命令行彩色输出 调用的是printfc.h（该代码是github上找的，具体地址不记得了）中的函数。

### 3. 命令和系统调用（sys.c)

这部分实现了10个基本命令和文件操作的系统调用。系统调用包括基本的文件打开，关闭，读，写，文件指针移动。基本命令是通过调用系统调用和底层函数实现的，下面举系统调用 最常用的文件打开 sys_open()为例

```c
int sys_open(string filename, int flag, int mode) {
	struct m_inode * inode;
	struct file * f;
	int i, fd;
    //查询空闲文件
	for (fd = 0; fd < NR_OPEN; fd++)
	{
		if (!fileSystem->filp[fd])
			break;
	}

	if (fd >= NR_OPEN)
		return -EINVAL;//打开文件达到上限，返回错误码
    //创建新文件
	f = fileSystem->filp[fd] = new file;
    //open_file根据给出的文件信息，查询目录返回指定文件的inode节点，如果指定文件不存在，则在磁盘中创建它，然后返回inode节点，
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
    //返回文件编号
	return (fd);
}
```
sys_open首先检查该进程打开的文件数量是否超过上限，如果超过上限则返回错误码。之后内存中开辟新空间存储文件FCB，设定FCB的各项初始值后返回，其中文件的inode属性是通过调用open_file得到的。


### 4. 上层基础复用函数

这部分在底层结构的基础上，实现了一些基础复用函数，包括目录查询 find_entry()、新增目录 add_entry()、文件搜索dir_namei()、文件读写 filewrite(),fileread()、open_file,承接上文，以open_file为例

```c
int open_file(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode)
{
	const char * basename;
	int inr, dev, namelen;
	struct m_inode * dir, *inode;
	struct buffer_head * bh;
	struct dir_entry * de;

	if (!(dir = dir_namei(pathname, &namelen, &basename)))
		return -ENOENT;
	if (!namelen) {			/* special case: '/usr/' etc */
		iput(dir);
		return -EISDIR;
	}
	/*如果打开的文件不存在，则创建它*/
	bh = find_entry(&dir, basename, namelen, &de);
	if (!bh) {
		inode = new_inode(dir->i_dev);
		if (!inode) {
			iput(dir);
			return -ENOSPC;
		}
		inode->i_mode = mode;
		inode->i_dirt = 1;
		bh = add_entry(dir, basename, namelen, &de);
		if (!bh) {
			inode->i_nlinks--;
			iput(inode);
			iput(dir);
			return -ENOSPC;
		}
		de->inode = inode->i_num;
		bh->b_dirt = 1;
		brelse(bh);
		iput(dir);
		*res_inode = inode;
		return 0;
	}
	/*文件存在*/
	inr = de->inode;
	dev = dir->i_dev;
	brelse(bh);
	iput(dir);
	if (!(inode = iget(dev, inr)))
		return -EPERM;
	if(S_ISDIR(inode->i_mode)&& flag!=O_RDONLY) {
		iput(inode);
		return -EACCES;
	}
	inode->i_atime = CurrentTime();
	*res_inode = inode;
	return 0;
}
```

open_file首先查询目录，判断要打开的文件是否存在，如果不存在，则创建它。创建文件这里简略说下，首先创建文件的inode，然后将该inode加入到父级目录下，具体实现细节可以查看源码。经过上述操作，要打开的文件一定存在了，根据之前获得的i节点号调用iget()获取文件的inode，然后进行一些权限检查，比如目录文件只能以只读模式打开。最后返回文件的inode。


### 4. MINIX1.0底层结构

这部分根据MINIX文件系统的理论，实现了MINIX1.0文件系统，包括超级块读取，i节点位图，逻辑块位图管理，inode节点表管理，逻辑块管理。
举一例最常用的iget()函数

```c
struct m_inode *iget(int dev,int nr) 
{
	struct m_inode * inode;
	/*首先查看inode是否已经在内存中*/
	for (int i = 0; i < NR_INODE; ++i)
	{
		inode = inode_tabel[i];
		if (inode->i_num == nr)
		{
            //引用计数加一，然后返回
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
```

上层代码通过调用iget函数获取指定的inode节点。iget首先判断目标inode是否已经读入内存，如果读入内存，则将该inode的引用计数加一，然后返回该inode节点。否则，则从磁盘中读取inode。首先申请内存中的空闲inode，然后调用read_inode从磁盘读取。read_inode实际上是调用的bread()将inode所在磁盘块一次性读入，然后将目标inode返回。所有的inode内存空间都是由get_empty_inode创建出来的（inode的creator），并统一存储在inode_table数组中。这一切对上层代码都是透明的，上层代码只需调用iget()获取目标inode，使用完之后调用iput()释放inode，而无需关心inode的磁盘读取，空间管理。


### 5. 磁盘读写

这部分实现了读写磁盘文件，以及内存中数据块的管理。为防止内存泄漏，所有的数据块空间均由该层管理。上层代码调用读数据块函数bread(),使用之后调用brelse()释放磁盘块,无需关心底层的磁盘读写。下面以bread()为例

```c
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
	disk.close();
	bh->b_uptodate = 1;
	bh->b_dirt = 0;
	bh->b_count = 1;
	return bh;
}
```

你可能已经注意到，这部分代码与iget()十分类似，实际上他们就是原理相同。但需要注意的是，由于磁盘读写十分慢，故在内存中会保存大量的数据块。而对于大量的数据块采用顺序查找的速度是十分慢的，故blocks采用的是散列表，通过哈希函数加速block的查询。

### 6. 其它信息

所有的代码关键处都有大量的注释，读者可以参考注释阅读代码。