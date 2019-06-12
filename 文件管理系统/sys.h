#pragma once
#include<iostream>
#include<string>
int sys_open(std::string filename, int flag, int mode);
int sys_close(unsigned int fd);
int sys_read(unsigned int fd, char * buf, int count);
int sys_write(unsigned int fd, char * buf, int count);
int sys_lseek(unsigned int fd, off_t offset, int origin);
int sys_get_work_dir(struct m_inode* inode, std::string & out);

int cmd_ls();
int cmd_stat(std::string path);
int cmd_cd(std::string s);
int cmd_pwd();
int cmd_cat(std::string s);
int cmd_vi(std::string path);
int cmd_mkdir(const char * pathname, int mode);
int cmd_touch(const char * filename, int mode);
int cmd_rmdir(const char * name);
int cmd_rm(const char * name);
int cmd_sync();
int cmd_exit();

void myhint(int code);