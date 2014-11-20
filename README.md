os1shell
========

os1shell is a simple command shell that will fork and execute commands 
entered by standard input. Programs may be run in the background by 
appending the '&' character to the end of the command or as the final
command parameter.  To quit the shell the user must enter an EOF,
CTRL+D, or send SIGQUIT signal, CTRL+\.
