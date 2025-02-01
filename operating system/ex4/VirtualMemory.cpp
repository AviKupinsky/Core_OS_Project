#include "VirtualMemory.h"
#include "PhysicalMemory.h"


#define INIT 0
#define ROOT_LEVEL 0
#define CLEAR_VAL 0
#define ROOT_ADDRESS 0
#define FIRST_INDEX 0
#define ONE_LONG_LONG 1LL
#define FAIL 0
#define SUCCESS 1
#define SAVING 4
#define FRAME_INDEX 3
#define ROW_NUMBER 2
#define PARENT 1
#define PAGE_INDEX 0

/**
 * Gets the number of bits that represent how many of bits of the virtual address are in the preset for determining
 * which child of the root table to traverse down to.
 */
#define ROOT_WIDTH ((VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH == 0) ? OFFSET_WIDTH : (VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH))


/**
 * Gets offset of the virtual address (address).
 */
#define OFFSET(address) ((address & (PAGE_SIZE - 1)))


/**
 * Gets the page number representation in the virtual address (address).
 */
#define NUM_OF_PAGES(address) ((address & (~(PAGE_SIZE - 1))) >> OFFSET_WIDTH)


/**
 * Gets the OFFSET_WIDTH last bits of index.
 */
#define GET_OFFSET_WIDTH_BITS(index) (index) & (PAGE_SIZE - 1)


/**
 * Gets the distance between a and b (|a-b|).
 */
#define DISTANCE(a, b) a > b ? a - b : b - a


/**
 * Gets the circular distance between a and b (min{NUM_PAGES - |a-b|, |a-b|}).
 */
#define CIRCULAR_DISTANCE(distance) (NUM_PAGES - distance < distance) ? NUM_PAGES - distance : distance


/**
 * Gets the number of children they have.
 */
#define CHILDREN(frame_index) (frame_index == ROOT_ADDRESS) ? ONE_LONG_LONG << ROOT_WIDTH : PAGE_SIZE


/**
 * Gets max frame there is we can have.
 */
#define MAX_FRAME(max_frame_index, frame_index) (max_frame_index < frame_index + 1) ? frame_index + 1 : max_frame_index


/**
 * Gets the max tree level (TABLES_DEPTH - 1)
 */
#define MAX_TREE_LEVEL (TABLES_DEPTH - 1)


/**
 * Adds one to a.
 */
#define ADD_ONE(a) (a + 1)


/**
 * Shifts address by OFFSET_WIDTH to the right.
 */
#define SHIFT_BY_OFFSET_RIGHT(address) (address >> OFFSET_WIDTH)


/**
 * Shifts address by OFFSET_WIDTH to the left.
 */
#define SHIFT_BY_OFFSET_LEFT(address) (address << OFFSET_WIDTH)


/**
 * Checks if this frame is empty.
 * @param frame_index - the frame index.
 * @return true if frame is empty and false otherwise.
 */
bool check_frame_empty(uint64_t frame_index){
    int check = ROOT_ADDRESS;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMread(frame_index * PAGE_SIZE + i, &check);
        if (check != ROOT_ADDRESS)
        {
            return false;
        }
    }
    return true;
}


/**
 * Clears the frame in frame index.
 * @param frame_index - the frame index.
 */
void clear_table(uint64_t frame_index){
    for (uint64_t i  = 0; i < PAGE_SIZE ; ++i) {
        PMwrite(frame_index * PAGE_SIZE + i,CLEAR_VAL);
    }
}


/**
 * Converts a tree route stored in an array to its decimal representation.
 * @param array_depth - the route stored in array.
 * @return the number in decimal representation.
 */
uint64_t convert_tree_route_to_decimal(const uint64_t *array_depth){
    uint64_t temp = ROOT_ADDRESS;
    for (int i = 0; i < TABLES_DEPTH; ++i) {
        temp = SHIFT_BY_OFFSET_LEFT(temp) | array_depth[i];
    }
    return temp;
}


/**
 * Converts a decimal number to its representation of a tree route in an array.
 * @param page_index - the number (page index) to convert.
 * @param array_depth - an array that will store the output (tree route).
 */
void get_bites_array(uint64_t page_index, uint64_t *array_depth){
    int count = MAX_TREE_LEVEL;
    while (page_index > ROOT_ADDRESS) {
        array_depth[count] = GET_OFFSET_WIDTH_BITS(page_index);
        page_index = SHIFT_BY_OFFSET_RIGHT(page_index);
        count--;
    }
}


/**
 * Finds the max value of the circular distance between the current page and the page that needs to be swapped in
 * (recursion base case).
 * @param saving - an array representing all the elements will need for calling PMevict.
 * @param page_route - an array representing the route in the table tree that leads to the page to evict.
 * @param max_value - the current max value of.
 * @param page_swapped - the page index of the page to be restored.
 * @param frame_index - the current root frame (table) for the algorithm.
 * @param row_number - the current row number.
 * @param parent - the parent frame of page_to_evict.
 */
void finding_max_value(uint64_t *saving, uint64_t *page_route, uint64_t &max_value, uint64_t page_swapped,
                       word_t frame_index, uint64_t row_number, uint64_t parent){
    uint64_t this_value;
    uint64_t page_index = convert_tree_route_to_decimal(page_route);
    uint64_t distance = DISTANCE(page_swapped, page_index);
    this_value = CIRCULAR_DISTANCE(distance);
    if (this_value > max_value) {
        saving[PAGE_INDEX] =  page_index;
        saving[PARENT] =  parent;
        saving[ROW_NUMBER] =  row_number;
        saving[FRAME_INDEX] =  frame_index;
        max_value = this_value;
    }
}


/**
 * DFS implementation for finding the frame index of page to evict. maximum value of
 * min{NUM_PAGES - |page_swapped - page_to_evict_index|, |page_swapped - page_to_evict_index|} is achieved with the page
 * stored in the frame page_to_evict - the page to evict.
 * @param frame_index - the current root frame (table) for the algorithm.
 * @param page_swapped - the page index of the page to be restored.
 * @param tree_level - the current tree level.
 * @param max_value - the current max value of.
 * min{NUM_PAGES - |page_swapped - page_to_evict_index|, |page_swapped - page_to_evict_index|}.
 * @param tree_level - the current tree level.
 * @param saving - an array representing all the elements will need for calling PMevict.
 * @param page_route - an array representing the route in the table tree that leads to the page to evict.
 * @param row_number - the current row number.
 * @param parent - the parent frame of page_to_evict.
 */
void dfs_max_frame(word_t frame_index, uint64_t page_swapped, int tree_level,uint64_t &max_value, uint64_t *saving,
                   uint64_t *page_route, uint64_t row_number, uint64_t parent){
    if (tree_level == TABLES_DEPTH) {
        finding_max_value(saving, page_route, max_value, page_swapped, frame_index, row_number, parent);
        return;
    }
    uint64_t num_of_children = CHILDREN(frame_index);
    for (uint64_t i=0; i < num_of_children; i++){
        word_t this_frame_index = FIRST_INDEX;
        PMread(frame_index * PAGE_SIZE + i, &this_frame_index);
        if (this_frame_index != FIRST_INDEX) {
            page_route[tree_level] = i;
            dfs_max_frame(this_frame_index, page_swapped, ADD_ONE(tree_level),
                          max_value, saving, page_route, i, frame_index); // recursive call
        }
    }
    if (frame_index == INIT){
        PMevict(saving[FRAME_INDEX], saving[PAGE_INDEX]);
        PMwrite(saving[PARENT] * PAGE_SIZE + saving[ROW_NUMBER], CLEAR_VAL);
    }
}


/**
 * DFS implementation for finding the max frame index in use or a frame index of an empty frame.
 * @param frame_index - the current root frame (table) for the algorithm.
 * @param max_frame_index - the current max frame index.
 * @param parent - parent of current frame.
 * @param row_in_parent - the row in the parent page where the reference to frame_index is stored.
 * @param finished_empty - will store the index of an empty frame if exist and 0 otherwise.
 * @param tree_level - the current tree level.
 * @param currently_working_on - a frame index of the frame we are currently working on (it will point to the output of
 * the algorithm) - won't count as an empty frame.
 * @return the max used frame index + 1.
 */
uint64_t find_free_frame(uint64_t frame_index, uint64_t max_frame_index, uint64_t parent, uint64_t row_in_parent,
                         uint64_t& finished_empty, int tree_level, uint64_t currently_working_on){
    if (tree_level == TABLES_DEPTH){
        // Base of recursion:
        return MAX_FRAME(max_frame_index, frame_index);
    }
    uint64_t num_of_children = CHILDREN(frame_index);
    for (uint64_t i = 0; i < num_of_children; i++){
        word_t this_frame_index = FIRST_INDEX;
        PMread(frame_index * PAGE_SIZE + i, &this_frame_index);
        if (this_frame_index != FIRST_INDEX) {
            max_frame_index = find_free_frame(this_frame_index, max_frame_index, frame_index, i, finished_empty,
                                              ADD_ONE(tree_level), currently_working_on); // recursive call
        }
        max_frame_index = MAX_FRAME(max_frame_index, frame_index);
    }
    // in this case we check if there is an empty frame
    if (frame_index != FIRST_INDEX && check_frame_empty(frame_index)
    && frame_index != currently_working_on && !finished_empty){
        PMwrite(parent * PAGE_SIZE + row_in_parent, CLEAR_VAL);
        finished_empty = frame_index;
    }
    return max_frame_index;
}


/**
 * Gets an empty frame or evicts a frame and writes a reference to it in the parent table.
 * @param current_frame - the frame that we are currently at
 * @param curr_parent - the frame index of the frame that will store the reference to 'current_frame'.
 * @return an available frame.
 */
uint64_t get_frame(uint64_t current_frame, uint64_t curr_parent){
    uint64_t empty_frame = ROOT_ADDRESS; // will hold an empty frame index if exists.
    uint64_t check = find_free_frame(FIRST_INDEX, FIRST_INDEX, ROOT_ADDRESS, FIRST_INDEX, empty_frame, ROOT_LEVEL,
                                     curr_parent);
    // if there is an empty frame
    if (empty_frame != FIRST_INDEX){
        check = empty_frame;
    }
    // if the frame index we got is bigger then the maximum number of frames, then evict a frame:
    if(check >= NUM_FRAMES){
        // creating all of these parameters in order to pass them to 'dfs_max_frame'.
        uint64_t page_route[TABLES_DEPTH];
        for (uint64_t & j : page_route) {
            j = FIRST_INDEX;
        }
        uint64_t saving_data[SAVING];
        uint64_t max_value = INIT;
        dfs_max_frame(INIT, current_frame, ROOT_LEVEL, max_value, saving_data, page_route, INIT, INIT);
        check = saving_data[FRAME_INDEX];
    }
    return check;
}


/**
 * Gets the frame index that stores the page with index 'page_index'.
 * @param page_index the page index of the page.
 * @return frame index that stores the page with index 'page_index'.
 */
uint64_t get_physical_address(uint64_t page_index){
    uint64_t curr_parent = ROOT_ADDRESS;
    word_t curr_child = ROOT_ADDRESS;
    uint64_t get_page_depth[TABLES_DEPTH];
    for (uint64_t & j : get_page_depth) {
        j = FIRST_INDEX;
    }
    get_bites_array(page_index, get_page_depth);
    bool restored = false;
    for (uint64_t i = 0; i < TABLES_DEPTH; ++i) {
        PMread(curr_parent * PAGE_SIZE + get_page_depth[i], &curr_child);
        if (curr_child == ROOT_ADDRESS){
            if (i == MAX_TREE_LEVEL) {
                restored = true;
            }
            curr_child = (word_t) get_frame(page_index, curr_parent);
            if (i != MAX_TREE_LEVEL){
                clear_table(curr_child);
            }
            PMwrite(curr_parent * PAGE_SIZE + get_page_depth[i], curr_child);
        }
        curr_parent = curr_child;
    }
    if (restored){
        PMrestore(curr_child, page_index);
    }
    return curr_child;
}


/**
 * Initialize the virtual memory.
 */
void VMinitialize() {
    clear_table(FIRST_INDEX);
}


/** Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t *value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE){
        return FAIL;
    }
    uint64_t address = get_physical_address(NUM_OF_PAGES(virtualAddress));
    PMread(address * PAGE_SIZE + OFFSET(virtualAddress), value);
    return SUCCESS;
}


/** Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE){
        return FAIL;
    }
    uint64_t address = get_physical_address(NUM_OF_PAGES(virtualAddress));
    PMwrite(address * PAGE_SIZE + OFFSET(virtualAddress), value);
    return SUCCESS;
}
