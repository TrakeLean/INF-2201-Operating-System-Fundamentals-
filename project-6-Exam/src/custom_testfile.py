# This pre-code test file has been corrected by nlo014@uit.no spring semester 2022. 
# Improved readability of tests to predict what should happen
# File has been updated to run at python 3.8.10 or newer

# Difference from old 2.7.17 to 3.8.10 is that all strings have to
# be passed into subprocesses with a bit converter.
# subprocess.communicate also does not support [0] as argument anymore

import os, subprocess

# old imports that are not needed to run the test
# import sys, string, time

# Legacy variables that will simply be left as false
fsck_implemented = False
flush_implemented = False

# INSERT NAME OF SIMULATION EXECUTABLE HERE
executable = 'p6sh'

# legacy function that does not do anything in our case besides doing ls
def do_fsck() :
    if (fsck_implemented==True) :
        p.stdin.write(b'fsck\n')
        p.stdin.write(b'ls\n')
    else :
        p.stdin.write(b'ls\n')
    p.stdin.flush()

# seems like a legacy function that no longer has much of a use by default
# it seems to verify that a given input command will generate a given output
def check(input, output) :
    p.stdin.write(b'%s\n' %(input))
    p.stdin.flush()
    p.stdout.flush()
    out = p.stdout.read()
    print(out)
    if out != output :
        print ("File content does not match, command", input)
    else:
        print ("ok\n")

# exits the file system        
def do_exit():
    p.stdin.write(b'exit\n')
    return p.communicate()

# test1: uses most of the commands,
#        should not generate any errors
def test1():
    print('THIS IS A FIXED VERSION OF THE PRE-CODE TEST FILE\nCORRECTIONS AND MODIFICATIONS HAVE BEEN DONE BY nlo014@uit.no SPRING 2022 SEMESTER\n')
    print ("----------Starting Test1----------\n")
    print("testing that most commands work\n")
    do_fsck()
    p.stdin.write(b'cat foo\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'stat foo\n')
    p.stdin.write(b'mkdir d1\n')
    p.stdin.write(b'stat d1\n')
    p.stdin.write(b'cd d1\n')
    p.stdin.write(b'ls\n')
    p.stdin.write(b'cat foo\n')
    p.stdin.write(b'ABCD\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'ln foo bar\n')
    do_fsck()
    p.stdin.write(b'ls\n')
    p.stdin.write(b'cat bar\n')
    p.stdin.write(b'.\n')
    do_exit()
    print("\n\nno errors should be generated\n")
    print ("\n----------Test1 Finished----------\n")

# test2: open/write/close,
#        should not give any errors
def test2() :
    print ("----------Starting Test2----------\n")
    print("testing open/write/close\n")
    do_fsck()
    p.stdin.write(b'cat testf\n')
    p.stdin.write(b'WELL\n')
    p.stdin.write(b'DONE\n')
    p.stdin.write(b'.\n')
    do_exit()
    print("\n\nno errors should be generated\n")
    print ("\n----------Test2 Finished----------\n")

# test3: open with readonly test,
#        should not give any errors
def test3():
    print ("----------Starting Test3----------\n")
    print("testing open with readonly\n")
    do_fsck()
    p.stdin.write(b'more testf\n')
    do_exit()
    print("\n\nno errors should be generated\n")
    print ("\n----------Test3 Finished----------\n")

# test4: rm tests,
#       should only give error on cat command
def test4():
    print ("----------Starting Test4----------\n")
    print("testing all remove commands\n")
    do_fsck()
    p.stdin.write(b'cd d1\n')
    p.stdin.write(b'rm foo\n')
    p.stdin.write(b'ls\n')
    p.stdin.write(b'more bar\n')  # ABCD
    p.stdin.write(b'rm bar\n')
    p.stdin.write(b'ls\n')
    p.stdin.write(b'more bar\n') # cat should fail
    do_exit()
    print("\n\nshould not be able to open bar\n")
    print ("\n----------Test4 Finished----------\n")


# test5: simple error cases,
#        all commands should get an error
def test5() :
    print ("----------Starting Test5----------\n")
    print("generating errors with rmdir, cd, ln and rm\n")
    do_fsck()
    p.stdin.write(b'rmdir non_exitsting\n') # Problem with removing directory
    p.stdin.write(b'cd non_exitsing\n') # Problem with changing directory
    p.stdin.write(b'ln non_existing non_existing_too\n') # Problem with ln
    p.stdin.write(b'rm non_existing\n') # Problem with rm
    do_exit()
    print("\n\ntested removing invalid directory\n")
    print("tested changng to invalid directory\n")
    print("tested linking to invalid directory\n")
    print("tested removing invalid hardlink\n")
    print ("\n----------Test5 Finished----------\n")

# test 6: create lots of files in one dir, 
#         should not generate any errors,
#         requires implementation of directories using multiple blocks for dirents
#         requires the use of multiple blocks to hold disk inodes
def test6():
    print ("----------Starting Test6----------\n")
    print("stress testing by generating many files in same directory\n")
    p.stdin.write(b'mkdir d00\n')
    p.stdin.write(b'cd d00\n')
    # num_files by default is 45
    num_files = 45
    for i in range(1, num_files):
        p.stdin.write(b'cat f%d\n' % i)
        p.stdin.write(b'ABCDE\n')
        p.stdin.write(b'.\n')
    p.stdin.write(b'stat f3\n')  # 6
    p.stdin.write(b'more f1\n')  # ABCDE
    p.stdin.write(b'more f33\n')   # ABCDE
    do_exit()
    print("it should print ABCDE twice\n")
    print ("\n----------Test6 Finished----------\n")


# test 7: create a lot of directories,
        # requires expansion of the shell_sim's path buffer
def test7() :
    print ("----------Starting Test7----------\n")
    print("stress testing directory depth on file system\n")
    p.stdin.write(b'ls\n')
    p.stdin.write(b'mkdir d2\n')
    p.stdin.write(b'cd d2\n')
    # p.stdin.write(b'ls\n')
    # idx is by default 50
    idx = 50
    for i in range (1, idx) :
        p.stdin.write(b'mkdir d%d\n' %i)
        p.stdin.write(b'stat .\n')   #DIRECTORY
        p.stdin.write(b'cd d%d\n' %i)
    # by default steps_back is 4
    steps_back = idx
    for j in range (1, steps_back) :
        p.stdin.write(b'cd ..\n')
    p.stdin.write(b'stat .\n') # DIRECTORY
    # p.stdin.write(b'ls\n')
    do_exit()
    print('\n\nThere should be no errors\n')
    print ("\n----------Test7 Finished----------\n")
    
# test 8: Test special cases
def test8() :
    print ("----------Starting Test8----------\n")
    print("special cases\n")
    
    # CD recursive + absolute path + mkdir recursive + absolute path
    p.stdin.write(b'cd /\n')
    p.stdin.write(b'mkdir a/b/c/d/e/f/g/h/i/j/k\n')
    p.stdin.write(b'cd a/b/c/d/e/f/g/h/i/j/k\n')
    p.stdin.write(b'mkdir /1/2/3/4/5/6/7/8/9/10/11\n')
    p.stdin.write(b'cd /1/2/3/4/5/6/7\n')
    p.stdin.write(b'ls\n')
    
    
    # RMDIR recursive + absolute path
    p.stdin.write(b'cd /\n')
    p.stdin.write(b'ls\n')
    p.stdin.write(b'rmdir a/b/c/d/e/f/g/h/i/j/k\n')
    p.stdin.write(b'mkdir /1/2/3/4/5/6/7/8/9/10/11\n')
    p.stdin.write(b'ls\n')
    
    # CAT test if we're able to write and read more than 1 block
    p.stdin.write(b'cat huge_file\n')
    # 693 bytes
    p.stdin.write(b'Ready for the operating system exam? It was every students worst nightmare, worse than a blue screen of death! They sat there with sweaty hands, trying to decipher cryptic questions that flew over their heads like confusing pop-ups. Process management, memory handling, file systems - it felt like navigating through a labyrinth full of bugs. But the students didnt give up! They wrestled through code examples, racing against time as if it were a real startup race. Finally, the exam was over, and they left the room, either with sparkling screens in their eyes or ready to press "Ctrl+Z" on the whole experience. Regardless of the outcome, they had at least managed to avoid a system crash!\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'stat huge_file\n')
    p.stdin.write(b'more huge_file\n')
    
    # LN test if we're able to create hardlinks and remove a file
    p.stdin.write(b'ln huge_file huge_file_copy\n')
    p.stdin.write(b'ls\n')
    
    # Remove the original file
    p.stdin.write(b'rm huge_file\n')
    
    # Check if the hardlink still exists
    p.stdin.write(b'stat huge_file_copy\n')
    p.stdin.write(b'more huge_file_copy\n')
    
    # Build upon the file with more text
    p.stdin.write(b'cat huge_file_copy\n')
    p.stdin.write(b'MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE\n')
    p.stdin.write(b'MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE\n')
    p.stdin.write(b'MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE\n')
    p.stdin.write(b'MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE\n')
    p.stdin.write(b'MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE\n')
    p.stdin.write(b'MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE TEXT MORE\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'more huge_file_copy\n')
    
    
    

    do_exit()
    
    print('\n\nThere should be no errors\n')
    print ("\n----------Test8 Finished----------\n")
    
def test9():
    print ("----------Starting Test9----------\n")
    # Cat directory
    p.stdin.write(b'mkdir existing\n')
    p.stdin.write(b'cat existing\n')
    
    # CD to file and non existing
    p.stdin.write(b'cd huge_file_copy\n')
    p.stdin.write(b'cd non_existing\n')
    
    # rmdir file and non existing
    p.stdin.write(b'rmdir huge_file_copy\n')
    p.stdin.write(b'rmdir non_existing\n')
    
    # rmdir where the directory has a file
    p.stdin.write(b'mkdir i_hold_a_file\n')
    p.stdin.write(b'cd i_hold_a_file\n')
    p.stdin.write(b'cat file\n')
    p.stdin.write(b'hello there\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'cd ..\n')
    p.stdin.write(b'rmdir i_hold_a_file\n')
    p.stdin.write(b'cd i_hold_a_file\n')
    p.stdin.write(b'ls\n')
    p.stdin.write(b'stat file\n')
    p.stdin.write(b'more file\n')
    p.stdin.write(b'cd ..\n')
    
    
    
    # rm directory and non existing
    p.stdin.write(b'rm existing\n')
    p.stdin.write(b'rm non_existing\n')
    
    # ln directory and non existing
    p.stdin.write(b'ln existing existing_copy\n')
    p.stdin.write(b'ln non_existing non_existing\n')
    
    # More directory and non existing
    p.stdin.write(b'more existing\n')
    p.stdin.write(b'more non_existing\n')
    
    # Stat existing
    p.stdin.write(b'stat non_existing\n')
    
    # Mkdir dir that already exists, name too long, invalid name
    p.stdin.write(b'mkdir existing\n')
    p.stdin.write(b'mkdir AAAAAAAAAAAAAAAAAAAA\n') # 20bytes, limit is 14
    p.stdin.write(b'mkdir .\n')
    
    # Write to much to a file 8 * 512 + 1
    p.stdin.write(b'cat fill_me_up\n')
    p.stdin.write(b'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n')
    p.stdin.write(b'.\n')
    p.stdin.write(b'more fill_me_up\n')
    p.stdin.write(b'stat fill_me_up\n')
    
    
    
    
    p.stdin.write(b'ls\n')
    do_exit()

    
    # STAT
    print('\n\nThere should be almost only errors here\n')
    print ("\n----------Test9 Finished----------\n")

# launch a subprocess of the shell
def spawn_lnxsh():
    global p
    p = subprocess.Popen('./%s' %executable, shell=True, stdin=subprocess.PIPE)
 
# moves from test folder to src folder
os.chdir('..')
os.chdir('src')

# cleans object files, and compiles the simulation executable
os.system('make clean; make %s\n' %executable)
os.system('clear')

####### Main Test Program #######
print ("..........Starting..........\n\n")
spawn_lnxsh()
test1()
spawn_lnxsh()
test2()
spawn_lnxsh()
test3()
spawn_lnxsh()
test4()
spawn_lnxsh()
test5()
spawn_lnxsh()
test6()
spawn_lnxsh()
test7()
spawn_lnxsh()
test8()
spawn_lnxsh()
test9()
print ("\nFinished !")

# cleans object files
os.system('make clean\n')
