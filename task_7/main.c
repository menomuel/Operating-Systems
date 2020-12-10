#include <stdio.h> //printf, errno
#include <sys/types.h> //open, lseek
#include <sys/stat.h> //open
#include <fcntl.h> //open
#include <unistd.h> //read, lseek, close
#include <stdlib.h> //malloc
#include <errno.h> //errno
#include <stdbool.h> //bool
#include <string.h> //strchr
#include <poll.h>
#include <fcntl.h> //fcntl, fstat
#include <sys/mman.h> //mmap

#define INC_COEF 1.5
#define DEFAULT_CAPACITY 8
#define BUFF_SIZE 16
#define TIMEOUT 5000

typedef struct _IndentTable
{
	off_t* arr;
	size_t size;
	size_t cap;
} IndentTable;


bool initIndentTable(IndentTable* table)
{
	errno = 0;

	table->arr = calloc(DEFAULT_CAPACITY, sizeof(off_t));
	if(table->arr == NULL)
	{
		perror("Cannot allocate memory for ident array");
		return false;
	}

	table->size = 0;
	table->cap = DEFAULT_CAPACITY;
	return true;
}

bool resizeTable(IndentTable* table)
{
	errno = 0;

	off_t* oldArr = table->arr;
	size_t oldCap = table->cap;

	size_t newCap = oldCap * INC_COEF;

	table->arr = realloc(table->arr, newCap * sizeof(off_t));
	if (table->arr == NULL)
	{
		table->arr = oldArr;
		perror("Cannot realloc memory");
		return false;
	}
	table->cap = newCap;

	return true;
}

bool pushIndent(IndentTable* table, int val)
{
	if (table->size == table->cap)
	{
		if(!resizeTable(table))
		{
			return false;
		}
	}

	table->arr[table->size] = val;
	++table->size;
	return true;
}

#define TIMEWAIT 1000

int waitRead(int fildes, int timeout)
{
	struct pollfd fds = {
		.fd = fildes,
		.events = POLLIN,
		.revents = 0
	};

	return poll(&fds, 1, timeout);
}


void* buffViaMmap(int fildes, int* buffSize)
{
	errno = 0;
	struct stat fileSt;
	while (fstat(fildes, &fileSt) != 0)
	{
		if(errno == EINTR)
		{
			errno = 0;
			continue;
		}

		perror("Cannot obtain file state");
		return NULL;
	}

	*buffSize = fileSt.st_size;
	while (1)
	{
		char* fp = mmap(NULL, *buffSize, PROT_READ,
			MAP_PRIVATE, fildes, 0);
		if(fp == MAP_FAILED)
		{
			if(errno == EINTR)
			{
				errno = 0;
				continue;
			}

			perror("Cannot map file");
		}
		return fp;
	}
}

bool fillIndentTable(IndentTable* table, char const* buff)
{
	char const* endlinePos = buff;

	while((endlinePos = strchr(endlinePos, '\n')) != NULL)
	{
		if(!pushIndent(table, endlinePos - buff))
		{
			perror("Cannot push element to indent table");
			return false;
		}

		++endlinePos;
	}
	return true;
}

void printIndentTable(IndentTable* table)
{
	printf("{ ");
	for(size_t i = 0; i < table->size; ++i)
	{
		printf("%ld, ", table->arr[i]);
	}
	printf(" }\n");
}

void destroyIndentTable(IndentTable* table)
{
	if(table->arr != NULL)
	{
		free(table->arr);
	}
}

bool printLine(char const* buff, IndentTable* table, int lineNum)
{
	--lineNum;

	errno = 0;

	if(lineNum < 0 || lineNum >= table->size)
	{
		printf("No line with such number\n");
		return false;
	}

	off_t beginPos =  lineNum == 0
		  	? 0
			: table->arr[lineNum-1] + 1; //+1 to get next pos after '\n'
	off_t length = table->arr[lineNum] - beginPos  + 1; //+1 to include '\n'
	buff += beginPos;

	for(int i = 0; i < length; ++i)
	{
		fputc(buff[i], stdout);
	}

	return true;
}

int exitTimer(int timeout)
{
	errno = 0;

	static struct pollfd fds = {
			.fd = 0, //stdin
			.events = POLLIN,
			.revents = 0
	};

	return poll(&fds, 1, timeout);
}

bool scanAndCheckLine(off_t* lineNum)
{
	errno = 0;

	if(scanf("%ld", lineNum) == 0)
	{
		perror("Not off_t format");
		return false;
	}

	if(*lineNum == 0)
	{
		return false;
	}
	return true;
}

int main()
{
	errno = 0;

	int fildes = open("text.txt", O_RDONLY);

	if(fildes == -1)
	{
		perror("Cannot open file");
		return -1;
	}

	int bufferSize = 0;
	char* buff = buffViaMmap(fildes, &bufferSize);
	if(buff == NULL)
		return 0;
	printf("%c", &buff[0]);
	close(fildes);

	IndentTable table;
	if(!initIndentTable(&table) || !fillIndentTable(&table, buff))
	{
		perror("Cannot make indent table");
		destroyIndentTable(&table);
		return 0;
	}

	off_t lineNum;
	for(;;)
	{
		printf("Type line num to read within 5 sec. Type 0 to exit: ");
		fflush(stdout);

		int option = exitTimer(TIMEOUT);

		if (option == -1)
		{
			if(errno == EINTR)
			{
				perror("Signal in poll");
				errno = 0;
				continue;
			}
			else
			{
				perror("Poll fail");
				break;
			}
		}
		else if(option != 0)
		{
			if(!scanAndCheckLine(&lineNum))
			{
				break;
			}
			printLine(buff, &table, lineNum);
		} else
		{
			printf("Out of time. The program will exit\nWhole file:\n");
			for(int lineNum = 1; lineNum <= table.size; ++lineNum)
			{
				printLine(buff, &table, lineNum);
			}
			break;
		}
	}

	munmap(buff,bufferSize);
	destroyIndentTable(&table);
	return 0;
}
