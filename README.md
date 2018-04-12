# KFIFO-Queue

**note: this is a work in progress, but the core implementation is complete**

Hi there! So this is an implementation of a k-FIFO queue from this paper: [Fast and Scalable, Lock-free k-FIFO Queues](https://pdfs.semanticscholar.org/d585/11fa208b071b82a85099c87d48a47d861818.pdf). 

To run the program, you'll need to use C++11. On Mac/Linux systems just run this in the terminal:
```shell
$ git clone https://github.com/farzaa/KFIFO-Queue.git
$ cd KFIFO-Queue
$ g++ -std=c++11 kfifo.cpp && ./a.out
```
