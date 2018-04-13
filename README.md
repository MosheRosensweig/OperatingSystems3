# OperatingSystems3
Lab3

1] Names
Moshe Rosensweig
Yehuda Gale

2] Files/Dirs
There is one file included the fat32_reader.c file. This contains all the code for the shell program.

3] Instructions for compliling
We compiled as follows "gcc -g -o fat32_reader ./fat32_reader.c"

4] Instructions to run
We ran as follows:
./fat32_reader path_to_dot_img
ex: ./fat32_reader ./fat32.img

5] Any challenges you encountered along the way
- Sometimes we needed forgot to zero out / properly initialize variables.
- Sometimes we needed to mask off extra bits that weren't "clean"

6] Any sources you used to help you write your programs/scripts
We borrowed a few lines of code from the internet. 
	[a] mmap: 			https://www.linuxquestions.org/questions/programming-9/mmap-tutorial-c-c-511265/
	[b] find file size: https://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
Other than that, we used the fat spec, paying attention in class, and wikipedia to know how to write the code.
