// Name(s): Andrew Fuller
// Description: Acts as a bash shell. Repeatedly allows a user to enter input that gets interpreted as a shell command.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// #define MAX_INPUT 1024

typedef struct {
    char **args;
    int arg_count;
    char *input_file;
    char *output_file;
} ShellCommand;

//////// Some function ideas: ////////////
// Note: Some code is reflected in main that represents these functions,
// but it is up to you to determine how you want to organize your code.



/* 
    A function that causes the prompt to 
    display in the terminal
*/
void displayPrompt(void)
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s$ ", cwd);
    }
    else {
        printf("$ ");
    }

    fflush(stdout);
}

/*
    A function that takes input from the user.
    It may return return the input to the calling statement or 
    store it at some memory location using a pointer.
*/ 
char *getInput(void)
{
    char *input = NULL;
    size_t size = 0;

    if (getline(&input, &size, stdin) == -1) {
        free(input);
        return NULL;
    }

    return input;
}


/*
    A function that parses through the user input.
    Consider having this function return a struct that stores vital
    information about the parsed instruction such as:
    - The command itself
    - The arguments that come after the command 
        Hint: When formatting your data, 
        look into execvp and how it takes in args.
    - Information about if a redirect was detected such as >, <, or |
    - Information about whether or not a new file 
        needs to be created and what that filename may be.
    

    Some helpful functions when doing this come from string.h and stdlib.h, such as
    strtok, strcmp, strcpy, calloc, malloc, realloc, free, and more

    Be sure to consider/test for situations when a backslash is used to escape the space char
    and when quotes are used to group together various tokens.
*/

ShellCommand parseInput(char *input)
{
    ShellCommand command = {0};

    size_t length = strlen(input);

    command.args = calloc(length + 2, sizeof(char *));

    if (command.args == NULL)
    {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    char *token = malloc(length + 1);

    if (token == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int token_pos = 0;
    int in_quotes = 0;
    int escaped = 0;
    char redirect = '\0';

    // FIXED: i now increases every iteration.
    for (int i = 0; i <= (int)length; i++)
    {
        char c = input[i];

        if (escaped)
        {
            token[token_pos++] = c;
            escaped = 0;
        }
        else if (c == '\\' && i < (int)length)
        {
            escaped = 1;
        }
        else if (c == '"' && i < (int)length)
        {
            in_quotes = !in_quotes;
        }
        else if (!in_quotes &&
                 (c == ' ' || c == '\t' ||
                  c == '\n' || c == '<' ||
                  c == '>' || c == '\0'))
        {
            if (token_pos > 0)
            {
                token[token_pos] = '\0';

                // NEW: Store redirect filename or argument.
                if (redirect == '<')
                {
                    command.input_file = strdup(token);
                    redirect = '\0';
                }
                else if (redirect == '>')
                {
                    command.output_file = strdup(token);
                    redirect = '\0';
                }
                else
                {
                    command.args[command.arg_count++] =
                        strdup(token);
                }

                token_pos = 0;
            }

            if (c == '<' || c == '>')
            {
                redirect = c;
            }
        }
        else
        {
            token[token_pos++] = c;
        }
    }

    // NEW: Handle a final escaped backslash.
    if (escaped)
        token[token_pos++] = '\\';

    // NEW: Handle any remaining token.
    if (token_pos > 0)
    {
        token[token_pos] = '\0';

        if (redirect == '<')
            command.input_file = strdup(token);
        else if (redirect == '>')
            command.output_file = strdup(token);
        else
            command.args[command.arg_count++] = strdup(token);
    }

    command.args[command.arg_count] = NULL;

    free(token);

    return command;
}


/* Release mem for command. */
void freeCmd(ShellCommand *command)
{
    if ((*command).args != NULL) {
        for (int i = 0; i < command->arg_count; i++) {
            free((*command).args[i]);
        }
        free((*command).args);
    }

    free((*command).input_file);
    free((*command).output_file);

    memset(command, 0, sizeof(*command));
}

/*
    A function that executes the command. 
    This function might take in a struct that represents the shell command.

    Be sure to consider each of the following:
    1. The execvp() function. 
        This can execute commands that already exist, that is, 
        you don't need to recreate the functionality of 
        the commands on your computer, just the shell.
        Keep in mind that execvp takes over the current process.
    2. The fork() function. 
        This can create a process for execvp to take over.
    3. cd is not a command like ls and mkdir. 
        cd is a toold provided by the shell, 
        so you WILL need to recreate the functionality of cd.
    4. Be sure to handle standard output redirect and standard input redirects here 
        That is, there symbols: > and <. 
        Pipe isn't required but could be a nice addition.
*/
void executeCommand(ShellCommand command)
{
    if (command.args == NULL || command.args[0] == NULL){
        return;
    }

    if (strcmp(command.args[0], "exit") == 0){
        exit(0);
    }

    if (strcmp(command.args[0], "cd") == 0){
        char *directory = command.args[1];

        if (directory == NULL){
            directory = getenv("HOME");
        }

        if (directory == NULL || chdir(directory) == -1){
            printf("Error %d (%s)\n", errno, strerror(errno));

            return;
        }
    }

    pid_t pid = fork();

    if (pid < 0){
        printf("Error %d (%s)\n", errno, strerror(errno));
    }

    if (pid == 0){
        if (command.input_file != NULL){
            int fd = open(command.input_file, O_RDONLY);

            if (fd == -1){
                printf("Error %d (%s)\n", errno, strerror(errno));
                _exit(1);
            }

            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (command.output_file != NULL){
            int fd = open(command.output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd == -1){
                printf("Error %d (%s)\n", errno, strerror(errno));
                _exit(1);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(command.args[0], command.args);

        int error = errno;
        fprintf(stderr, "Error %d (%s)\n", error, strerror(error));
        _exit(1);
    }
    else{
        waitpid(pid, NULL, 0);
    }
}


int main(void) // MAIN
{
	char* input;
	ShellCommand command;
		
	// repeatedly prompt the user for input
	for (;;)
	{
        // display the prompt
        displayPrompt();

	    // get the user's input
	    input = getInput();
	    
        if (input == NULL) {
            break;
        }

	    // parse the command line
	    command = parseInput(input);
	    
	    // execute the command
	    executeCommand(command);

        freeCmd(&command);
        free(input);
	}

	exit(0);
}

