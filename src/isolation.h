/* Only the container build may execute a shell. The host binary is a launcher. */
static void isolated_exec(const Session*s){
#ifdef ZSHLAB_CONTAINER_BUILD
 const char*editor="vim -Nu NONE -i NONE";
 setenv("HOME","/home/learner",1);setenv("TMPDIR","/tmp",1);setenv("TMPPREFIX","/tmp/zsh",1);
 setenv("ZDOTDIR",s->root,1);setenv("TERM","xterm-256color",1);
 setenv("EDITOR",editor,1);setenv("VISUAL",editor,1);
 unsetenv("ENV");unsetenv("ZSHLAB_COMPLETED");
 if(chdir("/work")!=0)_exit(126);
 execl(s->shell,s->shell,"-d","-i",(char*)NULL);
#else
 (void)s;
 dprintf(2,"練習用 zsh は Docker コンテナ内でのみ起動できます。\n");
#endif
 _exit(126);
}
