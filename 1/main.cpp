#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>

using namespace std;

int pipeFD[2];
pid_t child1PID;
pid_t child2PID;

void parentInt(int sigNum) {
    kill(child1PID, SIGUSR1);
    kill(child2PID, SIGUSR2);
}

void child1Exit(int sigNum) {
    close(pipeFD[1]); // close write end
    cout << "Child Process 1 is Killed by Parent!" << endl;
    _exit(EXIT_SUCCESS);
}

void child2Exit(int sigNum) {
    close(pipeFD[0]); // close read end
    cout << "Child Process 2 is Killed by Parent!" << endl;
    _exit(EXIT_SUCCESS);
}

int main() {
    if (pipe(pipeFD) == -1) { // failed to create pipe
        cout << "Failed to create pipe." << endl;
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, parentInt);
    child1PID = fork();
    if (child1PID == -1) { // failed to fork child 1
        cout << "Failed to fork child 1." << endl;
        exit(EXIT_FAILURE);
    } else if (child1PID == 0) { // child 1 process
        signal(SIGINT, SIG_IGN);
        signal(SIGUSR1, child1Exit);
        close(pipeFD[0]); // close read end
        int x = 1;
        while (true) {
            string message = "I send you " + to_string(x++) + " times.\n";
            write(pipeFD[1], message.c_str(), message.length());
            sleep(1);
        }
    } else {
        child2PID = fork();
        if (child2PID == -1) { // failed to fork child 2
            cout << "Failed to fork child 2." << endl;
            exit(EXIT_FAILURE);
        } else if (child2PID == 0) { // child 2 process
            signal(SIGINT, SIG_IGN);
            signal(SIGUSR2, child2Exit);
            close(pipeFD[1]); // close write end
            char buf;
            while (true) {
                read(pipeFD[0], &buf, 1);
                write(STDOUT_FILENO, &buf, 1);
            }
        } else { // parent process
            close(pipeFD[0]); // close read end
            close(pipeFD[1]); // close write end
            waitpid(child1PID, nullptr, 0);
            waitpid(child2PID, nullptr, 0);
            cout << "Parent Process is Killed!" << endl;
            exit(EXIT_SUCCESS);
        }
    }
}