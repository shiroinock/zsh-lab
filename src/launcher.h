#include "image_context.h"
#define IMAGE "zsh-lab-workshop:" ZSHLAB_VERSION
/* Docker argv is constructed directly: no host shell or user-provided flags. */
static int docker_command(char*const args[],const void*input,size_t size,char*output,size_t capacity){
 int in[2],out[2];if(pipe(in)||pipe(out))return -1;pid_t pid=fork();
 if(pid<0){close(in[0]);close(in[1]);close(out[0]);close(out[1]);return -1;}
 if(!pid){dup2(in[0],0);if(output)dup2(out[1],1);close(in[0]);close(in[1]);close(out[0]);close(out[1]);execvp("docker",args);perror("docker");_exit(127);}
 close(in[0]);close(out[1]);int wrote=0;if(size)wrote=write_all(in[1],input,size);close(in[1]);
 size_t used=0;bool overflow=false;char block[4096];ssize_t n;
 while((n=read(out[0],block,sizeof(block)))!=0){if(n<0){if(errno==EINTR)continue;break;}if(output){if((size_t)n<capacity-used){memcpy(output+used,block,(size_t)n);used+=(size_t)n;}else {overflow=true;break;}}}
 close(out[0]);if(output&&capacity)output[used]=0;
 int status;while(waitpid(pid,&status,0)<0){if(errno!=EINTR)return -1;}
 return wrote||overflow?-1:WIFEXITED(status)?WEXITSTATUS(status):1;
}
static int prepare_image(void){
 char output[256];char*info[]={"docker","info","--format","{{.OSType}}",NULL};
 if(docker_command(info,NULL,0,output,sizeof(output))||strncmp(output,"linux",5)){
  fputs("Docker を起動してください。Docker Desktop / OrbStack などの Linux エンジンが必要です。\n",stderr);return 1;
 }
 char*inspect[]={"docker","image","ls","--quiet","--no-trunc",IMAGE,NULL};
 if(!docker_command(inspect,NULL,0,output,sizeof(output))&&output[0])return 0;
 fputs("初回の練習環境を作成しています…\n",stderr);
 char*build[]={"docker","build","--target","runtime","-t",IMAGE,"-",NULL};
 return docker_command(build,image_context,sizeof(image_context),NULL,0);
}
/* docker cp emits an archive. Accept only bounded regular-file contents,
   never extract container-controlled filenames or symbolic links on the host. */
static void import_progress(Progress*p,const unsigned char*tar,size_t length){
 for(size_t pos=0;pos+512<=length;){
  const unsigned char*h=tar+pos;if(!h[0])break;
  size_t size=0;for(int i=124;i<136;i++){if(h[i]>='0'&&h[i]<='7'){if(size>65536/8)return;size=size*8+h[i]-'0';}else if(h[i]&&h[i]!=' ')return;}
  pos+=512;if(size>65536||size>length-pos)return;
  if((h[156]==0||h[156]=='0')&&size<4096){
   char data[4096];memcpy(data,tar+pos,size);data[size]=0;
   char*save=NULL;for(char*id=strtok_r(data,"\n",&save);id;id=strtok_r(NULL,"\n",&save))for(int i=0;i<NLESSONS;i++)if(!strcmp(id,lessons[i].id))progress_save(p,i);
  }
  pos+=(size+511)/512*512;
 }
}
static int docker_launch(int index,Progress*p){
 if(prepare_image())return 1;
 char completed[4096]="ZSHLAB_COMPLETED=";for(int i=0;i<NLESSONS;i++)if(p->done[i]){strcat(completed,lessons[i].id);strcat(completed,":");}
 char lesson[16];snprintf(lesson,sizeof(lesson),"%d",index+1);char id[128];
 char*create[]={"docker","create","--pull=never","-it","--label","io.github.shiroinock.zsh-lab.session=true","--init","--network=none","--read-only","--cap-drop=ALL",
 "--security-opt=no-new-privileges","--pids-limit=64","--memory=256m","--memory-swap=256m","--cpus=1",
 "--ulimit","nofile=256:256","--ulimit","fsize=1048576:1048576","--log-driver=none",
 "--tmpfs","/tmp:rw,nosuid,nodev,noexec,size=64m,mode=1777",
 "--tmpfs","/work:rw,nosuid,nodev,size=32m,uid=1000,gid=1000,mode=0700",
 "--tmpfs","/home/learner:rw,nosuid,nodev,size=16m,uid=1000,gid=1000,mode=0700",
 "--mount","type=volume,destination=/state","--env",completed,
 IMAGE,"--lesson",lesson,"--progress","/state/progress.txt",NULL};
 if(docker_command(create,NULL,0,id,sizeof(id)))return 1;id[strcspn(id,"\r\n")]=0;
 if(strlen(id)!=64||strspn(id,"0123456789abcdef")!=64){fputs("Docker のコンテナ ID が不正です。\n",stderr);return 1;}
 signal(SIGINT,stop_handler);signal(SIGTERM,stop_handler);signal(SIGHUP,stop_handler);
 pid_t pid=fork();int result=1;
 if(pid==0){execlp("docker","docker","start","--attach","--interactive","--detach-keys=",id,(char*)NULL);_exit(127);}
 if(pid>0){int status=0;pid_t waited;while((waited=waitpid(pid,&status,0))<0&&errno==EINTR){if(stop_requested)kill(pid,SIGTERM);}if(waited==pid&&WIFEXITED(status))result=WEXITSTATUS(status);}
 char source[160];snprintf(source,sizeof(source),"%s:/state/progress.txt",id);
 char archive[65536]={0};char*copy[]={"docker","cp",source,"-",NULL};
 if(!docker_command(copy,NULL,0,archive,sizeof(archive)))import_progress(p,(unsigned char*)archive,sizeof(archive));
 char*remove[]={"docker","rm","--force","--volumes",id,NULL};char ignored[256];
 if(docker_command(remove,NULL,0,ignored,sizeof(ignored)))fputs("練習コンテナを削除できませんでした。Docker の状態を確認してください。\n",stderr);
 return result;
}
