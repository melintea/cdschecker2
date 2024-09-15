set auto-load safe-path /

handle SIGSEGV nostop noprint

b common.cc:45
b shapshot.cc:110

