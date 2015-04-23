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

SpaceId SysExecV(int args, char *argv[]) {

}

SpaceId SysExec(char *exec_name) {
	char *argv[] = { exec_name };
	return SysExecV(1, argv);
}

#endif /* ! __USERPROG_KSYSCALL_H__ */
