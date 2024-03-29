#include <stdio.h>
#include <stdlib.h>
//#include <netinet/in.h>
#include "computer.h"
#undef mips			/* gcc already has a def for mips */

unsigned int endianSwap(unsigned int);

void PrintInfo (int changedReg, int changedMem);
unsigned int Fetch (int);
void Decode (unsigned int, DecodedInstr*, RegVals*);
int Execute (DecodedInstr*, RegVals*);
int Mem(DecodedInstr*, int, int *);
void RegWrite(DecodedInstr*, int, int *);
void UpdatePC(DecodedInstr*, int);
void PrintInstruction (DecodedInstr*);

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer (FILE* filein, int printingRegisters, int printingMemory,
  int debugging, int interactive) {
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k=0; k<32; k++) {
        mips.registers[k] = 0;
    }
    
    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        mips.memory[k] = 0;
    }

    k = 0;
    while (fread(&instr, 4, 1, filein)) {
	/*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k>MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

/*
 *  Run the simulation.
 */
void Simulate () {
    char s[40];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    DecodedInstr d;
    
    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;
    while (1) {
        if (mips.interactive) {
            printf ("> ");
            fgets (s,sizeof(s),stdin);
            if (s[0] == 'q') {
                return;
            }
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch (mips.pc);

        printf ("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

        /* 
	 * Decode instr, putting decoded instr in d
	 * Note that we reuse the d struct for each instruction.
	 */
        Decode (instr, &d, &rVals);

        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
	 * Perform computation needed to execute d, returning computed value 
	 * in val 
	 */
        val = Execute(&d, &rVals);

	UpdatePC(&d,val);

        /* 
	 * Perform memory load or store. Place the
	 * address of any updated memory in *changedMem, 
	 * otherwise put -1 in *changedMem. 
	 * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(&d, val, &changedMem);

        /* 
	 * Write back to register. If the instruction modified a register--
	 * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo (changedReg, changedMem);
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo ( int changedReg, int changedMem) {
    int k, addr;
    printf ("New pc = %8.8x\n", mips.pc);
    if (!mips.printingRegisters && changedReg == -1) {
        printf ("No register was updated.\n");
    } else if (!mips.printingRegisters) {
        printf ("Updated r%2.2d to %8.8x\n",
        changedReg, mips.registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf ("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k+1)%4 == 0) {
                printf ("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1) {
        printf ("No memory location was updated.\n");
    } else if (!mips.printingMemory) {
        printf ("Updated memory at address %8.8x to %8.8x\n",
        changedMem, Fetch (changedMem));
    } else {
        printf ("Nonzero memory\n");
        printf ("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
             addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
             addr = addr+4) {
            if (Fetch (addr) != 0) {
                printf ("%8.8x  %8.8x\n", addr, Fetch (addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch ( int addr) {
    return mips.memory[(addr-0x00400000)/4];
}

int getRegisterValue(unsigned int instr, int y, int x, DecodedInstr* d){
    int mask = ((1 << x) - 1) << y;
    int registerValue = 0; 
    registerValue = instr & mask;
    if((d->type) == J){
        registerValue = registerValue << 2;
    }else{
        registerValue = registerValue >> y;
    }    
    return registerValue;
}

int checkNegImmed(unsigned int immedValue){

    int negMask = 1 << 14;
    int mask = ((1 << 15) - 1) << 0;
    int registerValue = immedValue; 
    int negValue = 0;

    if((immedValue & negMask) > 0){
        negValue = (~immedValue) + 1;
        registerValue = negValue & mask;
        return -registerValue;
    }else{
        return registerValue;
    }

}

/* Decode instr, returning decoded instruction. */
void Decode ( unsigned int instr, DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */   

    int rsLocation = 21;
    int rtLocation = 16;
    int rdLocation = 11;
    int shamtLocation = 6;
    int functLocation = 0;
    int immedLocation = 0;
    int addressLocation = 0;

    int registerLength = 5;
    int functLength = 6;     
    int immedLength = 15;
    int addressLength = 25;

    d->op = instr >> 26;    

    if(d->op == 0){
        d->type = R;
        d->regs.r.rs = getRegisterValue(instr, rsLocation, registerLength, d);
        d->regs.r.rt = getRegisterValue(instr, rtLocation, registerLength, d);
        d->regs.r.rd = getRegisterValue(instr, rdLocation, registerLength, d);
        d->regs.r.shamt = getRegisterValue(instr, shamtLocation, registerLength, d);
        d->regs.r.funct = getRegisterValue(instr, functLocation, functLength, d);
        rVals->R_rd = mips.registers[d->regs.r.rd];
        rVals->R_rt = mips.registers[d->regs.r.rt];
        rVals->R_rs = mips.registers[d->regs.r.rs];
    } else if(d->op == 2 || d->op == 3){
        d->type = J;
        d->regs.j.target = getRegisterValue(instr, addressLocation, addressLength, d);
    } else{
        d->type = I;
        d->regs.i.rs = getRegisterValue(instr, rsLocation, registerLength, d);
        d->regs.i.rt = getRegisterValue(instr, rtLocation, registerLength, d);
        d->regs.i.addr_or_immed = checkNegImmed(getRegisterValue(instr, immedLocation, immedLength, d));
        rVals->R_rt = mips.registers[d->regs.i.rt];
        rVals->R_rs = mips.registers[d->regs.i.rs];
    }
}
/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {
    /* Your code goes here */
    char* instr = " ";
    int supportedInstruction = 1;
    switch(d->op){
    case 0:
       switch(d->regs.r.funct){
            case 0:
                instr = "sll";
                break;
            case 2:
                instr = "srl";
                break;
            case 8:
                instr = "jr";
                break;
            case 33:
                instr = "addu";
                break;
            case 35:
                instr = "subu";
                break;
            case 36:
                instr = "and";
                break;
            case 37:
                instr = "or";
                break;
            case 42:
                instr = "slt";
                break;
            default:
                supportedInstruction = 0;
                break;
       }
       break;
    case 2:
        instr = "j";
        break;
    case 3:
        instr = "jal";
        break; 
    case 4: 
        instr = "beq";
        break;
    case 5:
        instr = "bne";
        break;
    case 9:
        instr = "addiu";
        break;
    case 12:
        instr = "andi";
        break;
    case 13:
        instr = "ori";
        break;
    case 15:
        instr = "lui";
        break;
    case 35:
        instr = "lw";
        break;
    case 43:
        instr = "sw";
        break;
    default:
        supportedInstruction = 0;
        break;
    }

    if(supportedInstruction == 0){
        exit(0);
    }
    if(d->type == R){
        if(d->regs.r.funct == 2){
            printf("%s\t$%d, $%d, %d\n", instr, d->regs.r.rd, d->regs.r.rs, d->regs.r.shamt);
        }else if(d->regs.r.funct == 8){
            printf("%s\t$%d,\n", instr,d->regs.r.rs);
        }else{
            printf("%s\t$%d, $%d, $%d\n", instr, d->regs.r.rd,d->regs.r.rs,d->regs.r.rt);
        }        
    }else if(d->type == J){
        printf("%s\t0x%08x\n", instr, d->regs.j.target);
    }else if(d->type == I){
        if(d->op == 43 || d->op == 35){
            printf("%s \t$%d, %d($%d)\n", instr, d->regs.i.rt, d->regs.i.addr_or_immed, d->regs.i.rs);
        } else if(d->op == 15){
            printf("%s\t$%d, 0x%x\n", instr, d->regs.i.rt, d->regs.i.addr_or_immed);
        } else if(d->op == 12 || d->op == 13){
            printf("%s \t$%d, $%d, 0x%x\n", instr, d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        } else if(d->op == 4 || d->op == 5){
            printf("%s \t$%d, $%d, 0x%08x\n", instr, d->regs.i.rs, d->regs.i.rt, (mips.pc + 4 + (d->regs.i.addr_or_immed << 2)));
        } else {
            printf("%s \t$%d, $%d, %d\n", instr, d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
        }
    }
}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */

    switch(d->op){
        case 0:
            switch(d->regs.r.funct){
                //sll
                case 0:
                    return rVals->R_rt << d->regs.r.shamt;
                //srl
                case 2:
                    return rVals->R_rt >> d->regs.r.shamt;
                //jr
                case 8:
                    return rVals->R_rs;
                //addu
                case 33:
                    return rVals->R_rs + rVals->R_rt;
                //subu
                case 35:
                    return rVals->R_rs - rVals->R_rt;
                //and
                case 36:
                    return rVals->R_rs & rVals->R_rt;
                //or
                case 37:
                    return rVals->R_rs | rVals->R_rt;
                //slt
                case 38:
                    return (rVals->R_rs - rVals->R_rt) > 0;
            }
            break;
        case 2:
        //j
            break;
        case 3:
        //jal
            return mips.pc + 4;
            break;
        //beq
        case 4:
            if((rVals->R_rs - rVals->R_rt) == 0){
                return d->regs.i.addr_or_immed << 2;
                break;
            } else {
                return 0;
                break;
            }
        //bne
        case 5:
            if(rVals->R_rs - rVals->R_rt == 0){
                return 0;
                break;
            } else {
                return d->regs.i.addr_or_immed << 2;
                break;
            }
        //addiu
        case 9:
            return rVals->R_rs + d->regs.i.addr_or_immed;
            break;
        //andi
        case 12:
            return rVals->R_rs & d->regs.i.addr_or_immed;
            break;
        //ori
        case 13:
            return rVals->R_rs | d->regs.i.addr_or_immed;
            break;
        //lui
        case 15:
            return d->regs.i.addr_or_immed << 16;
            break;
        //lw
        case 35:
            return rVals->R_rs + d->regs.i.addr_or_immed;
            break;
        //sw
        case 43:
            return rVals->R_rs + d->regs.i.addr_or_immed;
            break;
    }
  return 0;
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    
    /* Your code goes here */
    mips.pc += 4;
    if(d->op == 0){
        if(d->regs.r.funct == 8){
            mips.pc = val;
        }
    }else if(d->op == 2 || d->op == 3){
        mips.pc = d->regs.j.target;
    }else if(d->op == 4 || d->op == 5){
        mips.pc += val;
    }
    
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem( DecodedInstr* d, int val, int *changedMem) {
    /* Your code goes here */

    *changedMem = -1;
    int newAddr = 0;
    if(d->op == 43){
        if(val < 0x00400000 || val > 0x00410000){
            printf("Memory Access Expection at 0x%08x: address 0x%08x\n",mips.pc-4, val);
            exit(0);
        }
        newAddr = (val - 0x00400000)/4;
        mips.memory[newAddr] = mips.registers[d->regs.i.rt];
        *changedMem = val;
        
    } else if(d->op == 35){
        if(val < 0x00400000 || val > 0x00410000){
            printf("Memory Access Expection at 0x%08x: address 0x%08x\n",mips.pc-4, val);
            exit(0);
        }
        mips.registers[d->regs.i.rt] = Fetch(val);
        val = mips.registers[d->regs.i.rt];
        *changedMem = -1;
    }
  return val;
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    /* Your code goes here */

    *changedReg = -1;
    if(d->op == 0){
        if(d->regs.r.funct == 8){
            //address
        }
        mips.registers[d->regs.r.rd] = val;
        *changedReg = d->regs.r.rd;
               
    }else if(d->op == 3){
        mips.registers[31] = val;
        *changedReg = 31;
    }
    else if(d->op == 9 || d->op == 12 || d->op == 13 || d->op == 15 || d->op == 43){
        if(d->regs.r.rt == 0){
            // cuz its already empty
        }
        mips.registers[d->regs.i.rt] = val;
        *changedReg = d->regs.i.rt;      
    }
}
