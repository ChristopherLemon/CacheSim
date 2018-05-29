CacheSim is an example of how to use Intel Pin to instrument a multithreaded code, then performing data analysis on the results.
CacheSim consists of an Intel Pin tool and a Jupyter notebook with DataShader to handle the potentially large amounts of data. 
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
