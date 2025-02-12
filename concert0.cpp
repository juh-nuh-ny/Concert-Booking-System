#include <iostream>
#include <pthread.h>
#include <queue>
#include <fstream>
#include <cstdlib>
#include <string>
#define MAX_BUFFER_SIZE 5  //for the fixed size queue
using namespace std;


//global variables
int totalTickets; //mutex-ed later
int numAgents = 5;
struct customer {
    string name;//from input file
    int tickets;//from input file
    int sequence; // for printing order, updated in main
};
typedef struct customer Customer;

queue<Customer> customerRequests;
pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;//queue mutex
//conditional variables for the queue
pthread_cond_t notEmpty = PTHREAD_COND_INITIALIZER; 
pthread_cond_t notFull = PTHREAD_COND_INITIALIZER;  
//ticket mutex
pthread_mutex_t ticketMutex = PTHREAD_MUTEX_INITIALIZER;
//file reading flag
bool doneReading = false;
pthread_mutex_t printMutex = PTHREAD_MUTEX_INITIALIZER;//print mutex
pthread_cond_t condPrint = PTHREAD_COND_INITIALIZER;//print condition
int nextToPrint = 0;//uses the sequence number

//agents : each agent one thread, services multiple customers, but one each
void* bookingAgent(void* arg) {
    while (true) {
        //get customer info
        pthread_mutex_lock(&queueMutex);
        //check for eof 
        while (customerRequests.empty() && !doneReading) {
            pthread_cond_wait(&notEmpty, &queueMutex);                  
        }
        //exit to go print final number of tickets if eof for input
        if (customerRequests.empty() && doneReading) {
            pthread_mutex_unlock(&queueMutex);
            break;
        }
        //read from file and make an instance of
        Customer currentCustomer = customerRequests.front();
        customerRequests.pop();
        //update conditional variables
        pthread_cond_signal(&notFull);
        pthread_mutex_unlock(&queueMutex);

        //allocate tickets
        int allocated = 0;
        pthread_mutex_lock(&ticketMutex);
        if (totalTickets >= currentCustomer.tickets) {
            allocated = currentCustomer.tickets;
            totalTickets -= currentCustomer.tickets;
        } else if (totalTickets > 0) {
            allocated = totalTickets;
            totalTickets = 0;
        } else {
            allocated = 0;
        }
        pthread_mutex_unlock(&ticketMutex); // unlock mutex

        //print in sequence
        pthread_mutex_lock(&printMutex);
        while (currentCustomer.sequence != nextToPrint) {
            pthread_cond_wait(&condPrint, &printMutex);            
        }
        cout << "Customer " << currentCustomer.name << " requested " 
             << currentCustomer.tickets << " tickets\n";
        cout << "Customer " << currentCustomer.name << " given " 
             << allocated << " tickets\n";
        nextToPrint++;
        pthread_cond_broadcast(&condPrint);//update conditional variables
        pthread_mutex_unlock(&printMutex);
    }
    return nullptr;
}

int main(int argc, char* argv[]) {
    //optional command line input for number of agents, try scaling for dynamic no. of agents based on input and resources later
    if (argc >= 2) {
        numAgents = atoi(argv[1]);
        if (numAgents <= 0) {
            cerr << "Number of agents must be positive. Using default 5 agents.\n";
            numAgents = 5;
        }
    }
    // input file
    ifstream file("sample_input.txt");
    if (!file) {
        cerr << "Error: Could not open sample_input.txt\n";
        exit(1);
    }
    file >> totalTickets;

    //init threads
    pthread_t agents[numAgents];
    for (int i = 0; i < numAgents; i++) {
        if (pthread_create(&agents[i], nullptr, bookingAgent, nullptr) != 0) {
            cerr << "Error: Unable to create thread " << i << "\n";
            exit(1);
        }
    }

    // enqueue
    int seq = 0;//sequence counter for ordering the outputs
    string name;
    int tickets;
    while (file >> name >> tickets) {
        Customer cust;
        cust.name = name;
        cust.tickets = tickets;
        cust.sequence = seq++; 
        pthread_mutex_lock(&queueMutex);
        while (customerRequests.size() >= MAX_BUFFER_SIZE) {
            pthread_cond_wait(&notFull, &queueMutex);
        }
        //once free
        customerRequests.push(cust);
        //update conditional variables
        pthread_cond_signal(&notEmpty);
        pthread_mutex_unlock(&queueMutex);
    }
    file.close();

    //close file reading
    pthread_mutex_lock(&queueMutex);
    doneReading = true;
    pthread_cond_broadcast(&notEmpty);
    pthread_mutex_unlock(&queueMutex);//let go mutexes

    //join ops
    for (int i = 0; i < numAgents; i++) {
        pthread_join(agents[i], nullptr);
    }
    //finally print remaininf tickets
    cout << "Remaining tickets: " << totalTickets << "\n";
    return 0;
}
