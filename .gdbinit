set auto-load safe-path /

handle SIGSEGV nostop noprint
# fairness
set args -m 2 -f 10 

b common.cc:45
b shapshot.cc:110

