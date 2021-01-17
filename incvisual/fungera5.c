#include <stdlib.h> 
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <ncurses.h>
#include <unistd.h>

int MAX_N_OF_ERRORS = 5000;
int MAX_ITERS_WITHOUT_REPRODUCTION = 100000;
int TEMPLATE[1024];
int STACK_SIZE = 8;

int COMMAND_LENGTH = 5;

typedef struct {
	int size;
	char *content;
} array;

char commands[25] = {'.', ':', 'a', 'b', 'c', 'd', 'x', 'y', '^', 'v', '>', '<', '&', '?', '1', '0', '-', '+', '~', 'L', 'W', '@', '$', 'S', 'P'};
int N_OF_COMMANDS = 25;

int id = 0;
typedef struct {
	int parent_id;
	int id;
	int startx;
	int starty;
	int width;
	int height;
	int ptrx;
	int ptry;
	int deltax;
	int deltay;
	int ax;
	int ay;
	int bx;
	int by;
	int cx;
	int cy;
	int dx;
	int dy;
	int stackx[8];
	int stacky[8];
	int stacktop;
	int errors;
	// reproduction
	int reproduction_cycle;
	// child
	int childx;
	int childy;
	int child_width;
	int child_height;
	int children;
} organism;

typedef struct {
	int top;
	int size;
	organism *organisms;
} queue;

int kill_organism(organism org, array* memory);

void queue_create(queue* q) {
	q->top = 0;
	q->size = 1;
	q->organisms = calloc(q->size, sizeof(organism));
}

void print_queue(queue* q) {
	for (int j = 0; j < q->top; j++) {
		//printf("%d id %d at (%d %d) errors %d\n", j, q->organisms[j].id, q->organisms[j].startx, q->organisms[j].starty, q->organisms[j].errors);
	}
}

int get_queue(queue* q, organism* org, int i) {
	if ((i < 0) || (i >= q->size)) {
		return -1;
	}
	*org = q->organisms[i];
	return 0;
}

void update_queue(queue* q, array* memory) {
	for (int j = 1; j < q->top; j++) {
		for (int i = j; i > 0; i--) {
			if (q->organisms[i].errors < q->organisms[i-1].errors) {
				// swipe
				organism temp = q->organisms[i];
				q->organisms[i] = q->organisms[i-1];
				q->organisms[i-1] = temp;
			}
		}
	}
	//print_queue(q);
	int end = q->top;
	for (int i = q->top-1; i >= 0; i--) {
		//printf("iteration %d organism %d (errors %d)\n", i, (q->organisms[i]).id, (q->organisms[i]).errors);
		if ((q->organisms[i].errors) > MAX_N_OF_ERRORS) {
			end = i;
			break;
		}
	}
	for (int i = end; i < q->top; i++) {
		if (!(kill_organism(q->organisms[i], memory) == 0)) {
			return;
		}
	}
	q->top = end;
}

int append_queue(queue* q, organism* org) {
	//printf("pushing into queue Organism %d\n", org->id);
	if (q->top == q->size) {
		organism *newq = calloc(q->size*2, sizeof(organism));
		if (!(newq)) {
			return -1;
		}
		for (int i = 0; i < q->top; i++) {
			newq[i] = q->organisms[i];
		}
		free(q->organisms);
		q->organisms = newq;
		q->size = q->size*2;
	}
	q->organisms[q->top] = *org;
	q->top++;
	//print_queue(q);
//	printf("Q %d %d %d %d %d %d|", org->id, org->parent_id, org->startx, org->starty, org->width, org->height);
	return 0;
}

int command_symbol_to_code(char s) {
	for (int i = 0; i < N_OF_COMMANDS; i++) {
		if (s == commands[i]) {
			return i;
		}
	}
	//printf("unknown command %c\n", s);
	return -1;
}

/*
'.' : 00000001 : 'no_operation',
':' : 00000010 : 'no_operation',
'a' : 00000011 : 'no_operation',
'b' : 00000100 : 'no_operation',
'c' : 00000101 : 'no_operation',
'd' : 00000110 : 'no_operation',
'x' : 00000111 : 'no_operation',
'y' : 00001000 : 'no_operation',
'^' : 00001001 : 'move_organism_pointer_up',
'v' : 00001010 : 'move_organism_pointer_down',
'>' : 00001011 : 'move_organism_pointer_right',
'<' : 00001100 : 'move_organism_pointer_left',
'&' : 00001101 : 'find_template',
'?' : 00001110 : 'if_not_zero',
'1' : 00001111 : 'one',
'0' : 00010000 : 'zero',
'-' : 00010001 : 'decrement',
'+' : 00010010 : 'increment',
'~' : 00010011 : 'subtract',
'L' : 00010100 : 'load_inst',
'W' : 00010101 : 'write_inst',
'@' : 00010110 : 'allocate_child',
'$' : 00010111 : 'split_child',
'S' : 00011000 : 'push',
'P' : 00011001 : 'pop',
*/

/*
free :      00000000
allocated : 00000001
active :    00000010
*/

int set_register_value(organism* org, int register_code, int x, int y) {
	if ((register_code < 0) || (register_code >= N_OF_COMMANDS)) {
		//printf("unknown command code %d\n", register_code);
		return -1;
	}
	char reg = commands[register_code];
	if (reg == 'a') {
		org->ax = x;
		org->ay = y;
	}
	else if (reg == 'b') {
		org->bx = x;
		org->by = y;
	}
	else if (reg == 'c') {
		org->cx = x;
		org->cy = y;
	}
	else if (reg == 'd') {
		org->dx = x;
		org->dy = y;
	}
	else {
		//printf("register %c doesn't exist\n", reg);
		return -1;
	}
	return 0;
}

int get_register_value(organism* org, int register_code, int* x, int* y) {
	if ((register_code < 0) || (register_code >= N_OF_COMMANDS)) {
		//printf("unknown command code %d\n", register_code);
		return -1;
	}
	char reg = commands[register_code];
	if (reg == 'a') {
		*x = org->ax;
		*y = org->ay;
	}
	else if (reg == 'b') {
		*x = org->bx;
		*y = org->by;
	}
	else if (reg == 'c') {
		*x = org->cx;
		*y = org->cy;
	}
	else if (reg == 'd') {
		*x = org->dx;
		*y = org->dy;
	}
	else {
		//printf("register %d (%c) doesn't exist\n", register_code, reg);
		return -1;
	}
	return 0;
}

int organism_create(organism* org, int startx, int starty, int width, int height, int parent_id) {
	if (!org) {
		return -1;
	}
	org->parent_id = parent_id; //remember ancestor
	org->id = id;
	id++;
	org->startx = startx;
	org->starty = starty;
	org->width = width;
	org->height = height;
	//!!! absolute position of the pointer
	org->ptrx = 0;
	org->ptry = 0;
	org->deltax = 1;
	org->deltay = 0;
	org->stacktop = 0;
	// registers
	org->ax = 0;
	org->ay = 0;
	org->bx = 0;
	org->by = 0;
	org->cx = 0;
	org->cy = 0;
	org->dx = 0;
	org->dy = 0;
	// reproduction
	org->errors = 0;
	org->reproduction_cycle = 0;
	// child
	org->child_height = 0;
	org->child_width = 0;
	org->childx = 0;
	org->childy = 0;
	org->children = 0;
	return 0;
}

#define STATUS_NCOLS 32
#define CTRL_NROWS 4
#define NCHARACTERISTICS 1

void refresh_status_win(WINDOW *status_win, organism *org);
void refresh_mem_win(array *arr);
void my_mvwaddstr(WINDOW *status_win, int y, int x, char *s);

WINDOW *mem_win;
int max_y, max_x;
int mem_nrows, mem_ncols;
int begin_i = 10, begin_j = 10;

int ip_in_range(organism *org)
{
    int y, x;

    y = org->starty + org->ptry;
    x = org->startx + org->ptrx;
	return begin_i+1 < x && begin_j+1 < y && x < begin_i+mem_nrows-1 && y < begin_j+mem_ncols-1;
}

void move_organism_pointer(organism* org) {
	//printf("moving organism %d by (%d %d)\n", org->id, org->deltax, org->deltay);
	int c;
    int y, x;
	
	// Main logic
	org->ptrx += org->deltax;
	org->ptry += org->deltay;
	
    y = org->starty + org->ptry;
    x = org->startx + org->ptrx;
    if (ip_in_range(org)) {
        c = mvwinch(mem_win, x-begin_i, y-begin_j);
        wattron(mem_win, COLOR_PAIR(1));
		mvwaddch(mem_win, x-begin_i, y-begin_j, c & A_CHARTEXT);
        wattroff(mem_win, COLOR_PAIR(1));
        wrefresh(mem_win);
    }
}

void print_array(array* arr) {
	for (int i = 0; i < arr->size; i++) {
		for (int j = 0; j < arr->size; j++) {
//			printf("%c", commands[arr->content[i*arr->size + j] >> 2]);
		}
//		printf("\n");
	}
//	printf("\n");
}

void print_array_mask(array* arr) {
	for (int i = 0; i < arr->size; i++) {
		for (int j = 0; j < arr->size; j++) {
//			printf("%d", 3 & arr->content[i*arr->size + j]);
		}
//		printf("\n");
	}
//	printf("\n");
}


void print_field(array* arr, organism* org) {
	for (int i = 0; i < arr->size; i++) {
		for (int j = 0; j < arr->size; j++) {
			if ((org->ptrx + org->startx == i) && (org->ptry + org->starty == j)) {
				//printf("#");
			}
			else {
				//printf("%c", commands[arr->content[i*arr->size + j] >> 2]);
			}
		}
		//printf("\n");
	}
	//printf("\n");
}

int array_create(array* arr, int size) {
	arr->size = size;
	arr->content = calloc(size*size, sizeof(char));
	if (!arr->content) {
		return -1;
	}
	return 0;
}

int does_fit(array* memory, int x, int y, int w, int h) {
	//check wether chunk (x, y) (x+w, y+h) is withing 2d array matrix
	return ((x >= 0) && (x < memory->size) && 
		(x+w >= 0) && (x+w < memory->size) &&
		(y >= 0) && (y < memory->size) &&
		(y+h >= 0) && (y+h < memory->size));
}

int memory_is_free(array* memory, int x, int y, int w, int h) {
	if (!does_fit(memory, x, y, w, h)) {
		return 0;
	}
	for (int i = x; i < x+w; i++) {
		for (int j = y; j < y+h; j++) {
			if (!((3 & memory->content[i*memory->size+j]) == 0)) {
				return 0;
			}
		}
	}
	return 1;
}

int memory_is_active(array* memory, int x, int y, int w, int h) {
	if (!does_fit(memory, x, y, w, h)) {
		return 0;
	}
	for (int i = x; i < x+w; i++) {
		for (int j = y; j < y+h; j++) {
			if (!((3 & memory->content[i*memory->size+j]) == 2)) {
				return 0;
			}
		}
	}
	return 1;
}

int memory_allocate(array* memory, int x, int y, int w, int h) {
	if (!does_fit(memory, x, y, w, h)) {
		return -1;
	}
	if (!memory_is_free(memory, x, y, w, h)) {
		//printf("can't allocate: chunk %d %d, %d %d not free\n", x, y, w, h);
		return -1;
	}
	for (int i = x; i < x+w; i++) {
		for (int j = y; j < y+h; j++) {
			memory->content[i*memory->size+j] ^= 3 & memory->content[i*memory->size+j];
			memory->content[i*memory->size+j] |= 1;
		}
	}
//	printf("ML %d %d %d %d|", x, y, w, h);
	return 0;
}

int memory_activate(array* memory, int x, int y, int w, int h) {
	if (!does_fit(memory, x, y, w, h)) {
		return -1;
	}
	if (memory_is_active(memory, x, y, w, h)) {
		return -1;
	}
	for (int i = x; i < x+w; i++) {
		for (int j = y; j < y+h; j++) {
			memory->content[i*memory->size+j] ^= 3 & memory->content[i*memory->size+j];
			memory->content[i*memory->size+j] |= 2;
		}
	}
//	printf("MA %d %d %d %d|", x, y, w, h);
	return 0;
}

int memory_free(array* memory, int x, int y, int w, int h) {
	if (!does_fit(memory, x, y, w, h)) {
		return 0;
	}
	for (int i = x; i < x+w; i++) {
		for (int j = y; j < y+h; j++) {
			memory->content[i*memory->size+j] ^= 3 & memory->content[i*memory->size+j];
		}
	}
//	printf("MF %d %d %d %d|", x, y, w, h);
	return 0;
}

char array_get(array* arr, int x, int y) {
	if ((x < 0) || (x >= arr->size) || (y < 0) || (y >= arr->size)) {
		//printf("out of range\n");
		return -1;
	}
	int index = x*arr->size + y;
	return arr->content[index];
}

int array_get_command_code(array* arr, int x, int y) {
	if ((x < 0) || (x >= arr->size) || (y < 0) || (y >= arr->size)) {
		//printf("out of range\n");
		return -1;
	}
	int index = x*arr->size + y;
	return (arr->content[index])>>2;
}

int array_get_cell_type(array* arr, int x, int y) {
	if ((x < 0) || (x >= arr->size) || (y < 0) || (y >= arr->size)) {
		//printf("out of range\n");
		return -1;
	}
	int index = x*arr->size + y;
	return 3 & arr->content[index];
}

int array_set(array* arr, int x, int y, char c) {
	if ((x < 0) || (x >= arr->size) || (y < 0) || (y >= arr->size)) {
		//printf("out of range\n");
		return -1;
	}
	int command_code = c >> 2;
	if ((command_code < 0) || (command_code >= N_OF_COMMANDS)) {
		//printf("invalid command\n");
		return -1;
	}
	int cell_type = 3 & c;
	if ((cell_type < 0) || (cell_type > 2)) {
		//printf("invalid memorycell type\n");
		return -1;
	}
	int index = x*arr->size + y;
	arr->content[index] = c;
//	printf("MS %d %d %d %d|", x, y, command_code, cell_type);
	return 0;
}

int array_set_command(array* arr, int x, int y, int code) {
	if ((x < 0) || (x >= arr->size) || (y < 0) || (y >= arr->size)) {
		//printf("out of range\n");
		return -1;
	}
	if ((code < 0) || (code >= N_OF_COMMANDS)) {
		return -1;
	}
	int index = x*arr->size + y;
	arr->content[index] = (code << 2) + (3 & arr->content[index]);
//	printf("MW %d %d %c|", x, y, commands[code]);
	return 0;
}


int kill_organism(organism org, array* memory) {
	if (!(memory_free(memory, org.startx, org.starty, org.width, org.height) == 0)) {
		return -1;
	}
//	printf("K %d\n", org.id);
	return 0;
}

int left(organism* org) {
	//printf("left\n");
	if (!org) {
		return -1;
	}
	org->deltax = 0;
	org->deltay = -1;
	return 0;
}

int right(organism* org) {
	//printf("right\n");
	if (!org) {
		return -1;
	}
	org->deltax = 0;
	org->deltay = 1;
	return 0;
}

int up(organism* org) {
	//printf("up\n");
	if (!org) {
		return -1;
	}
	org->deltax = -1;
	org->deltay = 0;
	return 0;
}

int down(organism* org) {
	if (!org) {
		return -1;
	}
	org->deltax = 1;
	org->deltay = 0;
	return 0;
}

int Stack(array* memory, organism* org) {
	//printf("stacking\n");
	char reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	int x;
	int y;
	if (!(get_register_value(org, reg, &x, &y) == 0)) {
		return -1;
	}
	if (org->stacktop >= STACK_SIZE) {
		return -1;
	}
	//printf("stacking %d %d\n", x, y);
	org->stackx[org->stacktop] = x;
	org->stacky[org->stacktop] = y;
	org->stacktop++;
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	return 0;
}

int Pop(array* memory, organism* org) {
	//printf("popping\n");
	if (org->stacktop == 0) {
		//printf("can't pop from empty stack\n");
		return -1; //empty stack
	}
	int x = org->stackx[org->stacktop-1];
	int y = org->stacky[org->stacktop-1];
	int reg = array_get_command_code(memory, org->ptrx+org->startx+org->deltax, org->ptry+org->starty+org->deltay);
	if (!(set_register_value(org, reg, x, y) == 0)) {
		return -1;
	}
	//printf("popped (%d %d) (memory %d %d) into %c\n", x, y, org->ptrx+org->startx+org->deltax, org->ptry+org->starty+org->deltay, commands[reg]);
	org->stacktop--;
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	return 0;
}

int jumpc(array* memory, organism* org) {
	//printf("jumping");
	int reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	if ((reg < 0) || (reg >= N_OF_COMMANDS)) {
		//printf("register %c doesn't exist\n", commands[reg]);
		return -1;
	}
	int x = 0;
	int y = 0;
	int xs;
	int ys;
	if ((commands[reg] == 'x') || (commands[reg] == 'y')) {
		reg = commands[reg];
		//printf("partial jump %c\n", reg);
		x = (reg == 'x') ? 1 : 0;
		y = (reg == 'y') ? 1 : 0;
		//printf("%d %d\n", org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
		reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
		//
		move_organism_pointer(org);
	}
	if (!(get_register_value(org, reg, &xs, &ys) == 0)) {
		return -1;
	}
	if (((x == 0) && (y == 0) && (xs == 0) && (ys == 0)) ||
	((x == 1) && (xs == 0)) || ((y == 1) && (ys == 0))) {
		move_organism_pointer(org);
	}
	else {
		move_organism_pointer(org);
		move_organism_pointer(org);
	}
	return 0;
}

int increment(array* memory, organism* org) {
	//printf("increment\n");
	int reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	if ((reg < 0) || (reg >= N_OF_COMMANDS)) {
		//printf("register %c doesn't exist\n", commands[reg]);
		return -1;
	}
	int x = 1;
	int y = 1;
	int xs;
	int ys;
	if ((commands[reg] == 'x') || (commands[reg] == 'y')) {
		reg = commands[reg];
		//printf("partial increment %c\n", reg);
		x = (reg == 'x') ? 1 : 0;
		y = (reg == 'y') ? 1 : 0;
		//printf("%d %d\n", org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
		reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
		//this command is longer, but it is still one command
		move_organism_pointer(org);
	}
	if (!(get_register_value(org, reg, &xs, &ys) == 0)) {
		return -1;
	}
	//printf("value of %c : %d %d\n", commands[reg], xs, ys);
	if (!(set_register_value(org, reg, xs+x, ys+y) == 0)) {
		return -1;
	}
	//printf("changed value of %c by: %d %d\n", commands[reg], x, y);
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	return 0;
}

int decrement(array* memory, organism* org) {
	//printf("decrement\n");
	int reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	if ((reg < 0) || (reg >= N_OF_COMMANDS)) {
		//printf("register %c doesn't exist\n", commands[reg]);
		return -1;
	}
	int x = 1;
	int y = 1;
	int xs;
	int ys;
	if ((commands[reg] == 'x') || (commands[reg] == 'y')) {
		reg = commands[reg];
		//printf("partial decrement %c\n", reg);
		x = (reg == 'x') ? 1 : 0;
		y = (reg == 'y') ? 1 : 0;
		//printf("%d %d\n", org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
		reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
		//this command is longer, but it is still one command
		move_organism_pointer(org);
	}
	if (!(get_register_value(org, reg, &xs, &ys) == 0)) {
		return -1;
	}
	//printf("value of %c : %d %d\n", commands[reg], xs, ys);
	if (!(set_register_value(org, reg, xs-x, ys-y) == 0)) {
		return -1;
	}
	//printf("changed value of %c by: %d %d\n", commands[reg], -x, -y);
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	return 0;
}

int write_instance(array* memory, organism* org) {
	//print_array(memory);
	//printf("\n\n\n\nwriting an instance\n");
	int reg1 = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	int reg2 = array_get_command_code(memory, org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
	int addressx;
	int addressy;
	int x;
	int y;
	if (!(get_register_value(org, reg2, &x, &y) == 0)) {
		return -1;
	}
	if (!(get_register_value(org, reg1, &addressx, &addressy) == 0)) {
		return -1;
	}
	if ((addressx < org->childx) || (addressx >= org->childx+org->child_width) || 
	(addressy < org->childy) || (addressy >= org->childy+org->child_height)) {
		//printf("can't write into other's memory (%d %d) \n", addressx, addressy);
		return -1;
	}
	int c = x*10 + y;
	if ((c < 0) || (c >= N_OF_COMMANDS)) {
		//printf("unknown command %d\n", c);
		return -1;
	}
	//printf("command %c\n", commands[c]);
	//c = commands[c];
	if (!(array_set_command(memory, addressx, addressy, c) == 0)) {
		return -1;
	}
	//memory->content[addressx*memory->size+addressy] = (c<<2)+1;
	/*//printf("wrote command %d (%c) (%d %d) on (%d %d) (register %c) successfully\n", \
c, commands[c], x, y, addressx, addressy, commands[reg1]);*/
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	move_organism_pointer(org);
	return 0;
}

int load_instance(array* memory, organism* org) {
	//printf("loading instance\n");
	int reg_dest = array_get_command_code(memory, org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
	int reg_source = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	if ((reg_source < 0) || (reg_source >= N_OF_COMMANDS)) {
		//printf("no such command\n");
		return -1;
	}
	if ((reg_dest < 0) || (reg_dest >= N_OF_COMMANDS)) {
		//printf("no such command\n");
		return -1;
	}
	int x;
	int y;
	if (!(get_register_value(org, reg_source, &x, &y) == 0)) {
		//printf("can't extract value of register %c\n", commands[reg_source]);
		return -1;
	}
	int command = array_get_command_code(memory, x, y);
	if ((command < 0) || (command >= N_OF_COMMANDS)) {
		//printf("invalid command code %d", command);
		return -1;
	}
	if (!(set_register_value(org, reg_dest, command/10, command%10) == 0)) {
		//printf("can't set value of register %c", commands[reg_dest]);
		return -1;
	}
	//printf("loaded inst successfully from (%d %d) %c [%d %d]\n", x, y, commands[command], command/10, command%10);
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	move_organism_pointer(org);
	return 0;
}

int find_template(array* memory, organism* org) {
	//printf("looking_for_template\n");
	int x = org->ptrx + org->startx + org->deltax;
	int y = org->ptry + org->starty + org->deltay;
	int reg = array_get_command_code(memory, x, y);
	//printf("registercode %d\n", reg);

	int maxlen = (org->width < org->height) ? (org->height) : (org->width);
	//printf("length of template 1: %d %i\n", maxlen, maxlen);
	//int template[maxlen+1];// = calloc(maxlen, sizeof(int));
	//printf("length of template 2: %d %i\n", maxlen, maxlen);
	//if (!template) {
		//printf("couldn't allocate memory to store template\n");
	//	return -1;
	//}

	x += org->deltax;
	y += org->deltay;
	int c = array_get_command_code(memory, x, y);
	int i = 0;
	while ((i < maxlen) && ((commands[c] == '.') || (commands[c] ==':' ))) {
		TEMPLATE[i] = (commands[c] == '.') ? command_symbol_to_code(':') : command_symbol_to_code('.');
		x += org->deltax;
		y += org->deltay;
		c = array_get_command_code(memory, x, y);
		i++;
	}
	if (i == 0) {
		//printf("empty template\n");
		//free(template);
		return -1;
	}

	int j = 0;
	while ((j < i) &&
	(org->startx <= x) && (x < org->startx+maxlen) && 
	(org->starty <= y) && (y < org->starty+maxlen)) {
		x += org->deltax;
		y += org->deltay;
		c = array_get_command_code(memory, x, y);
		if (c == TEMPLATE[j]) {
			j++;
		}
		else {
			j = 0;
		}
	}
	//free(template);
	if (j != i) {
		//printf("template not found\n");
		return -1;
	}
	if (!(set_register_value(org, reg, x, y) == 0)) {
		return -1;
	}
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	return 0;
}

int allocate_child(array* memory, organism* org) {
	//printf("allocating child\n");
	int child_size = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	int child_start = array_get_command_code(memory, org->ptrx + org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
	if ((child_size < 0) || (child_size >= N_OF_COMMANDS) || (child_start < 0) || (child_start >= N_OF_COMMANDS)) {
		//printf("no such command\n");
		return -1;
	}
	int width, height;
	if (!(get_register_value(org, child_size, &width, &height) == 0)) {
		return -1;
	}
	if ((width < 0) || (height < 0)) {
		//printf("can't allocate child of negative area\n");
		// not an error
		return -1;
	}
	if ((width == 0) || (height == 0)) {
		//printf("can't allocate child of area 0\n");
		// not an error
		return 0;
	}
	int x = org->ptrx + org->startx+org->deltax*2;
	int y = org->ptry + org->starty+org->deltay*2;
	int is_space_found = 0;
	//printf("looking for space %d by %d in direction (%d %d) starting from (%d %d)\n", width, height, org->deltax, org->deltay, x, y);
    for (int i = 2; ((i < memory->size) && (!is_space_found) && 
	(x >= 0) && (y >= 0) && (x < memory->size) && (y < memory->size)); i++) {
		//printf("looking for space %d %d\n", x, y);
		x += org->deltax;
		y += org->deltay;
        if (memory_is_free(memory, x, y, width, height) == 1) {
            org->childx = x;
            org->childy = y;
			//printf("found space %d %d (%d %d)\n", x, y, width, height);
			if (!(set_register_value(org, child_start, x, y) == 0)) {
				return -1;
			}
            is_space_found = 1;
		}
	}
    if (is_space_found) {
		//printf("allocatin_memory\n");
        if (!(memory_allocate(memory, org->childx, org->childy, width, height) == 0)) {
			return -1;
		}
		//printf("allocated successfully\n");
		org->child_width = width;
		org->child_height = height;
	}
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	move_organism_pointer(org);
	return 0;
}

int zero(array* memory, organism* org) {
	int reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	if ((reg < 0) || (reg >= N_OF_COMMANDS)) {
		//printf("register %c doesn't exist\n", commands[reg]);
		return -1;
	}
	if (!(set_register_value(org, reg, 0, 0) == 0)) {
		return -1;
	}
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	return 0;
}

int one(array* memory, organism* org) {
	int reg = array_get_command_code(memory, org->ptrx + org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	if ((reg < 0) || (reg >= N_OF_COMMANDS)) {
		//printf("register %c doesn't exist\n", commands[reg]);
		return -1;
	}
	if (!(set_register_value(org, reg, 1, 1) == 0)) {
		return -1;
	}
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	return 0;
}

int subtract(array* memory, organism* org) {
	//printf("subtracting\n");
	int reg1 = array_get_command_code(memory, org->ptrx + 
	org->startx+org->deltax, org->ptry + org->starty+org->deltay);
	int reg2 = array_get_command_code(memory, org->ptrx + 
	org->startx+org->deltax*2, org->ptry + org->starty+org->deltay*2);
	int reg3 = array_get_command_code(memory, org->ptrx + 
	org->startx+org->deltax*3, org->ptry + org->starty+org->deltay*3);
	//printf("%d = %d - %d\n", commands[reg3], commands[reg1], commands[reg2]);
	int xa, ya;
	int xb, yb;
	int xc, yc;
	if (!(get_register_value(org, reg1, &xa, &ya) == 0)) {
		return -1;
	}
	if (!(get_register_value(org, reg2, &xb, &yb) == 0)) {
		return -1;
	}
	if (!(get_register_value(org, reg3, &xc, &yc) == 0)) {
		return -1;
	}
	if (!(set_register_value(org, reg3, xa - xb, ya - yb) == 0)) {
		return -1;
	}
	//this command is longer, but it is still one command
	move_organism_pointer(org);
	move_organism_pointer(org);
	move_organism_pointer(org);
	return 0;
}

int split_child(array* memory, organism* org, queue* q) {
	//printf("splitting child\n");
	if ((org->child_width < 0) || (org->child_height < 0)) {
		//printf("child with negative area\n");
		return -1;
	}
	if ((org->child_width == 0) || (org->child_height == 0)) {
		//printf("empty child\n");
		return 0;
	}
	if (!(memory_activate(memory, org->childx, org->childy, org->child_width, org->child_height) == 0)) {
		return -1;
	}
	organism neworg;
	organism_create(&neworg, org->childx, org->childy, org->child_width, org->child_height, org->id);
	append_queue(q, &neworg);
	org->children++;
    org->reproduction_cycle = 0;
    org->child_width = 0;
	org->child_height = 0;
    org->childx = 0;
	org->childy = 0;
	org->reproduction_cycle = 0;
	//printf("organism %d reproduced\n", org->id);
	return 0;
}

int operation(array* memory, organism* org, queue* q, int command) {
	
	//int memorycelltype = (oc % 4);

	switch(command)
	{
		case '.': return 0;
		case ':': return 0;
		case 'a' : return 0;
		case 'b' : return 0;
		case 'c' : return 0;
		case 'd' : return 0;
		case 'x' : return 0;
		case 'y' : return 0;
		case '>' : return right(org);
		case '^': return up(org);
		case '<' : return left(org);
		case 'v' : return down(org);
		case '&' : return find_template(memory, org);
		case '?' : return jumpc(memory, org);
		case '1': return one(memory, org);
		case '0': return zero(memory, org);
		case '-' : return decrement(memory, org);
		case '+' : return increment(memory, org);
		case '~' : return subtract(memory, org);
		case 'L' : return load_instance(memory, org);
		case 'W' : return write_instance(memory, org);
		case '@': return allocate_child(memory, org);
		case '$': return split_child(memory, org, q);
		case 'S' : return Stack(memory, org);
		case 'P' : return Pop(memory, org);
	}
	return -1;
}



int life(array* memory, organism* org, queue* q) {
	org->reproduction_cycle++;
	move_organism_pointer(org);
	if ((org->ptrx < 0) || (org->ptrx >= memory->size)) {
		return -1;
	}
	if ((org->ptry < 0) || (org->ptry >= memory->size)) {
		return -1;
	}
	int oc = array_get_command_code(memory, org->ptrx + org->startx, org->ptry + org->starty);
	//char command = commands[id];
	char command = commands[oc];
	int code = operation(memory, org, q, command);
	if (code != 0) {
		//printf("*********************ERROR DETECTED\n");
		org->errors += 1;
	}
	if (org->reproduction_cycle > MAX_ITERS_WITHOUT_REPRODUCTION) {
		org->errors += MAX_N_OF_ERRORS;
	}
//	printf("O %d %d %d %c %d|", org->id, org->ptrx, org->ptry, command, code==0);
	return 0;
}

/*
void print_organism(organism* org) {
	//printf("organism %d\nparent %d\nsize %d %d\nstart \
%d %d\npointer %d %d\nstacktop %d\nchild size (%d %d)\n\
child position (%d %d)\n\n a (%d %d)\nb (%d %d)\nc(%d %d)\nd(%d %d)\n",\
org->id, org->parent_id, org->width, org->height, \
org->startx, org->starty, org->ptrx + org->startx, org->ptry + org->starty, org->stacktop,\
org->child_height, org->child_width, org->childx, org->childy,\
org->ax, org->ay, org->bx, org->by, org->cx, org->cy, org->dx, org->dy);
}
*/

int write_chunck_from_file(array* memory, int startx, int starty, int width, int height, char* filename) {
	FILE* file = fopen(filename, "r");
	if(!file) {
		//printf("couldn't open file\n");
	    return -1;
	}

	if ((startx < 0) || (startx >= memory->size-height) || (starty < 0) || (starty >= memory->size-width)) {
		//printf("no space in memory\n");
		return -1;
	}
	char c;
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			c = getc(file);
			array_set_command(memory, startx+i, starty+j, command_symbol_to_code(c));
		}
		c = getc(file);
	}
	memory_activate(memory, startx, starty, height, width);
	return 0;
}


int radiation(array* memory) {
	int x = rand()%memory->size;
	int y = rand()%memory->size;
	int z = rand()%N_OF_COMMANDS;
	//printf("trying to spawn command %c at (%d %d)\n", commands[z], x, y);
	if ((3 & memory->content[x*memory->size+y]) == 2) {
		// can never change active memory
		//printf("can't change active memory\n");
		return 0;
	}
	if (!(array_set_command(memory, x, y, z) == 0)) {
		//printf("couldn't set command\n");
		return -1;
	}
	//printf("successfully spawned command %c at (%d %d)\n", commands[z], x, y);
	//printf("R %d %d %d|", x, y, z);
	return 0;
}

int main() {
	//if (!freopen("log.txt", "w", stderr)) {
	//	return -1;
	//}
	srand(time(0));
	array arr;
	array_create(&arr, 512);
	write_chunck_from_file(&arr, 18, 24, 23, 17, "initial.gen");

	organism org;
	organism_create(&org, 18, 24, 17, 23, 0);

    queue q;
    queue_create(&q);
    append_queue(&q, &org);
    int top;
    int n;

    int c;
	bool is_running;

    initscr();

    noecho();
    timeout(0);
    start_color();

    init_pair(0, COLOR_BLACK, COLOR_WHITE);
    init_pair(1, COLOR_BLACK, COLOR_RED);

    getmaxyx(stdscr, max_y, max_x);

    WINDOW *status_win = newwin(max_y-CTRL_NROWS, STATUS_NCOLS, 0, 0);
    refresh();
    box(status_win, 0, 0);
    mvwaddstr(status_win, 0, 1, " Status ");
    refresh_status_win(status_win, &org);

    mem_nrows = max_y - CTRL_NROWS;
    mem_ncols = max_x - STATUS_NCOLS;
    mem_win = newwin(mem_nrows, mem_ncols, 0, STATUS_NCOLS);
    box(mem_win, 0, 0);
    mvwaddstr(mem_win, 0, 1, " Memory ");
    refresh_mem_win(&arr);

    WINDOW *ctrl_win = newwin(CTRL_NROWS, max_x, max_y-CTRL_NROWS, 0);
    box(ctrl_win, 0, 0);
    mvwaddstr(ctrl_win, 0, 1, " Controls ");
    my_mvwaddstr(ctrl_win, 1, 2, "\016w\017 Up  \016a\017 Left  \016s\017 Down   \016d\017 Right");
    wnoutrefresh(ctrl_win);

	doupdate();

	is_running = false;
    for (int i = 0; ((i < 10000000) && (q.top != 0)); i++) {
		start_loop:
			refresh_status_win(status_win, &org);
			refresh_mem_win(&arr);
			doupdate();
			// While there is input from user
			while ((c = getch()) != ERR) {
				switch (c) {
					case 'w':
					case 'W':
						if (begin_i > 0) {
							begin_i -= 2;
						}
						break;
					case 's':
					case 'S':
						begin_i += 2;
						break;
					case 'a':
					case 'A':
						if (begin_j > 0) {
							begin_j -= 2;
						}
						break;
					case 'd':
					case 'D':
						begin_j += 2;
						break;
					case ' ':
						is_running = !is_running;
						break;
					case 'e':
						if (!is_running) {
							goto execute_cycle;
						}
						break;
					default:
						break;
				}
			}
			if (!is_running) {
				goto start_loop;
			}
			// Main logic
			execute_cycle:
				n = q.top;
				top = n-1;
				while (top > -1) {
					if (get_queue(&q, &org, top) == 0) {
						life(&arr, &org, &q);
						q.organisms[top] = org;
					}
					top--;
				}
				if (i%1000 == 0) {
					radiation(&arr);
				}
				update_queue(&q, &arr);
		}
	endwin();

	free(arr.content);
	free(q.organisms);
	fclose(stdout);
	return 0;
}

void refresh_status_win(WINDOW *status_win, organism *org)
{
    mvwprintw(status_win, 1, 2, "IP: %d, %d", org->startx+org->ptrx, org->starty+org->ptry);
	mvwprintw(status_win, 2, 2, "begin_i : %d, begin_j : %d", begin_i, begin_j);
	mvwprintw(status_win, 3, 2, " : %d,  : %d", org->startx+org->ptrx-begin_i, org->starty+org->ptry-begin_j);
	mvwprintw(status_win, 4, 2, "Current operation: %c", mvwinch(mem_win, org->startx+org->ptrx-begin_i, org->starty+org->ptry-begin_j));
	wnoutrefresh(status_win);
}

void refresh_mem_win(array *arr)
{
    int y, x;

    for (y = 1; y < mem_nrows-1; y++) {
        for (x = 1; x < mem_ncols-1; x++) {
            mvwaddch(mem_win, y, x, commands[array_get_command_code(arr, begin_i+y, begin_j+x)]);
        }
    }
    wnoutrefresh(mem_win);
}

void my_mvwaddstr(WINDOW *status_win, int y, int x, char *s)
{
    wmove(status_win, y, x);
    while (*s) {
        switch (*s) {
            case '\016':
                wattron(status_win, A_STANDOUT);
                break;
            case '\017':
                wattroff(status_win, A_STANDOUT);
                break;
            default:
                waddch(status_win, *s);
                break;
        }
        s++;
    }
}
