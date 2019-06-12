/*������λͼ������������i�ڵ�λͼ���߼���λͼ*/

#include"fs.h"
//����ĳһλΪ1
#define _set_bit(x,y)  x|=(1<<y)
//����ĳһλΪ0
#define _clr_bit(x,y)  x&=~(1<<y)
#define _get_bit(x,y)   ((x) >> (y)&1)
/*����data ���ݿ�ĵ�kλΪ1*/
int set_bit(int k, char* data)
{
	_set_bit(data[k / 8], k % 8);
	return 1;
}
/*����data ���ݿ�ĵ�kλΪ0*/
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

