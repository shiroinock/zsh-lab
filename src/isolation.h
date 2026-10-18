/* The shell always crosses an OS sandbox boundary before executing any rc file. */
extern char **environ;
static void isolated_environment(const Session*s){
 const char*names[]={"PATH","LANG","LC_ALL","LOCALE_ARCHIVE","TERMINFO","TERMINFO_DIRS","EDITOR","VISUAL",NULL};
 char*values[9]={0};
 for(int i=0;names[i];i++){const char*v=getenv(names[i]);if(v)values[i]=strdup(v);}
 while(environ&&environ[0]){char*name=strdup(environ[0]);if(!name)_exit(126);char*equals=strchr(name,'=');if(equals)*equals=0;if(unsetenv(name)!=0)_exit(126);free(name);}
 for(int i=0;names[i];i++){if(values[i]){setenv(names[i],values[i],1);free(values[i]);}}
 char home[PATH_MAX],tmp[PATH_MAX];make_path(home,sizeof(home),s->root,"home");make_path(tmp,sizeof(tmp),s->root,"tmp");
 char prefix[PATH_MAX];make_path(prefix,sizeof(prefix),tmp,"zsh");setenv("TMPPREFIX",prefix,1);
 setenv("HOME",home,1);setenv("TMPDIR",tmp,1);setenv("ZDOTDIR",s->root,1);setenv("TERM","xterm-256color",1);
 setenv("XDG_CONFIG_HOME",home,1);setenv("XDG_CACHE_HOME",tmp,1);setenv("XDG_STATE_HOME",home,1);
 setenv("USER","learner",1);setenv("LOGNAME","learner",1);
}
static void isolated_exec(const Session*s){
 /* Do not carry an already-open host file or credential socket across exec. */
 DIR*descriptors=opendir("/dev/fd");
 if(!descriptors)_exit(126);
 struct dirent*entry;int directory_fd=dirfd(descriptors);
 while((entry=readdir(descriptors))){char*end;long fd=strtol(entry->d_name,&end,10);if(!*end&&fd>=3&&fd!=directory_fd)close((int)fd);}
 closedir(descriptors);
 isolated_environment(s);
 if(chdir(s->root)!=0)_exit(126);
#ifdef __APPLE__
 const char*policy=
 "(version 1)(deny default)"
 "(allow process-exec process-fork)"
 "(allow signal (target same-sandbox))"
 "(allow sysctl-read)"
 "(allow file-read* (subpath (param \"WORK\"))"
 " (subpath \"/System\") (subpath \"/usr\") (subpath \"/bin\") (subpath \"/sbin\")"
 " (subpath \"/Library/Apple\") (subpath \"/nix/store\") (subpath \"/private/var/db/dyld\")"
 " (literal \"/\") (literal \"/dev\") (literal \"/private\") (literal \"/private/var\")"
 " (literal (param \"TTY\")) (literal \"/dev/null\") (literal \"/dev/tty\") (literal \"/dev/random\") (literal \"/dev/urandom\"))"
 "(allow file-write* (literal (param \"TTY\")) (subpath (param \"WORK\")) (literal \"/dev/null\") (literal \"/dev/tty\"))"
 "(allow file-ioctl)";
 char tty_parameter[PATH_MAX+8];const char*tty=ttyname(0);if(!tty)_exit(126);snprintf(tty_parameter,sizeof(tty_parameter),"TTY=%s",tty);
 char parameter[PATH_MAX+8];snprintf(parameter,sizeof(parameter),"WORK=%s",s->root);
 execl("/usr/bin/sandbox-exec","sandbox-exec","-D",parameter,"-D",tty_parameter,"-p",policy,s->shell,"-d","-i",(char*)NULL);
 dprintf(2,"隔離を有効にできません: sandbox-exec が必要です。\r\n");
#else
 /* No host home, runtime sockets or network namespace is shared. */
 char*args[128];int n=0;
 #define ARG(x) args[n++]=(char*)(x)
 ARG("bwrap");ARG("--unshare-all");ARG("--die-with-parent");ARG("--new-session");
 const char*dirs[]={"/usr","/bin","/sbin","/lib","/lib64","/nix/store",NULL};
 for(int i=0;dirs[i];i++)if(access(dirs[i],F_OK)==0){ARG("--ro-bind");ARG(dirs[i]);ARG(dirs[i]);}
 ARG("--dir");ARG("/etc");
 if(access("/etc/ld.so.cache",R_OK)==0){ARG("--ro-bind");ARG("/etc/ld.so.cache");ARG("/etc/ld.so.cache");}
 ARG("--proc");ARG("/proc");ARG("--dev");ARG("/dev");ARG("--tmpfs");ARG("/tmp");
 ARG("--bind");ARG(s->root);ARG(s->root);ARG("--chdir");ARG(s->root);
 ARG("--");ARG(s->shell);ARG("-d");ARG("-i");args[n]=NULL;
 execvp("bwrap",args);
 #undef ARG
 dprintf(2,"隔離を有効にできません: bubblewrap が必要です。Nix パッケージで起動してください。\r\n");
#endif
 _exit(126);
}
