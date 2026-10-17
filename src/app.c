#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <time.h>
#include <ncurses.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#include "terminal.h"
#ifndef ZSHLAB_VERSION
#define ZSHLAB_VERSION "development"
#endif

typedef struct {
 const char *id,*title;int page;const char *goal,*hint,*buffer;int cursor;
 const char *setup,*widget,*expected;int position;
 const char *command,*probe,*answer;
} Lesson;
#include "lessons.h"
#include "shell.h"

typedef struct {
 pid_t pid;int fd,events,index,generation;
 char root[PATH_MAX],shell[PATH_MAX];
 bool ready,complete,seen_kill,seen_undo,seen_whole;
 char latest[160];int cursor;
 char event[262144];size_t event_len;int zeros;bool overflow;
} Session;

typedef struct { bool done[NLESSONS];char path[PATH_MAX];bool warning; } Progress;
static volatile sig_atomic_t stop_requested=0;
static void stop_handler(int sig){(void)sig;stop_requested=1;}
static void configure_locale(void){
 if(setlocale(LC_ALL,"") && MB_CUR_MAX>1)return;
 const char*choices[]={"en_US.UTF-8","C.UTF-8",NULL};
 for(int i=0;choices[i];i++)if(setlocale(LC_ALL,choices[i])&&MB_CUR_MAX>1){setenv("LC_ALL",choices[i],1);return;}
}
static double now_seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void pause_ms(int ms){struct timespec t={ms/1000,(ms%1000)*1000000L};nanosleep(&t,NULL);}
static int write_all(int fd,const void*data,size_t n){
 const char*p=data;
 while(n){ssize_t k=write(fd,p,n);if(k<0){if(errno==EINTR)continue;if(errno==EAGAIN||errno==EWOULDBLOCK){fd_set set;FD_ZERO(&set);FD_SET(fd,&set);struct timeval tv={0,100000};select(fd+1,NULL,&set,NULL,&tv);continue;}return -1;}if(k==0)return -1;p+=k;n-=(size_t)k;}return 0;
}
static int make_path(char*dst,size_t cap,const char*a,const char*b){int n=snprintf(dst,cap,"%s/%s",a,b);return n<0||(size_t)n>=cap?-1:0;}
static void remove_tree(const char*path){
 struct stat st;if(lstat(path,&st)!=0)return;
 if(!S_ISDIR(st.st_mode)){unlink(path);return;}
 DIR*d=opendir(path);if(!d)return;struct dirent*e;
 while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;char child[PATH_MAX];if(make_path(child,sizeof(child),path,e->d_name)==0)remove_tree(child);}
 closedir(d);rmdir(path);
}
static char* shell_quote(const char*s){
 size_t n=strlen(s);char*out=malloc(n*4+3);if(!out)return NULL;char*p=out;*p++='\'';
 for(;*s;s++){if(*s=='\''){memcpy(p,"'\\''",4);p+=4;}else *p++=*s;}*p++='\'';*p=0;return out;
}
static int locate_zsh(char*output,size_t cap){
 const char*path=getenv("PATH");char*copy=strdup(path?path:"/bin:/usr/bin");if(!copy)return -1;
 char*save=NULL;for(char*p=strtok_r(copy,":",&save);p;p=strtok_r(NULL,":",&save)){
  char candidate[PATH_MAX];if(make_path(candidate,sizeof(candidate),p,"zsh")==0&&access(candidate,X_OK)==0){
   char resolved[PATH_MAX];if(realpath(candidate,resolved)){snprintf(output,cap,"%s",resolved);free(copy);return 0;}
  }
 }
 free(copy);if(access("/bin/zsh",X_OK)==0){snprintf(output,cap,"/bin/zsh");return 0;}return -1;
}
static int session_prepare(Session*s,int index){
 if(index<0||index>=NLESSONS)return -1;
 s->index=index;s->generation++;s->ready=s->complete=false;s->seen_kill=s->seen_whole=s->seen_undo=false;s->latest[0]=0;
 char path[PATH_MAX],tmp[PATH_MAX];if(make_path(path,sizeof(path),s->root,"lesson.zsh")||make_path(tmp,sizeof(tmp),s->root,"lesson.new"))return -1;
 const Lesson*l=&lessons[index];char*buffer=shell_quote(l->buffer),*probe=shell_quote(l->probe);if(!buffer||!probe){free(buffer);free(probe);return -1;}
 FILE*f=fopen(tmp,"w");if(!f){free(buffer);free(probe);return -1;}
 fprintf(f,"_lab_generation=%d\n%s\n_lab_buffer=%s\n_lab_cursor=%d\n_lab_probe=%s\n",s->generation,l->setup,buffer,l->cursor<0?(int)strlen(l->buffer):l->cursor,probe);
 free(buffer);free(probe);if(fclose(f)!=0)return -1;return rename(tmp,path);
}
static int session_open(Session*s,int index,int rows,int cols){
 memset(s,0,sizeof(*s));s->fd=s->events=-1;
 if(locate_zsh(s->shell,sizeof(s->shell))!=0){fputs("zsh が見つかりません。\n",stderr);return -1;}
 const char*tmp=getenv("TMPDIR");if(!tmp||!*tmp)tmp="/tmp";
 if(snprintf(s->root,sizeof(s->root),"%s/zsh-lab-XXXXXX",tmp)>=(int)sizeof(s->root))return -1;
 if(!mkdtemp(s->root))return -1;
 char path[PATH_MAX];make_path(path,sizeof(path),s->root,"events");
 int event_write=open(path,O_CREAT|O_WRONLY|O_EXCL,0600);if(event_write<0)goto fail;close(event_write);
 s->events=open(path,O_RDONLY);if(s->events<0)goto fail;
 if(session_prepare(s,index)!=0)goto fail;
 make_path(path,sizeof(path),s->root,".zshrc");FILE*f=fopen(path,"w");if(!f)goto fail;
 if(fputs(shell_init,f)==EOF){fclose(f);goto fail;}if(fclose(f)!=0)goto fail;
 struct winsize ws={.ws_row=(unsigned short)rows,.ws_col=(unsigned short)cols};
 s->pid=forkpty(&s->fd,NULL,NULL,&ws);
 if(s->pid<0)goto fail;
 if(s->pid==0){
  signal(SIGINT,SIG_DFL);signal(SIGTERM,SIG_DFL);signal(SIGHUP,SIG_DFL);signal(SIGWINCH,SIG_DFL);signal(SIGPIPE,SIG_DFL);
  close(s->events);setenv("ZDOTDIR",s->root,1);setenv("TERM","xterm-256color",1);unsetenv("ENV");
  if(chdir(s->root)!=0)_exit(126);
  execl(s->shell,s->shell,"-d","-i",(char*)NULL);_exit(127);
 }
 fcntl(s->fd,F_SETFL,fcntl(s->fd,F_GETFL)|O_NONBLOCK);return 0;
fail:
 if(s->events>=0)close(s->events);remove_tree(s->root);return -1;
}
static void session_close(Session*s){
 if(s->pid>0){
  if(kill(-s->pid,SIGHUP)!=0)kill(s->pid,SIGHUP);
  if(s->fd>=0){close(s->fd);s->fd=-1;}
  double deadline=now_seconds()+.5;int status;
  while(now_seconds()<deadline){pid_t r=waitpid(s->pid,&status,WNOHANG);if(r==s->pid||(r<0&&errno==ECHILD)){s->pid=0;break;}pause_ms(10);}
  if(s->pid>0){if(kill(-s->pid,SIGKILL)!=0)kill(s->pid,SIGKILL);while(waitpid(s->pid,&status,0)<0&&errno==EINTR){}s->pid=0;}
 }
 if(s->fd>=0)close(s->fd);if(s->events>=0)close(s->events);remove_tree(s->root);
}
static int session_load(Session*s,int index){if(session_prepare(s,index)!=0)return -1;return write_all(s->fd,"\030\024",2);}
static void session_resize(Session*s,int rows,int cols){struct winsize ws={.ws_row=(unsigned short)rows,.ws_col=(unsigned short)cols};ioctl(s->fd,TIOCSWINSZ,&ws);}
static char*trim_command(char*s){while(*s==' '||*s=='\t'||*s=='\n')s++;size_t n=strlen(s);while(n&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'))s[--n]=0;return s;}
static void session_event(Session*s,char**f){
 char *generation=strchr(f[0],':');if(!generation)return;*generation++=0;if(atoi(generation)!=s->generation)return;
 if(!strcmp(f[0],"ready")){s->ready=true;return;}if(!s->ready)return;
 const Lesson*l=&lessons[s->index];
 if(!strcmp(f[0],"widget")){
  if(*l->widget&&!strcmp(l->widget,f[1])){
   bool yes=true;
   if(l->expected&&strcmp(l->expected,f[2]))yes=false;
   if(l->position>=0&&atoi(f[3])!=l->position)yes=false;
   if(!strcmp(l->id,"yank")&&!s->seen_kill)yes=false;
   if(!strcmp(l->id,"undo")&&!s->seen_whole)yes=false;
   if(!strcmp(l->id,"redo")&&!(s->seen_kill&&s->seen_undo))yes=false;
   if(yes)s->complete=true;
  }
  if(!strcmp(f[1],"backward-kill-word"))s->seen_kill=true;
  if(!strcmp(f[1],"kill-whole-line"))s->seen_whole=true;
  if(!strcmp(f[1],"undo"))s->seen_undo=true;
  if(*f[1]&&strcmp(f[1],"self-insert")&&strcmp(f[1],"zle-line-init")&&strcmp(f[1],"_lab_load")){snprintf(s->latest,sizeof(s->latest),"%s",f[1]);s->cursor=atoi(f[3]);}
 }else if(!strcmp(f[0],"command")&&!strcmp(f[4],"0")){
  bool yes=*l->command||*l->probe;
  if(*l->command&&strcmp(l->command,trim_command(f[2])))yes=false;
  if(*l->probe&&strcmp(l->answer,f[1]))yes=false;
  if(yes)s->complete=true;
 }
}
static void session_poll(Session*s){
 char buf[8192];ssize_t n;
 while((n=read(s->events,buf,sizeof(buf)))>0){for(ssize_t i=0;i<n;i++){
  char c=buf[i];if(!s->overflow){if(s->event_len<sizeof(s->event))s->event[s->event_len++]=c;else s->overflow=true;}
  if(c==0&&++s->zeros==5){
   if(!s->overflow){char*f[5],*p=s->event;for(int j=0;j<5;j++){f[j]=p;p+=strlen(p)+1;}session_event(s,f);}
   s->event_len=0;s->zeros=0;s->overflow=false;
  }
 }}
}
static void progress_init(Progress*p,const char*custom){
 memset(p,0,sizeof(*p));
 if(custom)snprintf(p->path,sizeof(p->path),"%s",custom);
 else {const char*base=getenv("XDG_STATE_HOME"),*home=getenv("HOME");if(base&&*base)snprintf(p->path,sizeof(p->path),"%s/zsh-lab/progress.txt",base);else if(home)snprintf(p->path,sizeof(p->path),"%s/.local/state/zsh-lab/progress.txt",home);else p->warning=true;}
 FILE*f=fopen(p->path,"r");if(!f)return;char line[128];
 while(fgets(line,sizeof(line),f)){line[strcspn(line,"\r\n")]=0;for(int i=0;i<NLESSONS;i++)if(!strcmp(line,lessons[i].id))p->done[i]=true;}fclose(f);
}
static int mkdir_parents(const char*path){
 char copy[PATH_MAX];snprintf(copy,sizeof(copy),"%s",path);
 for(char*p=copy+1;*p;p++)if(*p=='/'){*p=0;if(mkdir(copy,0700)!=0&&errno!=EEXIST)return -1;*p='/';}return 0;
}
static void progress_save(Progress*p,int index){
 p->done[index]=true;if(!*p->path){p->warning=true;return;}
 if(mkdir_parents(p->path)!=0){p->warning=true;return;}
 char tmp[PATH_MAX];if(snprintf(tmp,sizeof(tmp),"%s.XXXXXX",p->path)>=(int)sizeof(tmp)){p->warning=true;return;}
 int fd=mkstemp(tmp);if(fd<0){p->warning=true;return;}FILE*f=fdopen(fd,"w");if(!f){close(fd);unlink(tmp);p->warning=true;return;}
 bool failed=false;for(int i=0;i<NLESSONS;i++)if(p->done[i]&&fprintf(f,"%s\n",lessons[i].id)<0)failed=true;
 if(fclose(f)!=0)failed=true;if(failed||rename(tmp,p->path)!=0){unlink(tmp);p->warning=true;}
}

typedef struct {bool prefix,paste;unsigned char pending[8];int len;double since;} Router;
typedef struct {unsigned char data[8192];size_t len;char actions[4096];int n;} Routed;
static void route_feed(Router*r,const unsigned char*data,size_t n,Routed*out){
 memset(out,0,sizeof(*out));
 for(size_t i=0;i<n;i++){
  unsigned char c=data[i];r->since=now_seconds();r->pending[r->len++]=c;
  const char*marker=r->paste?"\033[201~":"\033[200~";
  if(!memcmp(r->pending,marker,(size_t)r->len)){
   if(r->len<6)continue;memcpy(out->data+out->len,r->pending,6);out->len+=6;r->len=0;r->paste=!r->paste;continue;
  }
  for(int j=0;j<r->len;j++){
   c=r->pending[j];
   if(r->paste)out->data[out->len++]=c;
   else if(r->prefix){r->prefix=false;if(strchr("nprhq",c)&&c)out->actions[out->n++]=(char)c;else if(c==29)out->data[out->len++]=c;else{out->data[out->len++]=29;out->data[out->len++]=c;}}
   else if(c==29)r->prefix=true;else out->data[out->len++]=c;
  }
  r->len=0;
 }
}
static int utf_len(unsigned char c){return c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:1;}
static void draw_text(int y,int x,const char*text,int max_width,int attrs){
 if(y<0||y>=LINES||x<0||x>=COLS||max_width<=0)return;
 int limit=COLS-x-1;if(max_width<limit)limit=max_width;
 const char*p=text;int width=0;
 while(*p){int n=utf_len((unsigned char)*p);if(strlen(p)<(size_t)n)break;int cp=utf_code((const unsigned char*)p,n),w=wcwidth((wchar_t)cp);if(w<0)w=1;if(width+w>limit)break;width+=w;p+=n;}
 attrset(attrs);mvaddnstr(y,x,text,(int)(p-text));
}
static int draw_wrap(int y,int x,const char*text,int width,int attrs,int bottom){
 if(width<1)return y;const char*p=text,*start=p;int cells=0;
 while(*p&&y<bottom){
  int n=utf_len((unsigned char)*p);if(strlen(p)<(size_t)n)break;int w=wcwidth((wchar_t)utf_code((const unsigned char*)p,n));if(w<0)w=1;
  if(*p=='\n'||cells+w>width){size_t bytes=(size_t)(p-start);char line[2048];if(bytes>=sizeof(line))bytes=sizeof(line)-1;memcpy(line,start,bytes);line[bytes]=0;draw_text(y++,x,line,width,attrs);cells=0;if(*p=='\n')p++;start=p;continue;}
  cells+=w;p+=n;
 }
 if(y<bottom){size_t bytes=(size_t)(p-start);char line[2048];if(bytes>=sizeof(line))bytes=sizeof(line)-1;memcpy(line,start,bytes);line[bytes]=0;draw_text(y++,x,line,width,attrs);}return y;
}
static int color_attributes(const Cell*c){
 int attr=(c->bold?A_BOLD:0)|(c->reverse?A_REVERSE:0)|(c->underline?A_UNDERLINE:0);
 if(has_colors()){
  int fg=c->fg,bg=c->bg;
  int pair=COLOR_PAIRS>=82?1+(fg+1)*9+(bg+1):fg+1;
  if(pair>0&&pair<COLOR_PAIRS)attr|=COLOR_PAIR(pair);
 }
 return attr;
}
static void colors_init(void){
 if(!has_colors())return;start_color();use_default_colors();
 if(COLOR_PAIRS>=82){for(int fg=-1;fg<8;fg++)for(int bg=-1;bg<8;bg++)init_pair((short)(1+(fg+1)*9+(bg+1)),(short)fg,(short)bg);}
 else for(int i=0;i<8&&i+1<COLOR_PAIRS;i++)init_pair((short)(i+1),(short)i,-1);
}
static void render(Session*s,Terminal*t,Progress*p,Router*r,bool hint,const char*notice,int left){
 erase();
 if(COLS<95||LINES<22){draw_text(0,0,"画面を 95 列 × 22 行以上に広げてください。",COLS-1,A_BOLD);draw_text(2,0,"Ctrl+] → q で終了できます。",COLS-1,0);curs_set(0);refresh();return;}
 draw_text(0,1,"ZSH LAB  /  実際の zsh で練習",COLS-2,A_BOLD);
 for(int y=1;y<LINES-2;y++)draw_text(y,left,"│",1,A_DIM);
 for(int y=0;y<t->rows&&y<LINES-3;y++)for(int x=0;x<t->cols&&x<left;x++){
  Cell*c=&t->grid[y*t->cols+x];if(c->width>0)draw_text(y+1,x,c->text,c->width,color_attributes(c));
 }
 const Lesson*l=&lessons[s->index];int x=left+3,width=COLS-x-2,y=2;char text[512];
 snprintf(text,sizeof(text),"%02d / %d  ·  スライド p.%d",s->index+1,NLESSONS,l->page);draw_text(y,x,text,width,A_DIM);y+=2;
 y=draw_wrap(y,x,l->title,width,A_BOLD,LINES-5)+1;
 y=draw_wrap(y,x,l->goal,width,0,LINES-5)+2;
 y=draw_wrap(y,x,s->complete?"✓ クリア！ Ctrl+] → n で次へ":"○ 実際の操作を待っています",width,A_BOLD,LINES-4)+1;
 if(hint){draw_text(y++,x,"HINT",width,A_BOLD);y=draw_wrap(y,x,l->hint,width,0,LINES-5);}
 if(*s->latest){snprintf(text,sizeof(text),"Widget: %s  /  CURSOR: %d",s->latest,s->cursor);y=draw_wrap(y,x,text,width,A_DIM,LINES-4);}
 if(*notice)draw_wrap(y,x,notice,width,0,LINES-3);else if(p->warning)draw_wrap(y,x,"進捗を保存できません。この実行中は続けられます。",width,0,LINES-3);
 draw_text(LINES-2,1,r->prefix?"操作: n 次へ / p 前へ / h ヒント / r やり直す / q 終了":"Ctrl+] を離して n:次へ  p:前へ  h:ヒント  r:やり直す  q:終了",COLS-2,r->prefix?A_BOLD:A_DIM);
 draw_text(LINES-1,1,"Esc → 文字 は順に押す   ·   練習用の設定・履歴   ·   exit でも終了",COLS-2,A_DIM);
 curs_set(t->hidden?0:1);if(!t->hidden)move(t->y+1,t->x);refresh();
}
static int run_app(int index,Progress*progress){
 if(!initscr()){fputs("ターミナルを初期化できません\n",stderr);return 1;}
 raw();noecho();colors_init();
 signal(SIGINT,stop_handler);signal(SIGTERM,stop_handler);signal(SIGHUP,stop_handler);signal(SIGPIPE,SIG_IGN);
 int rows=LINES,cols=COLS,left=cols*58/100;if(left<20)left=20;
 int height=rows-3;if(height<2)height=2;
 Session*s=calloc(1,sizeof(*s));if(!s){endwin();return 1;}
 if(session_open(s,index,height,left)!=0){endwin();free(s);fputs("練習用 zsh を起動できませんでした。\n",stderr);return 1;}
 Terminal terminal;term_init(&terminal,height,left);
 Router router={0};bool hint=false,saved=false;char notice[256]="";int result=0;
 write_all(STDOUT_FILENO,"\033[?2004h",8);
 while(!stop_requested){
  struct winsize ws;if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0&&ws.ws_row&&ws.ws_col&&(ws.ws_row!=rows||ws.ws_col!=cols)){
   rows=ws.ws_row;cols=ws.ws_col;if(rows>512)rows=512;if(cols>2048)cols=2048;
   resizeterm(rows,cols);left=cols*58/100;if(left<20)left=20;height=rows-3;if(height<2)height=2;
   session_resize(s,height,left);term_resize(&terminal,height,left);clearok(stdscr,TRUE);
  }
  fd_set set;FD_ZERO(&set);FD_SET(STDIN_FILENO,&set);FD_SET(s->fd,&set);int max=s->fd>STDIN_FILENO?s->fd:STDIN_FILENO;struct timeval tv={0,50000};
  int available=select(max+1,&set,NULL,NULL,&tv);if(available<0){if(errno==EINTR)continue;result=1;break;}
  if(available>0&&FD_ISSET(s->fd,&set)){
   unsigned char data[65536];ssize_t n=read(s->fd,data,sizeof(data));if(n==0||(n<0&&errno==EIO))break;
   if(n>0)term_feed(&terminal,data,(size_t)n,s->fd);else if(errno!=EAGAIN&&errno!=EINTR){result=1;break;}
  }
  if(available>0&&FD_ISSET(STDIN_FILENO,&set)){
   unsigned char input[4096];ssize_t n=read(STDIN_FILENO,input,sizeof(input));if(n<=0)break;
   Routed routed;route_feed(&router,input,(size_t)n,&routed);
   if(routed.len&&write_all(s->fd,routed.data,routed.len)<0)break;
   for(int a=0;a<routed.n;a++){
    char action=routed.actions[a];if(action=='q')goto done;
    if(action=='h'){hint=!hint;clearok(stdscr,TRUE);}
    int next=s->index;
    if(action=='n'){
     if(!s->complete){snprintf(notice,sizeof(notice),"クリアしてから次へ。h でヒントを表示。");continue;}
     if(s->index==NLESSONS-1){snprintf(notice,sizeof(notice),"全課題クリア！ p で復習 / q で終了");continue;}next++;
    }
    if(action=='p'&&next>0)next--;
    if(action=='n'||action=='p'||action=='r'){
     if(session_load(s,next)!=0){result=1;goto done;}hint=saved=false;notice[0]=0;clearok(stdscr,TRUE);
    }
   }
  }
  if(router.len&&now_seconds()-router.since>.08){write_all(s->fd,router.pending,(size_t)router.len);router.len=0;}
  session_poll(s);if(s->complete&&!saved){progress_save(progress,s->index);saved=true;}
  render(s,&terminal,progress,&router,hint,notice,left);
 }
done:
 session_close(s);free(s);term_free(&terminal);write_all(STDOUT_FILENO,"\033[?2004l",8);endwin();return result;
}
#ifndef ZSHLAB_TEST
int main(int argc,char**argv){
 configure_locale();int lesson=-1;const char*progress_path=NULL;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--list")){for(int j=0;j<NLESSONS;j++)printf("%2d  %s  (slide %d)\n",j+1,lessons[j].title,lessons[j].page);return 0;}
  if(!strcmp(argv[i],"--version")){printf("zsh-lab %s\n", ZSHLAB_VERSION);return 0;}
  if(!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h")){puts("zsh-lab [--lesson N] [--progress PATH] [--list] [--version]\n\ntmux 不要の zsh 練習アプリ。Ctrl+] → h でヒント、q で終了。\nPython / tmux / パッケージのインストールは不要です。");return 0;}
  if(!strcmp(argv[i],"--lesson")&&i+1<argc){char*end;long v=strtol(argv[++i],&end,10);if(*end||v<1||v>NLESSONS){fputs("課題番号が範囲外です。\n",stderr);return 2;}lesson=(int)v-1;}
  else if(!strcmp(argv[i],"--progress")&&i+1<argc)progress_path=argv[++i];
  else{fprintf(stderr,"不明な引数: %s\n",argv[i]);return 2;}
 }
 if(!isatty(STDIN_FILENO)||!isatty(STDOUT_FILENO)){fputs("ターミナルから起動してください。\n",stderr);return 2;}
 Progress progress;progress_init(&progress,progress_path);if(lesson<0){lesson=0;for(int i=0;i<NLESSONS;i++)if(!progress.done[i]){lesson=i;break;}}
 return run_app(lesson,&progress);
}
#endif
