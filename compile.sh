gcc -c createUI.c -o createUI.o -I/usr/include/python2.7 -lpython2.7 -fPIC
gcc -shared createUI.o -o createUI.so
gcc play_with_plugin.c -ldl -lasound -lm -lpthread -I/usr/include/python2.7 -lpython2.7 -o play.o
