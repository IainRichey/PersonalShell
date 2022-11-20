//if you get errors with things like cd & trying to be in backround, its because you only handle & as the backround process sign
//if you are in the non built in process mode

#define _GNU_SOURCE //for asprintf
#include <unistd.h> //for chdir
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> //for file io
#include <string.h>
#include <sys/types.h> //I think for pid stuff
#include <sys/wait.h> //or this one is
#include <signal.h>
sig_atomic_t globalTstpFlag = 0; //ewe icky gross global I hate you do I really need you

struct backPros{ //backround process's
    pid_t pid;
    int status;
}; 

struct input{
    char command[2048];
    char* arguments[512]; //load in everything before the < 
    char inputFile[255]; //check if it is <
    char outputFile[255]; //check if it is > after it is the output file
    int numArgs;
    pid_t backP[500]; //array of 500 backround commands possible
    int numBR;

    int status; //
    int signal;
};

void changeDirectory(char* userInput){ //function that uses chdir to change directory
    chdir(userInput); 

    //char* temp = NULL;
    //size_t bufferSize = 2048;    
    //printf("%s\n", getcwd(temp, bufferSize));     
    //check if it exists. If incorrect path, print message and go back to main
}

void status(struct input user){
    if(user.status != -5){ //ie if we terminated with a status
        printf("exit value %d\n", user.status);
    }
    if(user.signal != -5){ //ie if we terminated with a status
        printf("terminated by signal %d\n", user.signal);
    }
}

void expand(struct input user){ //does this have to be a pointer? I dont think so cuz what I am changing is arrays anyways?

    char* asptr;
    char* tempArr;
    char* token;
    int curPid;
    //just for testing
    //for(int k = 0; k < user.numArgs; k++){
        //printf("arg num %d is %s\n", k, user.arguments[k]); fflush(stdin);
    //}

    for(int i = 0; i < user.numArgs; i++){
        curPid = 0;
        tempArr = NULL;
        //memset(asptr, '\0', sizeof(asptr));
        while(tempArr = strstr(user.arguments[i], "$$")){//get the location of a $$ if it exists
            curPid = getpid();
            tempArr[0] = '\0';
            tempArr[1] = '\0'; //replace the two $ with nulls
            tempArr += 2; 

            asprintf(&asptr, "%s%d%s", user.arguments[i], curPid, tempArr);
            free(user.arguments[i]);
            user.arguments[i] = malloc(sizeof(asptr));
            strcpy(user.arguments[i], asptr);
            free(asptr); //DELETE THIS IF IT BREAKS BEFORE YOU ADDED THIS IT WORKED
        }
    }

    //just for testing
}

//this changes file IO if we need too and is taken almost directly from https://canvas.oregonstate.edu/courses/1890464/pages/exploration-processes-and-i-slash-o?module_item_id=22345189 
void changeIO(struct input user){
    if(user.inputFile[0] != '\0'){
        // Open source file
        int sourceFD = open(user.inputFile, O_RDONLY);
        if (sourceFD == -1) { 
            perror("source open()"); 
            exit(1); 
        }

        // Redirect stdin to source file
        int result = dup2(sourceFD, 0);
        if (result == -1) { 
            perror("source dup2()"); 
            exit(2); 
        }
    }

    if(user.outputFile[0] != '\0'){
        // Open target file
        int targetFD = open(user.outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
        if (targetFD == -1) { 
            perror("target open()"); 
            exit(1); 
        }
    
        // Redirect stdout to target file
        int result = dup2(targetFD, 1);
        if (result == -1) { 
            perror("target dup2()"); 
            exit(2); 
        }        
    }
}

void checkBR(struct input* user){
    int childExitStatus = -5;
    pid_t actualPid;
    for(int i = 0; i < user->numBR; i++){
        actualPid = waitpid(user->backP[i], &childExitStatus, WNOHANG); //will loop through and check each one to see if it ended   
        if(actualPid != 0){ //if it is 0, then the child hasn't exited yet
            if (WIFEXITED(childExitStatus) != 0){ //if we exited normally
                //print that it terminated normally
                int exitStatus = WEXITSTATUS(childExitStatus);
                printf("Process with id %d exited with exit status %d\n", user->backP[i], exitStatus);
            }
            if (WIFSIGNALED(childExitStatus) != 0){ //if we exited by a signal
                //print that it terminated normally
                int termSignal = WTERMSIG(childExitStatus);
                printf("Process with id %d exited with signal %d\n", user->backP[i], termSignal);
            }
            for(int j = i; j < user->numBR; j++){
                user->backP[j] = user->backP[j + 1]; //shift em all left one
            }
            user->numBR--;
        }
    }
}

void forkTime(struct input* user, struct sigaction SIGINT_default, struct sigaction SIGTSTP_ign){
    int backround = 0; //default not backround mode
    pid_t spawnPid = -5; //variable to keep track of parent pid I think
    int childExitStatus = -5;

    if(strcmp(user->arguments[user->numArgs - 1], "&") == 0){ //-1 because I make the last argumenta  null 
        backround = 1; //if the last argument is a &, set the baackround flag
        free(user->arguments[user->numArgs - 1]); 
        user->arguments[user->numArgs - 1] = NULL; //now that we have seen we are in a backround process, put it into the backround
        user->numArgs--; 
        //printf("\nlast argument is %s\n", user.arguments[user.numArgs - 1]); fflush(stdin);
    }

    if(globalTstpFlag == 1){ //if we are in foreground only mode, everything is in forground, but we still want to get rid of &
        backround = 0;
    }

    //I do not know where I make the change so that we waitpid with WNOHANG. Does all of this go in an if statement? I assume I can just put default in it
    spawnPid = fork(); //we have had our first child - rare opportunity for a cs major
    switch (spawnPid){
        case -1: {perror("Something went wrong spawning\n"); exit(1); break;} //error handling spawning a child
        case 0: {

            sigaction(SIGTSTP, &SIGTSTP_ign, NULL); //weither it is for or back round, ignore CNTRL z

            if(backround == 1 && user->inputFile[0] == '\0'){ //if it is backround and we have no io specified, send it to dev null
                strcpy(user->inputFile, "/dev/null"); //maube this is problem
            }
            if(backround == 1 && user->outputFile[0] == '\0'){
                strcpy(user->outputFile, "/dev/null");
            }

            if(backround == 0){ //if we are in the forground, switch sigint to default so we can ^c stuff
                sigaction(SIGINT, &SIGINT_default, NULL);
            }

            changeIO(*user); //check if we have IO file changes, if so change them
            if (execvp(*user->arguments, user->arguments) < 0){ //error handling calling exec, and should run it as well
                perror("Exec Failure");
                exit(1);        
            }
        }

        default: {
            if(backround == 0){ //if foreground process
                pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
                //I think we would set status here
                if (WIFEXITED(childExitStatus) != 0){ //if we exited normally
                    //print that it terminated normally
                    int exitStatus = WEXITSTATUS(childExitStatus);
                    user->status = exitStatus;
                    user->signal = -5;
                }
                if (WIFSIGNALED(childExitStatus) != 0){ //if we exited by a signal
                    //print that it terminated normally
                    int termSignal = WTERMSIG(childExitStatus);
                    user->signal = termSignal;
                    user->status = -5;
                }
            }
            else if(backround == 1){
                printf("background pid is %d\n", spawnPid); fflush(stdin);

                //pid_t actualPid = waitpid(spawnPid, &childExitStatus, WNOHANG); //don't wait for the child process
                user->backP[user->numBR] = spawnPid; 
                user->numBR++; //inc the num of backround commands we have


                //unsure what to do now. I need to have a dynamic array of backPros and add to it here each toime, but for soime reason I can't think of how to do that. 
            }            
        }
    }
}

void catchSIGTSTP(int signo){ //function that allows us to catch and handle CNTRL Z
    if(globalTstpFlag == 0){ //IE we are in normal mode, flip it
        char* message = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 49);
        globalTstpFlag = 1;
    }

    else if(globalTstpFlag == 1){ //IE we are in normal mode, flip it
        char* message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 29);
        globalTstpFlag = 0; //switch it back
    }
}

int main(){
    //get this in a while loop when everything works
    int i = 0;
    int builtin = 0; //so if we enter a built in, we dont also enter a fork
   //char userInput[2048]; //store user input here
    int argCount = 0;
    int bufferSize = 2048;
    int numCharEntered = 0; 
    struct input user;
    struct backPros* Back;
    char* userptr = NULL;
    user.status = 0;
    user.numBR = 0;
    user.numArgs = 0;

    //start signal handling CNTR-C
    struct sigaction SIGINT_default = {0};
	SIGINT_default.sa_handler = SIG_DFL;
    struct sigaction SIGINT_ign = {0};
    SIGINT_ign.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_ign, NULL); //should ignore sigint
    //end signal handling CNTR-C

    //start signal handling CNTR-z
    struct sigaction SIGTSTP_ign = {0}; //set this up so in child we can ignore CNTR-z
    SIGTSTP_ign.sa_handler = SIG_IGN;

    struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = catchSIGTSTP; //can we do this I want a flag in here   
	sigfillset(&SIGTSTP_action.sa_mask); 
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL); //do this so that it is active in main
    //start signal handling CNTR-z

    //start while loop here I think 
    int cont = 1;
    while(cont == 1){ //since we want to stop it with exit only

        checkBR(&user); //check backround processes 

        //user.inputLoc = -5;  //reset the position check if they are neg five if they are no io needed
        //user.outputLoc = -5;
        memset(user.inputFile, '\0', sizeof(user.inputFile));   //clear out the two files in case they want to use them
        memset(user.outputFile, '\0', sizeof(user.outputFile)); //possible attach a flag if you can't figure out giving it a way to check 
        builtin = 0;
        argCount = 0;
        memset(user.command, '\0', sizeof(user.command)); //clear it out before using it again

        //start of ignoring # and nothing entered
        do{    
            argCount = 0;
            printf(": "); fflush(stdout);
            memset(user.command, '\0', sizeof(user.command));
            userptr = fgets(user.command, bufferSize, stdin);
            if(user.command[strlen(user.command) - 1] == '\n'){ //remove the last icky newline
                user.command[strlen(user.command) - 1] = '\0';
            }
        } while(user.command[0] == '\0' ||  user.command[0] == '\n' || user.command[0] == '#');
        //end of ignoring # and nothing entered

        //start of handling user input
        char* saveptr;
        char* tempArr; //so we can find $$
        char* tempTok = NULL;
        int curPid = 0;
        char* asptr;
        char* token = strtok_r(user.command, " ", &saveptr);

        while(token != NULL && (strcmp(token, "<") != 0) && (strcmp(token, ">") != 0)){ // so we go until we hit a <

            user.arguments[argCount] = malloc(sizeof(token));
            strcpy(user.arguments[argCount], token); //should copy token into our array of command
            //printf("argument at %d is %s \n", argCount, user.arguments[argCount]); fflush(stdin);
            argCount++;
            //memset(token, '\0', sizeof(token)); //if breaks, remove this
            token = strtok_r(NULL, " ", &saveptr);
        }
        user.numArgs = argCount; //save the num args for convenient use later
        expand(user); //expand any $$'s

        //check if there is input or output files //doesn't quite work rn
        while(token != NULL){ //could also just save where the index of them are, but then you cant pass the whole arg array into a function, cuz it'll get mad

            if((strcmp(token, "<") == 0)){ //if he next argument is a input file
                token = strtok_r(NULL, " ", &saveptr); //move to the input file //MIGHT BREAK THINGS IF WE HAVE TO ERROR HANDLE INPUTS

                strcpy(user.inputFile, token);                //make sure you check if inputfile is null, if so dont set it to new

                token = strtok_r(NULL, " ", &saveptr); //check if anything comes afterwards 
            }

            if(token != NULL && (strcmp(token, ">") == 0)){ //if he next argument is a output file added not null in case first case makes it null
                token = strtok_r(NULL, " ", &saveptr); //move to the input file //MIGHT BREAK THINGS IF WE HAVE TO ERROR HANDLE INPUTS

                strcpy(user.outputFile, token);                //make sure you check if inputfile is null, if so dont set it to new

                token = strtok_r(NULL, " ", &saveptr); //check if anything comes afterwards 
            }
        }
        //printf("input file is %s output file is %s\n", user.inputFile, user.outputFile); fflush(stdin);
        //end of of handling user input

        //start of handling built in "exit" command
        if(strcmp(user.arguments[0], "exit") == 0){ 
            for(i = 0; i < argCount; i++){
                free(user.arguments[i]);
            }
            return 0; // cuz if we enter exit then stop
        }
        //end of handling built in "exit" command

        //start of handling built in "cd" command
        if(strcmp(user.arguments[0], "cd") == 0){ 
            builtin = 1; //so we dont go into the fork process later
            if(argCount != 1){ //if we entered more than jsut cd
                //printf("\narg 1 is %s\n", user.arguments[1]); fflush(stdin); //testing
                changeDirectory(user.arguments[1]);
            }   
            else{
                chdir(getenv("HOME")); //go to our home directory 
                    //char* temp = NULL;
                    //size_t bufferSize = 2048;    
                    //printf("%s\n", getcwd(temp, bufferSize));  
            }       
        }
        //end of handling built in "cd" command

        //start of status handling
        if(strcmp(user.arguments[0], "status") == 0){ 
            builtin = 1; //so we dont go into the fork process later
            status(user);
        }
        //end of status handling


        //start of process forking and exec'ing
        if(builtin == 0){ 
            user.arguments[argCount] = malloc(sizeof(token));
            user.arguments[argCount] = NULL; //IF ERRORS ACCURE, MESS WITH THIS
            forkTime(&user, SIGINT_default, SIGTSTP_ign);                      //make a null ptr at the end of our array, so that when we exec it it works
        }
        //end of process forking and exec'ing

        for(i = 0; i < argCount; i++){
            free(user.arguments[i]);
        }

    }
    return 0;
}
