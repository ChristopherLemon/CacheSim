#include "pin.H"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>
#include "tool_macros.h"

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

std::ofstream LogFile;
std::ofstream MemoryFile;

// input string
std::string function_name;

// No input string, so trace everything
bool print_all;

// For each thread, indicate if memory trace is currently turned on
std::vector<int> write_output;

// Create map between routine name and current call count
std::map<std::string,int> num_calls;

int max_calls_to_sample;

// List of all functions that match input string, and will be traced
std::vector<string> all_functions;

// Indicate if thread data has been initialised
std::vector<int> thread_init;
// Indicate if thread has terminated
std::vector<int> thread_end;

int num_threads;
int max_threads;

// if detach_early==True, detach pin when required data has been captured
bool detach_early;

// Do not trace memory, just list routines that can be traced
bool list_routines;

// For each thread, create copy of map between routine name and current call count
std::vector<std::map<std::string,int> > t_num_calls;

PIN_LOCK lock1;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobLogFile(KNOB_MODE_WRITEONCE, "pintool",
    "log", "memory.log", "specify trace file name");
	
KNOB<string> KnobMemoryFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "memory.out", "specify memory file name");
	
KNOB<string> KnobFunctionName(KNOB_MODE_WRITEONCE, "pintool",
    "match", "", "specify function name");
	
KNOB<int> KnobMaxCalls(KNOB_MODE_WRITEONCE, "pintool",
    "n", "1", "number of calls to sample");
	
KNOB<int> KnobMaxThreads(KNOB_MODE_WRITEONCE, "pintool",
    "max_threads", "64", "maximum number of threads");
	
KNOB<bool> KnobDetachEarly(KNOB_MODE_WRITEONCE, "pintool",
    "detach", "true", "detach PIN tool after sampling");
	
KNOB<bool> KnobListRoutines(KNOB_MODE_WRITEONCE, "pintool",
    "list", "false", "do not trace, just list traceable routines");


/* ===================================================================== */


/* ===================================================================== */
/* Analysis routines                                                     */
/* ===================================================================== */

// Called on entry to a matched routine - turn trace on for this thread
VOID matchBefore(UINT32 function_id, THREADID tid)
{
    std::string routine_name = all_functions[function_id];
    if (thread_init[tid] < 0)
    {
	PIN_GetLock(&lock1, tid+1);
	thread_init[tid] = 1;
	thread_end[tid] = all_functions.size();
	t_num_calls[tid].insert(num_calls.begin(), num_calls.end());
	PIN_ReleaseLock(&lock1);
    }
    if (t_num_calls[tid].find(routine_name) == t_num_calls[tid].end())
    {
	LogFile << "not found: " << tid << " : " << routine_name << " : " << t_num_calls[tid].size() << "\n";
    }
    else
    {
	t_num_calls[tid][routine_name]++;
	if ( t_num_calls[tid][routine_name] <= max_calls_to_sample )
	{
	    write_output[tid] = 1;
	    PIN_GetLock(&lock1, tid+1);
	    MemoryFile << "TID" << tid << " entering " << routine_name << ":" << t_num_calls[tid][routine_name] << "\n"; 
	    LogFile << "TID" << tid << " entering " << routine_name << ":" << t_num_calls[tid][routine_name] << "\n"; 
	    PIN_ReleaseLock(&lock1);
	}
	else
	{
	    PIN_GetLock(&lock1, tid+1);
	    thread_end[tid] -= 1;
	    PIN_ReleaseLock(&lock1);
	}
    }
}

// Called on exit from a matched routine - turn trace off for this thread
VOID matchAfter(UINT32 function_id, THREADID tid)
{
    if (write_output[tid] == 1)
    {
	write_output[tid] = 0;
	std::string routine_name = all_functions[function_id];
	if (t_num_calls[tid][routine_name] <= max_calls_to_sample)
	{
	    PIN_GetLock(&lock1, tid+1);
	    MemoryFile << "TID" << tid << " leaving " << routine_name << ":" << t_num_calls[tid][routine_name] <<"\n"; 
	    LogFile << "TID" << tid << " leaving " << routine_name << ":" << t_num_calls[tid][routine_name] <<"\n"; 
	    PIN_ReleaseLock(&lock1);
	}
    }
    if( detach_early && *std::max_element(thread_end.begin(), thread_end.end()) <= 0)
    {
	PIN_GetLock(&lock1, tid+1);
	LogFile << "Detaching PIN" << "\n";
	PIN_ReleaseLock(&lock1);
	PIN_Detach();
    }
}

// Print a memory read record, if trace is on for this thread
VOID RecordMemRead(VOID * ip, VOID * addr, THREADID tid)
{
    if ( write_output[tid] == 1 )
    {
	PIN_GetLock(&lock1, 0);
	MemoryFile << "TID" << tid << " INS" << ip << " R" << addr << "\n";
	PIN_ReleaseLock(&lock1);
    }
}

// Print a memory write record, if trace is on for this thread
VOID RecordMemWrite(VOID * ip, VOID * addr, THREADID tid)
{
    if ( write_output[tid] == 1 )
    {
	PIN_GetLock(&lock1, 0);
	MemoryFile << "TID" << tid << " INS" << ip << " W" << addr << "\n";
	PIN_ReleaseLock(&lock1);
    }
}

VOID ByeWorld(VOID *v)
{
    LogFile << "memory_trace: detached PIN" << "\n";
}

// Check thread count against max_threads
VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    LogFile << "memory_trace: thread " << tid << " : " << num_calls.size() << " started\n";
    num_threads++;
    if (num_threads > max_threads)
    {
	LogFile << "memory_trace: too many threads" << "\n";
	PIN_Detach();
    }		
}

VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    LogFile << "memory_trace: thread " << tid << " finished\n";
}


/* ===================================================================== */
/* Instrumentation routines                                              */
/* ===================================================================== */

// Add instrumentation for read and write instructions - added to all 
// instructions, in all routines
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
	if (INS_MemoryOperandIsRead(ins, memOp))
	{
	    INS_InsertPredicatedCall(
		ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
		IARG_INST_PTR,
		IARG_MEMORYOP_EA, memOp, IARG_THREAD_ID,
		IARG_END);
	}
	// Note that in some architectures a single memory operand can be 
	// both read and written (for instance incl (%eax) on IA-32)
	// In that case we instrument it once for read and once for write.
	if (INS_MemoryOperandIsWritten(ins, memOp))
	{
	    INS_InsertPredicatedCall(
		ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
		IARG_INST_PTR,
		IARG_MEMORYOP_EA, memOp, IARG_THREAD_ID,
		IARG_END);
	}
    }
}  

// Scan image for matched routines, and add matchBefore/matchAfter calls on entry/exit   
VOID Image(IMG img, VOID *v)
{
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
	    std::string routine_name = PIN_UndecorateSymbolName(RTN_Name(rtn), UNDECORATION_NAME_ONLY);
	    if ( print_all || routine_name.find(function_name) != std::string::npos)
	    {
		LogFile << "matched function: " << routine_name.c_str() << "\n";
		num_calls[routine_name] = 0;
		all_functions.push_back(routine_name);
		if (!list_routines)
		{
		    RTN_Open(rtn);
		    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)matchBefore, IARG_UINT32, all_functions.size()-1, IARG_THREAD_ID, IARG_END);
		    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)matchAfter, IARG_UINT32, all_functions.size()-1, IARG_THREAD_ID, IARG_END);
		    RTN_Close(rtn);
		}
	    }
        }
    }
}

/* ===================================================================== */

VOID Fini(INT32 code, VOID *v)
{
    LogFile.close();
    MemoryFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */
   
INT32 Usage()
{
    cerr << "This tool produces a memory trace." << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    // Initialize pin & symbol manager
    PIN_InitSymbols();
    num_threads = 0;
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    PIN_InitLock(&lock1);
    max_calls_to_sample = KnobMaxCalls.Value();
    max_threads = KnobMaxThreads.Value();
    detach_early = KnobDetachEarly.Value();
    list_routines = KnobListRoutines.Value();
    for (int i = 0; i < max_threads; ++i)
    {
	t_num_calls.push_back(std::map<std::string,int> ());
	write_output.push_back(0);
	thread_init.push_back(-1);
	thread_end.push_back(-1);
    }

    // Write to a file since cout and cerr maybe closed by the application
    LogFile.open(KnobLogFile.Value().c_str());
    LogFile << hex;
    LogFile.setf(ios::showbase);
	
    MemoryFile.open(KnobMemoryFile.Value().c_str());
    MemoryFile << hex;
    MemoryFile.setf(ios::showbase);
	
    function_name = KnobFunctionName.Value().c_str();
    print_all = function_name.compare("") == 0;
    LogFile << "max calls: " << max_calls_to_sample << "\n";
    LogFile << "print all: " << print_all << "\n";
    LogFile << "match: " << function_name << " " << "" << " " << function_name.compare("") << "\n"; 
    
    // Register Image to be called to instrument functions.
    IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_AddDetachFunction(ByeWorld, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
