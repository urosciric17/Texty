#include <ncurses.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>

#define backspace() printw("\b \b")
#define XMAX 0
#define CTRL_W 23
#define BACKSPACE 8
#define TAB 9
#define ENTER 10
#define SPACE 32
#define CTRL_K 11
#define CTRL_S 19
#define DQUOTES 34
#define ESC 27
#define CTRL_C 3 
#define CTRL_V 22
 
#define last_col (cols-1)
#define last_row (rows-1)
#define initial_capacity (size_t)last_row
#define update_x_max() {getyx(stdscr,y,x); if(x_max[y]<last_col) x_max[y]++;}
#define update_lu_row() {getyx(stdscr,y,x); if(y>lu_row) lu_row=y}
#define pop_up(win,height,width,center_y,center_x,popup_str_len,popup_str) {win = newwin(height, width, center_y, center_x); box(win, 0, 0);\
mvwprintw(win, height/2, width/2-popup_str_len/2, "%s", popup_str); curs_set(0); wrefresh(win);}
#define move_and_scan(i,step,ch) {i+=step; move(y,i); ch=winch(stdscr);}
#define read_line_and_term(line,row) {read_line(line,row); line[last_col]=0;}

#define write_help(); { write_to_win(help_win,help_height,help_width,"This is the help window!",3);\
write_to_win(help_win,help_height,help_width,"Arrows are used to move through the file, but home and end are also used, to go to the start and end of the line, respectively",3);\
write_to_win(help_win,help_height,help_width,"Press ctrl+w to quit, ctrl+s to save, ctrl+shift+f to search and ctrl+left/right arrow to skip through words",3);\
write_to_win(help_win,help_height,help_width,"Also, press alt+h to open this help window whenever you need it :D",3);\
write_to_win(help_win,help_height,help_width,"Press any key to exit help...",3); }

#define NORMAL 0
#define HIGHLIGHTED 1
#define BLUE 2
#define RED 3
#define CYAN 4
#define MAGENTA 5
#define GREEN 6
#define YELLOW 7
#define initial_size cols
#define max_specifier_len 5
#define KEY_CTRL_LEFT 1000
#define KEY_CTRL_RIGHT 1001
#define ALT_H 1002
#define KEY_DELETE 1003
#define CTRL_END 1004
#define CTRL_HOME 1005

int rows, cols;

typedef struct
{
long long int top;
size_t capacity;
char** str_stack;
} stack_t;

bool is_empty(stack_t* stack)
{
return (stack->top<0);
}

bool is_full(stack_t* stack)
{
return (stack->top>=(long long int)stack->capacity);
}


stack_t* create_stack(size_t stack_size,size_t line_size)
{
stack_t* string_stack=malloc(sizeof(stack_t));
if(string_stack!=NULL)
{
(string_stack->str_stack)=malloc(stack_size*sizeof(char*));
for(size_t i=0;i<stack_size;i++)
{
(string_stack->str_stack)[i]=malloc(line_size);
}
string_stack->top=-1;
string_stack->capacity=initial_capacity;
}
return string_stack;
}

char** resize_stack(size_t new_size,size_t old_size,size_t line_size,stack_t* stack)
{
char help_lines[old_size][line_size];
for(size_t i=0;i<old_size;i++)
strcpy(help_lines[i],stack->str_stack[i]);
char** tmp=realloc(stack->str_stack,new_size*sizeof(char*));
if(tmp!=NULL)
{
if(tmp!=(stack->str_stack))
{
for(size_t i=0;i<new_size;i++)
{
tmp[i]=malloc(line_size);
if(i<old_size)
strcpy(tmp[i],help_lines[i]);
if(tmp[i]==NULL)
{
return NULL;
}
}
stack->str_stack=tmp;
}
else
{
for(size_t i=old_size;i<new_size;i++)
{
(stack->str_stack)[i]=malloc(line_size);
if((stack->str_stack)[i]==NULL)
return NULL;
}
}
stack->capacity=new_size;
}
return tmp;
}

void push(stack_t* stack,char* line,size_t line_size)
{
stack->top++;
if(is_full(stack))
resize_stack((stack->capacity+initial_capacity),stack->capacity,line_size,stack);
strcpy(stack->str_stack[stack->top],line);
}

char* pop(stack_t* stack)
{
return (!is_empty(stack)) ? stack->str_stack[stack->top--] : NULL;
}

void write_stack1_into_file(stack_t* stack,FILE* fp)
{
int lines_written=0;
if(!is_empty(stack))
{
while(lines_written<=stack->top)
{
fprintf(fp,"%s\n",stack->str_stack[lines_written]);
lines_written++;
}
}
}

void write_stack2_into_file(stack_t* stack,FILE* fp)
{
int lines_written=stack->top;
if(!is_empty(stack))
{
//memset(written_str,0,cols);
while(lines_written>=0)
{
fprintf(fp,"%s\n",stack->str_stack[lines_written]);
lines_written--;
}
}
}


void fill_stack_from_file(stack_t* stack,FILE* fp,char* line)
{
char read_ch;
size_t read;
int i=0;
unsigned cur_line=0;
long position=0;
position=ftell(fp);
while((read=fread(&read_ch,sizeof(char),1,fp))>0)
{
if(read_ch!='\n' && i<last_col)
i++;
else 
{
cur_line++;
if(cur_line>=stack->capacity)
resize_stack((stack->capacity+initial_capacity),stack->capacity,cols,stack);
i=0;
}
}
stack->top=cur_line;
fseek(fp,(position-1),SEEK_SET);
while((read=fread(&read_ch,sizeof(char),1,fp))>0)
{
if(read_ch!='\n' && i<last_col)
line[i++]=read_ch;
else 
{
line[i]=0;
strcpy(stack->str_stack[cur_line],line);
cur_line--;
i=0;
}
}
}

typedef struct
{
size_t size;
size_t index;
} Points_metadata;

typedef struct
{
unsigned x;
unsigned y;
} Point;

Point* create_points(Points_metadata* metadata)
{
Point* points=malloc(sizeof(Point)*metadata->size);
if(points==NULL)
return NULL;
else return points;
}

Point* more_points(Point* prev_points,size_t new_size,Points_metadata* metadata)
{
Point* new_points=realloc(prev_points,new_size*sizeof(Point));
if(new_points==NULL)
exit(69);
else metadata->size=new_size;
return new_points;
}

void add_point(Point** points,unsigned x_pos,unsigned y_pos,Points_metadata* metadata)
{
if(metadata->index>=metadata->size)
(*points)=more_points(*points,(metadata->size+initial_size),metadata);
(*points)[metadata->index].x=x_pos;
(*points)[metadata->index].y=y_pos;
metadata->index++;
}

void add_point_front(Point** points,unsigned x_pos,unsigned y_pos,Points_metadata* metadata)
{
if(metadata->index>=metadata->size)
(*points)=more_points(*points,(metadata->size+initial_size),metadata);
for(size_t i=metadata->index;i>0;i--)
(*points)[i]=(*points)[i-1];
(*points)[0].x=x_pos;
(*points)[0].y=y_pos;
metadata->index++;
}

void reset_points(Points_metadata* metadata,int* highlight_count)
{
metadata->index=0;
*highlight_count=0;    
}

void delete_last_point(Points_metadata* metadata)
{
if(metadata->index>0)
metadata->index--;
}

void delete_point_front(Point* points,Points_metadata* metadata)
{
if(metadata->index>0)
{
for(unsigned i=0;i<metadata->index-1;i++)
points[i]=points[i+1];
metadata->index--;
}
}

int x=0,y=0;
long long unsigned int first_row=0;
bool saved=true;
const char popup_str[]="Would you like to save this file before closing? Press 'c' to cancel, 'y' to save or 'n' to exit without saving";
const char help_str[]="This is the help window. Just press Alt+h whenever you need help. Press any key to exit...";
void exit_w_err(int exit_code,const char* msg)
{
fprintf(stderr,"%s\n",msg);
exit(exit_code);
}

bool is_fspecifier(char* word)
{
const char* specifiers[]={"%f","%d","%s","%c","%lf","%ld","%lu","%ld","%llu","%lld","%zu","%hu","%hhu","%hd","%hhd"};
const size_t size=sizeof(specifiers)/sizeof(specifiers[0]);
for(size_t i=0;i<size;i++)
{
if(!strcmp(specifiers[i],word))
return true;
}
return false;
}

bool mv_crsr(int x_offset,int y_offset)
{
getyx(stdscr,y,x);
if(move(y+y_offset,x+x_offset)==OK)
{
getyx(stdscr,y,x);
return true;
}
else return false;
}

void dlt_lst_chr(int* x_max)
{
getyx(stdscr,y,x);
if(x>0)
{
backspace();
delch();
if(x_max[y]>0)
x_max[y]--;
}
}

static char* remove_punct(char* str,char* punct1,char* punct2)
{
if(str==NULL || punct1== NULL || punct2==NULL)
return NULL;
static char word[200];
unsigned i=0,j=0,k=0;
while(ispunct(str[i]))
{
punct1[i]=str[i];
i++;
}
punct1[i]=0;
while(!ispunct(str[i]) && str[i]!=0)
{
word[j]=str[i];
i++;
j++;
}
while(ispunct(str[i]))
{
punct2[k]=str[i];
k++;
i++;
}
punct2[k]=0;

word[i]=0;
return word;
}

void write_to_win(WINDOW* win,int height,int width,const char* str,short unsigned offset_step)
{
static unsigned offset=0;
mvwprintw(win, height/5+offset, width/2-strlen(str)/2, "%s", str);
offset+=offset_step;
if(offset>12)
offset=0;
wrefresh(win);
}

bool is_type(char* word)
{
char* types[]={"int","unsigned","float","double","char","bool","void","static","extern","struct","union","const","long","short","volatile"};
const size_t types_size=sizeof(types)/sizeof(types[0]);
for(size_t i=0;i<types_size;i++)
{
if(!strcmp(types[i],word))
return true;
}
return false;
}


bool is_keyword(char* word)
{
char* keywords[]={"return","goto","break","for","while","continue","do","true","false"};
const size_t keywords_size=sizeof(keywords)/sizeof(keywords[0]);
for(size_t i=0;i<keywords_size;i++)
{
if(!strcmp(keywords[i],word))
return true;
}
return false;
}

char* get_word(void)
{
static char word[50000];
unsigned long i=0;
while(word[i]!=' ')
{
word[i]=winch(stdscr);
i++;
mv_crsr(1,0);
}
return word;
}

void set_title(const char* title,char* filename,char* cur_dir) {
    if(filename==NULL)
    filename="Untitled";
    printf("\033]0;%s/%s - %s\007",cur_dir,filename,title); // Send the escape sequence
    fflush(stdout);                // Ensure it's sent immediately
}

void print_colored_char(char ch,unsigned short color)
{
attron(COLOR_PAIR(color));
printw("%c",ch);
attroff(COLOR_PAIR(color));
}

void go_to_beg(void)
{
char read_ch=0;
getyx(stdscr,y,x);
move(y,0);
read_ch=winch(stdscr);
while(!isprint(read_ch) || read_ch==' ')
{
read_ch=winch(stdscr);
mv_crsr(1,0);
if(x==last_col)
return;
}
mv_crsr(-1,0);
}

void go_to_end(void)
{
char read_ch=0;
getyx(stdscr,y,x);
move(y,last_col);
read_ch=winch(stdscr);
while(!isprint(read_ch) || read_ch==' ')
{
read_ch=winch(stdscr);
mv_crsr(-1,0);
if(x==0)
return;
}
mv_crsr(2,0);
}

void clear_line(int y,int* x_max)
{
move(y,0);
for(int i=0;i<last_col;i++)
printw(" ");
move(y,0);
x_max[y]=0;
}

bool are_dquotes_before(char* str,int end)
{
unsigned dq=0;
while(end>=0)
{
if(str[end]==DQUOTES)
dq++;
end--;
}
if(dq%2!=0)
return true;
else return false;
}

char* find_directive(char* line)
{
char* start;
int prev;
if(start=strstr(line,"#include "))
{
if(start==line)
return start;
prev=start-line-1;
if(are_dquotes_before(line,prev))
return NULL;
else return start;
}
else if(start=strstr(line,"#define "))
{
if(start==line)
return start;
prev=start-line-1;
if(are_dquotes_before(line,prev))
return NULL;
else return start;
}
else return NULL;
}

void clear_part_of_line(int y,int* x_max,int start,int end)
{
for(int i=start;i<end;i++)
{
move(y,i);
printw(" ");
}
move(y,start);
x_max[y]=start;
}

size_t prints(char* str,short unsigned* colors)
{
size_t i=0;
while(str[i])
{
if(str[i]!=' ' && colors!=NULL)
print_colored_char(str[i],colors[i]);
else printw("%c",str[i]);
i++;
}
return i;
}

void overwrite_line(int y,int* x_max,char* with,short unsigned* colors)
{
clear_line(y,x_max);
move(y,0);
prints(with,colors);
x_max[y]=strlen(with);
}

void print_part_of_str(char* str,unsigned start,unsigned end)
{
for(unsigned i=start;i<end;i++)
printw("%c",str[i]);
}

int get_ch_at_pos(int y_pos,int x_pos)
{
int ret_ch;
getyx(stdscr,y,x);
move(y_pos,x_pos);
ret_ch=winch(stdscr);
move(y,x);
return ret_ch;
}

bool is_empty_space(int c)
{
if(ispunct(c) || isalnum(c))
return false;
else return true;
}

void insert_ch(int ch) 
{
insch(ch);
mv_crsr(1,0);
}

void move_to_line_end(void)
{
move(y,last_col);
}

void move_to_line_beg(void)
{
move(y,0);
}

int scan_line(void)
{
int cur_ch,last_ch_pos=0;
for(int i=0;i<cols;i++)
{
cur_ch=winch(stdscr);
if(!isspace(cur_ch))
last_ch_pos=i;
move(y,i);
}
move(y,x);
return last_ch_pos;
}

bool is_line_empty(int* x_max,int y)
{
if(x_max[y]==0)
return true;
else return false;
}

int get_lu_row(int* x_max)
{
int lu_row=0;
for(int i=0;i<rows;i++)
{
if(!is_line_empty(x_max,i))
lu_row=i;
}
return lu_row;
}

void read_line(char* line,int row)
{
getyx(stdscr,y,x); // get current position
for(int i=0;i<cols;i++)
{
move(row,i);
line[i]=winch(stdscr);
} // put the characters in the line inside the array
move(y,x); //restore the position
}

void read_line_upto(char* line,short unsigned* colors,int row,int* x_max,int from,int to)
{
int i=from;
chtype ch;
getyx(stdscr,y,x); // get current position
if(to<=0)
to=x_max[row]-abs(to);
for(i=from;i<to;i++)
{
move(row,i);
ch=winch(stdscr);
if(colors!=NULL)
colors[i-from]=PAIR_NUMBER(ch & A_COLOR);
line[i-from]=ch;
} // put the characters in the line inside the array
line[i-from]=0;
move(y,x); //restore the position
}

void replace_colored_chars(int row,int from,int to,int* x_max,unsigned short old_color,unsigned short new_color)
{
chtype ch;
int x_copy,y_copy;
getyx(stdscr,y_copy,x_copy);
for(int i=from;i<to;i++)
{
move(row,i);
ch=winch(stdscr);
if(old_color==PAIR_NUMBER(ch & A_COLOR))
{
print_colored_char((ch & A_CHARTEXT),new_color);
}
}
move(y_copy,x_copy);
}

void get_colors(short unsigned* colors,int row,int* x_max)
{
int x_copy,y_copy;
int i;
chtype ch;
getyx(stdscr,y,x);
for(i=0;i<x_max[row];i++)
{
move(row,i);
ch=winch(stdscr);
colors[i]=PAIR_NUMBER(ch & A_COLOR);
}
move(y_copy,x_copy);
}

void redraw_lines(char* line,int* x_max,int starting_row,int row_num)
{
int x_copy,y_copy;
short unsigned colors[cols];
getyx(stdscr,y_copy,x_copy);
for(int i=starting_row;i<starting_row+row_num;i++)
{
read_line_upto(line,colors,i,x_max,0,XMAX);
move(i,0);
prints(line,colors);
}
move(y_copy,x_copy);
}

void create_empty_file(FILE* fp,char** argv)
{
if(argv[1]!=NULL)
fp=fopen(argv[1],"w");
else fp=fopen("Untitled","w");
fclose(fp);
}

bool file_exists(char* filename)
{
DIR* dir=opendir(".");
struct dirent* entry;
while(1)
{
entry=readdir(dir);
if(entry==NULL)
{
closedir(dir);
return false;
}
if(!strcmp(entry->d_name,filename) && entry->d_type==DT_REG)
{
closedir(dir);
return true;
}
}
}

void color_init(void)
{
start_color();
init_pair(0,COLOR_WHITE,COLOR_BLACK);
init_pair(1, COLOR_BLACK, COLOR_WHITE);
init_pair(2, COLOR_BLUE, COLOR_BLACK);
init_pair(3, COLOR_RED, COLOR_BLACK);
init_pair(4, COLOR_CYAN, COLOR_BLACK); 
init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
init_pair(6, COLOR_GREEN, COLOR_BLACK);
init_pair(7, COLOR_YELLOW, COLOR_BLACK);
}

void print_colored_word(char* word,unsigned color_pair)
{
attron(COLOR_PAIR(color_pair));
printw("%s",word);
attroff(COLOR_PAIR(color_pair));
}

void highlight_line(int y)
{
attron(COLOR_PAIR(HIGHLIGHTED));
move(y,0);
attroff(COLOR_PAIR(HIGHLIGHTED));
}

void get_file_ext(char* filename,char* ext)
{
size_t file_len=strlen(filename);
size_t i=file_len-1,j=0;
while(i>0 && filename[i]!='.')
{
ext[j]=filename[i];
i--;j++;
}
ext[j]=0;
}

void color_fspecifiers(char* ln,int y_copy,unsigned i)
{
static short unsigned j=0;
static bool began=false;
char specifier[max_specifier_len];
if(ln[i]=='%' && began==false)
began=true;
else if(ln[i]=='%' && began==true)
{
j=0;
if(is_fspecifier(specifier))
{
move(y_copy,i-strlen(specifier)+1);
print_colored_word(specifier,RED);
}
}
if(began==true && j<max_specifier_len-1)
{
specifier[j]=ln[i];
specifier[j+1]=0;
if(is_fspecifier(specifier))
{
move(y_copy,i-strlen(specifier)+1);
print_colored_word(specifier,RED);
j=0;
began=false;
}
else j++;
}
else if(began==true && j==max_specifier_len-1){
j=0;
if(is_fspecifier(specifier))
{
move(y_copy,i-strlen(specifier)+1);
print_colored_word(specifier,RED);
}
began=false;
}
}

bool highlight_char(char dir)
{
char ch;
curs_set(0);
switch(dir)
{
case -1:
if(mv_crsr(dir,0))
{
ch=winch(stdscr);
attron(COLOR_PAIR(HIGHLIGHTED));
printw("%c",ch);
attroff(COLOR_PAIR(HIGHLIGHTED));
mv_crsr(dir,0);
}
else return false;
break;
case 1:
ch=winch(stdscr);
attron(COLOR_PAIR(HIGHLIGHTED));
printw("%c",ch);
attroff(COLOR_PAIR(HIGHLIGHTED));
break;
}
return true;
}

bool unhighlight_char(char dir)
{
char ch;
curs_set(0);
switch(dir)
{
case -1:
if(mv_crsr(dir,0))
{
ch=winch(stdscr);
attron(COLOR_PAIR(NORMAL));
printw("%c",ch);
attroff(COLOR_PAIR(NORMAL));
mv_crsr(dir,0);
}
else return false;
break;
case 1:
ch=winch(stdscr);
attron(COLOR_PAIR(NORMAL));
printw("%c",ch);
attroff(COLOR_PAIR(NORMAL));
break;
}
return true;
}

void save_file(char** argv,char* line,int* x_max,stack_t* stack1,stack_t* stack2)
{
unsigned i;
FILE* fp;
int lu_row=get_lu_row(x_max);
if(argv[1]!=NULL)
fp=fopen(argv[1],"w");
else fp=fopen("Untitled","w");
write_stack1_into_file(stack1,fp);
for (i=0;i<=last_row;i++)
{
read_line_upto(line,NULL,i,x_max,0,XMAX);
fprintf(fp,"%s\n",line);
}
write_stack2_into_file(stack2,fp);
fclose(fp);
}

char* find_next_substr(char* haystack,const char* needle)
{
static int cur_index;
static char* cur_ptr=NULL;
if(cur_ptr==NULL)
cur_index=0;
cur_ptr=strstr(haystack+cur_index,needle);
if(cur_ptr!=NULL)
cur_index=cur_ptr-haystack+3;
return cur_ptr;
}

char* find_next_substr2(char* haystack,const char* needle)
{
static int cur_index;
static char* cur_ptr=NULL;
if(cur_ptr==NULL)
cur_index=0;
cur_ptr=strstr(haystack+cur_index,needle);
if(cur_ptr!=NULL)
cur_index=cur_ptr-haystack+3;
return cur_ptr;
}

void highlight_syntax(int* x_max,char* filename,int y_copy)
{
char ch;
char ln[cols];
char word[cols];
char ext[256];
get_file_ext(filename,ext);
if(strcmp(ext,"c")!=0)
return;
unsigned i=0,j=0,start=0;
size_t wordlen=0;
unsigned pos=0;
read_line_upto(ln,NULL,y_copy,x_max,0,XMAX);
int saved_x_max=0;
char* start_ptr;
unsigned length=strlen(ln);
ln[length]=' ';
ln[length+1]=0;
while(ln[i])
{
if ( (ispunct(ln[i]) && ln[i]!='_' && ln[i]!='#') || ln[i]==' ')
{
word[j]=0;
wordlen=strlen(word);
pos=i-wordlen;
move(y_copy,pos);
if(is_type(word))
print_colored_word(word,BLUE);
else if(is_keyword(word))
print_colored_word(word,RED);
else printw("%s",word);
pos=i;
j=0;
}
else
{
word[j]=ln[i];
j++;
}
i++;
}
{
unsigned i=0,dq=0;
while(ln[i])
{
if(ln[i]==DQUOTES)
dq++;
if(dq%2!=0 || ln[i]==DQUOTES)
{
move(y_copy,i);
color_fspecifiers(ln,y_copy,i);
if(ln[i]=='\\' && (ln[i+1]=='b' || ln[i+1]=='n' || ln[i+1]=='t'))
{
print_colored_char(ln[i],RED);
print_colored_char(ln[i+1],RED);
i++;
}
else if(ln[i]!=' ')
print_colored_char(ln[i],GREEN);
else printw("%c",ln[i]);
}
i++;
}
}

if(start_ptr=find_directive(ln))
{
start=start_ptr-ln;
attron(COLOR_PAIR(MAGENTA)); //MAGENTA on BLACK
move(y_copy,start);
print_part_of_str(ln,start,x_max[y_copy]);
attroff(COLOR_PAIR(MAGENTA));
}
if(start_ptr=strchr(ln,'<'))
{
char* end_ptr=strchr(start_ptr,'>');
if(end_ptr!=NULL)
{
start=start_ptr-ln;
unsigned end=end_ptr-ln;
if(end>start)
{
move(y_copy,start);
for(unsigned i=start;i<=end;i++)
print_colored_char(ln[i],YELLOW);
}
}
}
if(start_ptr=strstr(ln,"//"))
{
start=start_ptr-ln;
attron(COLOR_PAIR(CYAN)); //CYAN on BLACK
move(y_copy,start);
print_part_of_str(ln,start,x_max[y_copy]);
attroff(COLOR_PAIR(CYAN));
}
else replace_colored_chars(y_copy,0,x_max[y_copy],x_max,CYAN,NORMAL);
while(start_ptr=find_next_substr(ln,"/*"))
{
unsigned end=0;
start=start_ptr-ln;
char* end_ptr=find_next_substr2(ln,"*/");
if(end_ptr!=NULL)
end=end_ptr-ln+2;
else end=x_max[y_copy];
move(y_copy,start);
attron(COLOR_PAIR(CYAN)); //CYAN on BLACK
print_part_of_str(ln,start,end);
attroff(COLOR_PAIR(CYAN));
}
move(y,x);
}


void load_file(char** argv,char* line,int* x_max,stack_t* stack2)
{
size_t read;
char read_ch;
int i=0,cur_row=0,reset_value=0;
FILE* fp;
fp=fopen(argv[1],"r");
while((read=fread(&read_ch,sizeof(char),1,fp))>0 && cur_row<rows)
{
if(read_ch!='\n' && i<last_col)
line[i++]=read_ch;
else 
{
line[i]=0;
printw("%s",line);
x_max[cur_row]=i;
highlight_syntax(x_max,argv[1],cur_row);
if(i==last_col)
{
reset_value=1;
line[0]=read_ch;
}
else reset_value=0;
getyx(stdscr,y,x);
move(y+1,0);
cur_row++;
i=reset_value;
}
}
line[i]=0;
printw("%s",line);
x_max[cur_row]=i;
highlight_syntax(x_max,argv[1],cur_row);
fill_stack_from_file(stack2,fp,line);
fclose(fp);
}

void delete_highlighted(Point* points,Points_metadata* metadata,int* x_max)
{
for(size_t i=0;i<metadata->index;i++)
{
move(points[i].y,points[i].x+1);
dlt_lst_chr(x_max);
}
} 

void delete_highlighted_reverse(Point* points,Points_metadata* metadata,int* x_max)
{
for(long long i=metadata->index-1;i>=0;i--)
{
move(points[i].y,points[i].x+1);
dlt_lst_chr(x_max);
}
}

void skip_word(int* x_max,char dir)
{
assert(dir==1 || dir==-1);
char ch=0;
unsigned chars=0;
int x_copy,y_copy;
int i;
if(x==x_max[y] && dir>0)
{
move(y+1,0);
return;
}
else if(x==1 && dir<0)
{
move(y,0);
return;
}
else if(x==0 && dir<0)
{
move(y-1,x_max[y-1]);
return;
}
if(dir>0)
{
i=x;
ch=winch(stdscr);
}
else 
{
i=x+dir;
ch=get_ch_at_pos(y,i);
}
if(ispunct(ch))
{
while(ispunct(ch) && i<=x_max[y] && i>=0)
{
move_and_scan(i,dir,ch);
}
if(dir<0)
mv_crsr(1,0);
}
else if(ch==' ')
{
while(ch==' ' && i<=x_max[y] && i>=0)
{
move_and_scan(i,dir,ch);
}
if(dir<0)
mv_crsr(1,0);
}
else {
if(dir>0)
{
while(i<x_max[y] && isalnum(ch))
move_and_scan(i,dir,ch);
}
else
{
while(i<=x_max[y] && isalnum(ch) && i>=0)
move_and_scan(i,dir,ch);
if((x-i)>1 && i>0)
move(y,i+1);
}
}
getyx(stdscr,y_copy,x_copy);
if(x_copy>x_max[y_copy])
move(y_copy,x_max[y_copy]);
}

void enter(char* line,int* x_max,int last_used_row,stack_t* stack2,char** argv)
{
int cur_row=last_row-1;
int x_copy,y_copy;
char help_line[cols];
short unsigned colors[cols];
getyx(stdscr,y_copy,x_copy);
while(cur_row>y_copy)
{
move(cur_row,0);
read_line_upto(line,colors,cur_row,x_max,0,XMAX);
clear_line(cur_row,x_max);
move(cur_row+1,0);
if(cur_row==last_row-1)
{
read_line_upto(help_line,NULL,last_row,x_max,0,XMAX);
push(stack2,help_line,cols);
}
clear_line(cur_row+1,x_max);
prints(line,colors);
x_max[cur_row+1]=strlen(line);
cur_row--;
}
read_line_upto(help_line,NULL,y_copy,x_max,x_copy,XMAX);
clear_part_of_line(y_copy,x_max,x_copy,x_max[y_copy]);
move(y_copy+1,0);
printw("%s",help_line);
x_max[y_copy+1]=strlen(help_line);
move(y_copy,0);
highlight_syntax(x_max,argv[1],y_copy);
move(y_copy+1,0);
highlight_syntax(x_max,argv[1],y_copy);
}

void scroll_down(char* line,int* x_max,stack_t* stack,stack_t* stack2,char** argv)
{
size_t popped=0;
read_line_upto(line,NULL,0,x_max,0,XMAX);
push(stack,line,cols);
scroll(stdscr);
move(last_row,0);
if(!is_empty(stack2))
{
popped=prints(pop(stack2),NULL);
}
for(unsigned i=0;i<last_row;i++)
x_max[i]=x_max[i+1];
x_max[last_row]=popped;
highlight_syntax(x_max,argv[1],last_row-1);
first_row++;
}

void scroll_up(char* line,int* x_max,char** argv,stack_t* stack,stack_t* stack2)
{
if(first_row>0)
{
char cur_line[cols],next_line[cols];
read_line_upto(line,NULL,last_row,x_max,0,XMAX);
push(stack2,line,cols);
if(is_empty(stack))
strcpy(cur_line,"");
else strcpy(cur_line,pop(stack));
for(int i=-1;i<last_row;i++)
{
read_line_upto(next_line,NULL,i+1,x_max,0,XMAX);
overwrite_line(i+1,x_max,cur_line,NULL);
highlight_syntax(x_max,argv[1],i+1);
strcpy(cur_line,next_line);
}
move(0,x_max[0]);
first_row--;
}
}

void insert_tab(int row,int* x_max)
{
for(short unsigned i=0;i<4;i++)
insert_ch(' ');
x_max[row]+=4;
}

void modify_line(const char* insert_str,int* x_max,int row,bool before)
{
unsigned i=0;
getyx(stdscr,y,x);
if(before)
move(row,0);
else move(row,x_max[row]);
while(insert_str[i])
insert_ch(insert_str[i]);
move(y,x);
}

void reverse_enter(char* line,int* x_max,int last_used_row,stack_t* stack2,char** argv)
{
short unsigned colors[cols];
int x_copy,y_copy;
size_t line_len=0;
getyx(stdscr,y_copy,x_copy);
int cur_row=y_copy;
if(y_copy!=0)
{
while(cur_row<=last_used_row)
{
move(cur_row,0);
read_line_upto(line,colors,cur_row,x_max,0,XMAX);
clear_line(cur_row,x_max);
move(cur_row-1,x_max[cur_row-1]);
prints(line,colors);
if(cur_row==y_copy)
{
line_len=strlen(line);
x_max[cur_row-1]+=line_len;
}
else x_max[cur_row-1]=strlen(line);
cur_row++;
}
if(!is_empty(stack2))
{
move(last_row,0);
size_t popped=prints(pop(stack2),NULL);
x_max[last_row]=popped;
highlight_syntax(x_max,argv[1],last_row);
}
move(y_copy-1,x_max[y_copy-1]-line_len);
highlight_syntax(x_max,argv[1],y_copy-1);
}
}

void enter_and_push(char* line,int* x_max,stack_t* stack2)
{
char help_line[cols];
read_line_upto(line,NULL,last_row,x_max,0,XMAX);
push(stack2,line,cols);
read_line_upto(help_line,NULL,last_row-1,x_max,0,x);
read_line_upto(line,NULL,last_row-1,x_max,x,XMAX);
clear_part_of_line(last_row-1,x_max,x,x_max[last_row-1]);
overwrite_line(last_row,x_max,line,NULL);	 
}

void enter_and_scroll(char* line,int* x_max,stack_t* stack,stack_t* stack2,char** argv)
{
char* help_line=(char*)calloc(1,cols);
read_line_upto(help_line,NULL,last_row,x_max,x,XMAX);
clear_part_of_line(last_row,x_max,x,x_max[last_row]);
highlight_syntax(x_max,argv[1],last_row);
scroll_down(line,x_max,stack,stack2,argv);
move(last_row,0);
printw("%s",help_line);
x_max[last_row]=strlen(help_line);
free(help_line);
}

void copy_highlighted(Point* points,Points_metadata* metadata,char* clipboard)
{
char ch;
long long i;
for(i=metadata->index-1;i>=0;i--)
{
move(points[i].y,points[i].x);
ch=winch(stdscr);
clipboard[metadata->index-1-i]=ch;
}
clipboard[metadata->index]=0;
}

void insert_str(char* str,int* x_max)
{
int i=0;
char read_ch;
while(str[i])
{
if(x_max[y]==last_col)
{
if(move(y+1,0)==OK)
{
insert_ch(str[i]);
x_max[y+1]++;
}
}
else
{ 
insert_ch(str[i]);
x_max[y]++;
}
i++;
}
}

void paste_highlighted(char* clipboard,int* x_max)
{
insert_str(clipboard,x_max);
}

int main(int argc,char** argv) {
    chtype ch=0,read_ch,prev_ch=0,exit_ch=0;
    int x_copy,y_copy,last_x=0;
    char cwd[1024];
    if(argc>2)
    exit_w_err(69,"Error: Wrong usage\nCorrect usage: texty <filename>");
    initscr();             // Initialize ncurses
    raw();                 // Disable line buffering
    keypad(stdscr, TRUE);  // Enable special keys
    noecho();                // Disable echo for automatic character display
    curs_set(1);
    scrollok(stdscr, TRUE);
    color_init();
    define_key("\033[1;5D", KEY_CTRL_LEFT); // Map Ctrl+Left Arrow
    define_key("\033[1;5C", KEY_CTRL_RIGHT); 
    define_key("\033h", ALT_H);
    define_key("\033[3~", KEY_DELETE);
    define_key("\033", ESC);
    define_key("\033[1;5H", CTRL_HOME);
    define_key("\033[1;5F", CTRL_END); 
    getcwd(cwd,sizeof(cwd));
    set_title("Texty",argv[1],cwd);
    getmaxyx(stdscr,rows,cols);
    int x_max[rows+1],lu_row=0;
    char word[cols];
    char line[cols];
    char* clipboard=malloc(cols);
    WINDOW* popup;
    WINDOW* help_win;
    int popup_height = rows/10, popup_width = 3*cols/4;
    int center_x=cols/2-popup_width/2;
    int center_y=rows/2-popup_height/2;
    int help_height = rows/2 , help_width = 3*cols/4;
    int help_center_x=cols/2-help_width/2;
    int help_center_y=rows/2-help_height/2;
    int last_used_row=0;
    size_t popup_str_len=strlen(popup_str);
    size_t help_str_len=strlen(help_str);
    bool left_highlight=false,right_highlight=false;
    int highlight_count=0;
    unsigned int letter_count=0;
    stack_t* string_stack=create_stack(rows,cols);
    stack_t* string_stack2=create_stack(rows,cols);
    Points_metadata metadata={.size=initial_size,.index=0};
    Point* points=create_points(&metadata);
    for(int i=0;i<rows;i++)
    x_max[i]=0;
    if(argv[1]!=NULL)
    {
    if(file_exists(argv[1]))
    load_file(argv,line,x_max,string_stack2);
    }
    move(0,x_max[0]);
    refresh();
    while (1) {  // Exit when ctlr-W is pressed
    	left_highlight=false; right_highlight=false;
    	ch=getch();
    	getyx(stdscr,y,x);
    	getmaxyx(stdscr, rows, cols);
    	x_copy=x; y_copy=y;
        if(isprint(ch))
        {
        delete_highlighted(points,&metadata,x_max);
        if(x==last_col)
        {
        if(y!=last_row)
        move(y+1,0);
 	else scroll_down(line,x_max,string_stack,string_stack2,argv);
 	insert_ch(ch);
        }
        else if(prev_ch!=BACKSPACE)
	{
	if (x_max[y]==(cols-1))
	{
	read_ch=get_ch_at_pos(y,cols-2);
	move_to_line_end();
	printw("\b ");
	move(y+1,0);
	insert_ch(read_ch);
	update_x_max();
	move(y_copy,x_copy);
	}
	getyx(stdscr,y,x);
	insert_ch(ch);
	}
        else 
        {
        insert_ch(ch);
        }
        update_x_max();
        saved=false;
        }      
	 else
	 {
	 switch(ch)
	 {
	 case CTRL_END:
	 move(last_row,x_max[last_row]);
	 break;
	 case CTRL_HOME:
	 move(0,x_max[0]);
	 break;
	 case TAB:
	 insert_tab(y,x_max);
	 break;
	 case KEY_DELETE:
	 clear_line(y,x_max);
	 saved=false;
	 break; 
	 case ALT_H:
	 pop_up(help_win,help_height,help_width,help_center_y,help_center_x,0,"");
	 write_help();
	 getch();
	 curs_set(1);
	 wclear(help_win);
	 wrefresh(help_win);           // Refresh  to apply the change
	 redraw_lines(line,x_max,help_center_y,help_height);
	 break;
	 case KEY_CTRL_LEFT:
	 skip_word(x_max,-1);
	 break;
	 case KEY_CTRL_RIGHT:
	 skip_word(x_max,1);
	 break;
	 case KEY_LEFT:
	 if(x==0)
	 move(y-1,x_max[y-1]);
	 else
	 move(y,x-1);
	 break;
	 case KEY_RIGHT:
	 if(x>=x_max[y])
	 move(y+1,0);
	 else
	 move(y,x+1);
	 break;
	 case KEY_UP:
	 if(y!=0)
	 move(y-1,x_max[y-1]);
	 else
	 scroll_up(line,x_max,argv,string_stack,string_stack2);
	 break;
	 case ENTER:
	 if(y==last_row-1)
	 enter_and_push(line,x_max,string_stack2);
	 if(y==last_row)
	 {
	 enter_and_scroll(line,x_max,string_stack,string_stack2,argv);
	 }
	 else enter(line,x_max,last_used_row,string_stack2,argv);
	 saved=false;
	 break;
	 case KEY_DOWN:
	 if(y!=last_row)
	 move(y+1,x_max[y+1]);
	 else scroll_down(line,x_max,string_stack,string_stack2,argv);
	 break;
	 case BACKSPACE:
	 if(prev_ch==KEY_SLEFT || prev_ch==KEY_SRIGHT)
	 {
	 delete_highlighted(points,&metadata,x_max);
	 reset_points(&metadata,&highlight_count);
	 }
	 else {
	 if(x>0)
         {	
	 dlt_lst_chr(x_max);
	 }
	 else
	 {
	 if(y==0)
	 scroll_up(line,x_max,argv,string_stack,string_stack2);
	 reverse_enter(line,x_max,last_used_row,string_stack2,argv);
	 }
	 }
	 saved=false;
	 break;
	 case KEY_HOME:
	 if(scan_line()!=0)
	 go_to_beg();
	 else move_to_line_beg();
	 break;
	 case KEY_END:
	 move(y,x_max[y]);
	 break;
	 case CTRL_C:
	 if(metadata.index>0)
	 copy_highlighted(points,&metadata,clipboard);
	 //reset_points(&metadata,&highlight_count);
	 left_highlight=true;
	 right_highlight=true;
	 move(y,x);
	 break;
	 case CTRL_V:
	 paste_highlighted(clipboard,x_max);
	 if(strlen(clipboard)>0)
	 saved=false;
	 break;
	 case CTRL_K:
	 clear_part_of_line(y,x_max,x,x_max[y]);
	 break;
	 case KEY_SLEFT:
	 if(highlight_count>0)
	 {
	 unhighlight_char(-1);
	 delete_point_front(points,&metadata);
	 highlight_count--;
	 }
	 else
	 { 
	 if(highlight_char(-1))
	 {
	 add_point(&points,x,y,&metadata);
	 highlight_count--;
	 }
	 }
	 if(highlight_count<0)
	 left_highlight=true;
	 else if (highlight_count>0)
	 right_highlight=true;
	 break;
	 case KEY_SRIGHT:
	 if(highlight_count<0)
	 {
	 unhighlight_char(1);
	 delete_last_point(&metadata);
	 highlight_count++;
	 }
	 else if(x<x_max[y])
	 {
	 if(highlight_char(1))
	 {
	 add_point_front(&points,x,y,&metadata);
	 highlight_count++;
	 }
	 }
	 if(highlight_count<0)
	 left_highlight=true;
	 if(highlight_count>0)
	 right_highlight=true;
	 break;
	 case CTRL_S:
	 save_file(argv,line,x_max,string_stack,string_stack2);
	 saved=true;
	 break;
	 case ESC:
	 case CTRL_W:
	 if(saved==false)
	 {
	 pop_up(popup,popup_height,popup_width,center_y,center_x,popup_str_len,popup_str);
	 start:
	 exit_ch=getch();
	 if(exit_ch=='n');
	 else if(exit_ch=='y')
	 save_file(argv,line,x_max,string_stack,string_stack2);
	 else if(exit_ch=='c')
	 {
	 curs_set(1);
	 wclear(popup);
	 wrefresh(popup);           // Refresh  to apply the change
	 redraw_lines(line,x_max,center_y,popup_height);
	 break;
	 }
	 else goto start;
	 }
	 goto end;
	 break;
	 }
	}
	if(left_highlight==false && right_highlight==false)
	{
	curs_set(1);
	reset_points(&metadata,&highlight_count);
	highlight_syntax(x_max,argv[1],y);
	if((y-y_copy)==1)
	replace_colored_chars(y-1,0,x_max[y-1],x_max,HIGHLIGHTED,NORMAL);
	else if((y-y_copy)==-1)
	replace_colored_chars(y+1,0,x_max[y+1],x_max,HIGHLIGHTED,NORMAL);
	else replace_colored_chars(y,0,x_max[y],x_max,HIGHLIGHTED,NORMAL);
	}
	last_used_row=get_lu_row(x_max);
	prev_ch=ch;
	refresh();  // Refresh the screen to update output
    }
    end:
    delwin(popup);
    delwin(help_win);
    endwin();  // End ncurses and clean up
    return 0;
}
