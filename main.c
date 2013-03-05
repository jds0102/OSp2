/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct disk *disk;
int framesLeft;
int nframes;
int npages;
char *physmem;
char *virtmem;
char *method;

void replace_random (struct page_table *pt, int page)
{
  int i = 0;
  int frameReturned;
  int bitsReturned;
  int randNumPages = rand()%nframes;
  while (i < npages){
    page_table_get_entry(pt, i, &frameReturned, &bitsReturned);
    if (bitsReturned != 0){
	//printf("%i\n", randNumPages);
	if(randNumPages == 0){
	  disk_write(disk, i, &physmem[frameReturned*PAGE_SIZE]);
	  disk_read(disk, page, &physmem[frameReturned*PAGE_SIZE]);
	  page_table_set_entry(pt, page, frameReturned, PROT_READ);
	  page_table_set_entry(pt, i, 0, 0);
	  i = npages;
	}else{
	  randNumPages--;
	}
    }
    i++;
  }
  
}
void page_fault_handler( struct page_table *pt, int page )
{
  int frameReturned;
  int bitsReturned;
  page_table_get_entry(pt, page, &frameReturned, &bitsReturned);
  if (bitsReturned == PROT_READ){
    page_table_set_entry(pt,page,frameReturned,PROT_READ|PROT_WRITE);
  }else if (framesLeft > 0){
    framesLeft--;
    page_table_set_entry(pt,page,framesLeft,PROT_READ);
    disk_read(disk, page, &physmem[framesLeft*PAGE_SIZE]);
  }else{
      if (strcmp("rand", method) == 0){
	replace_random(pt, page);	
      }
  }
  
  
}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}


	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	const char *program = argv[4];
	method = argv[3];
	framesLeft = atoi(argv[2]);

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}

	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	virtmem = page_table_get_virtmem(pt);

	physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);

	}

	page_table_delete(pt);
	disk_close(disk);

	return 0;
}
