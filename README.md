CacheSim is an example of how to use Intel Pin to instrument a multithreaded code, to produce data for further analysis.
CacheSim consists of an Intel Pin tool and a Jupyter notebook, with DataShader to handle potentially large amounts of data. 
The Pin tool is used to instrument the code, to produce a memory trace of routines matching a user supplied string, for a 
specified number of calls, and for each thread. The Jupyter notebook can then be used to perform multithreaded cache simulations on the 
data, and to visualize the cache levels accessed by each thread.

<p>
<img src="https://github.com/ChristopherLemon/CacheSim/blob/master/Wiki/CacheTotals.jpg" width="45%" title="Cache Hits"">
<img src="https://github.com/ChristopherLemon/CacheSim/blob/master/Wiki/Threads.jpg" width="45%" title="Read/Writes per Thread">
</p>
<p>
<img src="https://github.com/ChristopherLemon/CacheSim/blob/master/Wiki/CacheLevels.jpg" width="45%" title="Read/Writes per Cache Level">
<img src="https://github.com/ChristopherLemon/CacheSim/blob/master/Wiki/CacheLevelsZoom.png" width="45%" title="Read/Writes per Cache Level">
</p>
                                                                                                                                            
# Setup  
Requires [Anaconda](https://conda.io/docs/user-guide/install/download.html) and [Intel Pin](https://software.intel.com/en-us/articles/pin-a-dynamic-binary-instrumentation-tool).  

Download Intel Pin. Then cd into the cache_sim directory and set the environment variable PIN_ROOT to the pin root directory (containing the pin executable) 

    PIN_ROOT=/path/to/pin_root_directory export PIN_ROOT
  
Run make for memory_trace tool, targeting the desired architecture, i.e. for intel64:

    make obj-intel64/memory_trace.so TARGET=intel64

Then use pin to run the pin tool memory_trace, with the executable and its arguments. For example:

    $PIN_ROOT/pin -t /path/to/cache_sim/obj-intel64/memory_trace.so -o memory.txt -match routine_name_string -- /path/to/executable/exe exe_args

This will search the executable image for routine names containing the substring routine_name_string, and output the memory trace to the file memory.txt

Next, copy the memory.txt file to the cache_sim/MEMORY_DUMPS directory and create the Conda environment for the Jupyter Notebook

    conda env create -f environment.yml
    
This may take a while. Once finished launch the Jupyter

    jupyter notebook
    
Select the CacheSim notebook. The first code block contains the list of memory traces to be analysed

    file_list = ["MEMORY_DUMPS/memory.txt"]
    
Run all of the code blocks to produce the cache simulation and visualization
