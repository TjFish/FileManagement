#include"fs.h"
#include"sys.h"
#include<iostream>
#include<string>
#include<sstream>
#include<Windows.h>
#include"printfc.h"

using namespace std;
void cmd();
void init();
extern FileManageMent* fileSystem = new FileManageMent();
int main()
{
	//let's go!!!
	init();
	cmd();
	system("pause");
}
void init()
{
	init_inode_table();
	mount_root();
	printfc(FG_YELLOW,string("系统时间为: ")+longtoTime(CurrentTime()));
}

void fresh_cmd() {
	printfc(FG_GREEN, "[Author:oy] ");
	printfc(FG_BLUE, fileSystem->name);
	printfc(FG_WHITE, "> ");
}
void cmd()
{
	fresh_cmd();
	string input, command, path, newPath;
	int i;
	while (getline(cin, input)) {
		istringstream is(input);
		string s;                                //deal this inputs
		for (i = 0; is >> s; i++) {
			if (s.compare("exit") == 0) {        //command is exit
				cmd_exit();
				return;
			}
		}
		if (i != 1 & i != 2) {
			perrorc("your input is Illegal" );
			fresh_cmd();
			continue;
		}
		istringstream temp(input);
		temp >> command >> path >> newPath;

		if (command.compare("ls") == 0) {    
			cmd_ls();
		}
		else if (command.compare("cd") == 0) {     //command is mkdir
			const char* pa = path.c_str();
			int code = cmd_cd(pa);
			myhint(code);
		}
		else if (command.compare("mkdir") == 0) {     //command is mkdir
			const char* pa = path.c_str();
			int code=cmd_mkdir(pa, S_IFDIR);
			myhint(code);
		}
		else if (command.compare("touch") == 0) {     //command is mkdir
			const char* pa = path.c_str();
			int code=cmd_touch(pa, S_IFREG);
			myhint(code);
		}
		else if (command.compare("cat") == 0) {//command is chdir
			const char* pa = path.c_str();
			int code = cmd_cat(pa);
			myhint(code);

		}
		else if (command.compare("rmdir") == 0) {//command is rmdir
			const char* pa = path.c_str();
			int code=cmd_rmdir(pa);
			myhint(code);
		}
		else if (command.compare("rm") == 0) {//command is opendir
			const char* pa = path.c_str();
			int code=cmd_rm(pa);
			myhint(code);
		}
		else if (command.compare("stat") == 0) {//command is readdir
			const char* pa = path.c_str();
			int code=cmd_stat(pa);
			myhint(code);
		}
		else if (command.compare("pwd") == 0) {//command is lldir
			int code=cmd_pwd();
			myhint(code);
		}
		else if (command.compare("vi") == 0) { //command is copy
			const char* pa = path.c_str();
			cmd_vi(pa);
		}
		else if (command.compare("sync") == 0) {
			cmd_sync();
		}
		fresh_cmd();
	}
}

int get_space(string s,int start)
{
	int namelen = start;
	while (s[namelen] == ' ') {
		namelen++;
	}
	return namelen;
}