﻿#define FUSE_USE_VERSION 26
#define nobody -1
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fuse.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <stdio.h>
#include <math.h>
struct inode
{
	int32_t inode_index;
    struct stat *st;				//描述文件属性的结构
    int32_t content[15];			//索引号，指向存放数据的数据块
};
struct filenode                     //此处的文件节点是存放在目录对应的数据块中的
{
    char filename[124];				//文件名
    int32_t inode_index;			//该文件对应的inode节点号
};
struct block_group					//块组结构体
{
    void *group_descriptor;			//块组描述符
    void *group_bitmap;				//块位图
    void *inode_bitmap;				//inode位图
    void *inode_table;				//inode表
    void *data_block;				//数据块起始地址的指针
};

static const size_t size = 4 * 1024 * 1024 * (size_t)1024;			//文件系统空间总大小4GB
static const int block_size = 4096;									//每个块的大小为4KB
static const int32_t block_number = (int32_t)1024 * 1024;				//总的块的个数
static const int block_group_number = 32;								//32个块组
static const int group_descriptor_addr = 0;							//块组描述符在一个块组中的相对位置（第几个块）
static const int group_bitmap_addr = 1;								//块位图在一个块组中的相对位置（第几个块）
static const int inode_bitmap_addr = 2;								//inode位图在一个块组中的相对位置（第几个块）
static const int inode_table_addr = 3;									//inode表在一个块组中的相对位置（第几个块）
static const int data_block_addr = 2051;								//数据块在一个块组中的相对位置（第几个块）
static const int inode_table_block_number = 2048;						//一个块组中，inode表占用2048个块
static const int inode_number = 32768;									//一个块组中，有32768个inode
static const int data_block_number = 30717;							//一个块组中，有30717个数据块
static const int block_number_of_group = 32768;						//一个块组有32768个块
static const int inode_size = 256;										//inode的大小为256个字节(144stat，60分级索引)
static const int filenode_size = 128;									//文件节点的大小为128个字节(124文件名，４inode绝对地址)
static void *mem[(int32_t)1024 * 1024];
static struct block_group block_group_pointer[32];
static void *root = NULL;												//指向根目录的inode所在的块
//以上部分信息应该作为文件系统的信息存储于超级块中
static void *super;													//超级块
static struct inode *node;

int32_t get_free_inode(int index)										//获取一块空闲的inode,返回其的相对位置
{
	printf("get_free_inode\n");
    int left;
    memcpy(&left,block_group_pointer[index].group_descriptor + sizeof(int),sizeof(int));
    if(left == 0)return -1;
    left--;
    memcpy(block_group_pointer[index].group_descriptor + sizeof(int),&left,sizeof(int));
    char status[block_size];
    memcpy(status,block_group_pointer[index].inode_bitmap,block_size);
    for(int i = 0; i < block_size; i++)
    {
        unsigned char temp = status[i];
        if(temp == 255)continue;
        for(int j = 0; j < 8; j++)
        {
            if((temp & 1) == 0)
            {
				if(j == 0)status[i] += 1;
                else status[i] += (2 << (j - 1));
                memcpy(block_group_pointer[index].inode_bitmap + i,&status[i],1);
                return index * inode_number + 8 * i - j + 7;
            }
            else temp = temp >> 1;
        }
    }
    return -1;
}

int32_t get_free_datablock()				//获取一块空闲的数据块，返回其编号
{
	printf("get_free_datablock\n");
    for(int index = 0; index < block_group_number; index++)
    {
        int left;
        memcpy(&left,block_group_pointer[index].group_descriptor + sizeof(int),sizeof(int));
        if(left == 0)continue;
        left--;
        memcpy(block_group_pointer[index].group_descriptor + sizeof(int),&left,sizeof(int));
        char status[block_size];
        memcpy(status,block_group_pointer[index].group_bitmap,block_size);
        for(int i = 0; i < block_size; i++)
        {
            unsigned char temp = status[i];
            if(temp == 255)continue;
            for(int j = 0; j < 8; j++)
            {
                if((temp & 1) == 0)
                {
					if(j == 0)status[i] += 1;
                    else status[i] += (2 << (j -1));
                    memcpy(block_group_pointer[index].group_bitmap + i,&status[i],1);
					mem[index * block_number_of_group + data_block_addr + 8 * i - j + 7] = 
						mmap(NULL,block_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
					memset(mem[index * block_number_of_group + data_block_addr + 8 * i - j + 7],0,block_size);
                    return index * block_number_of_group + data_block_addr + 8 * i - j + 7 ;
                }
                else temp = temp / 2;
            }
        }
    }
    return -1;
}

static struct inode *get_filenode(const char *name)		//根据文件路径，返回文件节点
{
	printf("get_filenode\n");
    int32_t index[15];
    memcpy(index,root + sizeof(struct stat),60);
    char *temp;
    char tmp_name[124];
    int32_t inode_index;
    for(int i = 0; i < 12; i++)
    {
		if(index[i] == -1)continue;
        temp = (char *)mem[index[i]];
        for(int j = 0; j < 32; j++)
        {	
            memcpy(tmp_name,temp,124);
            if(strcmp(tmp_name,name + 1) == 0)
            {
                memcpy(&inode_index,temp + 124,4);
				memcpy(node->st,block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size,sizeof(struct stat));
                memcpy(node->content,block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size + sizeof(struct stat),60);
				node->inode_index = inode_index;
                return node;
            }
            temp += 128;
        }
    }
    return NULL;
}

static void unmap(int32_t datablock_index)							//将编号为datablock_index的数据块释放掉
{
	printf("unmap\n");
    int block_group_index = datablock_index / block_number_of_group;
    int free_datablock;
    memcpy(&free_datablock,block_group_pointer[block_group_index].group_descriptor + sizeof(int),sizeof(int));
    free_datablock++;
    memcpy(block_group_pointer[block_group_index].group_descriptor + sizeof(int),&free_datablock,sizeof(int));

    unsigned char status[4096];
    memcpy(status,block_group_pointer[block_group_index].group_bitmap,block_size);
    int temp = datablock_index % block_number_of_group - data_block_addr;
    status[temp / 8] -= (2<<(7 - temp % 8)) / 2;
    memcpy(block_group_pointer[block_group_index].group_bitmap + temp / 8,&status[temp / 8],1);
	munmap(mem[datablock_index],block_size);
}


static struct inode *del_filenode(const char *name)		//根据文件路径，返回inode节点
{
	printf("del_filenode\n");
    int32_t index[15];
    memcpy(index,root + sizeof(struct stat),60);
    char *temp;
    char tmp_name[124];
    int32_t inode_index;
    for(int i = 0; i < 12; i++)
    {
		if(index[i] == -1)continue;
        temp = (char *)mem[index[i]];
        for(int j = 0; j < 32; j++)
        {
			
            memcpy(tmp_name,temp,124);
            if(strcmp(tmp_name,name + 1) == 0)
            {
                memcpy(&inode_index,temp + 124,4);
				memcpy(node->st,block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size,sizeof(struct stat));
                memcpy(node->content,block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size + sizeof(struct stat),60);
				node->inode_index = inode_index;
				
				//将根目录对应的数据块中的最后一个文件节点移到被删除的位置
				int32_t next;
				memcpy(&next,temp + 128,4);
				if(next == 0)						//被删除的文件就是最后一个文件
				{
					memset(temp,0,128);
					if(j == 0)
					{
						unmap(index[i]);
						index[i] = -1;
						memcpy(root + sizeof(struct stat),index,60);
					}
					return node;
				}
				else
				{
					for(int k = i; k < 12; k++)							//找到最后一个文件
					{
						if(index[k] != -1 && index[k + 1] == -1)
						{
							void *temp1 = mem[index[k]];
							for(int m = 0; m < 32; m++)
							{
								int32_t data1,data2;
								memcpy(&data1,temp1 + m * 128,4);
								if(m == 31)
								{	if(data1 != 0)
									{
										memcpy(temp,temp1 + m * 128,128);
										memset(temp1 + m * 128,0,128);
										return node;
									}
								}
								else
								{
									memcpy(&data2,temp1 + m * 128 + 128,4);
									if(data1 != 0 && data2 == 0)
									{
										memcpy(temp,temp1 + m * 128,128);
										memset(temp1 + m * 128,0,128);
										if(m == 0)							
										//如果该数据块就剩一个文件，则删除该块
										{
											unmap(index[k]);
											index[k] == -1;
											memcpy(root + sizeof(struct stat),index,60);
										}
										return node;
									}
								}
							}
						}
					}
				}	
				
            }
            temp += 128;
        }
    }
    return NULL;
}

static int32_t isinodeIndexFull(int index[15])				//查看根目录节点对应的数据块是否还有空闲，即是否达到文件数量上限，返回数据块编号
{
	printf("isinodeindexfull\n");
    for(int i = 0; i < 12; i++)
    {
        if((index[i + 1] == -1 || index[i + 1] == 0) && (index[i] != -1 && index[i] != 0))
        {
            void *temp = mem[index[i]];
            int32_t data;
            memcpy(&data,temp + 31 * 128,4);
            if(data == 0)return index[i];
            else
            {
            	index[i + 1] = get_free_datablock();
            	if(index[i + 1] == -1)return ENOSPC;
            	memset(mem[index[i]],0,block_size);
            	return index[i + 1];
            }
        }
    }
    index[0] = get_free_datablock();
    if(index[0] == -1)return ENOSPC;
    memset(mem[index[0]],0,block_size);
   	return index[0];
}

static void addItemInDirectory(void *dir,const char *filename,int32_t inode_index)		//在目录对应的数据块中添加新的文件节点
{
	printf("addItemInDirectory\n");
    int index[15];
    memcpy(index,dir + sizeof(struct stat),60);

    int32_t datablock_index = isinodeIndexFull(index);
	memcpy(dir + sizeof(struct stat),index,60);			//index应该写回去的，这是一个重大错误

    int32_t data[1024];
	int32_t inode_index_temp = inode_index;
    memcpy(data,mem[datablock_index],block_size);
    for(int i = 0; i < 32; i++)
    {
        if(data[32 * i] == 0)
        {
            memcpy(mem[datablock_index] + 128 * i,filename,strlen(filename));
            memcpy(mem[datablock_index] + 128 * i + 124,&inode_index_temp,4);
			break;
        }
    }
}

static void create_filenode(const char *filename,struct stat *st)
{
	printf("create_filenode\n");
    int32_t inode_index;
    int index = 0;
    for(index = 0; index < block_number_of_group; index++)
    {
        inode_index = get_free_inode(index);
        if(inode_index == -1)continue;
        else break;
    }
    if(inode_index == -1)return;
    memcpy(block_group_pointer[index].inode_table + inode_index * inode_size,st,sizeof(struct stat));
    memset(block_group_pointer[index].inode_table + inode_index * inode_size + sizeof(struct stat),-1,inode_size - sizeof(struct stat));
    addItemInDirectory(root,filename,inode_index);
}

static void *oshfs_init(struct fuse_conn_info *conn)			//相当于格式化
{
	printf("init\n");
    for(int i = 0; i < block_group_number; i++)
    {
        mem[i * block_number_of_group] = mmap(NULL,block_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        block_group_pointer[i].group_descriptor = mem[i * block_number_of_group];
        memcpy(block_group_pointer[i].group_descriptor,&inode_number,sizeof(int));
        memcpy(block_group_pointer[i].group_descriptor + sizeof(int),&data_block_number,sizeof(int));
		//给所有的块组描述符分配空间

        mem[i * block_number_of_group + 1] = mmap(NULL,block_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        block_group_pointer[i].group_bitmap = mem[i * block_number_of_group + 1];
        memset(mem[i * block_number_of_group + 1],0,block_size);
		//给所有的块位图分配空间

        mem[i * block_number_of_group + 2] = mmap(NULL,block_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        block_group_pointer[i].inode_bitmap = mem[i * block_number_of_group + 2];
        memset(mem[i * block_number_of_group + 2],0,block_size);
		//给所有的inode位图分配空间
		mem[i * block_number_of_group + 3] = mmap(NULL,block_size * inode_table_block_number,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		block_group_pointer[i].inode_table = mem[i * block_number_of_group + 3];
    }
	
    int index;
    int i;
    for(i = 0; i < block_group_number; i++)
    {
        index = get_free_inode(i);
		if(index != -1)break;
    }
    root = block_group_pointer[i].inode_table + index * inode_size;
    node = (struct inode*)malloc(sizeof(struct inode));
    node->st = (struct stat*)malloc(sizeof(struct stat));
    memset(block_group_pointer[i].inode_table + index * inode_size + sizeof(struct stat),-1,inode_size - sizeof(struct stat));
    
    //构建超级块
    super = mmap(NULL,block_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memcpy(super,&size,4);
    memcpy(super + 4,&block_size,4);
  	memcpy(super + 8,&block_number,4);
  	memcpy(super + 12,&block_group_number,4);
  	memcpy(super + 16,&block_number_of_group,4);
	for(int i = 0; i < 32; i++)
		memcpy(super + 16 + 4 * i,mem[i * block_number_of_group],4);			
  	memcpy(super + 140,root,4);
    return NULL;
}

static int oshfs_getattr(const char *path, struct stat *stbuf)		//获取文件属性
{
	printf("getattr\n");
    int ret = 0;
    struct inode *nd = get_filenode(path);
    if(strcmp(path, "/") == 0)
    {
        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_mode = S_IFDIR | 0755;
    }
    else if(nd) 
	{	
		memcpy(stbuf,nd->st,sizeof(struct stat));
	}
    else 
	{
		ret = -ENOENT;
	}
    return ret;
}

static int oshfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)	//显示当前目录下的文件
{
	printf("readdir\n");
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    int index[15];
    char name[124];
    struct stat tmp_stat;
    int32_t inode_index;
    memcpy(index,root + sizeof(struct stat),60);

    for(int i = 0; i < 12; i++)
    {
        if(index[i] == -1)break;
		
        for(int j = 0; j < 32; j++)
        {
            memcpy(&inode_index,mem[index[i]] + 128 * j + 124,4);
			memcpy(name,mem[index[i]] + 128 * j,124);
			if(inode_index == 0)continue;

			memcpy(&tmp_stat,block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size,sizeof(struct stat));
            filler(buf,name,&tmp_stat,0);
        }
    }
    return 0;
}

static int oshfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	printf("mknod\n");
    struct stat st;
    st.st_mode = S_IFREG | 0644;
    st.st_uid = fuse_get_context()->uid;		//文件所有者
    st.st_gid = fuse_get_context()->gid;		//文件所有者对应的组
    st.st_nlink = 1;					//文件的连接数
    st.st_size = 0;					//普通文件，对应的文件字节数
	st.st_blksize = 4096;
	st.st_blocks = 0;
    create_filenode(path + 1, &st);			//创建文件节点
    return 0;
}

static int oshfs_open(const char *path, struct fuse_file_info *fi)
{
	printf("open\n");
  	int32_t index[15];
    memcpy(index,root + sizeof(struct stat),60);
    char *temp;
    char tmp_name[124];
    int32_t inode_index;
    for(int i = 0; i < 12; i++)
    {
		if(index[i] == -1)continue;
        temp = (char *)mem[index[i]];
        for(int j = 0; j < 32; j++)
        {	
            memcpy(tmp_name,temp,124);
            if(strcmp(tmp_name,path + 1) == 0)return 0;
            temp += 128;
        }
    }
    return ENOENT;
}

static int oshfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("write\toffset %d\tsize %d\t\n",offset,size);
	printf("buf %s\n",buf);
    struct inode *nd = get_filenode(path);
	int32_t block_engaged = nd->st->st_blocks;
	
    if(offset + size > nd->st->st_size)
        nd->st->st_size = offset + size;

	if(offset < 48 * 1024)												//直接寻址
	{
		int block_index = offset / block_size;
		if(offset % block_size != 0)
		memcpy(mem[nd->content[block_index]] + offset % block_size,buf,
			(size > block_size - offset % block_size) ? (block_size - offset % block_size) : size);
		else
		{
			if(block_index >= block_engaged)
			{
				nd->content[block_index] = get_free_datablock();
				if(nd->content[block_index] == -1)return ENOSPC;
				block_engaged++;
				nd->st->st_blocks = block_engaged;
			}
			memcpy(mem[nd->content[block_index]],buf,(size > block_size) ? block_size : size);
		}
	}
	
	else if(offset < 48 * 1024 + 1024 * 4096)							//一级寻址
	{
		if(nd->content[12] == -1 || nd->content[12] == 0)nd->content[12] = get_free_datablock();
		if(nd->content[12] == -1)return ENOSPC;
		void *temp = mem[nd->content[12]];
		int block_index = offset / block_size;
		int block_index_real;
		if(offset % block_size != 0)
		{
			memcpy(&block_index_real,temp + 4 * (block_index - 12),4);
			memcpy(mem[block_index_real] + offset % block_size,buf,
				(size > block_size - offset % block_size) ? (block_size - offset % block_size) : size);
		}
		else
		{
			if(block_index >= block_engaged)
			{
				block_index_real = get_free_datablock();
				if(block_index_real == -1)return ENOSPC;
				memcpy(temp + 4 * (block_index - 12),&block_index_real,4);
				block_engaged++;
				nd->st->st_blocks = block_engaged;
			}
			memcpy(mem[block_index_real],buf,(size > block_size) ? block_size : size);
		}
	}
	else															//二级寻址
	{
		if(nd->content[13] == -1 || nd->content[13] == 0)nd->content[13] = get_free_datablock();
		void *temp = mem[nd->content[13]];
		int32_t block_index = offset / block_size;
		int32_t block_index1 = (block_index - 1036) / 1024;			//表示这个数据块的地址所在的数据块是第几个，变量名不知道怎么起
		int32_t block_index1_real;									//表示这个数据块的地址所在的数据块的地址，变量名不知道怎么起
		int32_t block_index_real;
		memcpy(&block_index1_real,temp + 4 * block_index1,4);
		if(block_index1_real == -1 || block_index1_real == 0)
		{
			block_index1_real = get_free_datablock();
			if(block_index1_real == -1)return ENOSPC;
			memcpy(temp + 4 * block_index1,&block_index1_real,4);
		}
	    void *temp1 = mem[block_index1_real];						//表示这个数据块的地址所在的数据块
		if(offset % block_size == 0)
		{
			if(block_index >= block_engaged)						//表明是追写，需要分配一个数据块
			{
				block_index_real = get_free_datablock();
				if(block_index_real == -1)return ENOSPC;
				memcpy(temp1 + 4 * ((block_index - 12) % 1024),&block_index_real,4);
				block_engaged++;
				nd->st->st_blocks = block_engaged;
			}
			memcpy(mem[block_index_real],buf,(size > block_size) ? block_size : size);
		}
		else
		{
			memcpy(&block_index_real,temp1 + 4 * ((block_index - 12) % 1024),4);
			memcpy(mem[block_index_real] + offset % block_size,buf,
				(size > block_size - offset % block_size) ? (block_size - offset % block_size) : size);
		}
	}
	
	int32_t inode_index = nd->inode_index;
	memcpy(block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size,
				nd->st,sizeof(struct stat));
	memcpy(block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size + sizeof(struct stat),
			nd->content,60);
    return size;
}

static int oshfs_truncate(const char *path, off_t size)
{
	printf("truncate size %d\n",size);
    struct inode *nd = get_filenode(path);
    int32_t block_engaged = (nd->st->st_size + 4095) / 4096;					//该文件占用的块的数量
    nd->st->st_size = size;
    int32_t block_left = (size + 4095) / 4096;									//该文件调整大小后剩余几个块
    for(int i = block_left; i < 12 && i < block_engaged; i++)
    {
        unmap(nd->content[i]);
        nd->content[i] = -1;
        nd->st->st_blocks--;
    }
    if(block_engaged > 12)													//一级寻址
    {
    	int start = (block_left > 12) ? block_left - 12 : 0;					//从第几个块开始删
    	int end =  (block_engaged > 1036) ? 1023 : block_engaged - 13;			//一直删到第几个块（包括这个块）
    	int32_t block_index;
    	void *temp = mem[nd->content[12]];
    	for(int i = start; i <= end; i++)
    	{
    		memcpy(&block_index,temp + 4 * i, 4);
    		unmap(block_index);
    		nd->st->st_blocks--;										//忘记加这个
    		memcpy(temp + 4 * i,&block_index,4);
    	}
    	if(start == 0)													//如果所有的索引对应的块都被删除，则删除该索引块
    	{
    		unmap(nd->content[12]);
    		nd->content[12] = -1;
    	}
    }
    if(block_engaged > 1036)													//二级寻址
    {
    	void *temp1 = mem[nd->content[13]];
    	int start1 = (block_left > 1036) ? (block_left - 1036) / 1024 : 0;		//要删除的第一个块的地址所在的块是第几个
    	int start2 = (block_left > 1036) ? (block_left - 1036) % 1024 : 0;		//要删除的第一个块的地址在相应的索引块中的相对位置
    	for(int i = start1; i < 1024 && nd->st->st_blocks > block_left; i++)
    	{
    		int32_t block_index1;
    		memcpy(&block_index1,temp1 + 4 * i,4);
    		if(block_index1 == 0 || block_index1 == -1)break;
    		void *temp2 = mem[block_index1];
    		int32_t block_index2;
    		int j = 0;
    		if(i == start1)j = start2;
    		while(j < 1024 && nd->st->st_blocks > block_left)
			{
				memcpy(&block_index2,temp2 + 4 * j,4);
				if(block_index2 == 0 || block_index2 == -1)break;
				unmap(block_index2);
				nd->st->st_blocks--;
				j++;
			}
			if(!(i == start1 && start2 != 0))unmap(block_index1);
		}
		if(start1 == 0 && start2 == 0)
		{
			unmap(nd->content[13]);
			nd->content[13] = -1;
		}
    }
    int32_t inode_index = nd->inode_index;
    memcpy(block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size,
				nd->st,sizeof(struct stat));
	memcpy(block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size + sizeof(struct stat),
			nd->content,60);
    return 0;
}

static int oshfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("read\n");
    struct inode *nd = get_filenode(path);
    int ret = size;
    if(offset + size > nd->st->st_size)
        ret = nd->st->st_size - offset;
    int32_t start_block = offset / block_size;
    int32_t end_block = (offset + size) / block_size;
    int32_t block_read = 0;
    for(int i = start_block; i < 12 && i < end_block; i++)
    {
    	if(i == end_block - 1)memcpy(buf + block_size * block_read,mem[nd->content[i]],
				(ret % block_size == 0 && ret != 0) ? block_size : ret % block_size);
    	else memcpy(buf + block_size * block_read,mem[nd->content[i]],block_size);
    	block_read++;
    }
    if(end_block > 12 && start_block < 1036)												//一级寻址
    {
    	void *temp = mem[nd->content[12]];
	int32_t block_index = (start_block >= 12) ? offset / block_size : 12;			//从第几个数据块开始读
	int32_t block_index_real;
	int i = 0;
	while(block_read < end_block - start_block)
	{
		if(i == 1024 - (block_index - 12))break;
		memcpy(&block_index_real,temp + 4 * (block_index - 12),4);
		int byte_read;
		if(block_read == end_block - start_block - 1)
		memcpy(buf + block_size * block_read,mem[block_index_real],(ret % block_size == 0 && ret != 0) ? block_size : ret % block_size);
			//读取字节大小设为ret % block_size应该是错的,这个bug找了一个晚上
		else memcpy(buf + block_size * block_read,mem[block_index_real],block_size);
		temp += 4;
		block_read++;
		i++;
	}
	}
	if(end_block > 1036)														//二级寻址
	{
		void *temp = mem[nd->content[13]];
		int32_t block_index = (start_block >= 1036) ? offset / block_size : 1036;			//从第几个数据块开始读
		int32_t start = (block_index - 1036) / 1024;					//第一个要读的数据块的地址所在的数据块是第几个
		int32_t end = (end_block - 1036) / 1024;						//最后一个要读的数据块的地址所在的数据块是第几个
		for(int i = start; i <= end; i++)
		{	
			int32_t block_index_real;								//存放要读的数据块的地址所在的数据块的地址，变量名不知道怎么起
			memcpy(&block_index_real,temp + 4 * i,4);
			if(block_index_real == -1 || block_index_real == 0)break;
			int32_t block_index1_real;
			void *temp1 = mem[block_index_real];
			int j = 0;
			if(i == start)j = (block_index - 1036) % 1024;
			while(j < 1024)
			{
				if(i == end && j == (end_block - 1036) % 1024)break;				//读完了最后一个块
				memcpy(&block_index1_real,temp1 + 4 * j,4);
				if(block_index1_real == 0 || block_index1_real == -1)break;
				if(i == end && j == (end_block - 1036) % 1024 - 1)
				memcpy(buf + block_size * block_read,mem[block_index1_real],(ret % block_size == 0 && ret != 0) ? block_size : ret % block_size);
				else memcpy(buf + block_size * block_read,mem[block_index1_real],block_size);
				//temp1 += 4;					//愚蠢而要命的错误
				j++;
				block_read++;
			}
		}
	}
    return ret;
}

static int oshfs_unlink(const char *path)
{
	printf("unlink\n");
    struct inode *nd = del_filenode(path);
    int32_t block_to_del = nd->st->st_blocks;
    for(int i = 0; i < 12; i++)
    {
    	if(block_to_del == 0)break;
        if(nd->content[i] == -1 || nd->content[i] == 0)break;
        unmap(nd->content[i]);
        block_to_del--;
    }
    if(nd->content[12] != -1 && nd->content[12] != 0)				//一级寻址
    {
    	void *temp = mem[nd->content[12]];
    	int32_t block_index;
    	int i = 0;
    	while(i < 1024)
    	{
    		if(block_to_del == 0)break;
    		memcpy(&block_index,temp,4);
    		if(block_index == 0 || block_index == -1)break;
    		unmap(block_index);
    		temp += 4;
    		i++;
    		block_to_del--;
    	}
    	unmap(nd->content[12]);
    }
    if(nd->content[13] != -1 && nd->content[13] != 0)				//二级寻址
    {
    	void *temp = mem[nd->content[13]];
    	int32_t block_index;
    	int i = 0;
    	while(i < 1024)
    	{
    		memcpy(&block_index,temp,4);
    		if(block_index == 0 || block_index == -1)break;
    		void *temp1 = mem[block_index];
    		int32_t block_index1;
    		int j = 0;
    		while(j < 1024)
    		{
    			if(block_to_del == 0)break;
    			memcpy(&block_index1,temp1,4);
    			if(block_index1 == 0 || block_index1 == -1)break;
    			unmap(block_index1);
    			temp1 += 4;
    			j++;
    			block_to_del--;
    		}
    		unmap(block_index);
    		temp += 4;
    		i++;
    	}
    	unmap(nd->content[13]);
    }
    memset(block_group_pointer[nd->inode_index/inode_number].inode_table + (nd->inode_index%inode_number) * inode_size,-1,inode_size);
    return 0;
}

static int oshfs_chown(const char *path, uid_t uid, gid_t gid)			//更改文件所有者
{
	struct inode *nd = get_filenode(path);
	if(nd == NULL)return ENOENT;
	if(uid != nobody)nd->st->st_uid = uid;
	if(gid != nobody)nd->st->st_gid = gid;
	int32_t inode_index = nd->inode_index;
	memcpy(block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size,nd->st,sizeof(struct stat));
	return 0;
}

static int oshfs_chmod(const char *path, mode_t mode)						//更改文件权限
{
	struct inode *nd = get_filenode(path);
	if(nd == NULL)return ENOENT;
	nd->st->st_mode = mode;
	int32_t inode_index = nd->inode_index;
	memcpy(block_group_pointer[inode_index/inode_number].inode_table + (inode_index%inode_number) * inode_size,nd->st,sizeof(struct stat));
	return 0;
}


static const struct fuse_operations op = {
    .init = oshfs_init,
    .getattr = oshfs_getattr,
    .readdir = oshfs_readdir,
    .mknod = oshfs_mknod,
    .open = oshfs_open,
    .write = oshfs_write,
    .truncate = oshfs_truncate,
    .read = oshfs_read,
    .unlink = oshfs_unlink,
    .chown = oshfs_chown,
    .chmod = oshfs_chmod,
};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &op, NULL);
}

