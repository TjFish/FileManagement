/*包含对位图操作函数，有i节点位图和逻辑块位图*/

#include"fs.h"
//设置某一位为1
#define _set_bit(x,y)  x|=(1<<y)
//设置某一位为0
#define _clr_bit(x,y)  x&=~(1<<y)
#define _get_bit(x,y)   ((x) >> (y)&1)
/*设置data 数据块的第k位为1*/
int set_bit(int k, char* data)
{
	_set_bit(data[k / 8], k % 8);
	return 1;
}
/*设置data 数据块的第k位为0*/
int clear_bit(int k,char* data)
{
	_clr_bit(data[k / 8], k % 8);
	return 1;
}
int get_bit(int k, char* data)
{
	return _get_bit(data[k / 8], k % 8);
}

int find_first_zero(char* data)
{
	int i = 0;
	for (i = 0; i < BLOCK_BIT; ++i)
	{
		if (!get_bit(i, data))
			break;
	}
	return i;
}

