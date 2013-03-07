OSp2
====

Virtual Memory project for  cs 537 operrating systems

Aaron and Justin (3/4 12 Pm)
                 - Implemented the random page replacement policy
                 - Still need to implement FIFO, 2FIFO, and Custom
                 - Error when larger physical space than virtual, ie ./virtmem 10 100 rand scan
                 
                 
Aaron and Justin (3/4 11 Pm)
                 -Implemented FIFO
                 -FIxed the error when physical memory is larger than virtual
                 -Started implementing 2fif0, need to handle the complex cases still

Aaron (3/6 7:45 pm)
                 -Fixed 2FIFO
                 -Fixed error with setting pages
                 
                 -Now have error with 2FIFO for large numbers (ex. 100 and 100) in which the page_table is trying to get a
                 -1 page entry
