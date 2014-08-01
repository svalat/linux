#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define MAP_PAGE_REUSE  0x80000
#define MADV_PAGE_REUSE 14      /* Enable page reuse on the given VMA. */
#define MADV_NO_PAGE_REUSE  15      /* Disable page reuse on the given VMA. */

//#define SIZE (2*1024UL*1024UL)
#define SIZE (128*1024UL)

int main(void)
{
	int i = 0;
	printf("First test with madvise\n");
	sleep(1);fflush(stdout);
	char * ptr = mmap(NULL,SIZE,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0);
	assert(ptr != MAP_FAILED);
	/*for (i = 0 ; i < 4096 ; i++)
		assert(ptr[i] == 0);*/
	int res = madvise(ptr, SIZE, MADV_PAGE_REUSE);
	madvise(ptr, SIZE, MADV_PAGE_REUSE);
	printf("Test access\n");
	sleep(1);fflush(stdout);
	//assert(res == 0);
	int cnt = 0;
	for (i = 0 ; i < SIZE ; i++)
	{
		if (i % 4096 == 0)
			ptr[i] = 0;
		//ptr[i] = 1;
		cnt += ptr[i];
		//ptr[i] = 1;
	}
	madvise(ptr, SIZE, MADV_PAGE_REUSE);
	assert(cnt == 0);
	//return 0;
	printf("Test with mmap now\n");
	sleep(1);fflush(stdout);
	ptr = mmap(NULL,SIZE,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE|MAP_PAGE_REUSE,-1,0);
	//madvise(ptr, SIZE, MADV_PAGE_REUSE);
	cnt = 0;
	for (i = 0 ; i < SIZE ; i++)
	{
		if (i % 4096 == 0)
			ptr[i] = 0;
		cnt += ptr[i];
		ptr[i] = 42;
	}
	assert(cnt == 0);
	printf("try to reuse pages");
	sleep(1);fflush(stdout);
	munmap(ptr,SIZE);
	ptr = mmap(NULL,SIZE*2,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE|MAP_PAGE_REUSE,-1,0);
	cnt = 0;
	for (i = 0 ; i < SIZE*2 ; i++)
	{
                if (i % 4096 == 0)
		{
                        ptr[i] = 0;
		} else if (i < SIZE) {
			printf("loop %d => %d\n",i,(int)ptr[i]);
			//assert(ptr[i] == 42);
		} else {
			assert(ptr[i] == 0);
		}
                cnt += ptr[i];
        }
	assert(cnt != 0);
	return 0;
}

