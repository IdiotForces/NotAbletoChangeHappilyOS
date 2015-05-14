// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "ksyscall.h"
#include "syscall.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType which)
{
    int type = kernel->machine->ReadRegister(2);

    DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

    switch (which) {
    case SyscallException:
        switch (type) {
        case SC_Halt:
            DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

            SysHalt();

            ASSERTNOTREACHED();
            break;

        case SC_Add:
            DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + "
                                 << kernel->machine->ReadRegister(5) << "\n");

            /* Process SysAdd Systemcall*/
            int result;
            result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
                /* int op2 */ (int)kernel->machine->ReadRegister(5));

            DEBUG(dbgSys, "Add returning with " << result << "\n");
            /* Prepare Result */
            kernel->machine->WriteRegister(2, (int)result);

            /* Modify return point */
            {
                /* set previous programm counter (debugging only)*/
                kernel->machine->WriteRegister(PrevPCReg,
                    kernel->machine->ReadRegister(PCReg));

                /* set programm counter to next instruction (all Instructions are 4 byte
         * wide)*/
                kernel->machine->WriteRegister(
                    PCReg, kernel->machine->ReadRegister(PCReg) + 4);

                /* set next programm counter for brach execution */
                kernel->machine->WriteRegister(
                    NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
            }

            return;

            ASSERTNOTREACHED();

            break;

		case SC_Exit:
			DEBUG(dbgSys, "Exit with status " << (int) kernel->machine->ReadRegister(4) << ".\n");

			SysExit((int) kernel->machine->ReadRegister(4));

            {
                /* set previous programm counter (debugging only)*/
                kernel->machine->WriteRegister(PrevPCReg,
                    kernel->machine->ReadRegister(PCReg));

                /* set programm counter to next instruction (all Instructions are 4 byte
         * wide)*/
                kernel->machine->WriteRegister(
                    PCReg, kernel->machine->ReadRegister(PCReg) + 4);

                /* set next programm counter for brach execution */
                kernel->machine->WriteRegister(
                    NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
            }

			return;
			ASSERTNOTREACHED();
			break;

        case SC_Read:

			DEBUG(dbgSys, "Read " << kernel->machine->ReadRegister(5) << " bytes from "
								<< kernel->machine->ReadRegister(6) << "\n");
			{
				int result = -1;
				char *buffer = (char *) kernel->machine->ReadRegister(4);
				ExceptionType tran_s = kernel->currentThread->space->Translate((unsigned int) buffer, (unsigned int *) &buffer, 1);

				if (tran_s == NoException) {
					result = SysRead(buffer, (int) kernel->machine->ReadRegister(5),
										(OpenFileId) kernel->machine->ReadRegister(6));
				}

				DEBUG(dbgSys, "Add returning with " << result << "\n");
				kernel->machine->WriteRegister(2, (int)result);
			}

			// ** WTF? **
			{
				kernel->machine->WriteRegister(PrevPCReg,
					kernel->machine->ReadRegister(PCReg));
				kernel->machine->WriteRegister(
					PCReg, kernel->machine->ReadRegister(PCReg) + 4);
				kernel->machine->WriteRegister(
					NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}

            return;
            ASSERTNOTREACHED();
            break;

        case SC_Write:


			DEBUG(dbgSys, "Write " << kernel->machine->ReadRegister(5) << " bytes to "
								<< kernel->machine->ReadRegister(6) << "\n");
			{

				int result = -1;
				char *buffer = (char *) kernel->machine->ReadRegister(4);
				ExceptionType tran_s = kernel->currentThread->space->Translate((unsigned int) buffer, (unsigned int *) &buffer, 0);

				if (tran_s == NoException) {
					result = SysWrite(buffer, (int)kernel->machine->ReadRegister(5),
										 (OpenFileId)kernel->machine->ReadRegister(6));
				}

				DEBUG(dbgSys, "Add returning with " << result << "\n");
				kernel->machine->WriteRegister(2, (int)result);
			}

			{
				kernel->machine->WriteRegister(PrevPCReg,
					kernel->machine->ReadRegister(PCReg));
				kernel->machine->WriteRegister(
					PCReg, kernel->machine->ReadRegister(PCReg) + 4);
				kernel->machine->WriteRegister(
					NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}


            return;
            ASSERTNOTREACHED();
            break;

		case SC_ExecV:

			DEBUG(dbgSys, "ExecV with " << kernel->machine->ReadRegister(5) << "args.\n");

			{
				SpaceId result;
				char *buffer = (char *) kernel->machine->ReadRegister(4);
				ExceptionType tran_s = kernel->currentThread->space->Translate((unsigned int) buffer, (unsigned int *) &buffer, 0);

				if (tran_s == NoException) {
					result = SysExecV((int) kernel->machine->ReadRegister(4),
										(char **) kernel->machine->ReadRegister(5));
				}
				DEBUG(dbgSys, "Add returning with " << result << "\n");
				kernel->machine->WriteRegister(2, (int)result);
			}

			{
				kernel->machine->WriteRegister(PrevPCReg,
						kernel->machine->ReadRegister(PCReg));
				kernel->machine->WriteRegister(
						PCReg, kernel->machine->ReadRegister(PCReg) + 4);
				kernel->machine->WriteRegister(
						NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}

			return;
			ASSERTNOTREACHED();
			break;

		case SC_Exec:

			DEBUG(dbgSys, "Exec \n");

			{
				SpaceId result;
				char *buffer = (char *) kernel->machine->ReadRegister(4);
				ExceptionType tran_s = kernel->currentThread->space->Translate((unsigned int) buffer, (unsigned int *) &buffer, 0);
				size_t len = user_strlen(buffer);
				char *buf_int = (char *) malloc(len+1);
				for (size_t i = 0; i < len; i++)
					kernel->machine->ReadMem((int) (buffer+i), 1, (int *) (buf_int+i));
				buf_int[len] = '\0';

				if (tran_s == NoException) {
					result = SysExec(buf_int);
				}

				free(buf_int);

				DEBUG(dbgSys, "Add returning with " << result << "\n");
				kernel->machine->WriteRegister(2, (int) result);
			}

			{
				kernel->machine->WriteRegister(PrevPCReg,
						kernel->machine->ReadRegister(PCReg));
				kernel->machine->WriteRegister(
						PCReg, kernel->machine->ReadRegister(PCReg) + 4);
				kernel->machine->WriteRegister(
						NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}

			return;
			ASSERTNOTREACHED();
			break;

		case SC_Join:

			DEBUG(dbgSys, "Joining " << kernel->machine->ReadRegister(4) << "\n");

			{
				int result = SysJoin((int) kernel->machine->ReadRegister(4));
				DEBUG(dbgSys, "Add returning with " << result << "\n");
				kernel->machine->WriteRegister(2, (int) result);
			}

			{
				kernel->machine->WriteRegister(PrevPCReg,
						kernel->machine->ReadRegister(PCReg));
				kernel->machine->WriteRegister(
						PCReg, kernel->machine->ReadRegister(PCReg) + 4);
				kernel->machine->WriteRegister(
						NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
			}

			return;
			ASSERTNOTREACHED();
			break;

        default:
            std::cerr << "Unexpected system call " << type << "\n";
            break;
        }
        break;
    default:
        std::cerr << "Unexpected user mode exception" << (int)which << "\n";
        break;
    }
    ASSERTNOTREACHED();
}
