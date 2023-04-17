#include "my_vm.h"

//global variables - MYCODE
unsigned long long p_count;		//physical page count
unsigned long long v_count;		//virtual page count
void * phys_mem_base;			//pointer to the start of our allocated physical memory
char * p_bitmap;			// physical bitmap to check if the physical pages are free
char * v_bitmap;			//virtual bitmap to check if the virtual pages are free
int offset_bits,level2_bits,level1_bits;		// for 2level page table 
unsigned long *page_dir; //the base physical address of the page directory
int phys_mem_allocated = 0;		// to check if the physical memory was allocated/initialized
int current_page_table_index;		// Stores the physical pag number of the page table which is currently being filled for new page
pthread_mutex_t mutex;			// mutex for synchronization
struct tlb tlb_t;			// initializing TLB
double tlb_lookup = 0;			// TLB metrics
double tlb_miss = 0;
double tlb_hit = 0;

/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    
    //number of pysical pages
    p_count = MEMSIZE/PGSIZE;
    //number of virtual pages
    v_count = MAX_MEMSIZE/PGSIZE;
    
    //allocate available physical memory - not typecasting to keep the pages generalized
    phys_mem_base = malloc(p_count*PGSIZE);
    
    
    //initializing physical and virtual bitmaps we divide page counts by 8 so that each page gets 1 bit to rpresent
    p_bitmap = (char *)malloc(p_count/8); 
    memset(p_bitmap,0, p_count/8); //clear everything
    
    v_bitmap = (char *)malloc(v_count/8);
    memset(p_bitmap,0, p_count/8); //clear everything 
   
    
    //allocate space to the page directory - change to end of the memory
    page_dir = (unsigned long *)malloc(sizeof(unsigned long));
    page_dir = phys_mem_base; 
    
    //update the corresponding p_bitmap
    set_bit_at_index(p_bitmap, 0);
    set_bit_at_index(v_bitmap, 0);
    
    
    //division of virtual address space bits
    offset_bits = log2(PGSIZE);
    level2_bits = log2(num_of_pd_entries);
    level1_bits = 32 - level2_bits;
    
    phys_mem_allocated = 1;
    
	
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
int
add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
	unsigned long vpn = (unsigned long)va >> offset_bits;
	
	//virtual page number
	tlb_t.cache[vpn%TLB_ENTRIES][0] = vpn;
	//physical page number
	tlb_t.cache[vpn%TLB_ENTRIES][1] = *(unsigned long*)pa;
	return 0;

	
    //return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *
check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    	tlb_lookup++;
    	
    	unsigned long v_addr = (unsigned long)va;
	unsigned long mask = (1 << offset_bits) - 1;
	unsigned long offset = v_addr & mask;
    	
	unsigned long vpn = (unsigned long)va >> offset_bits;
	
	if(tlb_t.cache[vpn%TLB_ENTRIES][0] == vpn)
	{	tlb_hit++;
		//return physical address formed by ppn stored in TLB
		return ((phys_mem_base + (tlb_t.cache[vpn%TLB_ENTRIES][1]*PGSIZE)) + offset*sizeof(pte_t));
	}
	else{
		//return if no entry found
		tlb_miss++;
		return NULL;
	}


   /*This function should return a pte_t pointer*/
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/

	
    miss_rate = tlb_miss/tlb_lookup;

	fprintf(stderr, "TLB Lookups 	= %lf \n", tlb_lookup);
	fprintf(stderr, "TLB miss 	= %lf \n", tlb_miss);
	fprintf(stderr, "TLB hit 	= %lf \n", tlb_hit);
	
    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
    
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
	
	//check if there is already a mapping for the va in TLB
	pte_t *pa = check_TLB(va);
	if(pa != NULL){
	
		return pa;
	}
	
	//using bit manipulation to make mask for extracting the offset
	unsigned long v_addr = (unsigned long)va;
	unsigned long mask = (1 << offset_bits) - 1;
	unsigned long offset = v_addr & mask;


	//extracting value of 1st level or page directory offset
	unsigned long PD_index = v_addr >> (level2_bits + offset_bits);
	
	//extracting value of 2nd level or page table offset
	unsigned long PT_index = v_addr >> offset_bits;
	mask = (1 << (level2_bits)) -1 ;
	PT_index = PT_index & mask;
	
	//point to the page directory entry
	unsigned long *PD_entry = pgdir + PD_index;
	
	//return if its empty
	if((unsigned long)*PD_entry == 0){
		return NULL;
	}
	
	//calulate page table address with value in page directory
	unsigned long *PT_entry = (phys_mem_base + (*PD_entry*PGSIZE)) + PT_index*sizeof(pte_t);
	
	//return if page table entry is empty
	if((unsigned long)*PT_entry == 0){
		return NULL;
	}
	//create physical address with the ppn stored in Page table entry
	pa = (pte_t *)((phys_mem_base + ((unsigned long)*PT_entry*PGSIZE)) + offset*sizeof(pte_t));
	
	//add the new translation in TLB
	add_TLB(va,(void*)PT_entry);
	
	return pa;

    //If translation not successful, then return NULL
    //return NULL; 
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    		
    
    	unsigned long v_addr = (unsigned long)va;
    	unsigned long p_addr = (unsigned long)pa;
    	
    	//extract offset from the virtual address
    	unsigned long mask = (1 << offset_bits) - 1;
	unsigned long offset = v_addr & mask;
	
	//check TLB if the translation already exists, as its the first time its a mandatory miss 
	pte_t *tlb_entry = check_TLB(va);
	if(tlb_entry == (pte_t*)(p_addr<<offset_bits)){
	
		return 0;
	}
	
	
	//extract PD_index Or 1st level page table offset  from va
	unsigned long PD_index = v_addr >> (level2_bits + offset_bits);
	
	//extract PT_index or second level page table offset from va
	unsigned long PT_index = v_addr >> offset_bits;
	mask = (1 << (level2_bits)) -1 ;
	PT_index = PT_index & mask;
	
	//calculate virtual and physical page numbers
	unsigned long vpn = v_addr >> offset_bits;
	unsigned long ppn = p_addr;
	

	
	
	//Find the Page directory enrty
	pde_t *PD_entry = pgdir + PD_index;
	
	//return if there is no data in the entry
	if((unsigned long)*PD_entry==0){
		
		//create a new inner page table for PD_entry
		int pages[1];
		if(get_next_avail_p(1, pages) == -1){
			printf("get_next_avail_p failed\n");
		}
		current_page_table_index = pages[0];
	
		
	}
	//store the physical page number of the 2nd level page table in the page directory entry
	*PD_entry = (pde_t)current_page_table_index;
	
	//form the address of the page table entry with the ppn of the current page table being filled
	pte_t *PT_entry = phys_mem_base + (current_page_table_index*PGSIZE) + PT_index*sizeof(pte_t);
	
	//store the physical page number in the 2nd level page table entry
	*PT_entry = ppn;
	
	//add the translation in the TLB
	add_TLB(va,(void*)PT_entry);

	

    return 0;
}


/*Function that gets the next available page
*/
int get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page
    int j = 0;
    int flag = 0;
    //check for contiguous free virtual pages
	for(int i=0; i<v_count; i++){
		if(get_bit_at_index(v_bitmap, i)==0){
			j++;
			if(flag == 0){
			
				//first index or page number of the continuous virtual pages
				flag = i;
			}
			
		}
		else{
			flag = 0;
		}
		
		
		if(j>=num_pages && flag != 0  ){
			for(int k = flag; k< (num_pages + flag) ; k++){
				//marking the pages to be used
				set_bit_at_index(v_bitmap, k);
			}
			//return start index or start page number
			return flag;
		}
	}
	return -1;
    
    
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */
     

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */
    
    pthread_mutex_lock(&mutex);
     
     //check if the physical memory was allocated 
     if(!phys_mem_allocated){
     	set_physical_mem();
     }
    
    //calculate num_pages
    unsigned int num_pages = (num_bytes+PGSIZE-1)/PGSIZE;  //ceil(num_bytes/PGSIZE)
    
   
    
    //get available virtual pages
    int va_start_index = get_next_avail(num_pages);
    
    //checking if the virtual pages are already filled
    if(va_start_index==-1){
    	printf("Virtual pages exhausted\n");
    	pthread_mutex_unlock(&mutex);
    	return NULL;
    }
    
    //get available physical pages
    int pa_index[num_pages];
    
    //check if the physical pages are already filled
    if(get_next_avail_p(num_pages, pa_index)==-1){
    	printf("Physical pages exhausted\n");
    	pthread_mutex_unlock(&mutex);
    	return NULL;
    }
    
    //for each entry in physical page array
    for(int i=0; i<num_pages; i++){
    	//create va
    	unsigned long va;
    	va = (unsigned long)(va_start_index+i << offset_bits);
    	
    	
    	//*IMPORTANT* pa is Physical page number in this context
    	unsigned long pa;
    	
    	pa = pa_index[i];
    	
    	//call page_map iteratively we pass the virtual address and ppn to the pagemap
    	if(page_map(page_dir, (void *)va, (void *)pa)==-1){
    		printf("Page fault\n");
    		pthread_mutex_unlock(&mutex);
    		return NULL;
    	}
    	
    }
    
    //return va of the first page
    unsigned long va_start = (unsigned long)(va_start_index << offset_bits);
   
	pthread_mutex_unlock(&mutex);
    return (void *)va_start;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {

    /* Part 1: Free the page table 
     starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
     pthread_mutex_lock(&mutex);
     
     //ceil(num_bytes/PGSIZE) calculated number of pages required 
     unsigned int num_pages = (size+PGSIZE-1)/PGSIZE;
     
     //calculate page number for our virtual address
     unsigned long v_start_index = (unsigned long)va >> offset_bits;
     int check = 0;
     
     //check the continuous virtual pages if they are filled 
     for(int i = v_start_index ; i < (v_start_index + num_pages); i ++){
     
     	if(get_bit_at_index(v_bitmap,i)==0)
     	{
     		pthread_mutex_unlock(&mutex);
     		return;
     	}
     
     }
     
     //reset bit for each virtual page and its corresponding physical page
     for(int i = v_start_index ; i < (v_start_index + num_pages); i ++){
     	
     		void* v_address = (void *)(i << offset_bits);
     		unsigned long p_address = (unsigned long)translate(page_dir,v_address);
     		unsigned long ppn = p_address >> offset_bits;
     		reset_bit_at_index(v_bitmap,i);
     		reset_bit_at_index(p_bitmap,ppn);
     }
     
     pthread_mutex_unlock(&mutex);
     
     //This is temp
     //free(phys_mem_base);
    
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
int put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
     	pthread_mutex_lock(&mutex);
     	
	//convert all the pointers to char type to copy the data byte by byte
	char* value = (char *)val;
	char* p_addr = (char *)translate(page_dir,va);
	char* v_addr = (char *)va;

	// calculate the last virtual address
	char* last_addr = v_addr + size;
	
	//calculate the page number of starting virtual address and ending virtual address
	unsigned long start_page = (unsigned long)v_addr >>offset_bits;
	unsigned long end_page = (unsigned long)last_addr >>offset_bits;
	
	//check if the virtual pages are filled
	for(int i = start_page; i <= end_page; i ++){
		if(get_bit_at_index(v_bitmap,i)==0)
     		{
     			pthread_mutex_unlock(&mutex);
     			return -1;
     		}
     
	}
	
	
	for(int i = 0; i <size; i ++){
	
		//copy the value from value pointer address to the physical address and increment to next byte 
		*p_addr = *value;
		p_addr +=1;
		value +=1;
		v_addr +=1;
		
		//if the offset of virtual page becomes zero it indicates we need to calculate the physical page again
		unsigned long var = (unsigned long)v_addr;
    		unsigned long mask = (1 << offset_bits) - 1;
		unsigned long offset = var & mask;
		if(offset ==0){
			p_addr = (char *)translate(page_dir,(void *)v_addr);
		}
		
	}
	pthread_mutex_unlock(&mutex);
	return 0;

    

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
    	pthread_mutex_lock(&mutex);
    	
    	//convert all the pointers to char type to copy the data byte by byte
    	char* value = (char *)val;
	char* p_addr = (char *)translate(page_dir,va);
	char* v_addr = (char *)va;
	
	// calculate the last virtual address
	char* last_addr = v_addr + size;
	
	//calculate the page number of starting virtual address and ending virtual address
	unsigned long start_page = (unsigned long)v_addr >>offset_bits;
	unsigned long end_page = (unsigned long)last_addr >>offset_bits;
	
	//check if the virtual pages are filled
	for(int i = start_page; i <= end_page; i ++){
		if(get_bit_at_index(v_bitmap,i)==0)
     		{
     			pthread_mutex_unlock(&mutex);
     			return;
     		}
     
	}
	
	
	for(int i = 0; i <size; i ++){
	
		//copy the data from to the value pointer address and increment to next byte 
		*value = *p_addr;
		p_addr +=1;
		value +=1;
		v_addr +=1;
		
		//if the offset of virtual page becomes zero it indicates we need to calculate the physical page again
		unsigned long var = (unsigned long)v_addr;
    		unsigned long mask = (1 << offset_bits) - 1;
		unsigned long offset = var & mask;
		if(offset ==0){
			p_addr = (char *)translate(page_dir,(void *)v_addr);
		}
		
	}
		
	pthread_mutex_unlock(&mutex);  


}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}




//MY Functions
/* 
 * Function 2: SETTING A BIT AT AN INDEX 
 * Function to set a bit at "index" bitmap
 * Consider bit-indexing starting from 0
 */
static void set_bit_at_index(char *bitmap, int index)
{
    //Implement your code here

    //initializing the bit mask	
    unsigned char temp = 1;

    //Finding which byte the bit lies in
    int i = index/8;

    /*index%8 gives the location of the bit to be set within byte i
     * left shift mask by index%8*/
   
    temp <<= index%8;
    
    // Apply the bit mask
    bitmap[i] |= temp;

    return;
}




/* 
 * Function 3: GETTING A BIT AT AN INDEX 
 * Function to get a bit at "index"
 * Consider bit-indexing starting from 0
 */
static int get_bit_at_index(char *bitmap, int index)
{
    // Get to the location in the character bitmap array
    unsigned char temp = 1;
    int i = index/8;

    temp <<= index%8;

    // Apply the bit mask
    temp &= bitmap[i];

    /*If the value of temp is greater than 0
    *  then it means the bit was set */

    return temp >>= index%8;

}

static void reset_bit_at_index(char *bitmap, int index)
{
    //Implement your code here

    //initializing the bit mask	
    unsigned char temp = 1;

    //Finding which byte the bit lies in
    int i = index/8;
	
    /*index%8 gives the location of the bit to be set within byte i
     * left shift mask by index%8*/
   
    temp <<= index%8;
    
    // Apply the bit mask
    bitmap[i] &= -temp;

    return;
}

static int get_next_avail_p(int num_pages, int a[]){
	int j = 0;
	//check physical bitmap for free pages
	for(int i=0; i<p_count; i++){
		if(get_bit_at_index(p_bitmap, i)==0){
			// filling the passed array with the ppns of free pages
			a[j] = i;
			j++;
		}
		if(j>=num_pages){
			for(int k = 0; k< num_pages ; k++)
			{
				//marking the pages to be used 
				set_bit_at_index(p_bitmap, a[k]);
			}
			return 0;
		}
	}
	return -1;
}





