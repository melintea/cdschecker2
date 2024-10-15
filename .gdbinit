set auto-load safe-path /

handle SIGSEGV nostop noprint

# fairness
#set args -m 2 -f 10 
set args -m 2 -y 

set breakpoint pending on
b user_main
b model_main
b common.cc:45 
b snapshot.cc:111 
b ModelChecker::print_bugs() const
rbreak Thread::Thread*

