// kernel.cc 
//	Initialization and cleanup routines for the Nachos kernel.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "main.h"
#include "kernel.h"
#include "sysdep.h"
#include "synch.h"
#include "synchlist.h"
#include "libtest.h"
#include "string.h"
#include "synchconsole.h"
#include "synchdisk.h"
#include "post.h"

//----------------------------------------------------------------------
// Kernel::Kernel
// 	Interpret command line arguments in order to determine flags 
//	for the initialization (see also comments in main.cc)  
//----------------------------------------------------------------------

Kernel::Kernel(int argc, char **argv)
{
    randomSlice = FALSE; 
    debugUserProg = FALSE;
    consoleIn = NULL;          // default is stdin
    consoleOut = NULL;         // default is stdout
#ifndef FILESYS_STUB
    formatFlag = FALSE;
#endif
    reliability = 1;            // network reliability, default is 1.0
    hostName = 0;               // machine id, also UNIX socket name
                                // 0 is the default machine id
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-rs") == 0) {
 	    ASSERT(i + 1 < argc);
	    RandomInit(atoi(argv[i + 1]));// initialize pseudo-random
					// number generator
	    randomSlice = TRUE;
	    i++;
        } else if (strcmp(argv[i], "-s") == 0) {
            debugUserProg = TRUE;
	} else if (strcmp(argv[i], "-ci") == 0) {
	    ASSERT(i + 1 < argc);
	    consoleIn = argv[i + 1];
	    i++;
	} else if (strcmp(argv[i], "-co") == 0) {
	    ASSERT(i + 1 < argc);
	    consoleOut = argv[i + 1];
	    i++;
#ifndef FILESYS_STUB
	} else if (strcmp(argv[i], "-f") == 0) {
	    formatFlag = TRUE;
#endif
        } else if (strcmp(argv[i], "-n") == 0) {
            ASSERT(i + 1 < argc);   // next argument is float
            reliability = atof(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-m") == 0) {
            ASSERT(i + 1 < argc);   // next argument is int
            hostName = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-u") == 0) {
            std::cout << "Partial usage: nachos [-rs randomSeed]\n";
	    std::cout << "Partial usage: nachos [-s]\n";
            std::cout << "Partial usage: nachos [-ci consoleIn] [-co consoleOut]\n";
#ifndef FILESYS_STUB
	    std::cout << "Partial usage: nachos [-nf]\n";
#endif
            std::cout << "Partial usage: nachos [-n #] [-m #]\n";
	}
    }
}

//----------------------------------------------------------------------
// Kernel::Initialize
// 	Initialize Nachos global data structures.  Separate from the 
//	constructor because some of these refer to earlier initialized
//	data via the "kernel" global variable.
//----------------------------------------------------------------------

void
Kernel::Initialize()
{
    // We didn't explicitly allocate the current thread we are running in.
    // But if it ever tries to give up the CPU, we better have a Thread
    // object to save its state. 
    currentThread = new Thread("main");		
    currentThread->setStatus(RUNNING);

    stats = new Statistics();		// collect statistics
    interrupt = new Interrupt;		// start up interrupt handling
    scheduler = new Scheduler();	// initialize the ready queue
    alarm = new Alarm(randomSlice);	// start up time slicing
    machine = new Machine(debugUserProg);
    synchConsoleIn = new SynchConsoleInput(consoleIn); // input from stdin
    synchConsoleOut = new SynchConsoleOutput(consoleOut); // output to stdout
    synchDisk = new SynchDisk();    //

	swap_disk = new SynchDisk();
	frame_table = (FrameInfoEntry **) malloc(NumPhysPages * sizeof(FrameInfoEntry *));
	memset(frame_table, 0, NumPhysPages * sizeof (FrameInfoEntry *));

#ifdef FILESYS_STUB
    fileSystem = new FileSystem();
#else
    fileSystem = new FileSystem(formatFlag);
#endif // FILESYS_STUB
    postOfficeIn = new PostOfficeInput(10);
    postOfficeOut = new PostOfficeOutput(reliability);

    interrupt->Enable();
}

//----------------------------------------------------------------------
// Kernel::~Kernel
// 	Nachos is halting.  De-allocate global data structures.
//----------------------------------------------------------------------

Kernel::~Kernel()
{
    delete stats;
    delete interrupt;
    delete scheduler;
    delete alarm;
    delete machine;
    delete synchConsoleIn;
    delete synchConsoleOut;
    delete synchDisk;
    delete fileSystem;
    delete postOfficeIn;
    delete postOfficeOut;
    
    Exit(0);
}

size_t Kernel::select_n_swap_page() {
	size_t ret = 0;
	size_t min_used = 0;

	for (size_t i = 0; i < NumPhysPages; i++) {
		if (frame_table[i] == NULL) {
			return i;
		} else {
			if (frame_table[i]->used < min_used) {
				ret = i; min_used = frame_table[i]->used;
			}
		}
	}

	for (size_t i = 0; i < swap_table.size(); i++) {
		if (swap_table[i] == frame_table[ret]) {
			frame_table[ret]->used = 0;
			frame_table[ret] = NULL;
			swap_disk->WriteSector(i, machine->mainMemory + ret * PageSize);

			swap_table[i]->program->pageTable[swap_table[i]->vpage].valid = false;
			swap_table[i]->program->pageTable[swap_table[i]->vpage].physicalPage = -1;
			swap_table[i]->program->pageTable[swap_table[i]->vpage].dirty = false;
		}
	}

	return ret;
}

void Kernel::new_page(AddrSpace *space, size_t vpn) {
	FrameInfoEntry *entry = NULL;
	for (size_t i = 0; i < swap_table.size(); i++) {
		if (swap_table[i]->valid == false) {
			entry = swap_table[i];
		}
	}

	if (entry == NULL) {
		entry = new FrameInfoEntry;
		swap_table.push_back(entry);
	}	   

	entry->valid = true;
	entry->used = 0;
	entry->locked = false;
	entry->program = space;
	entry->vpage = vpn;
}

void Kernel::load_page(AddrSpace *space, size_t vpn) {
	size_t slot = select_n_swap_page();
	for (size_t i = 0; i < swap_table.size(); i++) {
		if (swap_table[i]->program == space && swap_table[i]->vpage == vpn) {
			frame_table[slot] = swap_table[i];
			swap_disk->ReadSector(i, machine->mainMemory + slot * PageSize);

			space->pageTable[vpn].valid = true;
			space->pageTable[vpn].dirty = false;
			space->pageTable[vpn].physicalPage = slot;

			return;
		}
	}
}

bool Kernel::fetch_page(AddrSpace *space, size_t vpn) {

}

void Kernel::free_page(AddrSpace *space, size_t vpn) {

}

//----------------------------------------------------------------------
// Kernel::ThreadSelfTest
//      Test threads, semaphores, synchlists
//----------------------------------------------------------------------

void
Kernel::ThreadSelfTest() {
   Semaphore *semaphore;
   SynchList<int> *synchList;
   
   LibSelfTest();		// test library routines
   
   currentThread->SelfTest();	// test thread switching
   
   				// test semaphore operation
   semaphore = new Semaphore("test", 0);
   semaphore->SelfTest();
   delete semaphore;
   
   				// test locks, condition variables
				// using synchronized lists
   synchList = new SynchList<int>;
   synchList->SelfTest(9);
   delete synchList;

}

//----------------------------------------------------------------------
// Kernel::ConsoleTest
//      Test the synchconsole
//----------------------------------------------------------------------

void
Kernel::ConsoleTest() {
    char ch;

    std::cout << "Testing the console device.\n" 
        << "Typed characters will be echoed, until ^D is typed.\n"
        << "Note newlines are needed to flush input through UNIX.\n";
    std::cout.flush();

    do {
        ch = synchConsoleIn->GetChar();
        if(ch != EOF) synchConsoleOut->PutChar(ch);   // echo it!
    } while (ch != EOF);

    std::cout << "\n";

}

//----------------------------------------------------------------------
// Kernel::NetworkTest
//      Test whether the post office is working. On machines #0 and #1, do:
//
//      1. send a message to the other machine at mail box #0
//      2. wait for the other machine's message to arrive (in our mailbox #0)
//      3. send an acknowledgment for the other machine's message
//      4. wait for an acknowledgement from the other machine to our 
//          original message
//
//  This test works best if each Nachos machine has its own window
//----------------------------------------------------------------------

void
Kernel::NetworkTest() {

    if (hostName == 0 || hostName == 1) {
        // if we're machine 1, send to 0 and vice versa
        int farHost = (hostName == 0 ? 1 : 0); 
        PacketHeader outPktHdr, inPktHdr;
        MailHeader outMailHdr, inMailHdr;
        const char *data = "Hello there!";
        const char *ack = "Got it!";
        char buffer[MaxMailSize];

        // construct packet, mail header for original message
        // To: destination machine, mailbox 0
        // From: our machine, reply to: mailbox 1
        outPktHdr.to = farHost;         
        outMailHdr.to = 0;
        outMailHdr.from = 1;
        outMailHdr.length = strlen(data) + 1;

        // Send the first message
        postOfficeOut->Send(outPktHdr, outMailHdr, data); 

        // Wait for the first message from the other machine
        postOfficeIn->Receive(0, &inPktHdr, &inMailHdr, buffer);
        std::cout << "Got: " << buffer << " : from " << inPktHdr.from << ", box " 
                                                << inMailHdr.from << "\n";
        std::cout.flush();

        // Send acknowledgement to the other machine (using "reply to" mailbox
        // in the message that just arrived
        outPktHdr.to = inPktHdr.from;
        outMailHdr.to = inMailHdr.from;
        outMailHdr.length = strlen(ack) + 1;
        postOfficeOut->Send(outPktHdr, outMailHdr, ack); 

        // Wait for the ack from the other machine to the first message we sent
	postOfficeIn->Receive(1, &inPktHdr, &inMailHdr, buffer);
        std::cout << "Got: " << buffer << " : from " << inPktHdr.from << ", box " 
                                                << inMailHdr.from << "\n";
        std::cout.flush();
    }

    // Then we're done!
}

