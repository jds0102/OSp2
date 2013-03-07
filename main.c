
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
int nframes, secondChanceFramesLeft;
int npages;
char *physmem;
char *virtmem;
char *method;
int *queue, *secondChanceQueue;
int queueHead = 0, secondChanceHead = 0;
int secondFifoSize;
int *pagesToWrite;
int *customWriteQueue, *customReadQueue;
int customWriteQueueSize, customReadQueueSize;
int customWriteQueueHead, customReadQueueHead;
int pagesInWriteQueue, pagesInReadQueue;

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
	  if (bitsReturned != PROT_READ) {
	    disk_write(disk, i, &physmem[frameReturned*PAGE_SIZE]);
	  }
	  disk_read(disk, page, &physmem[frameReturned*PAGE_SIZE]);
	  page_table_set_entry(pt, i, 0, 0);
	  page_table_set_entry(pt, page, frameReturned, PROT_READ);
	  i = npages;
	}else{
	  randNumPages--;
	}
    }
    i++;
  }
  
}

void replace_fifo(struct page_table *pt, int page) {
  int frameReturned;
  int bitsReturned;
  page_table_get_entry(pt, queue[queueHead], &frameReturned, &bitsReturned);
  if (bitsReturned != PROT_READ) {
    disk_write(disk, queue[queueHead], &physmem[frameReturned*PAGE_SIZE]);
  }
  disk_read(disk, page, &physmem[frameReturned*PAGE_SIZE]);
  page_table_set_entry(pt, queue[queueHead], 0, 0);
  page_table_set_entry(pt, page, frameReturned, PROT_READ);
  queue[queueHead] = page;
  queueHead ++;
  queueHead %= nframes;
}

void replace_2fifo(struct page_table *pt, int page) {  
  //if the page is in the second chance queue
  int secondChanceIter = 0;
  int frameReturned;
  int bitsReturned;
  int writeQueueIter = 0;
  while (secondChanceIter < secondFifoSize){
    if (page == secondChanceQueue[secondChanceIter]){
	// Move page into first queue - then update page table
	int tmpPage = queue[queueHead];
	queue[queueHead] = page;
	page_table_get_entry(pt, page, &frameReturned, &bitsReturned);
	//Look for the page in our list of dirty pages, mark it as writeable
	writeQueueIter = 0;
	int pageAlreadyDirty = 0;
	while (writeQueueIter < secondFifoSize+1) {
	  if (pagesToWrite[writeQueueIter] == page) {
	    pageAlreadyDirty = 1;
	    page_table_set_entry(pt, page, frameReturned, PROT_READ|PROT_WRITE);
	    pagesToWrite[writeQueueIter] = -1;
	    writeQueueIter = secondFifoSize;
	  }
	  writeQueueIter++;
	}
	if (!pageAlreadyDirty) {
	  page_table_set_entry(pt, page, frameReturned, PROT_READ);
	}
	queueHead++;
	queueHead %= (nframes - secondFifoSize);
	//Setting the spot of the previous page that was replaced to empty
	secondChanceQueue[secondChanceIter] = -1;
	secondChanceIter = secondChanceHead;
	int nextItem;
	int queueCompressed = 0;
	while (!queueCompressed){
	  if (secondChanceQueue[secondChanceIter] == -1){
	    nextItem = secondChanceIter + 1;
	    nextItem %= secondFifoSize;
	    secondChanceQueue[secondChanceIter] = secondChanceQueue[nextItem];
	    secondChanceQueue[nextItem] = -1; 
	  }
	  secondChanceIter++;
	  // Changed this because secondChanceIter was running off end
	  secondChanceIter %= secondFifoSize;
	  int tmpHead;
	  tmpHead = (secondChanceHead - 1) % secondFifoSize;
	  if (tmpHead < 0){
	    tmpHead = secondFifoSize - 1;
	  }
	  if (secondChanceIter == tmpHead){
	    //printf("%i : %i\n", secondChanceIter, secondChanceHead);
	    
	    page_table_get_entry(pt, tmpPage, &frameReturned, &bitsReturned);
	    if (bitsReturned == (PROT_READ|PROT_WRITE)) {
	      secondChanceQueue[secondChanceIter] = tmpPage;
	      writeQueueIter = 0;
	      while (writeQueueIter < secondFifoSize+1) {
		if (pagesToWrite[writeQueueIter] == -1) {
		  pagesToWrite[writeQueueIter] = tmpPage;
		  writeQueueIter = secondFifoSize;
		}
		writeQueueIter++;
	      }
	    } else if (strcmp(method, "custom") == 0) {
	      page_table_set_entry(pt, tmpPage, 0, 0);
	    } else {
	      secondChanceQueue[secondChanceIter] = tmpPage;
	      page_table_set_entry(pt, tmpPage, frameReturned, 0);
	    }
	    queueCompressed = 1;
	  }
	}
	return;
    }
    
    secondChanceIter++;
  }
  //if the page is not in the second chance queue
  int tempPage = queue[queueHead];
  queue[queueHead] = page;
  queueHead ++;
  queueHead %= (nframes - secondFifoSize);
  int secondTempPage = secondChanceQueue[secondChanceHead];
  secondChanceQueue[secondChanceHead] = tempPage;
  secondChanceHead ++;
  secondChanceHead %= secondFifoSize;
  page_table_get_entry(pt, tempPage, &frameReturned, &bitsReturned);
  //If page has been written to, add it to our dirty queue
  if (bitsReturned == (PROT_READ|PROT_WRITE)) {
    writeQueueIter = 0;
    while (writeQueueIter < secondFifoSize+1) {
      if (pagesToWrite[writeQueueIter] == -1) {
	pagesToWrite[writeQueueIter] = tempPage;
	writeQueueIter = secondFifoSize;
      }
      writeQueueIter++;
    }
  }
  page_table_set_entry(pt, tempPage, frameReturned, 0);
  if (secondChanceFramesLeft > 0){
    secondChanceFramesLeft--;
    page_table_set_entry(pt, page, secondChanceFramesLeft, PROT_READ);
    //printf("%i\n",secondChanceFramesLeft);
    disk_read(disk, page, &physmem[secondChanceFramesLeft*PAGE_SIZE]);
  }else{
    page_table_get_entry(pt, secondTempPage, &frameReturned, &bitsReturned);
    writeQueueIter = 0;
    //If we find the page about to be evicted in our queue of dirty pages, write it
    while (writeQueueIter < secondFifoSize+1) {
      if (pagesToWrite[writeQueueIter] == secondTempPage) {
	pagesToWrite[writeQueueIter] = -1;
	writeQueueIter = secondFifoSize;
	disk_write(disk, secondTempPage , &physmem[frameReturned*PAGE_SIZE]);
      }
      writeQueueIter++;
    }
    disk_read(disk, page, &physmem[frameReturned*PAGE_SIZE]);
    page_table_set_entry(pt, secondTempPage, 0, 0);
    page_table_set_entry(pt, page, frameReturned, PROT_READ);
  }
}

// void replace_custom( struct page_table *pt, int page ) {
//   int frameReturned;
//   int bitsReturned;
//   page_table_get_entry(pt, page, &frameReturned, &bitsReturned);
//   // Moving entry from read queue to write queue
//   if (bitsReturned == PROT_READ){
//     page_table_set_entry(pt,page,frameReturned,PROT_READ|PROT_WRITE);
//     int writeQueueIter = customWriteQueueHead;
//     writeQueueIter ++;
//     writeQueueIter %= customWriteQueueSize;
//     int nextItem;
//     int queueCompressed = 0;
//     while (!queueCompressed){
//       if (customWriteQueue[writeQueueIter] == -1){
// 	
//       }
//     }
//     
//   }
// }

void page_fault_handler( struct page_table *pt, int page )
{
//   if (strcmp(method, "custom") == 0) {
// 	replace_custom(pt, page);
// 	return;
//   }
  
  int frameReturned;
  int bitsReturned;
  page_table_get_entry(pt, page, &frameReturned, &bitsReturned);
  if (bitsReturned == PROT_READ){
    page_table_set_entry(pt,page,frameReturned,PROT_READ|PROT_WRITE);
  }else if (framesLeft > 0){
    framesLeft--;
    // Insert into queue for fifo case
    if (strcmp(method, "fifo") == 0 ) {
     queue[nframes-framesLeft-1] = page; 
    // We were initializing the queue improperly - need to subtract
    // secondFifoSize as well
    }else if (strcmp(method, "2fifo") == 0){
     queue[nframes - secondFifoSize - framesLeft - 1] = page; 
    }
    page_table_set_entry(pt,page,framesLeft + secondFifoSize,PROT_READ);
    //printf("%i\n",framesLeft + secondFifoSize);
    disk_read(disk, page, &physmem[framesLeft + secondFifoSize*PAGE_SIZE]);
  }else{
      if (strcmp("rand", method) == 0){
	replace_random(pt, page);	
      } else if (strcmp(method, "fifo") == 0) {
	replace_fifo(pt, page);
      } else if (strcmp(method, "2fifo") == 0 || strcmp(method, "custom") == 0) {
	replace_2fifo(pt, page);
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

	if (strcmp(method, "fifo") == 0) {
	  queue = malloc(nframes*sizeof(int));
        } else if(strcmp(method, "2fifo") == 0 || strcmp(method, "custom") == 0) {
	  secondChanceFramesLeft = secondFifoSize = (int)(nframes*0.25);
	  framesLeft -= secondFifoSize;
	  queue = malloc(framesLeft*sizeof(int));
	  secondChanceQueue = malloc(secondFifoSize*sizeof(int));
	  pagesToWrite = malloc((secondFifoSize+1)*sizeof(int));
	  int iter = 0;
	  while (iter < secondFifoSize){
	    secondChanceQueue[iter] = -1;
	    pagesToWrite[iter] = -1;
	    iter++;
	  }
	  pagesToWrite[iter] = -1;
	} /*else if(strcmp(method, "custom") == 0) {
	  customReadQueueSize = (int)(nframes*0.25);
	  customWriteQueueSize = nframes - customReadQueueSize;
	  customReadQueue = malloc(customReadQueueSize*sizeof(int));
	  customWriteQueue = malloc(customWriteQueueSize*sizeof(int));
	  int iter = 0;
	  while (iter < customWriteQueueSize){
	    customWriteQueue[iter] = -1;
	    if (iter < customReadQueueSize) {
	      customReadQueue[iter] = -1;
	    }
	    iter++;
	  }
	}*/
	
	if (framesLeft > npages) {
	  framesLeft = npages;
	}
      
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
