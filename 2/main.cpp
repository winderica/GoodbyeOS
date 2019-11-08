#include <sys/sem.h>
#include <pthread.h>
#include <iostream>

using namespace std;

#define ID 100

int s;
int sum = 0;

key_t getKey(int id) {
    key_t key = ftok("/tmp", id);
    if (key == -1) {
        cerr << "Failed to get semaphore key." << endl;
        exit(1);
    }
    return key;
}

void P(int semID, unsigned short index) {
    sembuf sem = {index, -1, 0};
    semop(semID, &sem, 1);
}

void V(int semID, unsigned short index) {
    sembuf sem = {index, 1, 0};
    semop(semID, &sem, 1);
}

void *childThread1(void *) {
    for (int i = 1; i <= 100; i++) {
        P(s, 1);
        cout << "Sum from 1 to " << i << ": " << sum << endl;
        V(s, 0);
    }
    return nullptr;
}

void *childThread2(void *) {
    for (int i = 1; i <= 100; i++) {
        P(s, 0);
        sum += i;
        V(s, 1);
    }
    return nullptr;
}

int main() {
    // create semaphore
    s = semget(getKey(ID), 2, 0666 | IPC_CREAT);
    if (s < 0) {
        cerr << "Failed to create semaphore." << endl;
        exit(1);
    }
    // set value
    if (semctl(s, 0, SETVAL, 1) < 0
        || semctl(s, 1, SETVAL, 0) < 0) {
        cerr << "Failed to set semaphore value." << endl;
        exit(1);
    }
    // create child threads
    pthread_t t1, t2;
    if (pthread_create(&t1, nullptr, childThread1, nullptr) != 0
        || pthread_create(&t2, nullptr, childThread2, nullptr) != 0) {
        cerr << "Failed to create child threads." << endl;
        exit(1);
    }
    // wait child threads to return
    if (pthread_join(t1, nullptr) != 0
        || pthread_join(t2, nullptr) != 0) {
        cerr << "Failed to join and wait child threads." << endl;
        exit(1);
    }
    // destroy semaphore
    if (semctl(s, 0, IPC_RMID, 0) < 0) {
        cerr << "Failed to destroy semaphore." << endl;
        exit(1);
    }
}