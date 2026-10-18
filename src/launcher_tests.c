#define ZSHLAB_TEST
#include "app.c"
#include "launcher.h"
static int failures=0;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failures++;}}while(0)
int main(void){
 char root[]="/tmp/zsh-lab-launcher-XXXXXX";CHECK(mkdtemp(root)!=NULL);char path[PATH_MAX];make_path(path,sizeof(path),root,"progress.txt");
 Progress p;progress_init(&p,path);unsigned char tar[2048]={0};
 strcpy((char*)tar,"progress.txt");snprintf((char*)tar+124,12,"%011o",(unsigned)strlen(lessons[0].id)+1);tar[156]='0';
 snprintf((char*)tar+512,512,"%s\n",lessons[0].id);import_progress(&p,tar,sizeof(tar));CHECK(p.done[0]);CHECK(access(path,F_OK)==0);
 unlink(path);progress_init(&p,path);tar[156]='2';import_progress(&p,tar,sizeof(tar));CHECK(!p.done[0]);CHECK(access(path,F_OK)!=0);
 tar[156]='0';memset(tar+124,'7',12);import_progress(&p,tar,sizeof(tar));CHECK(access(path,F_OK)!=0);
 memset(tar+124,0,12);tar[124]='9';import_progress(&p,tar,sizeof(tar));CHECK(access(path,F_OK)!=0);
 CHECK(strstr(IMAGE,ZSHLAB_VERSION)!=NULL);remove_tree(root);
 fprintf(stderr,"Launcher archive boundary checks: %s\n",failures?"FAILED":"passed");return failures?1:0;
}
