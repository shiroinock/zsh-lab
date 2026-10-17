#ifndef TERMINAL_H
#define TERMINAL_H
#include <stdint.h>
#include <wchar.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct { char text[24]; short fg,bg; unsigned char width, bold, reverse, underline; } Cell;
typedef struct {
 int rows,cols,x,y,sx,sy,top,bottom;
 int main_x,main_y;
 bool hidden,alt,wrap,wrap_pending,origin,insert,bracket;
 Cell *grid,*primary,*alternate;
 Cell pen;
 int state,seq_len,utf_len,utf_need,charset_target;
 char seq[128],utf[8];
 bool graphics[2]; int charset;
} Terminal;

static Cell blank_cell(void) { Cell c={.text=" ",.fg=-1,.bg=-1,.width=1};return c; }
static void cells_blank(Cell *c,size_t n) { Cell b=blank_cell();for(size_t i=0;i<n;i++)c[i]=b; }
static void term_reset(Terminal *t) {
 t->x=t->y=t->sx=t->sy=t->top=0;t->bottom=t->rows-1;
 t->hidden=t->wrap_pending=t->origin=t->insert=false;t->wrap=true;
 t->pen=blank_cell();t->charset=0;t->graphics[0]=t->graphics[1]=false;
 cells_blank(t->grid,(size_t)t->rows*t->cols);
}
static void term_init(Terminal *t,int rows,int cols) {
 memset(t,0,sizeof(*t));t->rows=rows;t->cols=cols;
 t->primary=calloc((size_t)rows*cols,sizeof(Cell));t->alternate=calloc((size_t)rows*cols,sizeof(Cell));
 if(!t->primary||!t->alternate){fputs("画面用メモリを確保できません\n",stderr);exit(1);}
 t->grid=t->primary;cells_blank(t->alternate,(size_t)rows*cols);term_reset(t);
}
static void term_free(Terminal*t){free(t->primary);free(t->alternate);}
static void term_resize(Terminal*t,int rows,int cols){
 if(rows==t->rows&&cols==t->cols)return;
 Cell *a=calloc((size_t)rows*cols,sizeof(Cell)),*b=calloc((size_t)rows*cols,sizeof(Cell));
 if(!a||!b){free(a);free(b);return;}
 cells_blank(a,(size_t)rows*cols);cells_blank(b,(size_t)rows*cols);
 for(int y=0;y<rows&&y<t->rows;y++)for(int x=0;x<cols&&x<t->cols;x++){
  a[y*cols+x]=t->primary[y*t->cols+x];b[y*cols+x]=t->alternate[y*t->cols+x];
 }
 free(t->primary);free(t->alternate);t->primary=a;t->alternate=b;t->grid=t->alt?b:a;
 t->rows=rows;t->cols=cols;t->top=0;t->bottom=rows-1;
 if(t->x>=cols)t->x=cols-1;if(t->y>=rows)t->y=rows-1;
 t->wrap_pending=false;
}
static void term_scroll(Terminal*t,int count){
 int height=t->bottom-t->top+1;if(count>height)count=height;if(count < -height)count=-height;
 int width=t->cols;
 if(count>0){memmove(t->grid+t->top*width,t->grid+(t->top+count)*width,(size_t)(height-count)*width*sizeof(Cell));cells_blank(t->grid+(t->bottom-count+1)*width,(size_t)count*width);}
 if(count<0){count=-count;memmove(t->grid+(t->top+count)*width,t->grid+t->top*width,(size_t)(height-count)*width*sizeof(Cell));cells_blank(t->grid+t->top*width,(size_t)count*width);}
}
static void term_lf(Terminal*t){t->wrap_pending=false;if(t->y==t->bottom)term_scroll(t,1);else if(t->y<t->rows-1)t->y++;}
static int utf_code(const unsigned char*s,int len){
 if(len==1)return s[0];int v=s[0]&((1<<(7-len))-1);for(int i=1;i<len;i++)v=(v<<6)|(s[i]&63);return v;
}
static void term_text(Terminal*t,const char*s,int len){
 int cp=utf_code((const unsigned char*)s,len),width=wcwidth((wchar_t)cp);
 if(width<0)width=1;
 if(width==0){
  int x=t->wrap_pending?t->x:t->x-1;if(x>=0){Cell*c=&t->grid[t->y*t->cols+x];if(c->width==0&&x>0)c--;size_t n=strlen(c->text);if(n+(size_t)len<sizeof(c->text)){memcpy(c->text+n,s,len);c->text[n+len]=0;}}return;
 }
 if(t->wrap_pending){if(t->wrap){t->x=0;term_lf(t);}t->wrap_pending=false;}
 if(width==2&&t->x==t->cols-1){t->x=0;term_lf(t);}
 if(t->insert){int n=t->cols-t->x-width;if(n>0)memmove(t->grid+t->y*t->cols+t->x+width,t->grid+t->y*t->cols+t->x,(size_t)n*sizeof(Cell));}
 Cell*c=&t->grid[t->y*t->cols+t->x];
 if(c->width==0&&t->x>0)t->grid[t->y*t->cols+t->x-1]=blank_cell();
 if(c->width==2&&t->x+1<t->cols)t->grid[t->y*t->cols+t->x+1]=blank_cell();
 *c=t->pen;memcpy(c->text,s,len);c->text[len]=0;c->width=(unsigned char)width;
 if(width==2&&t->x+1<t->cols){c[1]=t->pen;c[1].text[0]=0;c[1].width=0;}
 t->x+=width;if(t->x>=t->cols){t->x=t->cols-1;t->wrap_pending=true;}
}
static void term_erase(Terminal*t,int first,int last){
 if(first<0)first=0;if(last>=t->rows*t->cols)last=t->rows*t->cols-1;
 for(int i=first;i<=last;i++){t->grid[i]=t->pen;strcpy(t->grid[i].text," ");t->grid[i].width=1;}
}
static void term_mode(Terminal*t,int mode,bool on,bool priv){
 if(priv){
  if(mode==25)t->hidden=!on;
  if(mode==7)t->wrap=on;
  if(mode==6){t->origin=on;t->x=0;t->y=on?t->top:0;}
  if(mode==2004)t->bracket=on;
  if(mode==47||mode==1047||mode==1049){
   if(on&&!t->alt){t->main_x=t->x;t->main_y=t->y;t->alt=true;t->grid=t->alternate;cells_blank(t->grid,(size_t)t->rows*t->cols);t->x=t->y=0;}
   if(!on&&t->alt){t->alt=false;t->grid=t->primary;t->x=t->main_x<t->cols?t->main_x:t->cols-1;t->y=t->main_y<t->rows?t->main_y:t->rows-1;}
   t->top=0;t->bottom=t->rows-1;t->wrap_pending=false;
  }
 }else if(mode==4)t->insert=on;
}
static void term_sgr(Terminal*t,int*p,int n){
 for(int i=0;i<n;i++){
  int a=p[i];
  if(a==0)t->pen=blank_cell();else if(a==1)t->pen.bold=1;else if(a==22)t->pen.bold=0;
  else if(a==4)t->pen.underline=1;else if(a==24)t->pen.underline=0;
  else if(a==7)t->pen.reverse=1;else if(a==27)t->pen.reverse=0;
  else if(a>=30&&a<=37)t->pen.fg=a-30;else if(a>=40&&a<=47)t->pen.bg=a-40;
  else if(a==39)t->pen.fg=-1;else if(a==49)t->pen.bg=-1;
  else if(a>=90&&a<=97){t->pen.fg=a-90;t->pen.bold=1;}else if(a>=100&&a<=107)t->pen.bg=a-100;
  else if((a==38||a==48)&&i+2<n&&p[i+1]==5){int v=p[i+2];if(v>=16&&v<232){int c=v-16;v=((c/36)>2?1:0)|(((c/6)%6)>2?2:0)|((c%6)>2?4:0);}else if(v>=232)v=v>=244?7:0;else v%=8;if(a==38)t->pen.fg=v;else t->pen.bg=v;i+=2;}
  else if((a==38||a==48)&&i+4<n&&p[i+1]==2){int v=(p[i+2]>127?1:0)|(p[i+3]>127?2:0)|(p[i+4]>127?4:0);if(a==38)t->pen.fg=v;else t->pen.bg=v;i+=4;}
 }
}
static void term_csi(Terminal*t,char final,int fd){
 int p[32]={0},n=1;bool priv=false,extension=false;
 for(int i=0;i<t->seq_len;i++){unsigned char c=t->seq[i];if(c=='?')priv=true;else if(c=='>'||c=='<'||c=='=')extension=true;else if(c>='0'&&c<='9'){if(p[n-1]<100000)p[n-1]=p[n-1]*10+c-'0';}else if((c==';'||c==':')&&n<32)n++;}
 int a=p[0]?p[0]:1,old_y=t->y;
 switch(final){
 case 'A':t->y-=a;break;case 'B':case 'e':t->y+=a;break;
 case 'C':case 'a':t->x+=a;break;case 'D':t->x-=a;break;
 case 'E':t->y+=a;t->x=0;break;case 'F':t->y-=a;t->x=0;break;
 case 'G':case '`':t->x=a-1;break;
 case 'd':t->y=a-1+(t->origin?t->top:0);break;
 case 'H':case 'f':t->y=a-1+(t->origin?t->top:0);t->x=(n>1&&p[1]?p[1]:1)-1;break;
 case 'J':if(p[0]==0)term_erase(t,t->y*t->cols+t->x,t->rows*t->cols-1);else if(p[0]==1)term_erase(t,0,t->y*t->cols+t->x);else if(p[0]==2||p[0]==3)term_erase(t,0,t->rows*t->cols-1);break;
 case 'K':if(p[0]==0)term_erase(t,t->y*t->cols+t->x,(t->y+1)*t->cols-1);else if(p[0]==1)term_erase(t,t->y*t->cols,t->y*t->cols+t->x);else if(p[0]==2)term_erase(t,t->y*t->cols,(t->y+1)*t->cols-1);break;
 case 'X':term_erase(t,t->y*t->cols+t->x,t->y*t->cols+(t->x+a<t->cols?t->x+a:t->cols)-1);break;
 case '@':case 'P':{
  int count=a<t->cols-t->x?a:t->cols-t->x;Cell*row=t->grid+t->y*t->cols;
  if(final=='@'){memmove(row+t->x+count,row+t->x,(size_t)(t->cols-t->x-count)*sizeof(Cell));cells_blank(row+t->x,count);}
  else{memmove(row+t->x,row+t->x+count,(size_t)(t->cols-t->x-count)*sizeof(Cell));cells_blank(row+t->cols-count,count);}break;}
 case 'L':case 'M':if(t->y>=t->top&&t->y<=t->bottom){int top=t->top;t->top=t->y;term_scroll(t,final=='L'?-a:a);t->top=top;}break;
 case 'S':term_scroll(t,a);break;case 'T':term_scroll(t,-a);break;
 case 'm':if(!priv&&!extension)term_sgr(t,p,n);break;
 case 'h':case 'l':for(int i=0;i<n;i++)term_mode(t,p[i],final=='h',priv);break;
 case 'r':if(!priv){int top=p[0]?p[0]-1:0,bottom=n>1&&p[1]?p[1]-1:t->rows-1;if(top>=0&&bottom<t->rows&&top<bottom){t->top=top;t->bottom=bottom;t->x=0;t->y=t->origin?top:0;}}break;
 case 's':t->sx=t->x;t->sy=t->y;break;case 'u':t->x=t->sx;t->y=t->sy;break;
 case 'n':if(fd>=0){char reply[64];if(p[0]==6){int k=snprintf(reply,sizeof(reply),"\033[%s%d;%dR",priv?"?":"",t->y+1,t->x+1);(void)write(fd,reply,k);}else if(p[0]==5)(void)write(fd,"\033[0n",4);}break;
 case 'c':if(fd>=0&&!extension)(void)write(fd,"\033[?1;2c",7);break;
 default:break;
 }
 if(t->x<0)t->x=0;if(t->x>=t->cols)t->x=t->cols-1;
 int min=t->origin?t->top:0,max=t->origin?t->bottom:t->rows-1;
 if(t->y<min)t->y=min;if(t->y>max)t->y=max;
 if(old_y!=t->y||final!='m')t->wrap_pending=false;
}
static void term_feed(Terminal*t,const unsigned char*data,size_t n,int fd){
 for(size_t i=0;i<n;i++){
  unsigned char c=data[i];
  if(t->state==3){if(c==7)t->state=0;else if(c==27)t->state=4;continue;}
  if(t->state==4){t->state=c=='\\'?0:3;continue;}
  if(t->state==5){t->graphics[t->charset_target]=(c=='0');t->state=0;continue;}
  if(t->state==6){t->state=0;continue;}
  if(t->state==2){if(c>=0x40&&c<=0x7e){term_csi(t,(char)c,fd);t->state=0;t->seq_len=0;}else if(c==27){t->state=1;t->seq_len=0;}else if(t->seq_len<(int)sizeof(t->seq)-1)t->seq[t->seq_len++]=(char)c;else{t->state=0;t->seq_len=0;}continue;}
  if(t->state==1){t->state=0;
   if(c=='['){t->state=2;t->seq_len=0;}else if(c==']'||c=='P'||c=='_'||c=='^')t->state=3;
   else if(c=='('||c==')'){t->charset_target=c==')';t->state=5;}else if(c=='#'||c=='%')t->state=6;
   else if(c=='7'){t->sx=t->x;t->sy=t->y;}else if(c=='8'){t->x=t->sx<t->cols?t->sx:t->cols-1;t->y=t->sy<t->rows?t->sy:t->rows-1;}
   else if(c=='D')term_lf(t);else if(c=='E'){t->x=0;term_lf(t);}else if(c=='M'){if(t->y==t->top)term_scroll(t,-1);else if(t->y>0)t->y--;}
   else if(c=='c')term_reset(t);continue;
  }
  if(t->utf_len){
   if((c&0xc0)==0x80){t->utf[t->utf_len++]=(char)c;if(t->utf_len==t->utf_need){term_text(t,t->utf,t->utf_len);t->utf_len=0;}continue;}
   t->utf_len=0;term_text(t,"?",1);
  }
  if(c==27){t->state=1;continue;}
  if(c<32||c==127){if(c=='\r'){t->x=0;t->wrap_pending=false;}else if(c=='\n'||c==11||c==12)term_lf(t);else if(c=='\b'){if(t->x>0)t->x--;t->wrap_pending=false;}else if(c=='\t'){t->x=((t->x/8)+1)*8;if(t->x>=t->cols)t->x=t->cols-1;t->wrap_pending=false;}else if(c==14)t->charset=1;else if(c==15)t->charset=0;continue;}
  if(c>=0xc2&&c<=0xf4){t->utf[0]=(char)c;t->utf_len=1;t->utf_need=c<0xe0?2:c<0xf0?3:4;continue;}
  if(c>=128){term_text(t,"?",1);continue;}
  if(t->graphics[t->charset]){
   const char*s=NULL;switch(c){case 'q':s="─";break;case 'x':s="│";break;case 'l':s="┌";break;case 'k':s="┐";break;case 'm':s="└";break;case 'j':s="┘";break;default:break;}
   if(s){term_text(t,s,3);continue;}
  }
  char ascii=(char)c;term_text(t,&ascii,1);
 }
}
#endif
