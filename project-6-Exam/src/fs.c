#include "fs.h"
#include "fstypes.h"

#ifdef LINUX_SIM
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#endif /* LINUX_SIM */

#include "block.h"
#include "common.h"
#include "fs_error.h"
#include "inode.h"
#include "kernel.h"
#include "superblock.h"
#include "thread.h"
#include "util.h"

#define BITMAP_ENTRIES 256

static char inode_bmap[BITMAP_ENTRIES];
static char dblk_bmap[BITMAP_ENTRIES];

static int get_free_entry(unsigned char *bitmap);
static int free_bitmap_entry(int entry, unsigned char *bitmap);
static inode_t name2inode(char *name);
static blknum_t ino2blk(inode_t ino, int offset);
static blknum_t idx2blk(int index);

#define INODE_TABLE_ENTRIES 20
#define CEIL(x, y) ((x) / (y) + ((x) % (y) ? 1 : 0))
#define DISK_INODE_IN_BLOCK_MAX (int)(BLOCK_SIZE / sizeof(disk_inode_t))
#define DISK_INODE_MAX (int)(CEIL((BITMAP_ENTRIES), DISK_INODE_IN_BLOCK_MAX))

static mem_inode_t global_inode_table[INODE_TABLE_ENTRIES];
static mem_superblock_t super_block;
static int debug_counter = 0;

// Get a free inode
int get_table_entry() {
    // Store the table placement
    int table_placement = super_block.d_super.table_placement;
    int counter = 0;

    // Iterate through the inode table
    for (int i = 0; i < DISK_INODE_MAX; i++) {
        // Read inode table index: i
        disk_inode_t inode_table[DISK_INODE_IN_BLOCK_MAX];
        block_read_part(table_placement + i, 0, sizeof(disk_inode_t) * DISK_INODE_IN_BLOCK_MAX, &inode_table);
        // Iterate through the inode table index
        for (int j = 0; j < DISK_INODE_IN_BLOCK_MAX; j++) {
            // Check if the inode is free
            if (inode_table[j].nlinks == 0) {
                inode_table[j].nlinks = 1;
                super_block.d_super.ndata_blks++;
                // Write inode table back to disk
                block_modify(table_placement + i, sizeof(disk_inode_t) * j, sizeof(disk_inode_t), &inode_table[j]);
                block_modify(0, 0, sizeof(disk_inode_t), &super_block.d_super);
                // Check if we've reached the end of the inode table
                if (counter >= BITMAP_ENTRIES){
                    return FSE_BITMAP;
                }
                // Return the inode number
                else {
                    return counter;
                }
            } else {
                counter++;
            }
        }
    }
    return FSE_BITMAP;
}

// Initialize the disk inode table
void setup_disk_inode_table(){
    // Iterate through the inode table
    for (int i = 0; i < DISK_INODE_MAX; i++) {
        int current_inode_block = get_free_entry((unsigned char*)dblk_bmap);
        super_block.d_super.ndata_blks++;
        // Check if we're on the first block of the inode table and store as table placement
        if(i == 0){
            super_block.d_super.table_placement = current_inode_block;
        }

        disk_inode_t temp[DISK_INODE_IN_BLOCK_MAX];
        // Iterate through the inode table index
        for (int j = 0; j < DISK_INODE_IN_BLOCK_MAX; j++) {
            // Check if we've reached the end of the inode table
            if (super_block.d_super.ninodes >= BITMAP_ENTRIES) {
                break;
            }
            // Initialize the inode
            temp[j].current_size = 0;
            temp[j].nlinks = 0;
            temp[j].type = 0;
            for (int x = 0; x < INODE_NDIRECT; x++) {
                temp[j].direct[x] = 0;
            }
            super_block.d_super.ninodes++;
            get_free_entry((unsigned char*)inode_bmap);
        }
        block_modify(current_inode_block,  0, sizeof(disk_inode_t) * DISK_INODE_IN_BLOCK_MAX, &temp);
    }
    block_modify(0, 0, sizeof(disk_superblock_t), &super_block);
}

// Read inode table from disk and return the inode
disk_inode_t read_inode_table(int inode_num) {
    // Create inode table
    disk_inode_t inode_table[DISK_INODE_IN_BLOCK_MAX];

    // Calculate which block the inode is in
    int which_inode_table = (inode_num / DISK_INODE_IN_BLOCK_MAX);

    // Calculate which index in the inode table the inode is ink
    int inode_table_index = (inode_num % DISK_INODE_IN_BLOCK_MAX);

    // Read the inode table from disk
    block_read_part(super_block.d_super.table_placement + which_inode_table, inode_table_index*sizeof(disk_inode_t), sizeof(disk_inode_t), &inode_table[inode_table_index]);
    
    // Return the inode
    return inode_table[inode_table_index];
}

// Write inode to disk
void write_inode2table(int inode_num, disk_inode_t inode){
    disk_inode_t inode_table;

    // Calculate which block the inode is in
    int which_inode_table = (inode_num / DISK_INODE_IN_BLOCK_MAX);

    // Calculate which index in the inode table the inode is in
    int inode_table_index = (inode_num % DISK_INODE_IN_BLOCK_MAX);

    // Read the inode table from disk
    block_read_part(super_block.d_super.table_placement + which_inode_table, inode_table_index*sizeof(disk_inode_t), sizeof(disk_inode_t), &inode_table);

    // Write the inode to the inode table
    block_modify(super_block.d_super.table_placement + which_inode_table, inode_table_index*sizeof(disk_inode_t), sizeof(disk_inode_t), &inode);
}

/*
 * Exported functions.
 */
void fs_init(void) {
    block_init();

    // Check magic in superblock if there do not make, else make.
    block_read_part(0, 0, sizeof(disk_superblock_t), &super_block.d_super);

    // Check if the file system is initialized
    if (super_block.d_super.magic != 0x6969){
        fs_mkfs();
    }
    else {
        // Read the bitmap
        super_block.ibmap = super_block.d_super.bitmap_placement;
        super_block.dbmap = super_block.d_super.bitmap_placement;
        block_read_part(super_block.dbmap, 0, BITMAP_ENTRIES, (unsigned char*)dblk_bmap);
        block_read_part(super_block.ibmap, BITMAP_ENTRIES, BITMAP_ENTRIES, (unsigned char*)inode_bmap);
    }
    // Mount the filesystem
    fs_mount();


    // Initialize current running process file descriptor table
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        current_running->filedes[i].mode = MODE_UNUSED;
    }

    // Setup global inode table
    for (int i = 0; i < INODE_TABLE_ENTRIES; i++) {
        global_inode_table[i].open_count = 0;
        global_inode_table[i].dirty = 0;
        global_inode_table[i].pos = 0;
        global_inode_table[i].inode_num = -1;
        global_inode_table[i].d_inode.type = 0;
        global_inode_table[i].d_inode.nlinks = 0;
        global_inode_table[i].d_inode.current_size = 0;
        for (int j = 0; j < INODE_NDIRECT; j++) {
            global_inode_table[i].d_inode.direct[j] = 0;
        }
    }
}

/*
 * Make a new file system.
 * Argument: kernel size
 */
void fs_mkfs(void) {
    // Create Superblock
    super_block.d_super.max_filesize = (BLOCK_SIZE * INODE_NDIRECT);
    super_block.d_super.magic = 0x6969;
    super_block.d_super.ninodes = 0;
    super_block.d_super.ndata_blks = 0;
    super_block.d_super.max_filesize = (BLOCK_SIZE * INODE_NDIRECT);


    // Initialize inode bitmap and datablock bitmap
    for (int i = 0; i < BITMAP_ENTRIES; i++) {
        inode_bmap[i] = 0;
        dblk_bmap[i] = 0;
    }

    // Allocate first inode for root directory
    int super_block_entry = get_free_entry((unsigned char*)dblk_bmap);

    // Allocate second data block for the bitmap
    super_block.d_super.bitmap_placement = get_free_entry((unsigned char*)dblk_bmap);

    // Setup inode table and write to disk
    setup_disk_inode_table();
    // Setup root directory inode
    int current_inode = get_table_entry();
    disk_inode_t root_inode = read_inode_table(current_inode);
    root_inode.direct[0] = get_free_entry((unsigned char*)dblk_bmap);

    // Create root directory entries "." and ".."
    dirent_t root[DIRENTS_PER_BLK];
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        root[i].inode = (inode_t)0;
        bzero((char*)root[i].name, MAX_FILENAME_LEN);
        root[i].name[0] = '\0';
    }

    root[0].inode = current_inode;
    bzero((char*)root[0].name, MAX_FILENAME_LEN);
    strcpy(root[0].name, ".");

    root[1].inode = current_inode;
    bzero((char*)root[1].name, MAX_FILENAME_LEN);
    strcpy(root[1].name, "..");

    // Configure root directory inode
    root_inode.type = 2;
    root_inode.nlinks = 1;
    root_inode.current_size = sizeof(dirent_t) * 2;
    super_block.d_super.root_inode = current_inode;
    
    // Store the bitmap placement in the superblock
	super_block.ibmap = super_block.d_super.bitmap_placement;
    super_block.dbmap = super_block.d_super.bitmap_placement;

    // Write superblock to disk
    block_write(super_block_entry, &super_block.d_super);

    // Write root directory entries to disk
    block_write(root_inode.direct[0], &root);

    // Write root directory inode to disk
    write_inode2table(current_inode, root_inode);
}

// Mount the filesystem
void fs_mount(void) {
    current_running->cwd = super_block.d_super.root_inode;
}

// Update the bitmap
void fs_update_bitmap(void) {
    block_modify((int)super_block.dbmap, 0, BITMAP_ENTRIES, (unsigned char*)dblk_bmap);
    block_modify((int)super_block.ibmap, BITMAP_ENTRIES, BITMAP_ENTRIES, (unsigned char*)inode_bmap);
}

/* Extract every directory name out of a path. This consists of replacing every /
 * with '\0' (Taken from shell_sim.c and modified a bit)*/ 
int parse_path(char *path, char *argv[MAX_PATH_LEN], char buf[MAX_FILENAME_LEN * 2]) {
    char *s = path;
    int argc = 0;
    char *p = buf;  // pointer to the current position in buf

    // Handle the root directory as a special case
    if (*s == '/') {
        argv[argc++] = "/";
        s++;
    }

    while (*s != '\0') {
        argv[argc] = p;
        while (*s != '/' && *s != '\0') {
            *p++ = *s++;
        }
        *p++ = '\0';  // null terminate the directory/file name
        argc++;

        // Skip the '/' character
        if (*s == '/') {
            s++;
        }
    }
    return argc;
}

// Validate the name of a file or directory
int validate_name(char *name) {
    char banned_names[15][MAX_FILENAME_LEN] = {".", "..", "/", "", "\0", "*", "?", ":", "<", ">", "|", "\"", "\\"};
    for (int i = 0; i < 15; i++) {
        if (name[0] == banned_names[i][0]) {
            return FSE_INVALIDNAME;
        }
    }
    if (strlen(name) > MAX_FILENAME_LEN) {
        return FSE_NAMETOLONG;
    }
    return FSE_OK;
}

// Create a new inode
int create_inode(inode_t* inode_num, inode_t found_inode, int inode_type) {
    // Get a free inode
    *inode_num = get_table_entry();

    // Read inode from disk
    disk_inode_t current_inode = read_inode_table(*inode_num);

    // Give the inode a data block
    current_inode.direct[0] = get_free_entry((unsigned char*)dblk_bmap);
    int data_block = current_inode.direct[0];

    // Check if we were able to get a free inode and data block
    if (data_block == -1 || *inode_num == -1) {
        return FSE_BITMAP;
    }

    dirent_t dir[DIRENTS_PER_BLK];
    // Null out blocks
    for (int i = 0; i < DIRENTS_PER_BLK; i++) {
        dir[i].inode = (inode_t)0;
        bzero((char*)dir[i].name, MAX_FILENAME_LEN);
        dir[i].name[0] = '\0';
    }
    // Add "." and ".." entries
    if (inode_type == INTYPE_DIR) {
        dir[0].inode = *inode_num;
        bzero((char*)dir[0].name, MAX_FILENAME_LEN);
        strcpy(dir[0].name, ".");

        dir[1].inode = found_inode;
        bzero((char*)dir[1].name, MAX_FILENAME_LEN);
        strcpy(dir[1].name, "..");

        current_inode.type = INTYPE_DIR;
        current_inode.nlinks = 1;
        current_inode.current_size = sizeof(dirent_t) * 2;
    }
    // Special file case
    else if (inode_type == INTYPE_FILE) {
        current_inode.type = INTYPE_FILE;
        current_inode.nlinks = 1;
        current_inode.current_size = 0;
    }
    // Modify and write inode to disk
    block_modify(data_block, 0, sizeof(dirent_t) * DIRENTS_PER_BLK, &dir);
    write_inode2table(*inode_num, current_inode);
    return FSE_OK;
}

// Remove inode
int remove_inode(inode_t inode_num) {
    // Read inode from disk
    disk_inode_t active_inode = read_inode_table(inode_num);

    // Check if inode exists
    if (active_inode.type == MODE_UNUSED) {
        return FSE_NOTEXIST; // Inode does not exist
    }

    // Clear blocks
    for (int i = 0; i < INODE_NDIRECT; i++) {
        if (active_inode.direct[i] != 0) {
            char buf[BLOCK_SIZE];
            bzero((char*)&buf, BLOCK_SIZE);
            block_write(active_inode.direct[i], &buf);
            free_bitmap_entry(active_inode.direct[i], (unsigned char*)dblk_bmap);
            active_inode.direct[i] = 0;
        }
    }

    // Free inode entry
    free_bitmap_entry(inode_num, (unsigned char*)inode_bmap);
    bzero((char*)&active_inode, sizeof(disk_inode_t));

    // Write back inode to inode table
    write_inode2table(inode_num, active_inode);
    return FSE_OK; // Inode removed successfully
}

// Creater directory entry in parent directory
int create_directory_entry(inode_t parent_inode_num, char *name, inode_t new_inode_num) {
    // Read parent inode from table
    disk_inode_t parent_inode = read_inode_table(parent_inode_num);
    dirent_t dir[DIRENTS_PER_BLK];
    int free_entry_found = 0;
    blknum_t current_block = 0;
    
    // Check existing blocks for free entry
    for (int i = 0; i < INODE_NDIRECT; i++) {
        current_block = parent_inode.direct[i];
        if (current_block == 0) {
            continue;
        }
        // Read directory entries from current parent block
        block_read_part(current_block, 0, sizeof(dirent_t) * DIRENTS_PER_BLK, &dir);
        // Find free entry
        for (int j = 0; j < DIRENTS_PER_BLK; j++) {
            // If free entry found, write new entry to disk
            if (dir[j].name[0] == '\0') {
                dir[j].inode = new_inode_num;
                strcpy(dir[j].name, name);
                block_modify(current_block, j * sizeof(dirent_t), sizeof(dirent_t), &dir[j]);
                parent_inode.current_size += sizeof(dirent_t);
                free_entry_found = 1;
                break;
            }
        }
        if (free_entry_found) {
            break;
        }
    }
    
    // If no free entry found, allocate a new block
    if (!free_entry_found) {
        // Check if parent directory is full
        if (parent_inode.current_size >= (INODE_NDIRECT * BLOCK_SIZE)) {
            return FSE_INVALIDBLOCK;
        }
        // Allocate new block
        current_block = get_free_entry((unsigned char*)dblk_bmap);
        if (current_block == -1) {
            return FSE_INVALIDBLOCK;
        }
        // Update parent inode
        parent_inode.current_size += sizeof(dirent_t);
        parent_inode.direct[parent_inode.current_size / BLOCK_SIZE] = current_block;
        dir[0].inode = new_inode_num;
        strcpy(dir[0].name, name);
        block_modify(current_block, 0, sizeof(dirent_t), &dir[0]);
    }
    
    write_inode2table(parent_inode_num, parent_inode);
    return FSE_OK;
}

// Remove directory entry from parent directory
int remove_directory_entry(inode_t parent_inode_num, char* filename ) {
    dirent_t last_dir;
    blknum_t last_block = -1;
    int last_index = -1;

    // Read parent inode
    disk_inode_t parent_inode = read_inode_table(parent_inode_num);
    // Find the last entry first
    int found = 0;
    for (int i = 0; i < INODE_NDIRECT; i++) {
        blknum_t current_block = parent_inode.direct[i];
        dirent_t dir[DIRENTS_PER_BLK];
        // Read directory entries from current parent block
        block_read_part(current_block, 0, sizeof(dirent_t) * DIRENTS_PER_BLK, &dir);
        // Search for the last entry
        for (int j = 2; j < DIRENTS_PER_BLK; j++) {
            // If entry is not empty, store it as the last entry
            if (dir[j].inode != (inode_t)0) {
                last_dir = dir[j];
                last_block = current_block;
                last_index = j;
            } else {
                found = 1;
                break;
            }
        }
        if (found) {
            break;
        }
    }

    // Remove the found entry and replace it with the last entry
    found = 0;
    // Find the entry we are trying to remove
    for (int i = 0; i < INODE_NDIRECT; i++) {
        blknum_t current_block = parent_inode.direct[i];
        dirent_t dir[DIRENTS_PER_BLK];
        block_read_part(current_block, 0, sizeof(dirent_t) * DIRENTS_PER_BLK, &dir);
		
		int j = 2;
        for (; j < DIRENTS_PER_BLK; j++) {
            // Check if the entry is the one we are trying to remove
            if (same_string(dir[j].name, filename)) {
                // Check if the entry is the last entry
                if (current_block == last_block && j == last_index) {
                    // This is the last entry, just clear it
					dir[j].inode = (inode_t)0;
					dir[j].name[0] = '\0';
                } else {
                    // Replace with the last entry
                    dir[j] = last_dir;
                }
                block_modify(current_block, sizeof(dirent_t) * j, sizeof(dirent_t), &dir[j]);
                found = 1;
                break;
            }
        }
        if (found) {
            // Remove the last entry now
            if (current_block != last_block || j != last_index) {
                // Only remove the last entry if it's not the same as the one just removed
                dirent_t empty_dir;
				bzero((char*)&empty_dir, sizeof(dirent_t));
                block_modify(last_block, sizeof(dirent_t) * last_index, sizeof(dirent_t), &empty_dir);
            }
            parent_inode.current_size -= sizeof(dirent_t);
            write_inode2table(parent_inode_num, parent_inode);
            return FSE_OK;
        }
    }
    return FSE_ERROR;
}

int fs_open(const char *filename, int mode) {
    int inode_num = name2inode((char*)filename);
    int global_index = 0;

    // If user is trying to open a file that does not exist, create it if the mode is write
    if (mode == (MODE_WRONLY | MODE_CREAT | MODE_TRUNC)) {
        // If the file we are trying to write to is a directory, return error
		if (inode_num > 0) {
            disk_inode_t temp = read_inode_table(inode_num);
            if (temp.type == INTYPE_DIR) {
                return FSE_FILEISDIR;
            }
        }
        // If the file does not exist, create it
		else  {
			int ev = fs_mkfile((char*)filename);
			if (ev > 0) {
				inode_num = ev;
			}
			else {
				return ev;
			}
		}
    }
    if (inode_num < 0) {
        return FSE_NOTEXIST;
    }

    // Find free entry in global inode table
    int found_slot = -1;
    for (global_index = 0; global_index < INODE_TABLE_ENTRIES; global_index++) {
        // Check if file is already open
        if (global_inode_table[global_index].open_count > 0) {
            // Check if file is already open
            if (global_inode_table[global_index].inode_num == inode_num) {
                global_inode_table[global_index].open_count++;
                found_slot = global_index;
                break;
            }
        }
        // Check if file is not open
        else if (global_inode_table[global_index].open_count == 0) {
            disk_inode_t temp = read_inode_table(inode_num);
            bcopy((char*)&temp, (char*)&global_inode_table[global_index].d_inode, sizeof(disk_inode_t));
            // Fill in global inode table entry
            global_inode_table[global_index].inode_num = inode_num;
            global_inode_table[global_index].open_count++;
            global_inode_table[global_index].dirty = 0;
            global_inode_table[global_index].pos = 0;
            global_inode_table[global_index].pos_block = 0;
            found_slot = global_index;
            break;
        }
    }
    if (found_slot == -1) {
        return FSE_ERROR;
    }

    // Check if file is a regular file
    if (global_inode_table[found_slot].d_inode.type == INTYPE_FILE) {
        for (int i = 0; i < MAX_OPEN_FILES; i++) {
            // Check if file descriptor is open
            if (current_running->filedes[i].mode == MODE_UNUSED) {
                // Fill in file descriptor table entry
                current_running->filedes[i].idx = found_slot;
                current_running->filedes[i].mode = mode;
                if (mode != MODE_RDONLY) {
                    global_inode_table[found_slot].pos = global_inode_table[found_slot].d_inode.current_size;
                }
                return i;
            } else {
                return FSE_NOMOREFDTE;
            }
        }
    }
    // Check if file is a directory
    else if (global_inode_table[found_slot].d_inode.type == INTYPE_DIR) {
        // Check if write mode is enabled and return error if it is enabled for a directory
        if (mode == (MODE_WRONLY | MODE_CREAT | MODE_TRUNC)) {
            return FSE_ERROR;
        }
        for (int i = 0; i < MAX_OPEN_FILES; i++) {
            // Check if file descriptor is open
            if (current_running->filedes[i].mode == MODE_UNUSED) {
                current_running->filedes[i].idx = found_slot;
                current_running->filedes[i].mode = mode;
                return i;
            }
        }
        return FSE_NOMOREFDTE;
    }
    return FSE_ERROR;
}

int fs_close(int fd) {
    // Check if we are trying to close a file descriptor that is not open
    if (current_running->filedes[fd].mode == MODE_UNUSED) {
        return FSE_ERROR;
    }
    // Get inode from global inode table
    mem_inode_t* active_inode = &global_inode_table[current_running->filedes[fd].idx];
    current_running->filedes[fd].idx = -1;
    current_running->filedes[fd].mode = MODE_UNUSED;

    // Check if file is open by any other processes
    if (active_inode->open_count > 1) {
        active_inode->open_count--;
    }
    else {
        // Check if file is dirty and write to disk if it is
        if (active_inode->dirty == 1) {
            write_inode2table(active_inode->inode_num, active_inode->d_inode);
            active_inode->dirty = 0;
        }
        // Clear inode entry in global inode table
        bzero((char*)&active_inode->d_inode, sizeof(disk_inode_t));
        active_inode->inode_num = 0;
        active_inode->pos = 0;
        active_inode->open_count = 0;
    }
    return 0;
}

int fs_read(int fd, char *buffer, int size) {
    // Check if file descriptor is open
    if (current_running->filedes[fd].mode == MODE_UNUSED) {
        return FSE_ERROR;
    }
    // Get inode from global inode table
    mem_inode_t* active_inode = &global_inode_table[current_running->filedes[fd].idx];
    if (active_inode->d_inode.type == INTYPE_DIR) {
        // Check if we're trying to read more than a directory
        if (size > (int)sizeof(dirent_t)) {
            return FSE_OK;
        }
        // Check if file is a directory
        if (active_inode->d_inode.direct[active_inode->pos_block] != 0) {
            dirent_t dir;
            // Simply here to be "used" has no effect on the code
            fs_lseek(fd, 0, SEEK_CUR);
            block_read_part(active_inode->d_inode.direct[active_inode->pos_block], active_inode->pos % (sizeof(dirent_t) * DIRENTS_PER_BLK), size, &dir);
            // If the directory entry is not empty, copy it to the buffer
            if (dir.name[0] != '\0') {
                active_inode->pos += size;
                if (active_inode->pos % (sizeof(dirent_t) * DIRENTS_PER_BLK) == 0) {
                    active_inode->pos_block++;
                }
                bcopy((char *)&dir, buffer, size);
                return 1;
            }
        }
        return 0;
    }

    // Check if file is a regular file
    else if (active_inode->d_inode.type == INTYPE_FILE) {
        // Check if file is open for reading or reading and writing
        if (current_running->filedes[fd].mode == (MODE_RDONLY)) {
            // Check if file is empty
            if (active_inode->d_inode.current_size == 0) {
                return FSE_OK;
            }
            else {
                // Get the first available block
                blknum_t active_block_idx = active_inode->d_inode.direct[active_inode->pos_block];
                // Set read position to the beginning of the block
                fs_lseek(fd, 0, SEEK_SET);
                if (active_block_idx == 0) {
                    return 0;
                }
                // Read the data from the block
                block_read(active_block_idx, buffer);

                // Update file descriptor
                active_inode->pos += size;
                active_inode->pos_block++;

                // Return the number of bytes read
                return size;
                }
            }
        }
    return FSE_ERROR;
}

int fs_write(int fd, char *buffer, int size) {
    // Get inode from global inode table
    mem_inode_t* active_inode = &global_inode_table[current_running->filedes[fd].idx];

    // Check if file is open, read only, or a directory
    if ((current_running->filedes[fd].mode == MODE_UNUSED) ||
        (current_running->filedes[fd].mode == MODE_RDONLY) ||
        (active_inode->d_inode.type == INTYPE_DIR)) {
        return FSE_ERROR;
    }

    // Calculate which block to write to
    int block_num = active_inode->pos / BLOCK_SIZE;
    int rest = 0;
    // Calculate the remaining space in the current block
    int space_left_in_block = BLOCK_SIZE - (active_inode->pos % BLOCK_SIZE);
    if (size <= space_left_in_block) {
        // If the data fits into the remaining space of the current block, write it all
        rest = size;
    } else {
        // Otherwise, write only as much as the remaining space in the current block allows
        rest = space_left_in_block;
    }

    // Write the data to the current block
    blknum_t* active_block_idx = &global_inode_table[current_running->filedes[fd].idx].d_inode.direct[block_num];

    // Check if block is already allocated
    if (active_inode->d_inode.current_size == 0) {
        // Allocate a new block
        if (block_num >= INODE_NDIRECT) {
            return FSE_INVALIDBLOCK;
        }
        // Allocate a new block
        active_inode->d_inode.direct[block_num] = get_free_entry((unsigned char*)dblk_bmap);
        super_block.d_super.ndata_blks++;
        block_modify(0, 0, sizeof(disk_superblock_t), &super_block.d_super);
        if (active_inode->d_inode.direct[block_num] == -1) {
            return FSE_BITMAP;
        }
    }
    // Write the data to buffer and update inode
    block_modify(*active_block_idx, active_inode->pos % BLOCK_SIZE, rest, buffer);
    active_inode->d_inode.current_size += rest;
    active_inode->dirty = 1;
    active_inode->pos += rest;
    size -= rest;

    // If there's data left, write it to a new block
    if (size > 0) {

        // Check if block_num is within the direct block range or if (current size + size) > max_filesize, return error
        if ((block_num >= INODE_NDIRECT) || (active_inode->d_inode.current_size + size) > super_block.d_super.max_filesize) {
            return FSE_INVALIDBLOCK;
        }
        block_num++;

        // Check if block is already allocated
        if (active_inode->d_inode.direct[block_num] == 0) {
            // Allocate a new block
            active_inode->d_inode.direct[block_num] = get_free_entry((unsigned char*)dblk_bmap);
            super_block.d_super.ndata_blks++;
            block_modify(0, 0, sizeof(disk_superblock_t), &super_block.d_super);
            if (active_inode->d_inode.direct[block_num] == -1) {
                return FSE_BITMAP;
            }
        }

        // Write the remaining data to the new block
        active_block_idx = &global_inode_table[current_running->filedes[fd].idx].d_inode.direct[block_num];
        block_modify(*active_block_idx, active_inode->pos % BLOCK_SIZE, size, &buffer[rest]);
        active_inode->d_inode.current_size += size;
        active_inode->dirty = 1;
        active_inode->pos += size;
    }
    return FSE_OK;
}

/*
 * fs_lseek:
 * This function is really incorrectly named, since neither its offset
 * argument or its return value are longs (or off_t's).
 */
int fs_lseek(int fd, int offset, int whence) {
    // Get inode from global inode table
    mem_inode_t* active_inode = &global_inode_table[current_running->filedes[fd].idx];
    // Check if file is open for reading or reading and writing
    if ((current_running->filedes[fd].mode == MODE_RDONLY) ||
        (current_running->filedes[fd].mode == MODE_RDWR)) {
        // Check if whence is SEEK_SET
        if (whence == SEEK_SET) {
            // Check if offset is within the file size
            if (offset <= active_inode->d_inode.current_size) {
                active_inode->pos = offset;
                return 0;
            }
        }
        // Check if whence is SEEK_CUR
        else if (whence == SEEK_CUR) {
            // Check if offset is within the file size
            if ((active_inode->pos + offset) <= active_inode->d_inode.current_size) {
                active_inode->pos += offset;
                return 0;
            }
        }
        // Check if whence is SEEK_END
        else if (whence == SEEK_END) {
            // Check if offset is within the file size
            if ((active_inode->d_inode.current_size - offset) >= 0) {
                active_inode->pos = active_inode->d_inode.current_size - offset;
                return 0;
            }
        }
    }
    return FSE_ERROR;
}

int fs_mkfile(char *filename) {
    // Initialize variables
    char filename_copy[MAX_PATH_LEN];
    bcopy(filename, filename_copy, MAX_PATH_LEN);
    char *argv[MAX_PATH_LEN];
    char buf[MAX_FILENAME_LEN * 2];

    // Parse path
    int path_amount = parse_path(filename_copy, argv, buf);
    inode_t parent_inode_num = current_running->cwd;

    // Read last directory
    disk_inode_t parent_inode = read_inode_table(parent_inode_num);
    
    // Check if file already exists
    if (name2inode(argv[path_amount-1]) > 0) {
        return FSE_EXIST;
    }

    // Validate the file name
    if(validate_name(argv[path_amount-1]) != FSE_OK) {
        return FSE_INVALIDNAME;
    }

    // Create a new inode
    inode_t new_inode_num;
    int ev = create_inode(&new_inode_num, parent_inode_num, INTYPE_FILE);
    if (ev < 0) {
        return ev;
    }

    // Create directory entry in parent directory
    ev = create_directory_entry(parent_inode_num, argv[path_amount-1], new_inode_num);
    if (ev < 0) {
        return ev;
    }
    return new_inode_num;
}

int fs_mkdir(char* dirname) {
    // Initialize variables
    char dirname_copy[MAX_PATH_LEN];
    bcopy(dirname, dirname_copy, MAX_PATH_LEN);
    char* argv[MAX_PATH_LEN];
    char buf[MAX_FILENAME_LEN * 2];

    // Parse path
    int path_amount = parse_path(dirname_copy, argv, buf);

    inode_t found_inode = current_running->cwd;
    inode_t inode_num;

    int path_index = 0;
    // If path starts with /, start at root
    if (argv[path_index][0] == '/') {
        found_inode = super_block.d_super.root_inode;
        path_index++;
    }
    for (; path_index < path_amount; path_index++) {
        // Check if file name is valid
        if (validate_name(argv[path_index]) != FSE_OK) {
            return FSE_INVALIDNAME;
        }
        // Check if file already exists
        if (name2inode(argv[path_index]) > 0) {
            if (path_index == path_amount - 1) {
                return FSE_EXIST;
            } else {
                // If file exists, get inode number
                inode_num = name2inode(argv[path_index]);
            }
        }
        // If file does not exist, create it
        else {
            // Create new inode
            int ev = create_inode(&inode_num, found_inode, INTYPE_DIR);
            if (ev < 0) {
                return ev;
            }
            // Create directory entry
            ev = create_directory_entry(found_inode, argv[path_index], inode_num);
            if (ev < 0) {
                return ev;
            }
        }
        found_inode = inode_num;
    }
    return FSE_OK;
}

int fs_chdir(char *path) {
    // Find inode
    inode_t new_path = name2inode(path);

    // Check if file exists
    if (new_path < 0) {
        return FSE_NOTEXIST;
    }

    // Read inode from disk
    disk_inode_t active_inode = read_inode_table(new_path);

    // Check if file is a directory
    if (active_inode.type == INTYPE_FILE) {
        return FSE_DIRISFILE;
    }
    current_running->cwd = new_path;
    return FSE_OK;
}

int fs_rmdir(char *path) {
    // Find inode
    inode_t found_inode = name2inode(path);
    disk_inode_t active_inode = read_inode_table(found_inode);
    
    // Check if file is a directory
    if (active_inode.type == INTYPE_FILE) {
        return FSE_DIRISFILE;
    }

    // Check if directory is root
    if (active_inode.direct[0] == super_block.d_super.root_inode) {
        return FSE_ERROR;
    }

    // Check if directory is open
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (current_running->filedes[i].mode != MODE_UNUSED) {
            if (current_running->filedes[i].idx == found_inode) {
                return FSE_FILEOPEN;
            }
        }
    }
    // Handle parent first
    int found = 0;
    // Get parent information
    blknum_t current_block = active_inode.direct[0];
    dirent_t dir;
    block_read_part(current_block, sizeof(dirent_t), sizeof(dirent_t), &dir);
    inode_t parent_inode_num = dir.inode;
    // Remove directory entry from current directory
    int ev = remove_directory_entry(parent_inode_num, path);
    if (ev < 0) {
        // Directory entry not found or encountered an error
        return ev;
    }
    
    // Remove the inode
    ev = remove_inode(found_inode);
    if (ev < 0) {
        // Inode not found or encountered an error
        return ev;
    }

    return FSE_OK;
}

// Concatenate strings
void strconcat(char* source, const char* destination) {
    // Find the end of the source string
    while (*source != '\0') {
        source++;
    }
    // Concatenate destination to source
    while (*destination != '\0') {
        *source = *destination;
        source++;
        destination++;
    }
    // Null terminate the result
    *source = '\0';
}

int fs_recursive_rmdir(char *path) {
    char child_path[MAX_PATH_LEN];
    // Get inode
    inode_t inode_num = name2inode(path);
    if (inode_num < 0) {
        return FSE_PARSEERROR;
    }
    disk_inode_t inode = read_inode_table(inode_num);
    // Abort if inode is a file
    if (inode.type == INTYPE_FILE) {
        return FSE_DIRISFILE;
    }
    // Go through all direct blocks
    for (int i = 0; i < INODE_NDIRECT; i++) {
        blknum_t current_block = inode.direct[i];
        if (current_block != 0) {
            dirent_t dir[DIRENTS_PER_BLK];
            block_read_part(current_block, 0, sizeof(dirent_t) * DIRENTS_PER_BLK, &dir);
            // Skip "." and ".."
            for (int j = 2; j < DIRENTS_PER_BLK; j++) {
                if (dir[j].name[0] != '\0') {
                    // Construct full path for child
                    strcpy(child_path, path);
                    strconcat(child_path, "/");
                    strconcat(child_path, dir[j].name);
                    // Recursive call
                    int result = fs_recursive_rmdir(child_path);
                    if (result != FSE_OK) {
                        return result;
                    }
                }
            }
        }
    }
    // All subdirectories have been removed, now remove this directory
    return fs_rmdir(path);
}

int fs_link(char *source, char *destination) {
    // Get inode of source
    inode_t src_inode_num = name2inode(source);
	char child_path[MAX_PATH_LEN];

    // Check if source exists
    if (src_inode_num < 0) {
        return FSE_NOTEXIST;
    }
    // Check if destination exists
	if (name2inode(destination) > 0) {
		return FSE_EXIST;
	}

    // Check if source is a directory
    disk_inode_t src_inode = read_inode_table(src_inode_num);
    if (src_inode.type == INTYPE_DIR) {
        return FSE_ERROR;
    }
    // Get current directory information
	disk_inode_t parent_inode = read_inode_table(current_running->cwd);
	inode_t parent_inode_num = current_running->cwd;

    // Parse the destination path to get the name of the new link and the path to its parent directory
    char dirname_copy[MAX_PATH_LEN];
    bcopy(destination, dirname_copy, MAX_PATH_LEN);
    char *argv[MAX_PATH_LEN];
    char buf[MAX_FILENAME_LEN * 2];
    int path_amount = parse_path(dirname_copy, argv, buf);

	if (path_amount > 1) {
		return FSE_ERROR;
	}

    // Update parent directory inodes
    src_inode.nlinks++;
    write_inode2table(src_inode_num, src_inode);
    int ev = create_directory_entry(parent_inode_num, destination, src_inode_num);
	if (ev < 0) {
		return ev;
	}

    return FSE_OK;
}


int fs_unlink(char *source) {
    // Get inode of source
    inode_t src_inode_num = name2inode(source);

    // Check if source exists
    if (src_inode_num < 0) {
        return FSE_NOTEXIST;
    }

    // Read inode from disk
    disk_inode_t src_inode = read_inode_table(src_inode_num);

    // Check if source is a directory
    if (src_inode.type == INTYPE_DIR) {
        return FSE_ERROR;
    }

    // Check if source is a file
    if (src_inode.type == INTYPE_FILE) {
        // Check if file is open
        for (int i = 0; i < MAX_OPEN_FILES; i++) {
            if (current_running->filedes[i].idx == src_inode_num) {
                return FSE_ERROR;
            }
        }
    }
   
    // Remove directory entry from parent directory
    int ev = remove_directory_entry(current_running->cwd, source);
    if(ev < 0){
        return ev; //Return error if unable to remove directory entry
    }

    // Then handle source
    // Check if source is a link
    if (src_inode.nlinks > 1) {
        src_inode.nlinks--;
        write_inode2table(src_inode_num, src_inode);
    }
    else{
        // Use remove_inode to handle the cleaning up and removal of the inode
        ev = remove_inode(src_inode_num);
        if(ev < 0){
            return ev;
        }
    }
    return FSE_OK;
}


int fs_stat(int fd, char *buffer) {
    // Get inode from global inode table
    mem_inode_t* active_inode = &global_inode_table[current_running->filedes[fd].idx];
    // Check if file descriptor is open
    if (current_running->filedes[fd].mode == MODE_UNUSED) {
        return FSE_ERROR;
    }

    // Fill in buffer with inode information
    buffer[0] = active_inode->d_inode.type;
    buffer[1] = active_inode->d_inode.nlinks;
    bcopy((char*)&active_inode->d_inode.current_size, (char*)(buffer + 2), sizeof(int));
    return FSE_OK;
}

/*
 * Helper functions for the system calls
 */

/*
 * get_free_entry:
 *
 * Search the given bitmap for the first zero bit.  If an entry is
 * found it is set to one and the entry number is returned.  Returns
 * -1 if all entrys in the bitmap are set.
 */
static int get_free_entry(unsigned char *bitmap) {
    int i;
    /* Seach for a free entry */
    for (i = 0; i < BITMAP_ENTRIES / 8; i++) {
        if (bitmap[i] == 0xff) /* All taken */
            continue;
        if ((bitmap[i] & 0x80) == 0) { /* msb */
            bitmap[i] |= 0x80;
            fs_update_bitmap();
            return i * 8;
        }
        else if ((bitmap[i] & 0x40) == 0) {
            bitmap[i] |= 0x40;
            fs_update_bitmap();
            return i * 8 + 1;
        }
        else if ((bitmap[i] & 0x20) == 0) {
            bitmap[i] |= 0x20;
            fs_update_bitmap();
            return i * 8 + 2;
        }
        else if ((bitmap[i] & 0x10) == 0) {
            bitmap[i] |= 0x10;
            fs_update_bitmap();
            return i * 8 + 3;
        }
        else if ((bitmap[i] & 0x08) == 0) {
            bitmap[i] |= 0x08;
            fs_update_bitmap();
            return i * 8 + 4;
        }
        else if ((bitmap[i] & 0x04) == 0) {
            bitmap[i] |= 0x04;
            fs_update_bitmap();
            return i * 8 + 5;
        }
        else if ((bitmap[i] & 0x02) == 0) {
            bitmap[i] |= 0x02;
            fs_update_bitmap();
            return i * 8 + 6;
        }
        else if ((bitmap[i] & 0x01) == 0) { /* lsb */
            bitmap[i] |= 0x01;
            fs_update_bitmap();
            return i * 8 + 7;
        }
    }
    return -1;
}

/*
 * free_bitmap_entry:
 *
 * Free a bitmap entry, if the entry is not found -1 is returned, otherwise zero.
 * Note that this function does not check if the bitmap entry was used (freeing
 * an unused entry has no effect).
 */
static int free_bitmap_entry(int entry, unsigned char *bitmap) {
    unsigned char *bme;
    if (entry >= BITMAP_ENTRIES)
        return -1;

    bme = &bitmap[entry / 8];

    switch (entry % 8) {
    case 0:
        *bme &= ~0x80;
        fs_update_bitmap();
        break;
    case 1:
        *bme &= ~0x40;
        fs_update_bitmap();
        break;
    case 2:
        *bme &= ~0x20;
        fs_update_bitmap();
        break;
    case 3:
        *bme &= ~0x10;
        fs_update_bitmap();
        break;
    case 4:
        *bme &= ~0x08;
        fs_update_bitmap();
        break;
    case 5:
        *bme &= ~0x04;
        fs_update_bitmap();
        break;
    case 6:
        *bme &= ~0x02;
        fs_update_bitmap();
        break;
    case 7:
        *bme &= ~0x01;
        fs_update_bitmap();
        break;
    }

    return 0;
}

/*
 * ino2blk:
 * Returns the filesystem block (block number relative to the super
 * block) corresponding to the inode number passed.
 */
static blknum_t ino2blk(inode_t ino, int offset) {
	// Check if inode number is valid
	if (ino < 0 || ino >= DISK_INODE_MAX) {
		return FSE_INVALIDINODE;
	}

	// Calculate the inode index in the inode table
	inode_t ino_tbl_num = ino / DISK_INODE_IN_BLOCK_MAX;

	// Calculate the block number of the inode
	blknum_t ino_num = ino % DISK_INODE_IN_BLOCK_MAX;

	// Calculate the offset of the inode in the block
	disk_inode_t current_inode = read_inode_table(ino_num);

	// Find block
	blknum_t block_num = current_inode.direct[offset];
	return block_num;
}

/*
 * idx2blk:
 * Returns the filesystem block (block number relative to the super
 * block) corresponding to the data block index passed.
 */
static blknum_t idx2blk(int index) {
    return os_size + 2 + index;
}

/*
 * name2inode:
 * Parses a file name and returns the corresponding inode number. If
 * the file cannot be found, -1 is returned.
 */
static inode_t name2inode(char *name) {
    // Initialize variables
    char dirname_copy[MAX_PATH_LEN];
    bcopy(name, dirname_copy, MAX_PATH_LEN);
    char *argv[MAX_PATH_LEN];
    char buf[MAX_FILENAME_LEN * 2];
    int path_amount = parse_path(dirname_copy, argv, buf);

    // Set found_inode to current directory
    inode_t found_inode = current_running->cwd;

    int found, path_index = 0;

    // If path starts with /, start at root
    if (name[0] == '/') {
        found_inode = super_block.d_super.root_inode;
        path_index++;
    }

    // Iterate through path
    for (; path_index < path_amount; path_index++) {
        // Initialize found to 0 at the start of each new path iteration
        found = 0;

        // Read current directory
        disk_inode_t current_inode = read_inode_table(found_inode);

        // Iterate through direct blocks
        for (int i = 0; i < INODE_NDIRECT; i++) {
            // Only go through if current_block has data
            if (current_inode.direct[i] != 0) {
                dirent_t dir[DIRENTS_PER_BLK];
                block_read_part(current_inode.direct[i], 0, sizeof(dirent_t) * DIRENTS_PER_BLK, &dir);
                // Iterate through directory entries
                for (int j = 0; j < DIRENTS_PER_BLK; j++) {
                    // Check if there is a name (means that there is data in directory entry)
                    if (dir[j].name[0] != '\0') {
                        // Check if the name matches the current path index
                        if (strncmp(argv[path_index], dir[j].name, strlen(argv[path_index])) == 0) {
                            found_inode = dir[j].inode;
                            found = 1;
                            break;
                        }
                    }
                }
                // Exit the directory entry loop if the inode was found
                if (found) {
                    break;
                }
            }
        }
        // If inode not found, set found_inode to FSE_ERROR
        if (!found) {
            found_inode = FSE_ERROR;
            // Exit the path_index loop since the inode wasn't found
            break;
        }
    }
    return found_inode;
}