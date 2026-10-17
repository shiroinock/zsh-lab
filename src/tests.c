#define ZSHLAB_TEST
#include "app.c"
static int assertions=0,failures=0;
#define CHECK(expr) do{assertions++;if(!(expr)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);failures++;}}while(0)
static bool wait_session(Session*s,Terminal*t,bool complete,double seconds){
 double deadline=now_seconds()+seconds;
 while(now_seconds()<deadline){
  fd_set set;FD_ZERO(&set);FD_SET(s->fd,&set);struct timeval tv={0,20000};select(s->fd+1,&set,NULL,NULL,&tv);
  if(FD_ISSET(s->fd,&set)){unsigned char buf[65536];ssize_t n=read(s->fd,buf,sizeof(buf));if(n>0&&t)term_feed(t,buf,(size_t)n,s->fd);}
  session_poll(s);if(complete?s->complete:s->ready)return true;
 }
 fprintf(stderr,"timeout %s: ready=%d complete=%d widget=%s cursor=%d\n",lessons[s->index].id,s->ready,s->complete,s->latest,s->cursor);return false;
}
static void settle(Session*s,Terminal*t,int ms){
 double deadline=now_seconds()+ms/1000.;
 while(now_seconds()<deadline){unsigned char buf[65536];ssize_t n=read(s->fd,buf,sizeof(buf));if(n>0&&t)term_feed(t,buf,(size_t)n,s->fd);session_poll(s);pause_ms(5);}
}
static void exercise(int index,const char*before,const char**keys,int n){
 Session s;if(session_open(&s,index,29,75)!=0){CHECK(false);return;}Terminal t;term_init(&t,29,75);
 CHECK(wait_session(&s,&t,false,3));
 if(before&&*before){write_all(s.fd,"\025",1);write_all(s.fd,before,strlen(before));write_all(s.fd,"\r",1);settle(&s,&t,100);CHECK(session_load(&s,index)==0);CHECK(wait_session(&s,&t,false,3));}
 for(int i=0;i<n;i++){write_all(s.fd,keys[i],strlen(keys[i]));settle(&s,&t,100);}
 CHECK(wait_session(&s,&t,true,3));
 session_close(&s);term_free(&t);
 fprintf(stderr,"  %2d %-15s %s\n",index+1,lessons[index].id,failures?"checked":"ok");
}
static void test_exercises(void){
 const char*a[]={"\001"};exercise(0,NULL,a,1);
 const char*b[]={"\005"};exercise(1,NULL,b,1);
 const char*c[]={"\033b"};exercise(2,NULL,c,1);
 const char*d[]={"\033f"};exercise(3,NULL,d,1);
 const char*e[]={"\027"};exercise(4,NULL,e,1);
 const char*f[]={"\027","\031"};exercise(5,NULL,f,2);
 const char*g[]={"\013"};exercise(6,NULL,g,1);
 const char*h[]={"\025","\037"};exercise(7,NULL,h,2);
 const char*i[]={"\033."};exercise(8,NULL,i,1);
 const char*j[]={"\033q"};exercise(9,NULL,j,1);
 const char*k[]={"bindkey '^P'\r"};exercise(10,NULL,k,1);
 const char*l[]={"bindkey '^P' history-beginning-search-backward\r"};exercise(11,NULL,l,1);
 const char*m[]={"\020"};exercise(12,NULL,m,1);
 const char*n[]={"\033m"};exercise(13,NULL,n,1);
 const char*o[]={"\027","\037","\033/"};exercise(14,NULL,o,3);
 const char*p[]={" "};exercise(15,NULL,p,1);
 const char*q[]={"\033.","\033,"};exercise(16,NULL,q,2);
 const char*r[]={"\033e",":%s/draft/polished/\r",":wq\r"};
 exercise(17,"export EDITOR='/usr/bin/vi -Nu NONE -i NONE' VISUAL='/usr/bin/vi -Nu NONE -i NONE'",r,3);
 const char*s[]={"prepend-note() { BUFFER=\"# $BUFFER\"; CURSOR=${#BUFFER}; }; zle -N prepend-note; bindkey '^[s' prepend-note\r"};exercise(18,NULL,s,1);
 const char*t[]={"\033s"};exercise(19,NULL,t,1);
 const char*u[]={"setopt auto_cd auto_pushd\r"};exercise(20,NULL,u,1);
 const char*v[]={"constellation\r"};exercise(21,NULL,v,1);
 const char*w[]={"PROMPT='lab> '\r"};exercise(22,NULL,w,1);
}
static void test_guards(void){
 Session s;CHECK(session_open(&s,8,29,75)==0);CHECK(wait_session(&s,NULL,false,3));
 write_all(s.fd,"constellation",13);settle(&s,NULL,100);CHECK(!s.complete);
 CHECK(session_load(&s,8)==0);CHECK(wait_session(&s,NULL,false,3));
 write_all(s.fd,"\033.",2);CHECK(wait_session(&s,NULL,true,3));
 CHECK(session_load(&s,0)==0);CHECK(wait_session(&s,NULL,false,3));
 session_resize(&s,31,81);write_all(s.fd,"\001",1);CHECK(wait_session(&s,NULL,true,3));
 char root[PATH_MAX];snprintf(root,sizeof(root),"%s",s.root);session_close(&s);CHECK(access(root,F_OK)!=0);
}
static void test_router(void){
 Router r={0};Routed out;route_feed(&r,(const unsigned char*)"abc\035",4,&out);CHECK(out.len==3&&!memcmp(out.data,"abc",3)&&r.prefix);
 route_feed(&r,(const unsigned char*)"h",1,&out);CHECK(out.n==1&&out.actions[0]=='h'&&!out.len);
 const char*text="\033[200~hello\035q\033[201~\035n";char all[256]={0};size_t len=0;int n=0;
 for(size_t i=0;i<strlen(text);i++){route_feed(&r,(const unsigned char*)text+i,1,&out);memcpy(all+len,out.data,out.len);len+=out.len;for(int j=0;j<out.n;j++){CHECK(out.actions[j]=='n');n++;}}
 CHECK(n==1);CHECK(!strcmp(all,"\033[200~hello\035q\033[201~"));
}
static void feed(Terminal*t,const char*s){term_feed(t,(const unsigned char*)s,strlen(s),-1);}
static bool row_has(Terminal*t,int y,const char*needle){char row[16384]="";size_t n=0;for(int x=0;x<t->cols;x++){const char*s=t->grid[y*t->cols+x].text;size_t k=strlen(s);if(n+k<sizeof(row)){memcpy(row+n,s,k);n+=k;row[n]=0;}}return strstr(row,needle)!=NULL;}
static void test_terminal(void){
 Terminal t;term_init(&t,24,80);feed(&t,"shell prompt");CHECK(row_has(&t,0,"shell prompt"));
 feed(&t,"\033[?1049hEDITOR");CHECK(row_has(&t,0,"EDITOR"));term_resize(&t,30,100);
 feed(&t,"\033[?1049l");CHECK(row_has(&t,0,"shell prompt"));CHECK(!row_has(&t,0,"EDITOR"));CHECK(t.rows==30&&t.cols==100);
 feed(&t,"\033[>4m\033[?4m");CHECK(row_has(&t,0,"shell prompt"));
 feed(&t,"\r\033[2K日本語");CHECK(t.x==6);CHECK(!strcmp(t.grid[0].text,"日"));CHECK(t.grid[1].width==0);
 feed(&t,"\033[1;1HX");CHECK(!strcmp(t.grid[0].text,"X"));CHECK(t.grid[1].width==1);
 feed(&t,"\033[2J\033[Habc\033[2DZ\033[P");CHECK(row_has(&t,0,"aZ"));
 feed(&t,"\033[2J\033[H\033[31;44mred");CHECK(t.grid[0].fg==1&&t.grid[0].bg==4);
 feed(&t,"\033[38;5;196mR");CHECK(t.grid[3].fg==1);
 term_free(&t);
}
static void test_fragmented_sequences(void){
 Terminal t;term_init(&t,24,80);
 const char *s="日本語\033[?1049hvi\033[>4m\033[?1049l\033]0;title\007";
 for(size_t i=0;i<strlen(s);i++)term_feed(&t,(const unsigned char*)s+i,1,-1);
 CHECK(row_has(&t,0,"日本語"));CHECK(!t.alt);
 uint32_t random=1234567;unsigned char data[257];
 for(int j=0;j<200;j++){
  for(size_t i=0;i<sizeof(data);i++){random=random*1664525+1013904223;data[i]=(unsigned char)(random>>24);}
  term_feed(&t,data,sizeof(data),-1);
  CHECK(t.x>=0&&t.x<t.cols&&t.y>=0&&t.y<t.rows);
 }
 term_free(&t);
}
static void test_progress(void){
 char root[]="/tmp/zsh-lab-test-XXXXXX";CHECK(mkdtemp(root)!=NULL);char path[PATH_MAX];snprintf(path,sizeof(path),"%s/state/progress.txt",root);
 Progress p;progress_init(&p,path);CHECK(!p.done[0]);progress_save(&p,0);CHECK(!p.warning);
 Progress q;progress_init(&q,path);CHECK(q.done[0]&&!q.done[1]);remove_tree(root);
}
int main(void){setlocale(LC_ALL,"");test_terminal();test_fragmented_sequences();test_router();test_progress();test_exercises();test_guards();fprintf(stderr,"%d assertions, %d failures\n",assertions,failures);return failures?1:0;}
