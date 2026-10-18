#define ZSHLAB_TEST
#include "app.c"

/* Exercise the host launcher through Docker using the caller's Docker context. */
static bool screen_contains(Terminal*t,const char*needle){
 char *text=calloc((size_t)t->rows*t->cols*24+1,1);if(!text)return false;
 size_t n=0;
 for(int y=0;y<t->rows;y++){
  for(int x=0;x<t->cols;x++){const char*c=t->grid[y*t->cols+x].text;size_t k=strlen(c);memcpy(text+n,c,k);n+=k;}
  text[n++]='\n';
 }
 text[n]=0;bool found=strstr(text,needle)!=NULL;free(text);return found;
}
static bool pump_until(int fd,Terminal*t,const char*expected,double seconds){
 double deadline=now_seconds()+seconds;
 while(now_seconds()<deadline){
  fd_set set;FD_ZERO(&set);FD_SET(fd,&set);struct timeval tv={0,30000};
  int r=select(fd+1,&set,NULL,NULL,&tv);if(r<0&&errno!=EINTR)break;
  if(r>0){unsigned char buf[65536];ssize_t n=read(fd,buf,sizeof(buf));if(n>0)term_feed(t,buf,(size_t)n,fd);else if(n==0||(n<0&&errno==EIO))break;}
  if(screen_contains(t,expected))return true;
 }
 fprintf(stderr,"UI timeout: %s\n",expected);return false;
}
int main(int argc,char**argv){
 configure_locale();if(argc!=2){fputs("usage: smoke-ui /path/to/zsh-lab\n",stderr);return 2;}
 char binary[PATH_MAX];if(!realpath(argv[1],binary)){perror("binary");return 1;}
 char root[]="/tmp/zsh-lab-smoke-XXXXXX";if(!mkdtemp(root)){perror("mkdtemp");return 1;}
 char progress[PATH_MAX];snprintf(progress,sizeof(progress),"%s/progress.txt",root);
 int fd;struct winsize ws={.ws_row=32,.ws_col=130};pid_t pid=forkpty(&fd,NULL,NULL,&ws);
 if(pid<0){perror("forkpty");remove_tree(root);return 1;}
 if(pid==0){if(chdir(root))_exit(126);execl(binary,binary,"--lesson","1","--progress",progress,(char*)NULL);_exit(127);}
 Terminal t;term_init(&t,32,130);bool ok=true;
#define STEP(expr) do{if(!(expr)){fprintf(stderr,"UI assertion failed: %s\n",#expr);ok=false;goto cleanup;}}while(0)
 STEP(pump_until(fd,&t,"行頭へひと跳び",15));
 STEP(write_all(fd,"\001",1)==0);STEP(pump_until(fd,&t,"クリア！",5));
 STEP(write_all(fd,"\035n",2)==0);STEP(pump_until(fd,&t,"行末へ戻る",5));
 STEP(write_all(fd,"\035h",2)==0);STEP(pump_until(fd,&t,"HINT",5));
 ws.ws_row=18;ws.ws_col=80;ioctl(fd,TIOCSWINSZ,&ws);term_resize(&t,18,80);STEP(pump_until(fd,&t,"95 列",5));
 ws.ws_row=32;ws.ws_col=130;ioctl(fd,TIOCSWINSZ,&ws);term_resize(&t,32,130);STEP(pump_until(fd,&t,"行末へ戻る",5));
 STEP(write_all(fd,"\035q",2)==0);
 {
  double deadline=now_seconds()+5;int status=0;pid_t exited=0;
  while(now_seconds()<deadline){
   fd_set set;FD_ZERO(&set);FD_SET(fd,&set);struct timeval tv={0,20000};
   if(select(fd+1,&set,NULL,NULL,&tv)>0){unsigned char buf[65536];ssize_t n=read(fd,buf,sizeof(buf));if(n>0)term_feed(&t,buf,(size_t)n,fd);}
   exited=waitpid(pid,&status,WNOHANG);if(exited==pid)break;
  }
  STEP(exited==pid);pid=0;STEP(WIFEXITED(status)&&WEXITSTATUS(status)==0);
  Progress saved;progress_init(&saved,progress);STEP(saved.done[0]);
 }
cleanup:
 if(pid>0){kill(pid,SIGTERM);pause_ms(100);kill(pid,SIGKILL);waitpid(pid,NULL,0);}close(fd);term_free(&t);remove_tree(root);
 if(ok)puts("UI smoke passed: Docker launcher, Ctrl+A, next, hint, resize, exit");
 return ok?0:1;
}
