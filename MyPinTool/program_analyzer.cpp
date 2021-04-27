
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include <pin.H>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unordered_map>
#include <algorithm>
#include <numeric>
using std::cerr;
using std::string;
using std::endl;
using std::vector;
using std::unordered_map;
using std::max_element;
using std::accumulate;
using std::make_pair;

/* ================================================================== */
// Global variables 
/* ================================================================== */

UINT64 insCount = 0;        //number of dynamically executed instructions
UINT64 memTraceCount = 0;
UINT64 wss_count = 0;
vector <UINT64> wss_vec;
vector <UINT64> cwss_vec;
UINT64 wss_peak = 0;
UINT64 cwss_peak = 0;
UINT64 wss_acc = 0;
UINT64 cwss_acc = 0;

//UINT64 threadCount = 0;     //total number of threads, including main thread

std::ostream * out = &cerr;
unordered_map<void *, UINT8> wss_map;
unordered_map<void *, UINT8> wss_last_map;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool",
    "o", "tool.out", "specify file name for MyPinTool output");

KNOB<BOOL>   KnobCount(KNOB_MODE_WRITEONCE,  "pintool",
    "count", "1", "count instructions, basic blocks and threads in the application");


/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the number of dynamically executed " << endl <<
            "instructions, basic blocks and threads in the application." << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */


bool first = true;

VOID wss(void * r1_addr, void* r2_addr, void * w_addr){        
    if(r1_addr != 0){
        memTraceCount++;
        void * r1_c_addr = (void*)((uintptr_t)(r1_addr)& 0xFFFFFFFFFFFFFFC0);
        if(wss_map.find(r1_c_addr)==wss_map.end()){
            wss_map.insert(make_pair(r1_c_addr,1));
        }
    }
    if(r2_addr != 0){
        memTraceCount++;
        void * r2_c_addr = (void*)((uintptr_t)(r2_addr)& 0xFFFFFFFFFFFFFFC0);
        if(wss_map.find(r2_c_addr)==wss_map.end()){
            wss_map.insert(make_pair(r2_c_addr,1));
        }
    }
    if(w_addr != 0){
        memTraceCount++;
        void * w_c_addr = (void*)((uintptr_t)(w_addr)& 0xFFFFFFFFFFFFFFC0);
        if(wss_map.find(w_c_addr)==wss_map.end()){
            wss_map.insert(make_pair(w_c_addr,1));
        }
    }
    if(memTraceCount%10000000 == 0){
        wss_count = wss_map.size();
        wss_vec.push_back(wss_count*64);
        wss_peak = (wss_count*64>wss_peak) ? wss_count*64:wss_peak; 
        wss_acc += wss_count*64;
        UINT64 cwss_count = 0;
        if(!first){
            for(const auto& pair_elem: wss_map){
                if(wss_last_map.find(pair_elem.first)!=wss_last_map.end()){
                    cwss_count++;
                }
            }
            cwss_vec.push_back(cwss_count*64);
            cwss_peak = (cwss_count*64>cwss_peak) ? cwss_count*64:cwss_peak;
            cwss_acc += cwss_count*64; 
        }
        first = false;
        wss_last_map = wss_map;
        wss_map.clear();
    }
} 

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */


VOID Instruction(INS ins, VOID *v){

    if(INS_IsMemoryRead(ins) && INS_IsMemoryWrite(ins)){
        if(!INS_HasMemoryRead2(ins)){
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)wss, IARG_MEMORYREAD_EA, IARG_PTR, 0, IARG_MEMORYWRITE_EA,IARG_END);
        }
        else{
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)wss, IARG_MEMORYREAD_EA, IARG_MEMORYREAD2_EA, IARG_MEMORYWRITE_EA,IARG_END);
        }
    }
    else if(INS_IsMemoryRead(ins)){
        if(!INS_HasMemoryRead2(ins)){
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)wss, IARG_MEMORYREAD_EA, IARG_PTR, 0, IARG_PTR, 0,IARG_END);
        }
        else{
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)wss, IARG_MEMORYREAD_EA, IARG_MEMORYREAD2_EA, IARG_PTR, 0,IARG_END);
        }
    }
    else if(INS_IsMemoryWrite(ins)){
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)wss, IARG_PTR, 0, IARG_PTR, 0, IARG_MEMORYWRITE_EA,IARG_END);
    }
}


/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v)
{
    double wss_avg = (wss_vec.size() > 0) ? (double)(wss_acc)/wss_vec.size() : 0;
    double cwss_avg = (cwss_vec.size() > 0) ? (double)(cwss_acc)/cwss_vec.size() : 0;
    *out << "peak wss: " << wss_peak << endl;
    *out << "wss avg: " << wss_avg << endl;
    *out << "peak cwss: " << cwss_peak << endl;
    *out << "cwss avg: " << cwss_avg << endl;
    *out << "mem trace count: " << memTraceCount << endl;
    for(UINT32 i = 0; i<wss_vec.size(); i++){
        *out << wss_vec[i] << endl;
    }

}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid 
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
    string fileName = KnobOutputFile.Value();

    if (!fileName.empty()) { out = new std::ofstream(fileName.c_str());}

    if (KnobCount)
    {
        INS_AddInstrumentFunction(Instruction,0);
        // Register function to be called when the application exits
        PIN_AddFiniFunction(Fini, 0);
    }
    
    cerr <<  "===============================================" << endl;
    cerr <<  "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty()) 
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr <<  "===============================================" << endl;
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
