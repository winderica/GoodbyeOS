#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>

using namespace std;

#define ID 1234
#define BUFFER_NUMBER 256
#define BUFFER_SIZE 100

struct Buffer {
    int next;
    int size;
    bool end;
    char data[BUFFER_SIZE];
};

key_t getKey(int id) {
    key_t key = ftok("/tmp", id);
    if (key == -1) {
        cerr << "Failed to get semaphore key." << endl;
        exit(EXIT_FAILURE);
    }
    return key;
}

class Semaphore {
    int sem;
public:
    Semaphore(int id, int number) {
        // create semaphore in constructor
        sem = semget(getKey(id), number, 0666 | IPC_CREAT);
        if (sem < 0) {
            cerr << "Failed to create semaphore." << endl;
            exit(EXIT_FAILURE);
        }
    }

    ~Semaphore() {
        // destroy semaphore in destructor
        if (semctl(sem, 0, IPC_RMID, 0) < 0) {
            cerr << "Failed to destroy semaphore." << endl;
            exit(EXIT_FAILURE);
        }
    }

    void set(int number, int value) {
        // set value of semaphore
        if (semctl(sem, number, SETVAL, value)) {
            cerr << "Failed to set semaphore value." << endl;
            exit(EXIT_FAILURE);
        }
    }

    void P(unsigned short index) const {
        semop(sem, new sembuf{index, -1, 0}, 1);
    }

    void V(unsigned short index) const {
        semop(sem, new sembuf{index, 1, 0}, 1);
    }
};

template<typename T>
int createSharedMemory(int id) {
    int shm = shmget(getKey(id), sizeof(T), 0666 | IPC_CREAT);
    if (shm < 0) {
        cerr << "Failed to create shared memory." << endl;
        exit(EXIT_FAILURE);
    }
    return shm;
}

template<typename T>
auto getSharedMemory(int shm) {
    return (T *) shmat(shm, nullptr, 0);
}

void destroySharedMemory(int shm) {
    if (shmctl(shm, IPC_RMID, nullptr) < 0) {
        cerr << "Failed to destroy semaphore." << endl;
        exit(EXIT_FAILURE);
    }
}

void readProcess(const Semaphore &s, int shmID, char *filename) {
    auto file = fopen(filename, "rb");
    if (file == nullptr) {
        cerr << "Failed to open file." << endl;
        exit(EXIT_FAILURE);
    }
    while (true) {
        s.P(1);
        s.P(2); // mutex
        auto buffer = getSharedMemory<Buffer>(shmID);
        // `fread` is used here rather than `ifstream`
        auto size = fread(buffer->data, 1, BUFFER_SIZE, file);
        buffer->size = size;
        if (!size) { // EOF
            buffer->end = true;
            fclose(file);
            s.V(2);
            s.V(0);
            return;
        }
        shmID = buffer->next;
        s.V(2);
        s.V(0);
    }
}

void writeProcess(const Semaphore &s, int shmID, char *filename) {
    auto file = fopen(filename, "wb");
    if (file == nullptr) {
        cerr << "Failed to open file." << endl;
        exit(EXIT_FAILURE);
    }
    while (true) {
        s.P(0);
        s.P(3); // mutex
        auto buffer = getSharedMemory<Buffer>(shmID);
        // `fwrite` is used here rather than `ofstream`
        fwrite(buffer->data, buffer->size, 1, file);
        if (buffer->end) {
            fclose(file);
            s.V(3);
            s.V(1);
            return;
        }
        shmID = buffer->next;
        s.V(3);
        s.V(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cout << "Usage: " << argv[0] << " <input_file> <output_file>" << endl;
        exit(EXIT_FAILURE);
    }

    // create semaphore && set values
    Semaphore semaphore(ID, 4);
    semaphore.set(0, 0);
    semaphore.set(1, BUFFER_NUMBER);
    semaphore.set(2, 1);
    semaphore.set(3, 1);

    // create && initialize shared memories
    int shmTemp = createSharedMemory<Buffer>(0);
    int shmID = shmTemp;
    Buffer *buffer;
    for (int i = 1; i < BUFFER_NUMBER; i++) {
        buffer = getSharedMemory<Buffer>(shmTemp);
        shmTemp = createSharedMemory<Buffer>(i);
        buffer->size = 0;
        buffer->next = shmTemp; // pointer to next
        buffer->end = false;
    }
    buffer = getSharedMemory<Buffer>(shmTemp);
    buffer->size = 0;
    buffer->next = shmID; // pointer to head
    buffer->end = false;

    pid_t readPID = fork(); // fork read process
    if (readPID == -1) {
        cout << "Failed to fork read process." << endl;
        exit(EXIT_FAILURE);
    } else if (readPID == 0) { // read process
        readProcess(semaphore, shmID, argv[1]);
        exit(EXIT_SUCCESS);
    } else {
        pid_t writePID = fork(); // fork write process
        if (writePID == -1) {
            cout << "Failed to fork write process." << endl;
            exit(EXIT_FAILURE);
        } else if (writePID == 0) { // write process
            writeProcess(semaphore, shmID, argv[2]);
            exit(EXIT_SUCCESS);
        } else { // main process
            waitpid(readPID, nullptr, 0);
            waitpid(writePID, nullptr, 0);
            do {
                destroySharedMemory(buffer->next); // destroy shared memory
                buffer = getSharedMemory<Buffer>(buffer->next);
            } while (buffer->next != shmID);
            // semaphore is destroyed in its destructor
        }
    }
}