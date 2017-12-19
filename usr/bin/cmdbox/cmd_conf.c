/*
 * Copyright (c) 2007, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <prex/prex.h>
#include "cmdbox.h"

extern int help_main(int argc, char *argv[]);
extern int cal_main(int argc, char *argv[]);
extern int cat_main(int argc, char *argv[]);
extern int clear_main(int argc, char *argv[]);
extern int cp_main(int argc, char *argv[]);
extern int date_main(int argc, char *argv[]);
extern int dmesg_main(int argc, char *argv[]);
extern int echo_main(int argc, char *argv[]);
extern int free_main(int argc, char *argv[]);
extern int head_main(int argc, char *argv[]);
extern int hostname_main(int argc, char *argv[]);
extern int kill_main(int argc, char *argv[]);
extern int ls_main(int argc, char *argv[]);
extern int mkdir_main(int argc, char *argv[]);
extern int mount_main(int argc, char *argv[]);
extern int mv_main(int argc, char *argv[]);
extern int nice_main(int argc, char *argv[]);
extern int ps_main(int argc, char *argv[]);
extern int pwd_main(int argc, char *argv[]);
extern int reboot_main(int argc, char *argv[]);
extern int rm_main(int argc, char *argv[]);
extern int rmdir_main(int argc, char *argv[]);
extern int sh_main(int argc, char *argv[]);
extern int shutdown_main(int argc, char *argv[]);
extern int sleep_main(int argc, char *argv[]);
extern int sync_main(int argc, char *argv[]);
extern int test_main(int argc, char *argv[]);
extern int touch_main(int argc, char *argv[]);
extern int umount_main(int argc, char *argv[]);
extern int uname_main(int argc, char *argv[]);

/*
 * Bultin commands
 */
const struct cmd_entry builtin_cmds[] = {
	{ "help"     ,help_main       },
#ifdef CONFIG_CMD_CAL
	{ "cal"      ,cal_main        },
#endif
#ifdef CONFIG_CMD_CAT
	{ "cat"      ,cat_main        },
#endif
#ifdef CONFIG_CMD_CLEAR
	{ "clear"    ,clear_main      },
#endif
#ifdef CONFIG_CMD_CP
	{ "cp"       ,cp_main         },
#endif
#ifdef CONFIG_CMD_DATE
	{ "date"     ,date_main       },
#endif
#ifdef CONFIG_CMD_DMESG
	{ "dmesg"    ,dmesg_main      },
#endif
#ifdef CONFIG_CMD_ECHO
	{ "echo"     ,echo_main       },
#endif
#ifdef CONFIG_CMD_FREE
	{ "free"     ,free_main       },
#endif
#ifdef CONFIG_CMD_HEAD
	{ "head"     ,head_main   },
#endif
#ifdef CONFIG_CMD_HOSTNAME
	{ "hostname" ,hostname_main   },
#endif
#ifdef CONFIG_CMD_KILL
	{ "kill"     ,kill_main       },
#endif
#ifdef CONFIG_CMD_LS
	{ "ls"       ,ls_main         },
#endif
#ifdef CONFIG_CMD_MKDIR
	{ "mkdir"    ,mkdir_main      },
#endif
#ifdef CONFIG_CMD_MOUNT
	{ "mount"    ,mount_main      },
#endif
#ifdef CONFIG_CMD_MV
	{ "mv"       ,mv_main         },
#endif
#ifdef CONFIG_CMD_NICE
	{ "nice"     ,nice_main       },
#endif
#ifdef CONFIG_CMD_PS
	{ "ps"       ,ps_main         },
#endif
#ifdef CONFIG_CMD_PWD
	{ "pwd"      ,pwd_main        },
#endif
#ifdef CONFIG_CMD_REBOOT
	{ "reboot"   ,reboot_main     },
#endif
#ifdef CONFIG_CMD_RM
	{ "rm"       ,rm_main         },
#endif
#ifdef CONFIG_CMD_RMDIR
	{ "rmdir"    ,rmdir_main      },
#endif
#ifdef CONFIG_CMD_SH
	{ "sh"       ,sh_main         },
#endif
#ifdef CONFIG_CMD_SHUTDOWN
	{ "shutdown" ,shutdown_main   },
#endif
#ifdef CONFIG_CMD_SLEEP
	{ "sleep"    ,sleep_main      },
#endif
#ifdef CONFIG_CMD_SYNC
	{ "sync"     ,sync_main       },
#endif
#ifdef CONFIG_CMD_TEST
	{ "test"     ,test_main       },
#endif
#ifdef CONFIG_CMD_TOUCH
	{ "touch"    ,touch_main      },
#endif
#ifdef CONFIG_CMD_UMOUNT
	{ "umount"   ,umount_main     },
#endif
#ifdef CONFIG_CMD_UNAME
	{ "uname"    ,uname_main      },
#endif
	{ NULL       ,0               },
};
