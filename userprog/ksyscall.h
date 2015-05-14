/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__
#define __USERPROG_KSYSCALL_H__

#include "kernel.h"
#include "synchconsole.h"

typedef int OpenFileId;
typedef int SpaceId;

#define ConsoleInput 0
#define ConsoleOutput 1

void SysHalt() { kernel->interrupt->Halt(); }

int SysAdd(int op1, int op2) { return op1 + op2; }

int SysRead(char* buffer, int size, OpenFileId id) {
    if (id == ConsoleInput) {

        for (int i = 0; i < size; i++) {
			int t = kernel->synchConsoleIn->GetChar();

			bool write_s = kernel->machine->WriteMem((int) (buffer+i), 1, t);
			if (!write_s) {
				return -1;
			}
        }
        return size;
    }
	return 0;
}

int SysWrite(char* buffer, int size, OpenFileId id) {
    if (id == ConsoleOutput) {
        for (int i = 0; i < size; i++) {
			int t = 0;
			bool read_s = kernel->machine->ReadMem((int) (buffer+i), 1, &t);

			if (!read_s) {
				return -1;
			}

            kernel->synchConsoleOut->PutChar((char) t);
        }
        return size;
    }
	return -1;
}

size_t user_strlen(char *src) {
	size_t ret = 0;
	int t = 0;
	while (true) {
		kernel->machine->ReadMem((int) (src + ret), 1, &t);
		if (t == 0) return ret;
		ret += 1;
	}
}

struct execT {
	char *filename;
	AddrSpace *space;
};

void execvt(void *args_raw) {
	AddrSpace *space = new AddrSpace;
	struct execT *args = (struct execT *) args_raw;
	if (space->Load(args->filename))
		space->Execute();
}

#include <unistd.h>
#include <sys/wait.h>
// dunno why but you cannot inspect errno here

SpaceId SysExecV(int args, char *argv[]) {

	int pid = vfork();
	if (pid == -1) {
		return 0;
	} else if (pid != 0) {
		return (SpaceId) pid;
	} else {
		execvp(argv[0], argv);
		return -1208;
	}

}

SpaceId SysExec(char *exec_name) {
	char *argv[] = { exec_name, NULL };
	return SysExecV(1, argv);
}

int SysJoin(SpaceId id) {
	if (id > 0) {
		int status = -1;
		int r = waitpid(id, &status, 0);
		if (r <= 0) {
			return -1;
		}
		return status;
	}

	return -1;
}

void SysExit(int status) {
	kernel->currentThread->Finish();
}

#endif /* ! __USERPROG_KSYSCALL_H__ */
